/*
 * Copyright (c) 2014-2016,2019 Greg Becker.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <netdb.h>
#include <errno.h>
#include <assert.h>
#include <time.h>
#include <sysexits.h>
#include <sys/select.h>
#include <sys/mman.h>

#include <limits.h>
#include <unistd.h>
#include <pthread.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <rpc/types.h>
#include <rpc/auth.h>
#include <rpc/rpc.h>

#include "main.h"
#include "nct.h"
#include "nct_rpc.h"
#include "nct_req.h"

void *
nct_req_recv_loop(void *arg)
{
    struct nct_stats stats;
    uint64_t tsc_stats, tsc_stop, tsc_diff;
    nct_mnt_t *mnt = arg;
    nct_req_t *req0;
    uint32_t *markp;
    nct_msg_t *msg;
    int rc;

    req0 = nct_req_alloc(mnt);
    if (!req0)
        abort();

    msg = req0->req_msg;
    assert(msg);

    /* Don't wait for a subsequent RPC record mark if there isn't
     * sufficient parallelism.
     */
    markp = (mnt->mnt_jobs_max > 3) ? &mnt->mnt_recv_mark : NULL;

    /* Update the mount stats record at most once per millisecond
     * to reduce contention.
     */
    bzero(&stats, sizeof(stats));
    tsc_stats = (rdtsc() % 777) * 1000;
    tsc_stats = (tsc_stats * 1000000) / tsc_freq;
    tsc_stats += rdtsc();

    while (1) {
        const size_t rpcmin = BYTES_PER_XDR_UNIT * 6;
        enum clnt_stat stat;
        nct_req_t *req;
        nct_msg_t *tmp;
        ssize_t cc;
        u_int idx;
        int i;

        pthread_mutex_lock(&mnt->mnt_recv_mtx);
        cc = nct_rpc_recv(mnt->mnt_fd, msg->msg_data, NCT_MSGSZ_MAX, markp);

        if (cc < rpcmin) {
            mnt->mnt_recv_mark = 0;
            pthread_mutex_unlock(&mnt->mnt_recv_mtx);

            if (cc == 0)
                break;

            eprint("nct_rpc_recv() failed: %s\n", (cc == -1) ? strerror(errno) : "eof");

            /* TODO: Need to rework the reconnect logic now
             * that we can have multiple recv threads...
             */
            rc = nct_connect(mnt);
            if (rc)
                abort();

            /* Re-send all the pending inflight requests.
             */
            for (i = 0; i < NCT_REQ_MAX; ++i) {
                req = mnt->mnt_req_tbl[i];
                if (req->req_tsc_start > req->req_tsc_stop) {
                    req->req_tsc_stop = rdtsc();
                    mnt->mnt_stats.latency_cum += req->req_tsc_stop - req->req_tsc_start;
                    nct_req_send(req);
                }
            }

            continue;
        }

        if (mnt->mnt_recv_mark)
            ++stats.marks;
        pthread_mutex_unlock(&mnt->mnt_recv_mtx);

        stat = nct_rpc_decode(&msg->msg_xdr, msg->msg_data, cc, &msg->msg_rpc, &msg->msg_err);

        if (stat != RPC_SUCCESS) {
            dprint(1, "nct_rpc_decode(%p, %ld) failed: %d %s\n",
                   msg, cc, stat, clnt_sperrno(stat));

            /* TODO: For which errors is rm_xid not valid?
             */
            if (stat == RPC_CANTDECODERES)
                abort();
        }

        idx = msg->msg_rpc.rm_xid % NCT_REQ_MAX;
        req = mnt->mnt_req_tbl[idx];
        mnt->mnt_req_tbl[idx] = NULL;

        if (req->req_xid != msg->msg_rpc.rm_xid)
            abort();

        req->req_tsc_stop = rdtsc();
        tsc_stop = req->req_tsc_stop;

        /* Update cumulative stats.
         */
        tsc_diff = tsc_stop - req->req_tsc_start;
        stats.latency_cum += req->req_tsc_stop - req->req_tsc_start;
        stats.thruput_send += req->req_msg->msg_len;
        stats.thruput_recv += cc;
        stats.requests++;

        msg->msg_len = cc;
        msg->msg_stat = stat;

        /* Exchange the message buffer.
         */
        tmp = req->req_msg;
        req->req_msg = msg;
        msg = tmp;

        req->req_done = true;

        if (req->req_cb) {
            rc = req->req_cb(req);
            if (rc) {
                int n;

                n = __atomic_sub_fetch(&mnt->mnt_jobs_cnt, 1, __ATOMIC_SEQ_CST);
                if (n == 0)
                    shutdown(mnt->mnt_fd, SHUT_WR);
            }
        }
        else {
            pthread_mutex_lock(&mnt->mnt_wait_mtx);
            if (mnt->mnt_wait_waiters > 0)
                pthread_cond_broadcast(&mnt->mnt_wait_cv);
            pthread_mutex_unlock(&mnt->mnt_wait_mtx);
        }

        if (tsc_stop < tsc_stats)
            continue;

        pthread_spin_lock(&mnt->mnt_stats_spin);
        if (tsc_diff < mnt->mnt_stats.latency_min)
            mnt->mnt_stats.latency_min = tsc_diff;
        if (tsc_diff > mnt->mnt_stats.latency_max)
            mnt->mnt_stats.latency_max = tsc_diff;
        mnt->mnt_stats.latency_cum += stats.latency_cum;
        mnt->mnt_stats.thruput_send += stats.thruput_send;
        mnt->mnt_stats.thruput_recv += stats.thruput_recv;
        mnt->mnt_stats.requests += stats.requests;
        mnt->mnt_stats.marks += stats.marks;
        mnt->mnt_stats.updates++;
        pthread_spin_unlock(&mnt->mnt_stats_spin);

        /* Extend the next stats update by 1000us.
         */
        tsc_stats += (1000 * 1000000) / tsc_freq;
        bzero(&stats, sizeof(stats));
    }

    pthread_exit(NULL);
}

