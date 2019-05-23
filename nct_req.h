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
#ifndef NCT_REQ_H
#define NCT_REQ_H

/* Max number of NFS requests
 */
#define NCT_REQ_SHIFT      (10)
#define NCT_REQ_MAX        (1u << NCT_REQ_SHIFT)

/* TODO: Make this setable via the command line
 */
#define NCT_MSGSZ_MAX      (1024 * 256 - sizeof(struct nct_msg_s))

struct nct_mnt_s;
struct nct_req_s;

typedef int nct_req_cb_t(struct nct_req_s *req);

typedef struct nct_msg_s {
    XDR                 msg_xdr;            // RPC reply xdr
    struct rpc_msg      msg_rpc;            // RPC reply message
    struct rpc_err      msg_err;            // RPC reply error
    enum clnt_stat      msg_stat;           // RPC reply status code
    size_t              msg_len;            // TX/RX message length

    __aligned(64)
    char                msg_data[];         // TX/RX message buffer (NCT_MSGSZ_MAX)
} nct_msg_t;

typedef struct nct_req_s {
    __aligned(64)
    struct nct_req_s   *req_next;
    struct nct_req_s  **req_prev;

    uint32_t            req_xid;
    nct_req_cb_t       *req_cb;
    int                 req_done;
    nct_msg_t          *req_msg;

    void               *req_priv;
    int                 req_argc;
    char              **req_argv;

    volatile uint64_t   req_tsc_start;      // Most recent request start time
    volatile uint64_t   req_tsc_stop;       // Most recent request stop time

    uint64_t            req_tsc_finish;     // Finish time

    void               *req_mnt;
} nct_req_t;

extern void *nct_req_send_loop(void *arg);
extern void *nct_req_recv_loop(void *arg);

extern void nct_req_create(struct nct_mnt_s *mnt);

#endif // NCT_REQ_H
