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
 *
 * $Id: nct_req.c 392 2016-04-13 11:21:51Z greg $
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
#include <time.h>
#include <sysexits.h>
#include <sys/select.h>

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
nct_req_send_loop(void *arg)
{
    nct_mnt_t *mnt = arg;

    while (1) {
        nct_req_t *head, *req;
        ssize_t cc;

        pthread_mutex_lock(&mnt->mnt_send_mtx);
        while (!mnt->mnt_send_head) {
            ++mnt->mnt_send_waiters;
            pthread_cond_wait(&mnt->mnt_send_cv, &mnt->mnt_send_mtx);
            --mnt->mnt_send_waiters;

            if (mnt->mnt_worker_cnt < 1 && !mnt->mnt_send_head) {
                dprint(2, "exiting due to no workers...\n");
                return NULL;
            }
        }

        head = mnt->mnt_send_head;
        mnt->mnt_send_head = NULL;
        pthread_mutex_unlock(&mnt->mnt_send_mtx);

        while (( req = head )) {
            head = req->req_next;
            req->req_tsc_start = rdtsc();

            cc = nct_rpc_send(mnt->mnt_fd, req->req_msg->msg_data, req->req_msg->msg_len);
            if (cc != req->req_msg->msg_len) {
                eprint("nct_rpc_send() failed: %s\n",
                       (cc == -1) ? strerror(errno) : "eof or short write");

                sleep(1);   // Give time for nct_rx_main() to reconnect

                head = req; // Re-issue this request
            }
        }
    }

    pthread_exit(NULL);
}

void *
nct_req_recv_loop(void *arg)
{
    nct_mnt_t *mnt = arg;
    nct_msg_t *msg;
    int rc;

    msg = malloc(sizeof(*msg) + NCT_MSGSZ_MAX);
    if (!msg) {
        abort();
    }

    while (1) {
        enum clnt_stat stat;
        nct_req_t *req;
        nct_msg_t *tmp;
        ssize_t cc;
        int i;

        cc = nct_rpc_recv(mnt->mnt_fd, msg->msg_data, NCT_MSGSZ_MAX);
        if (cc <= 0) {
            if (mnt->mnt_worker_cnt < 1) {
                dprint(2, "exiting due to no workers...\n");
                break;
            }

            eprint("nct_rpc_recv() failed: %s\n", (cc == -1) ? strerror(errno) : "eof");

            rc = nct_connect(mnt);
            if (rc) {
                abort();
            }
            /* Re-send all the pending inflight requests.
             */
            for (i = 0; i < NCT_REQ_MAX; ++i) {
                req = mnt->mnt_req_tbl[i];
                if (req->req_tsc_start > req->req_tsc_stop) {
                    req->req_tsc_stop = rdtsc();
                    mnt->mnt_stats_latency += req->req_tsc_stop - req->req_tsc_start;
                    nct_req_send(req);
                }
            }
            continue;
        }

        stat = nct_rpc_decode(&msg->msg_xdr, msg->msg_data, cc, &msg->msg_rpc, &msg->msg_err);

        if (stat != RPC_SUCCESS) {
            dprint(1, "nct_rpc_decode(%p, %ld) failed: %d %s\n",
                   msg, cc, stat, clnt_sperrno(stat));

            /* TODO: For which errors is rm_xid not valid?
             */
            if (stat == RPC_CANTDECODERES) {
                abort();
            }
        }

        req = mnt->mnt_req_tbl[msg->msg_rpc.rm_xid & (NCT_REQ_MAX - 1)];
        if (req->req_xid != msg->msg_rpc.rm_xid) {
            abort();
        }

        req->req_tsc_stop = rdtsc();

        /* Update cumulative stats.
         */
        mnt->mnt_stats_latency += req->req_tsc_stop - req->req_tsc_start;
        mnt->mnt_stats_throughput_send += req->req_msg->msg_len;
        mnt->mnt_stats_throughput_recv += cc;
        mnt->mnt_stats_requests++;

        msg->msg_len = cc;
        msg->msg_stat = stat;

        /* Exchange the message buffer.
         */
        tmp = req->req_msg;
        req->req_msg = msg;
        msg = tmp;

        pthread_mutex_lock(&mnt->mnt_recv_mtx);
        if (req->req_cb) {
            req->req_next = mnt->mnt_recv_head;
            mnt->mnt_recv_head = req;
        }

        if (mnt->mnt_recv_waiters > 0) {
            pthread_cond_signal(&mnt->mnt_recv_cv);
        }
        req->req_done = 1;
        pthread_mutex_unlock(&mnt->mnt_recv_mtx);
    }

    pthread_exit(NULL);
}