/* Send a request.
 */
void
nct_req_send(nct_req_t *req)
{
    nct_mnt_t *mnt = req->req_mnt;
    struct rpc_msg *msg;
    uint32_t xid;
    ssize_t cc;

    msg = (void *)req->req_msg->msg_data + 4;
    req->req_done = false;

    /* Increase the xid by a prime number to reduce cache line
     * thrashing on mnt_req_tbl[] between send and recv threads.
     */
    pthread_mutex_lock(&mnt->mnt_send_mtx);
    xid = mnt->mnt_send_xid;
    mnt->mnt_send_xid += 11;

    msg->rm_xid = htonl(xid);
    mnt->mnt_req_tbl[xid % NCT_REQ_MAX] = req;
    req->req_xid = xid;

    cc = nct_rpc_send(mnt->mnt_fd, req->req_msg->msg_data, req->req_msg->msg_len);
    pthread_mutex_unlock(&mnt->mnt_send_mtx);

    if (cc != req->req_msg->msg_len) {
        // TODO...
        abort();
    }
}

/* Wait for the reply to the specified request to arrive.
 */
void
nct_req_wait(nct_req_t *req)
{
    nct_mnt_t *mnt = req->req_mnt;

    pthread_mutex_lock(&mnt->mnt_wait_mtx);
    while (!req->req_done) {
        ++mnt->mnt_wait_waiters;
        pthread_cond_wait(&mnt->mnt_wait_cv, &mnt->mnt_wait_mtx);
        --mnt->mnt_wait_waiters;
    }
    pthread_mutex_unlock(&mnt->mnt_wait_mtx);
}

/* Allocate a request object from the free pool.
 */
nct_req_t *
nct_req_alloc(nct_mnt_t *mnt)
{
    nct_req_t *req;

    pthread_mutex_lock(&mnt->mnt_req_mtx);
    while (!mnt->mnt_req_head) {
        ++mnt->mnt_req_waiters;
        pthread_cond_wait(&mnt->mnt_req_cv, &mnt->mnt_req_mtx);
        --mnt->mnt_req_waiters;
    }

    req = mnt->mnt_req_head;
    mnt->mnt_req_head = req->req_next;
    pthread_mutex_unlock(&mnt->mnt_req_mtx);

    req->req_cb = NULL;
    req->req_done = 0;

    return req;
}

/* Return a request object to the free pool.
 */
void
nct_req_free(nct_req_t *req)
{
    nct_mnt_t *mnt = req->req_mnt;

    pthread_mutex_lock(&mnt->mnt_req_mtx);
    req->req_next = mnt->mnt_req_head;
    mnt->mnt_req_head = req;
    if (mnt->mnt_req_waiters > 0) {
        pthread_cond_signal(&mnt->mnt_req_cv);
    }
    pthread_mutex_unlock(&mnt->mnt_req_mtx);
}

/* Create a pool of request objects.
 */
void
nct_req_create(nct_mnt_t *mnt)
{
    void *msgbase, *reqbase;
    size_t msgsz, tblsz, sz;
    int flags, prot;
    nct_req_t *req;
    int super = 0;
    int i;

#if __FreeBSD__
    super = MAP_ALIGNED_SUPER;
#elif __linux__
    super = MAP_HUGETLB;
#endif

    flags = MAP_ANONYMOUS | MAP_PRIVATE;
    prot = PROT_READ | PROT_WRITE;

    msgsz = sizeof(nct_msg_t) + NCT_MSGSZ_MAX;
    sz = (NCT_REQ_MAX + 1) * msgsz;
    sz = (sz + (2u << 20) - 1) & ~((2u << 20) - 1);

  again:
    msgbase = mmap(NULL, sz, prot, flags | super, -1, 0);
    if (msgbase == MAP_FAILED) {
        if (super) {
            super = 0;
            goto again;
        }

        dprint(0, "mmap(%zu, %x, %x) failed: %s\n",
               sz, prot, flags, strerror(errno));
        abort();
    }

    tblsz = NCT_REQ_MAX * sizeof(req) * 2;
    tblsz = (tblsz + 4096 - 1) & ~(4096 - 1);

    sz = NCT_REQ_MAX * sizeof(*req) + tblsz;
    sz = (sz + (2 << 20) - 1) & ~((2 << 20) - 1);

    reqbase = mmap(NULL, sz, prot, flags | super, -1, 0);
    if (reqbase == MAP_FAILED) {
        abort();
    }

    mnt->mnt_req_tbl = reqbase;
    reqbase += tblsz;

    for (i = 0; i < NCT_REQ_MAX; ++i) {
        req = (nct_req_t *)reqbase + i;

        memset(req, 0, sizeof(*req));
        req->req_mnt = mnt;
        req->req_msg = msgbase + (i * msgsz);

        pthread_mutex_lock(&mnt->mnt_send_mtx);
        req->req_next = mnt->mnt_req_head;
        mnt->mnt_req_head = req;
        pthread_mutex_unlock(&mnt->mnt_send_mtx);
    }
}
