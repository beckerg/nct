/*
 * Copyright (c) 2014-2015 Greg Becker.  All rights reserved.
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
 * $Id: nct_rpc.c 283 2015-01-02 11:42:52Z greg $
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
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

#include "nct_rpc.h"

uint32_t
nct_rpc_xid(void)
{
    static uint32_t xid;

    return __sync_fetch_and_add(&xid, 1);
}

ssize_t
nct_rpc_send(int fd, const void *msg, size_t len)
{
    uint32_t *mark;
    size_t nleft;
    ssize_t cc;

    mark = (uint32_t *)msg;
    *mark = htonl(0x80000000 | len);
    len += sizeof(*mark);
    nleft = len;

    while (nleft > 0) {
        cc = send(fd, msg + (len - nleft), nleft, 0);
        if (cc <= 0) {
            return cc;
        }
        nleft -= cc;
    }

    return (len - sizeof(*mark));
}

ssize_t
nct_rpc_recv(int fd, void *buf, size_t len)
{
    size_t nleft = len;
    uint32_t mark;
    ssize_t cc;

    cc = recv(fd, &mark, sizeof(mark), MSG_WAITALL);
    if (cc != sizeof(mark)) {
        return cc;
    }

    nleft = ntohl(mark);
    if (!(0x80000000 & nleft)) {
        abort();
    }
    nleft &= ~0x80000000;
    len = nleft;

    while (nleft > 0) {
        cc = recv(fd, (char *)buf + (len - nleft), nleft, MSG_WAITALL);
        if (cc <= 0) {
            return errno;
        }
        nleft -= cc;
    }

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
