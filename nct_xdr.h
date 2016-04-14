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
 * $Id: nct_xdr.h 292 2015-01-15 13:07:09Z greg $
 */
#ifndef NCT_XDR_H
#define NCT_XDR_H

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

extern int nct_xdr_create(struct rpc_msg *rpc_msg, uint32_t proc, AUTH *auth,
                           xdrproc_t xdrproc, void *args, char *buf, int bufsz);

extern bool_t nct_xdr_dirpath(XDR *xdrs, char **dirpath);

extern bool_t nct_xdr_mountres3_decode(char *msg, int len, mountres3 *mntres);

extern bool_t nct_xdr_getattr3_encode(XDR *xdr, getattr3_args *args);
extern bool_t nct_xdr_getattr3_decode(XDR *xdr, getattr3_res *res);

extern bool_t nct_xdr_read3_encode(XDR *xdrs, read3_args *args);

#endif /* NCT_XDR_H */
