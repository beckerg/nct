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

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <stdatomic.h>
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
#include "nct_rpc.h"

ssize_t
nct_rpc_send(int fd, void *buf, size_t bufsz)
{
    uint32_t mark;
    size_t nleft;
    ssize_t cc;

    mark = htonl((bufsz - sizeof(mark)) | 0x80000000u);
    memcpy(buf, &mark, sizeof(mark));

    nleft = bufsz;

    while (nleft > 0) {
        cc = send(fd, buf, nleft, 0);
        if (cc < 1) {
            return (cc == -1) ? -1 : 0;
        }

        nleft -= cc;
        buf += cc;
    }

    return bufsz;
}

ssize_t
nct_rpc_recv(int fd, void *buf, size_t bufsz)
{
    static uint32_t mark = 0;
    const size_t marksz = sizeof(mark);
    size_t nleft;
    ssize_t len;
    ssize_t cc;

    if (mark == 0) {
        cc = recv(fd, &mark, marksz, MSG_WAITALL);
        if (cc != marksz)
            return (cc == -1) ? -1 : 0;
    }

    mark = ntohl(mark);

    if (!(mark & 0x80000000u))
        abort();

    mark &= ~0x80000000u;
    if (mark + marksz > bufsz)
        abort();

    /* Try to read the next mark...
     */
    nleft = mark + marksz;
    len = mark;

    while (nleft > 0) {
        cc = recv(fd, buf, nleft, 0);
        if (cc < 1)
            return (cc == -1) ? -1 : 0;

        nleft -= cc;
        buf += cc;

        if (nleft == marksz) {
            mark = 0;
            return len;
        }
    }

    memcpy(&mark, buf - marksz, marksz);

    return len;
}

int
nct_rpc_encode(struct rpc_msg *msg, AUTH *auth,
               xdrproc_t xdrproc, void *args,
               char *buf, int bufsz)
{
    int len = -1;
    XDR xdr;

    if (!buf || bufsz < BYTES_PER_XDR_UNIT * 6 + 4)
        abort();

    if (auth) {
        msg->rm_call.cb_cred = auth->ah_cred;
        msg->rm_call.cb_verf = auth->ah_verf;
    } else {
        msg->rm_call.cb_cred = _null_auth;
        msg->rm_call.cb_verf = _null_auth;
    }

    /* Create the serialized RPC message, leaving room at the
     * front for the RPC record mark.
     */
    xdrmem_create(&xdr, buf + 4, bufsz - 4, XDR_ENCODE);

    if (xdr_callmsg(&xdr, msg) && (*xdrproc)(&xdr, args))
        len = xdr_getpos(&xdr) + 4;

    xdr_destroy(&xdr);

    return len;
}

enum clnt_stat
nct_rpc_decode(XDR *xdr, char *buf, int len,
               struct rpc_msg *rpc_msg, struct rpc_err *rpc_err)
{
    xdrmem_create(xdr, buf, len, XDR_DECODE);

    rpc_msg->acpted_rply.ar_verf = _null_auth;
    rpc_msg->acpted_rply.ar_results.where = NULL;
    rpc_msg->acpted_rply.ar_results.proc = (xdrproc_t)xdr_void;

    if (xdr_replymsg(xdr, rpc_msg)) {
        _seterr_reply(rpc_msg, rpc_err);
    } else {
        rpc_err->re_status = RPC_CANTDECODERES;
    }

    return rpc_err->re_status;
}
