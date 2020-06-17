/*
 * Copyright (c) 2014-2016 Greg Becker.  All rights reserved.
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
 * $Id: nct.h 392 2016-04-13 11:21:51Z greg $
 */
#ifndef NCT_H
#define NCT_H

#include "nct_mount.h"

#ifndef __read_mostly
#define __read_mostly       __attribute__((__section__(".read_mostly")))
#endif

#ifndef likely
#define likely(_expr)       __builtin_expect(!!(_expr), 1)
#endif

/* Sample interval record.  All times are in cycles/s (from rdtsc()).
 */
typedef struct {
    uint32_t           xsr_sample;              // Sample interval number
    uint32_t           xsr_rsvd;                // Available for use
    uint64_t           xsr_time;                // Time when sample was taken (in cycles)
    uint64_t           xsr_duration;            // Duration of sampling period
    uint64_t           xsr_requests;            // Toal number of requests completed
    uint64_t           xsr_throughput_send;     // Total bytes sent in the sample period
    uint64_t           xsr_throughput_recv;     // Total bytes rcvd in the sample period
    uint64_t           xsr_latency;             // Total latency of all ops in the sample
} nct_statsrec_t;

/*
 * The time-stamp counter on recent Intel processors is reset to zero each time
 * the processor package has RESET asserted. From that point onwards the invariant
 * TSC will continue to tick constantly across frequency changes, turbo mode and
 * ACPI C-states. All parts that see RESET synchronously will have their TSC's
 * completely synchronized. This synchronous distribution of RESET is required
 * for all sockets connected to a single PCH. For multi-node systems RESET might
 * not be synchronous.
 *
 * The biggest issue with TSC synchronization across multiple threads/cores/packages
 * is the ability for software to write the TSC. The TSC is exposed as MSR 0x10.d
 * Software is able to use WRMSR 0x10 to set the TSC. However, as the TSC continues
 * as a moving target, writing it is not guaranteed to be precise. For example a SMI
 * (System Management Interrupt) could interrupt the software flow that is attempting
 * to write the time-stamp counter immediately prior to the WRMSR. This could mean
 * the value written to the TSC could vary by thousands to millions of clocks.
 */

extern uint64_t tsc_freq;
extern bool have_tsc;

static inline uint64_t
rdtsc(void)
{
    struct timeval tv;

    if (likely(have_tsc))
        return __rdtsc();

    gettimeofday(&tv, NULL);

    return (tv.tv_sec * 1000000 + tv.tv_usec);
}

#if 0
static inline uint64_t
rdtsc(void)
{
    uint32_t low, high;

    __asm __volatile("rdtsc" : "=a" (low), "=d" (high));

    return (low | ((u_int64_t)high << 32));
}
#endif


extern void nct_req_send(nct_req_t *req);
extern int nct_req_recv(nct_mnt_t *mnt);
extern void nct_req_wait(nct_req_t *req);

extern nct_req_t *nct_req_alloc(nct_mnt_t *mnt);
extern void nct_req_free(nct_req_t *req);

extern void nct_stats_loop(nct_mnt_t *mnt, uint mark,
                           long sample_period, long duration,
                           nct_statsrec_t *statsv, u_int statsc,
                           const char *outfile, const char *gplot_term);

#endif /* NCT_H */
