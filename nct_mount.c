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
 * $Id: nct_mount.c 392 2016-04-13 11:21:51Z greg $
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
#include "nct_nfs.h"
#include "nct_xdr.h"
#include "nct.h"

int
nct_connect(nct_mnt_t *mnt)
{
    int rc;
    int i;

    dprint(1, "connecting to %s...\n", mnt->mnt_server);

    for (i = 0; i < 5; ++i) {
        if (mnt->mnt_fd != -1) {
            close(mnt->mnt_fd);
        }

        mnt->mnt_fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (mnt->mnt_fd == -1) {
            eprint("socket() failed: %s\n", strerror(errno));
            sleep((i * 3) + 1);
            continue;
        }

        rc = connect(mnt->mnt_fd, (struct sockaddr *)&mnt->mnt_faddr, sizeof(mnt->mnt_faddr));
        if (rc) {
            eprint("connect to %s failed: %s\n", mnt->mnt_server, strerror(errno));
            sleep((i * 3) + 1);
            continue;
        }

        dprint(1, "connected to %s fd=%d\n", mnt->mnt_server, mnt->mnt_fd);

        return 0;
    }

    return ETIMEDOUT;
}

/* 1) Create a mount object
 * 2) Connect to the specified filer
 * 3) Start the send/recv request loops
 * 4) Retrieve the file handle of the last component in the specified path
 * 5) Retrieve the attributes for the file handle from (4)
 *
 * Where path is:  [user@]host[:/export...]
 */
