/*
 * Copyright (c) 2014-2015,2019 Greg Becker.  All rights reserved.
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

#ifndef NCT_MOUNT_H
#define NCT_MOUNT_H

#include "nct_nfstypes.h"
#include "nct_vnode.h"
#include "nct_req.h"

#ifndef __aligned
#define __aligned(_n)   __attribute__((__aligned__(_n)))
#endif

#ifndef NELEM
#define NELEM(_a)       (sizeof((_a)) / sizeof((_a)[0]))
#endif

struct nct_stats {
    uint64_t            latency;      // Total latency of completed requests
    uint64_t            requests;     // Total number of requests completed
    uint64_t            thruput_send;
    uint64_t            thruput_recv;
    uint64_t            updates;
};

typedef struct nct_mnt_s {
    pthread_mutex_t     mnt_send_mtx;
    uint32_t            mnt_send_xid;
    pthread_cond_t      mnt_send_cv;

    __aligned(64)
    pthread_mutex_t     mnt_recv_mtx;
    uint32_t            mnt_recv_mark;

    __aligned(64)
    pthread_mutex_t     mnt_wait_mtx;
    int                 mnt_wait_waiters;
    pthread_cond_t      mnt_wait_cv;

    __aligned(64)
    pthread_mutex_t     mnt_req_mtx;
    nct_req_t          *mnt_req_head;           // List of free reqs
    nct_req_t         **mnt_req_tbl;            // Indexed by req_idx
    int                 mnt_req_waiters;
    pthread_cond_t      mnt_req_cv;

    __aligned(64)
    pthread_spinlock_t  mnt_stats_spin;
    struct nct_stats    mnt_stats;

    __aligned(64)
    u_int               mnt_jobs_max;
    u_int               mnt_jobs_cnt;
    u_int               mnt_tds_max;

    __aligned(64)
    int                 mnt_fd;
    nct_vn_t           *mnt_vn;
    AUTH               *mnt_auth;
    char               *mnt_server;             // NFS server host name
    char                mnt_serverip[INET_ADDRSTRLEN + 1];
    char               *mnt_path;               // Path on server
    char               *mnt_user;               // NFS server user name
    in_port_t           mnt_port;
    struct sockaddr_in  mnt_faddr;              // Foriegn/filer address

    char                mnt_hostname[_POSIX_HOST_NAME_MAX + 1];
    pthread_t           mnt_recv_tdv[128];
    char                mnt_args[];
} nct_mnt_t;

extern nct_mnt_t *nct_mount(const char *path, in_port_t port, u_int tds_max, u_int jobs_max);
extern void nct_umount(nct_mnt_t *mnt);
extern void nct_mnt_print(nct_mnt_t *mnt);

extern int nct_connect(nct_mnt_t *mnt);

#endif // NCT_MOUNT_H
