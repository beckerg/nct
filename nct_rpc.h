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
 * $Id: nct_rpc.h 285 2015-01-02 15:06:17Z greg $
 */
#ifndef NCT_RPC_H
#define NCT_RPC_H

#include "nct_nfstypes.h"

#define MOUNT_PROGRAM       (100005)
#define MOUNT_V3            (3)
#define MOUNT3_MNT          (1)

#define MNTPATHLEN          (1024)
#define FHSIZE3             (64)

#define NFS_PROGRAM         (100003)
#define NFS_V3              (3)

enum mountstat3 {
    MNT3_OK = 0,
    MNT3ERR_PERM = 1,
    MNT3ERR_NOENT = 2,
    MNT3ERR_IO = 5,
    MNT3ERR_ACCES = 13,
    MNT3ERR_NOTDIR = 20,
    MNT3ERR_INVAL = 22,
    MNT3ERR_NAMETOOLONG = 63,
    MNT3ERR_NOTSUPP = 10004,
    MNT3ERR_SERVERFAULT = 10006,
};

typedef enum mountstat3 mountstat3;

struct mountres3_ok {
    fhandle3 fhandle;
    struct {
        u_int auth_flavors_len;
        int *auth_flavors_val;
    } auth_flavors;
};

typedef struct mountres3_ok mountres3_ok;

struct mountres3 {
    mountstat3 fhs_status;
    union {
        mountres3_ok mountinfo;
    } mountres3_u;
};

typedef struct mountres3 mountres3;

extern int nct_rpc_encode(struct rpc_msg *msg, AUTH *auth,
                          xdrproc_t xdrproc, void *args,
                          char *buf, int bufsz);

extern ssize_t nct_rpc_send(int fd, void *buf, size_t len);
extern ssize_t nct_rpc_recv(int fd, void *buf, size_t len, uint32_t *markp);

extern enum clnt_stat nct_rpc_decode(XDR *xdr, char *buf, int len,
                                     struct rpc_msg *rpc_msg, struct rpc_err *rpc_err);

#endif // NCT_RPC_H
