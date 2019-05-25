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
 * $Id: nct_nfs.c 392 2016-04-13 11:21:51Z greg $
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
#include <sysexits.h>

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
#include "nct_rpc.h"
#include "nct_xdr.h"
#include "nct.h"

static const char *
strerror_mountstat3(enum mountstat3 stat)
{
    switch (stat) {
    case MNT3ERR_NOTSUPP:
        return "mnt3err_notsupp";

    case MNT3ERR_SERVERFAULT:
        return "mnt3err_serverfault";

    default:
        if (stat < MNT3ERR_NOTSUPP)
            return strerror(stat);
        break;
    }

    return "invalid mountstat3 value";
}

void
nct_nfs_mount(struct nct_mnt_s *mnt)
{
    const size_t rpcmin = BYTES_PER_XDR_UNIT * 6;
    char txbuf[1024], rxbuf[1024];
    struct sockaddr_in faddr;
    struct rpc_msg msg;
    struct rpc_err err;
    enum clnt_stat stat;
    mountres3 mntres;
    in_port_t port;
    ssize_t cc;
    char *path;
    int txlen;
    XDR xdr;
    int fd;
    int rc;

    memset(&faddr, 0, sizeof(faddr));
    faddr.sin_family = AF_INET;
    faddr.sin_addr.s_addr = inet_addr(mnt->mnt_serverip);

    port = pmap_getport(&faddr, MOUNT_PROGRAM, MOUNT_V3, IPPROTO_TCP);
    if (port == 0) {
        eprint("pmap_getport(MOUNT_V3) failed: %s\n", clnt_spcreateerror(""));
        abort();
    }

    faddr.sin_port = htons(port);

    fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd == -1) {
        eprint("socket() failed: %s\n", strerror(errno));
        abort();
    }

    if (geteuid() == 0) {
#ifdef __FreeBSD__
        int opt = IP_PORTRANGE_LOW;

        rc = setsockopt(fd, IPPROTO_IP, IP_PORTRANGE, &opt, sizeof(opt));
#endif
    }

    rc = connect(fd, (struct sockaddr *)&faddr, sizeof(struct sockaddr_in));
    if (rc) {
        eprint("connect(%d, ...) failed: %s\n", fd, strerror(errno));
        abort();
    }

    msg.rm_xid = 0;
    msg.rm_direction = CALL;
    msg.rm_call.cb_rpcvers = RPC_MSG_VERSION;
    msg.rm_call.cb_prog = MOUNT_PROGRAM;
    msg.rm_call.cb_vers = MOUNT_V3;
    msg.rm_call.cb_proc = MOUNT3_MNT;

    path = mnt->mnt_path;

    txlen = nct_rpc_encode(&msg, mnt->mnt_auth,
                           (xdrproc_t)nct_xdr_dirpath, &path,
                           txbuf, sizeof(txbuf));

    cc = nct_rpc_send(fd, txbuf, txlen);
    if (cc != txlen) {
        eprint("nct_rpc_send(%d, %p, %d) failed: %s\n",
               fd, txbuf, txlen, (cc == -1) ? strerror(errno) : "EOF");
        abort();
    }

    cc = nct_rpc_recv(fd, rxbuf, sizeof(rxbuf), NULL);
    if (cc < rpcmin) {
        eprint("nct_rpc_recv(%d, %p, %zu): cc %ld, %s\n",
               fd, rxbuf, sizeof(rxbuf), cc,
               (cc == -1) ? strerror(errno) : "EOF");
        abort();
    }

    stat = nct_rpc_decode(&xdr, rxbuf, cc, &msg, &err);
    if (stat != RPC_SUCCESS) {
        eprint("nct_rpc_decode(%p, %ld) failed: %d %s\n",
               rxbuf, cc, stat, clnt_sperrno(stat));
        abort();
    }

    if (!nct_xdr_mountres3_decode(xdr.x_private, xdr.x_handy, &mntres)) {
        eprint("nct_xdr_mountres3_decode() failed\n");
        abort();
    }

    if (mntres.fhs_status != MNT3_OK) {
        eprint("mount %s:%s failed: %d %s\n",
               mnt->mnt_server, mnt->mnt_path,
               mntres.fhs_status, strerror_mountstat3(mntres.fhs_status));

        switch (mntres.fhs_status) {
        case MNT3ERR_PERM:
        case MNT3ERR_ACCES:
            exit(EX_NOPERM);

        case MNT3ERR_IO:
            exit(EX_IOERR);

        case MNT3ERR_NOENT:
        case MNT3ERR_NOTDIR:
        case MNT3ERR_INVAL:
        case MNT3ERR_NAMETOOLONG:
            exit(EX_DATAERR);

        default:
            exit(EX_PROTOCOL);
            break;
        }

        exit(EX_OSERR);
    }

    XDR_DESTROY(&xdr);

    mnt->mnt_vn = nct_vn_alloc(&mntres.mountres3_u.mountinfo.fhandle, "/", 1);
    if (!mnt->mnt_vn) {
        eprint("nct_vn_alloc() failed\n");
        abort();
    }

    if (verbosity > 0) {
        char buf[136];
        int i;

        for (i = 0; i < mnt->mnt_vn->xvn_fh.fhandle3_len; ++i) {
            snprintf(buf + i*2, 3, "%02x", mnt->mnt_vn->xvn_fh.fhandle3_val[i] & 0xff);
        }

        dprint(1, "rx cc=%ld len=%u %s\n",
               cc, mntres.mountres3_u.mountinfo.fhandle.fhandle3_len, buf);
    }

    close(fd);
}