/* Send a request asynchronously.
 */
void
nct_req_send(nct_req_t *req)
{
    nct_mnt_t *mnt = req->req_mnt;

    req->req_done = 0;

    pthread_mutex_lock(&mnt->mnt_send_mtx);
    req->req_next = mnt->mnt_send_head;
    mnt->mnt_send_head = req;
    if (mnt->mnt_send_waiters > 0) {
        pthread_cond_signal(&mnt->mnt_send_cv);
    }
    pthread_mutex_unlock(&mnt->mnt_send_mtx);
}

/* Wait for any reply to arrive and then process it.
 */
int
nct_req_recv(nct_mnt_t *mnt)
{
    nct_req_t *req;

    pthread_mutex_lock(&mnt->mnt_recv_mtx);
    while (!mnt->mnt_recv_head) {
        ++mnt->mnt_recv_waiters;
        pthread_cond_wait(&mnt->mnt_recv_cv, &mnt->mnt_recv_mtx);
        --mnt->mnt_recv_waiters;
    }

    req = mnt->mnt_recv_head;
    mnt->mnt_recv_head = req->req_next;
    pthread_mutex_unlock(&mnt->mnt_recv_mtx);

    return req->req_cb(req);
}

/* Wait for the reply to the specified request to arrive.
 */
void
nct_req_wait(nct_req_t *req)
{
    nct_mnt_t *mnt = req->req_mnt;

    pthread_mutex_lock(&mnt->mnt_recv_mtx);
    while (!req->req_done) {
        ++mnt->mnt_recv_waiters;
        pthread_cond_wait(&mnt->mnt_recv_cv, &mnt->mnt_recv_mtx);
        --mnt->mnt_recv_waiters;
    }
    pthread_mutex_unlock(&mnt->mnt_recv_mtx);
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
    nct_req_t *req;
    void *msgbase;
    void *reqbase;
    size_t msgsz;
    int rc;
    int i;

    msgsz = sizeof(nct_msg_t) + NCT_MSGSZ_MAX;

    /* Attempt to allocate all the reqs and I/O buffer memory from superpages.
     */
    rc = posix_memalign(&msgbase, 1024 * 1024 * 2, NCT_REQ_MAX * msgsz);
    if (rc || msgbase == NULL) {
        abort();
    }

    rc = posix_memalign(&reqbase, 1024 * 1024 * 2, NCT_REQ_MAX * sizeof(*req));
    if (rc || reqbase == NULL) {
        abort();
    }

    mnt->mnt_req_tbl = calloc(NCT_REQ_MAX, sizeof(req));
    if (!mnt->mnt_req_tbl) {
        abort();
    }

    for (i = 0; i < NCT_REQ_MAX; ++i) {
        req = (nct_req_t *)reqbase + i;

        memset(req, 0, sizeof(*req));
        req->req_idx = i;
        req->req_mnt = mnt;
        req->req_msg = (nct_msg_t *)((char *)msgbase + (i * msgsz));

        mnt->mnt_req_tbl[req->req_idx] = req;

        pthread_mutex_lock(&mnt->mnt_send_mtx);
        req->req_next = mnt->mnt_req_head;
        mnt->mnt_req_head = req;
        pthread_mutex_unlock(&mnt->mnt_send_mtx);
    }
}