nct_mnt_t *
nct_mount(const char *path, in_port_t port)
{
    struct hostent *hent;
    nct_mnt_t *mnt;
    nct_req_t *req;
    size_t mntsz;
    int pathlen;
    char *pc;
    int rc;

    pathlen = strlen(path);
    mntsz = sizeof(*mnt) + pathlen + 1;
    mnt = calloc(1, mntsz);
    if (!mnt) {
        eprint("calloc(%zu) failed\n", mntsz);
        exit(EX_OSERR);
    }

    strlcpy(mnt->mnt_args, path, pathlen + 1);

    mnt->mnt_server = mnt->mnt_args;
    mnt->mnt_user = "root";
    mnt->mnt_port = port;
    mnt->mnt_path = "/";
    mnt->mnt_fd = -1;

    pc = strchr(mnt->mnt_args, '@');
    if (pc) {
        mnt->mnt_user = mnt->mnt_args;
        *pc++ = '\000';
        mnt->mnt_server = pc;
    }

    pc = strchr(mnt->mnt_server, ':');
    if (pc) {
        *pc++ = '\000';
        mnt->mnt_path = pc;
    }

    if (!isalpha(mnt->mnt_server[0]) && !isdigit(mnt->mnt_server[0])) {
        eprint("invalid host name %s\n", mnt->mnt_server);
        exit(EX_NOHOST);
    }

    gethostname(mnt->mnt_hostname, sizeof(mnt->mnt_hostname));

    mnt->mnt_faddr.sin_family = AF_INET;
    mnt->mnt_faddr.sin_port = htons(mnt->mnt_port);

    hent = gethostbyname(mnt->mnt_server);
    if (!hent) {
        eprint("gethostbyname(%s) failed: %s\n", mnt->mnt_server, hstrerror(h_errno));
        exit(EX_NOHOST);
    }

    if (hent->h_addrtype != AF_INET) {
        eprint("host %s does not have an AF_INET address: %s\n", mnt->mnt_server);
        exit(EX_NOHOST);
    }

    if (!inet_ntop(AF_INET, hent->h_addr_list[0],
                   mnt->mnt_serverip, sizeof(mnt->mnt_serverip))) {
        eprint("unable to convert server address %s to dotted quad notation: %s",
               mnt->mnt_server, strerror(errno));
        exit(EX_NOHOST);
    }

    mnt->mnt_serverip[sizeof(mnt->mnt_serverip) - 1] = '\000';
    mnt->mnt_faddr.sin_addr.s_addr = inet_addr(mnt->mnt_serverip);

    mnt->mnt_auth = authunix_create(mnt->mnt_hostname, geteuid(), getegid(), 0, NULL);
    if (!mnt->mnt_auth) {
        eprint("authunix_create() failed\n");
        abort();
    }

    rc = nct_connect(mnt);
    if (rc) {
        eprint("nct_connect() failed\n");
        abort();
    }

    rc = pthread_mutex_init(&mnt->mnt_send_mtx, NULL);
    if (rc) {
        eprint("pthread_mutex_init() failed: %s\n", strerror(errno));
        abort();
    }

    rc = pthread_cond_init(&mnt->mnt_send_cv, NULL);
    if (rc) {
        eprint("pthread_cond_init() failed: %s\n", strerror(errno));
        abort();
    }

    rc = pthread_mutex_init(&mnt->mnt_recv_mtx, NULL);
    if (rc) {
        eprint("pthread_mutex_init() failed: %s\n", strerror(errno));
        abort();
    }

    rc = pthread_cond_init(&mnt->mnt_recv_cv, NULL);
    if (rc) {
        eprint("pthread_cond_init() failed: %s\n", strerror(errno));
        abort();
    }

    rc = pthread_mutex_init(&mnt->mnt_req_mtx, NULL);
    if (rc) {
        eprint("pthread_mutex_init() failed: %s\n", strerror(errno));
        abort();
    }

    rc = pthread_cond_init(&mnt->mnt_req_cv, NULL);
    if (rc) {
        eprint("pthread_cond_init() failed: %s\n", strerror(errno));
        abort();
    }

    rc = pthread_create(&mnt->mnt_send_td, NULL, nct_req_send_loop, mnt);
    if (rc) {
        eprint("pthread_create() failed: %s\n", strerror(errno));
        abort();
    }

    rc = pthread_create(&mnt->mnt_recv_td, NULL, nct_req_recv_loop, mnt);
    if (rc) {
        eprint("pthread_create() failed: %s\n", strerror(errno));
        abort();
    }

    nct_req_create(mnt);

    nct_nfs_mount(mnt);

    nct_mnt_print(mnt);

    /* Get the attributes
     */
    req = nct_req_alloc(mnt);
    nct_nfs_getattr3_encode(req);
    nct_req_send(req);
    nct_req_wait(req);

    if (req->req_msg->msg_stat != RPC_SUCCESS) {
        eprint("getattr failed: %d %s\n",
               req->req_msg->msg_stat, clnt_sperrno(req->req_msg->msg_stat));
        nct_req_free(req);
        return NULL;
    }

    getattr3_res res;
    bool_t ok;

    ok = nct_xdr_getattr3_decode(&req->req_msg->msg_xdr, &res);
    if (ok) {
        mnt->mnt_vn->xvn_fattr = res.u.resok.obj_attributes;

        dprint(1, "  File: \"%s\"\n",
               mnt->mnt_vn->xvn_name);

        dprint(1, "  Size: %lu    FileType: %u\n",
               mnt->mnt_vn->xvn_fattr.size,
               mnt->mnt_vn->xvn_fattr.type);

        dprint(1, "  Mode: (%03o/%s): (%u/%s)  Gid: (%u/%s)\n",
               mnt->mnt_vn->xvn_fattr.mode, "?",
               mnt->mnt_vn->xvn_fattr.uid, "?",
               mnt->mnt_vn->xvn_fattr.gid, "?");

        dprint(1, "  Device: %u,%u  Inode: %lu  Links: %u\n",
               0, 0,
               mnt->mnt_vn->xvn_fattr.fileid,
               mnt->mnt_vn->xvn_fattr.nlink);

    } else {
        eprint("getattr3 decode of %s:%s failed: %d\n",
               mnt->mnt_server, mnt->mnt_path, res.status);
        abort();
    }

    nct_req_free(req);

    return mnt;
}

void
nct_umount(nct_mnt_t *mnt)
{
    void *val;
    int rc;

    while (mnt->mnt_worker_cnt > 0) {
        sleep(3);
    }

    shutdown(mnt->mnt_fd, SHUT_RDWR);

    pthread_cond_broadcast(&mnt->mnt_send_cv);
    pthread_cond_broadcast(&mnt->mnt_recv_cv);

    rc = pthread_join(mnt->mnt_send_td, &val);
    if (rc) {
        abort();
    }

    rc = pthread_join(mnt->mnt_recv_td, &val);
    if (rc) {
        abort();
    }

    auth_destroy(mnt->mnt_auth);
    close(mnt->mnt_fd);

    nct_vn_free(mnt->mnt_vn);
    free(mnt);
}

void
nct_mnt_print(nct_mnt_t *mnt)
{
    dprint(1, "hostname %s\n", mnt->mnt_hostname);
    dprint(1, "server   %s\n", mnt->mnt_server);
    dprint(1, "path     %s\n", mnt->mnt_path);
    dprint(1, "user     %s\n", mnt->mnt_user);
    dprint(1, "port     %u\n", mnt->mnt_port);
    dprint(1, "fd       %d\n", mnt->mnt_fd);
    dprint(1, "auth     %p\n", mnt->mnt_auth);
    dprint(1, "send     %p\n", mnt->mnt_send_td);
    dprint(1, "recv     %p\n", mnt->mnt_recv_td);
}
