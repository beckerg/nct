/*
 * Copyright (c) 2015-2016 Greg Becker.  All rights reserved.
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

#include <rpc/types.h>
#include <rpc/auth.h>
#include <rpc/rpc.h>

#include "clp.h"
#include "main.h"
#include "nct_nfs.h"
#include "nct_xdr.h"
#include "nct_getattr.h"
#include "nct.h"

typedef struct {
    int     pr_duration;
} test_getattr_priv_t;

static char *rhostpath;

static clp_posparam_t posparamv[] = {
    {
        .name = "rhostpath",
        .help = "[user@]rhost:path",
        .convert = clp_convert_string, .result = &rhostpath,
    },

    { .name = NULL }
};

static clp_option_t optionv[] = {
    CLP_OPTION_VERBOSE(verbosity),
    CLP_OPTION_HELP,

    CLP_OPTION_END
};

static void *test_getattr_start(void *arg);
static int test_getattr_cb(nct_req_t *req);

static bool
given(int c)
{
    clp_option_t *opt = clp_option_find(optionv, c);

    return (opt && opt->given);
}

void *
test_getattr_init(int argc, char **argv, int duration, start_t **startp, char **rhostpathp)
{
    char errbuf[CLP_ERRBUFSZ];
    test_getattr_priv_t *priv;
    int optind;
    int rc;

    rc = clp_parsev(argc, argv, optionv, posparamv, errbuf, &optind);
    if (rc) {
        eprint("%s\n", errbuf);
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
test_getattr_cb(nct_req_t *req)
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

    nct_nfs_getattr3_encode(req);
    nct_req_send(req);

    return 0;
}

static void *
test_getattr_start(void *arg)
{
    nct_req_t *req = arg;
    nct_mnt_t *mnt = req->req_mnt;
    test_getattr_priv_t *priv = req->req_priv;
    int rc;

    req->req_tsc_finish = rdtsc() + (tsc_freq * priv->pr_duration);
    req->req_cb = test_getattr_cb;

    nct_nfs_getattr3_encode(req);
    nct_req_send(req);

    while (1) {
        rc = nct_req_recv(mnt);
        if (rc) {
            break;
        }
    }

    dprint(2, "exiting...", strerror(rc));

    nct_worker_exit(mnt);

    return NULL;
}