void
nct_nfs_null_encode(nct_req_t *req)
{
    //nct_mnt_t *mnt = req->req_mnt;
    struct rpc_msg msg;
    int len;

    msg.rm_xid = 0;
    msg.rm_direction = CALL;
    msg.rm_call.cb_rpcvers = RPC_MSG_VERSION;
    msg.rm_call.cb_prog = NFS_PROGRAM;
    msg.rm_call.cb_vers = NFS_V3;
    msg.rm_call.cb_proc = NFS3_NULL;

    len = nct_rpc_encode(&msg, NULL,
                         (xdrproc_t)xdr_void, NULL,
                         req->req_msg->msg_data, NCT_MSGSZ_MAX);

    req->req_msg->msg_len = len;
}

void
nct_nfs_getattr3_encode(nct_req_t *req)
{
    nct_mnt_t *mnt = req->req_mnt;
    struct rpc_msg msg;
    int len;

    msg.rm_xid = 0;
    msg.rm_direction = CALL;
    msg.rm_call.cb_rpcvers = RPC_MSG_VERSION;
    msg.rm_call.cb_prog = NFS_PROGRAM;
    msg.rm_call.cb_vers = NFS_V3;
    msg.rm_call.cb_proc = NFS3_GETATTR;

    len = nct_rpc_encode(&msg, mnt->mnt_auth,
                         (xdrproc_t)nct_xdr_getattr3_encode, &mnt->mnt_vn->xvn_fh,
                         req->req_msg->msg_data, NCT_MSGSZ_MAX);

    req->req_msg->msg_len = len;
}

void
nct_nfs_read3_encode(nct_req_t *req, off_t offset, size_t length)
{
    nct_mnt_t *mnt = req->req_mnt;
    struct rpc_msg msg;
    read3_args args;
    int len;

    msg.rm_xid = 0;
    msg.rm_direction = CALL;
    msg.rm_call.cb_rpcvers = RPC_MSG_VERSION;
    msg.rm_call.cb_prog = NFS_PROGRAM;
    msg.rm_call.cb_vers = NFS_V3;
    msg.rm_call.cb_proc = NFS3_READ;

    args.file.data.data_len = mnt->mnt_vn->xvn_fh.fhandle3_len;
    args.file.data.data_val = mnt->mnt_vn->xvn_fh.fhandle3_val;
    args.offset = offset;
    args.count = length;

    len = nct_rpc_encode(&msg, mnt->mnt_auth,
                         (xdrproc_t)nct_xdr_read3_encode, &args,
                         req->req_msg->msg_data, NCT_MSGSZ_MAX);

    req->req_msg->msg_len = len;
}
