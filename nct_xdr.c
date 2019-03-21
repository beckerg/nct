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
 *
 * $Id: nct_xdr.c 292 2015-01-15 13:07:09Z greg $
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

#include "nct_xdr.h"

int
nct_xdr_create(struct rpc_msg *rpc_msg, uint32_t proc, AUTH *auth,
               xdrproc_t xdrproc, void *args, char *buf, int bufsz)
{
    XDR xdrs;
    long l;
    int rc;

    xdrmem_create(&xdrs, buf, bufsz, XDR_ENCODE);

    if (!xdr_callhdr(&xdrs, rpc_msg)) {
        abort();
    }

    l = proc;
    if (!XDR_PUTLONG(&xdrs, &l)) {
        abort();
    }
    if (!AUTH_MARSHALL(auth, &xdrs)) {
        abort();
    }
    if (!(*xdrproc)(&xdrs, args)) {
        abort();
    }

    rc = XDR_GETPOS(&xdrs);

    XDR_DESTROY(&xdrs);

    return rc;
}

bool_t
xdr_uint64(XDR *xdrs, uint64 *objp)
{
#ifdef __linux__
    return xdr_u_quad_t(xdrs, objp);
#else
    return xdr_uint64_t(xdrs, objp);
#endif
}

bool_t
xdr_int64(XDR *xdrs, int64 *objp)
{
#ifdef __linux__
    return xdr_quad_t(xdrs, objp);
#else
    return xdr_int64_t (xdrs, objp);
#endif
}

bool_t
xdr_uint32(XDR *xdrs, uint32 *objp)
{
    return xdr_u_long(xdrs, objp);
}

bool_t
xdr_int32(XDR *xdrs, int32 *objp)
{
    return xdr_long(xdrs, objp);
}

bool_t
xdr_fhandle3(XDR *xdrs, fhandle3 *objp)
{
    return xdr_bytes(xdrs, (char **)&objp->fhandle3_val, (u_int *)&objp->fhandle3_len, FHSIZE3);
}

bool_t
nct_xdr_offset3(XDR *xdrs, offset3 *arg)
{
    return xdr_uint64(xdrs, arg);
}

bool_t
nct_xdr_mode3(XDR *xdrs, mode3 *objp)
{
    return xdr_uint32(xdrs, objp);
}

bool_t
nct_xdr_count3(XDR *xdrs, count3 *arg)
{
    return xdr_uint32(xdrs, arg);
}

bool_t
nct_xdr_dirpath(XDR *xdrs, char **dirpath)
{
    return xdr_string(xdrs, dirpath, MNTPATHLEN);
}

bool_t
nct_xdr_uid3(XDR *xdrs, uid3 *arg)
{
    return xdr_uint32(xdrs, arg);
}

bool_t
nct_xdr_gid3(XDR *xdrs, uid3 *arg)
{
    return xdr_uint32(xdrs, arg);
}

bool_t
nct_xdr_size3(XDR *xdrs, uid3 *arg)
{
    return xdr_uint64(xdrs, arg);
}

bool_t
nct_xdr_ftype3(XDR *xdrs, ftype3 *arg)
{
    return xdr_enum(xdrs, (enum_t *)arg);
}

bool_t
nct_xdr_specdata3(XDR *xdrs, specdata3 *arg)
{
    return xdr_uint32(xdrs, &arg->specdata1) && xdr_uint32(xdrs, &arg->specdata2);
}

bool_t
nct_xdr_fileid3(XDR *xdrs, fileid3 *arg)
{
    return xdr_uint64(xdrs, arg);
}

bool_t
nct_xdr_nfstime3(XDR *xdrs, nfstime3 *arg)
{
    return xdr_uint32(xdrs, &arg->seconds) && xdr_uint32(xdrs, &arg->nseconds);
}

bool_t
nct_xdr_nfsstat3(XDR *xdrs, nfsstat3 *arg)
{
    return xdr_enum(xdrs, (enum_t *)arg);
}

