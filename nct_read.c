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
 * $Id: nct_read.c 392 2016-04-13 11:21:51Z greg $
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#include <rpc/types.h>
#include <rpc/auth.h>
#include <rpc/rpc.h>

#include "clp.h"
#include "main.h"
#include "nct_nfs.h"
#include "nct_mount.h"
#include "nct_read.h"
#include "nct.h"

typedef struct {
    off_t   pr_offset;
    size_t  pr_length;
    int     pr_duration;
} test_read_priv_t;

static size_t length = 4096;
static char *rhostpath;

static clp_posparam_t posparamv[] = {
    {
        .name = "rhostpath",
        .help = "[user@]rhost:path",
        .convert = clp_cvt_string,
        .cvtdst = &rhostpath,
    },

    {
        .name = "[length]",
        .help = "read length (in bytes)",
        .convert = clp_cvt_u_long,
        .cvtdst = &length,
    },

    CLP_PARAM_END
};

static clp_option_t optionv[] = {
    CLP_OPTION_VERBOSE(verbosity),
    CLP_OPTION_HELP,

    CLP_OPTION_END
};

static int test_read_start(struct nct_req *req);
static int test_read_cb(struct nct_req *req);

static bool
given(int c)
{
    clp_option_t *opt = clp_option_find(optionv, c);

    return (opt && opt->given);
}

void *
test_read_init(int argc, char **argv, int duration, start_t **startp, char **rhostpathp)
{
    test_read_priv_t *priv;
    char errbuf[128];
    int optind;
    int rc;

    rc = clp_parsev(argc, argv, optionv, posparamv, errbuf, sizeof(errbuf), &optind);
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

    priv->pr_offset = 0;
    priv->pr_length = length;
    priv->pr_duration = duration;

    if (priv->pr_length < 512 || priv->pr_length > NCT_MSGSZ_MAX) {
        eprint("invalid read length %zu\n", priv->pr_length);
        abort();
    }

    *startp = test_read_start;
    *rhostpathp = rhostpath;

    return priv;
}

static int
test_read_cb(struct nct_req *req)
{
    test_read_priv_t *priv = req->req_priv;
    nct_mnt_t *mnt = req->req_mnt;
    nct_vn_t *vn = mnt->mnt_vn;
    off_t offset;

    XDR_DESTROY(&req->req_msg->msg_xdr);

    if (req->req_msg->msg_stat != RPC_SUCCESS) {
        eprint("read failed: %d %s\n",
               req->req_msg->msg_stat, clnt_sperrno(req->req_msg->msg_stat));
        nct_req_free(req);
        return req->req_msg->msg_stat;
    }

    if (req->req_tsc_stop >= req->req_tsc_finish) {
        nct_req_free(req);
        return ETIMEDOUT;
    }

    offset = __sync_fetch_and_add(&priv->pr_offset, priv->pr_length);

    if (offset + priv->pr_length > vn->xvn_fattr.size) {
        offset = __sync_fetch_and_sub(&priv->pr_offset, priv->pr_offset);
    }

    req->req_tsc_start = rdtsc();
    nct_nfs_read3_encode(req, offset, priv->pr_length);
    nct_req_send(req);

    return 0;
}

static int
test_read_start(struct nct_req *req)
{
    test_read_priv_t *priv = req->req_priv;
    nct_mnt_t *mnt = req->req_mnt;
    nct_vn_t *vn = mnt->mnt_vn;
    off_t offset;

    req->req_tsc_finish = rdtsc() + (tsc_freq * priv->pr_duration);
    req->req_cb = test_read_cb;

    offset = __sync_fetch_and_add(&priv->pr_offset, priv->pr_length);

    if (offset + priv->pr_length > vn->xvn_fattr.size) {
        eprint("file smaller than request length: size=%lu length=%zu\n",
               vn->xvn_fattr.size, priv->pr_length);
        return EINVAL;
    }

    usleep(1000);

    req->req_tsc_start = rdtsc();
    nct_nfs_read3_encode(req, offset, priv->pr_length);
    nct_req_send(req);

    return 0;
}
