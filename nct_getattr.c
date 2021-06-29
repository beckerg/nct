/*
 * Copyright (c) 2015-2017,2019 Greg Becker.  All rights reserved.
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
 * $Id: nct_getattr.c 392 2016-04-13 11:21:51Z greg $
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <getopt.h>

#include <rpc/types.h>
#include <rpc/auth.h>
#include <rpc/rpc.h>

#include "clp.h"
#include "main.h"
#include "nct.h"
#include "nct_nfs.h"
#include "nct_xdr.h"
#include "nct_getattr.h"

typedef struct {
    int     pr_duration;
} test_getattr_priv_t;

static char *rhostpath;

static struct clp_posparam posparamv[] = {
    CLP_POSPARAM("rhostpath", string, rhostpath, NULL, NULL, "[user@]rhost:path"),
    CLP_POSPARAM_END
};

static struct clp_option optionv[] = {
    CLP_OPTION_VERBOSITY(verbosity),
    CLP_OPTION_HELP,
    CLP_OPTION_END
};

static int test_getattr_start(struct nct_req *req);
static int test_getattr_cb(struct nct_req *req);

static bool
given(int c)
{
    return !!clp_given(c, optionv, NULL);
}

void *
test_getattr_init(int argc, char **argv, int duration, start_t **startp, char **rhostpathp)
{
    test_getattr_priv_t *priv;
    int rc;

    rc = clp_parsev(argc, argv, optionv, posparamv);
    if (rc) {
        exit(rc);
    }

    if (given('h') || given('V'))
        exit(0);

    argc -= optind;
    argv += optind;

    priv = malloc(sizeof(*priv));
    if (!priv) {
        abort();
    }

    priv->pr_duration = duration;

    *startp = test_getattr_start;
    *rhostpathp = rhostpath;

    return priv;
}

static int
test_getattr_cb(struct nct_req *req)
{
    enum clnt_stat stat;
    getattr3_res res;
    bool_t ok;

    stat = req->req_msg->msg_stat;
    if (stat != RPC_SUCCESS) {
        eprint("getattr rpc failed: clnt_stat=%d %s\n",
               req->req_msg->msg_stat, clnt_sperrno(req->req_msg->msg_stat));
        XDR_DESTROY(&req->req_msg->msg_xdr);
        nct_req_free(req);
        return stat;
    }

    ok = nct_xdr_getattr3_decode(&req->req_msg->msg_xdr, &res);
    if (!ok) {
        eprint("getattr nfs failed: nfsstat3=%d %s\n",
               res.status, strerror(res.status));
        XDR_DESTROY(&req->req_msg->msg_xdr);
        nct_req_free(req);
        return res.status;
    }

    XDR_DESTROY(&req->req_msg->msg_xdr);

    if (req->req_tsc_stop >= req->req_tsc_finish) {
        nct_req_free(req);
        return ETIMEDOUT;
    }

    req->req_tsc_start = rdtsc();
    nct_nfs_getattr3_encode(req);
    nct_req_send(req);

    return 0;
}

static int
test_getattr_start(struct nct_req *req)
{
    test_getattr_priv_t *priv = req->req_priv;

    req->req_tsc_finish = rdtsc() + (tsc_freq * priv->pr_duration);
    req->req_cb = test_getattr_cb;

    req->req_tsc_start = rdtsc();
    nct_nfs_getattr3_encode(req);
    nct_req_send(req);

    return 0;
}