bool_t
nct_xdr_fattr3(XDR *xdrs, fattr3 *arg)
{
    return
        nct_xdr_ftype3(xdrs, &arg->type) &&
        nct_xdr_mode3(xdrs, &arg->mode) &&
        xdr_uint32(xdrs, &arg->nlink) &&
        nct_xdr_uid3(xdrs, &arg->uid) &&
        nct_xdr_gid3(xdrs, &arg->gid) &&
        nct_xdr_size3(xdrs, &arg->size) &&
        nct_xdr_size3(xdrs, &arg->used) &&
        nct_xdr_specdata3(xdrs, &arg->rdev) &&
        xdr_uint64(xdrs, &arg->fsid) &&
        nct_xdr_fileid3(xdrs, &arg->fileid) &&
        nct_xdr_nfstime3(xdrs, &arg->atime) &&
        nct_xdr_nfstime3(xdrs, &arg->mtime) &&
        nct_xdr_nfstime3(xdrs, &arg->ctime);
}


bool_t
nct_xdr_mountstat3_decode(XDR *xdrs, mountstat3 *args)
{
    return xdr_enum(xdrs, (enum_t *)args);
}

bool_t
nct_xdr_mountres3_ok(XDR *xdrs, mountres3_ok *args)
{
    return
        xdr_fhandle3(xdrs, &args->fhandle) &&
        xdr_array(xdrs, (char **)&args->auth_flavors.auth_flavors_val,
                  (u_int *)&args->auth_flavors.auth_flavors_len, ~0,
                  sizeof(int), (xdrproc_t)xdr_int);
}

bool_t
nct_xdr_mountres3_decode(char *msg, int len, mountres3 *mntres)
{
    bool_t rc;
    XDR xdr;

    xdrmem_create(&xdr, msg, len, XDR_DECODE);

    /* xdr_mountres3 and subsequent xdr decoding calls assume this
     * to be NULL if we want them to do the memory allocation for
     * the auth array. Make sure this is NULL here, because
     * there's a bunch of embedded structs in mntres3, its
     * possible that this pointer ends up being non-NULL but
     * pointing to a memory we did not allocate.
     */
    mntres->mountres3_u.mountinfo.auth_flavors.auth_flavors_val = NULL;
    mntres->mountres3_u.mountinfo.fhandle.fhandle3_val = NULL;

    rc = nct_xdr_mountstat3_decode(&xdr, &mntres->fhs_status);
    if (rc) {
        switch (mntres->fhs_status) {
        case MNT3_OK:
            rc = nct_xdr_mountres3_ok(&xdr, &mntres->mountres3_u.mountinfo);
            break;

        default:
            break;
        }
    }

    XDR_DESTROY(&xdr);

    return rc;
}

bool_t
nct_xdr_fh3(XDR *xdrs, nfs_fh3 *arg)
{
    return xdr_bytes(xdrs, (char **)&arg->data.data_val,
                     (u_int *)&arg->data.data_len, NFS3_FHSIZE);
}

bool_t
nct_xdr_getattr3_encode(XDR *xdr, getattr3_args *args)
{
    return nct_xdr_fh3(xdr, &args->object);
}

bool_t
nct_xdr_getattr3_resok(XDR *xdr, getattr3_resok *res)
{
    return nct_xdr_fattr3(xdr, &res->obj_attributes);
}

bool_t
nct_xdr_getattr3_decode(XDR *xdr, getattr3_res *res)
{
    if (nct_xdr_nfsstat3(xdr, &res->status)) {
        switch (res->status) {
        case NFS3_OK:
            return nct_xdr_getattr3_resok(xdr, &res->u.resok);

        default:
            break;
        }
    }

    return FALSE;
}

bool_t
nct_xdr_read3_encode(XDR *xdrs, read3_args *args)
{
    return
        nct_xdr_fh3(xdrs, &args->file) &&
        nct_xdr_offset3(xdrs, &args->offset) &&
        nct_xdr_count3(xdrs, &args->count);
}

