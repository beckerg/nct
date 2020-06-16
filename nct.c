/*
 * Copyright (c) 2014-2016,2019 Greg Becker.  All rights reserved.
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
 * $Id: nct.c 393 2016-04-14 09:21:59Z greg $
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <netdb.h>
#include <errno.h>
#include <time.h>
#include <sysexits.h>
#include <sys/select.h>

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
#include "nct_nfs.h"
#include "nct.h"

uint64_t tsc_freq = 1000000;

static void
nct_gplot(long nsamples, long sampersec, const char *term, const char *using,
           const char *title, const char *xlabel, const char *ylabel, const char *color)
{
    char file[128];
    char cmd[sizeof(file) + 32];
    time_t now;
    FILE *fp;
    int n;

    n = snprintf(file, sizeof(file), "%s.gnuplot", title);
    if (n >= sizeof(file)) {
        eprint("title %s too long\n", title);
        return;
    }

    fp = fopen(file, "w");
    if (!fp) {
        eprint("fopen(%s) failed: %s\n", file, strerror(errno));
        return;
    }

    time(&now);
    fprintf(fp, "# Created on %s", ctime(&now));
    fprintf(fp, "# %ld samples\n", nsamples);
    fprintf(fp, "# %ld samples/sec\n", sampersec);

    fprintf(fp, "set title \"%s\"\n", title);
    fprintf(fp, "set output '%s.%s'\n", title, term);
    fprintf(fp, "set term %s size 3840,1280\n", term);
    fprintf(fp, "set size 1, 0.76\n");
    fprintf(fp, "set origin 0, 0.24\n");
    fprintf(fp, "set autoscale\n");
    fprintf(fp, "set grid\n");
    fprintf(fp, "set ylabel \"%s\"\n", ylabel);
    fprintf(fp, "set ytics autofreq\n");
    fprintf(fp, "set mytics 5\n");
    fprintf(fp, "set yrange [0:]\n");
    fprintf(fp, "set xlabel \"%s\"\n", xlabel);

#if 1
    /* Limit xtics to no more than 30 tics.
     */
    int m = (nsamples / sampersec) / 30;

    if (m <= 1) {
        m = 1;
        n = 10;
    } else if (m <= 3) {
        m = 3;
        n = 3;
    } else if (m <= 10) {
        m = 10;
    } else if (m <= 15) {
        m = 15;
        n = 3;
    } else if (m <= 60) {
        m = 30;
        n = 3;
    } else if (m <= 180) {
        m = 60;
        n = 6;
    } else if (m <= 300) {
        m = 180;
        n = 3;
    } else {
        m = 300;
        n = 10;
    }

    fprintf(fp, "set xtics 0, %d rotate by -30\n", m);
    fprintf(fp, "set mxtics %d\n", n);
#endif

    fprintf(fp,
            "plot \"raw\" "
            "every ::1:::0 using %s "
            "with lines lc rgbcolor \"%s\" "
            "title \"%s\"",
            using, color, title);

    fclose(fp);

    snprintf(cmd, sizeof(cmd), "gnuplot %s", file);
    fp = popen(cmd, "r");
    if (!fp) {
        eprint("[%s] failed: %s\n", cmd, strerror(errno));
        return;
    }

    pclose(fp);
}

/* Collect samples of throughput data (every 100ms).
 * Print a throughput data sample to stdout (every 1s).
 * Terminates once all worker count for the mnt object
 * has dropped to zero.
 */
void
nct_stats_loop(nct_mnt_t *mnt, u_int mark,
               long sample_period_usec, long duration,
               nct_statsrec_t *statsv, u_int statsc,
               const char *outdir, const char *term)
{
    uint64_t throughput_send_cur, throughput_send_last;
    uint64_t throughput_recv_cur, throughput_recv_last;
    double throughput_send_avg, throughput_recv_avg;
    uint64_t latency_cur, latency_prev;
    uint64_t tsc_cur, tsc_last, tsc_interval;
    nct_statsrec_t *cur, *prev, *end;
    uint64_t reqs_cur, reqs_last;
    uint64_t tsc_start;
    long samples_tot;
    long loops;

    const long samples_per_sec = 1000000 / sample_period_usec;
    long print_period = mark * tsc_freq;
    long sample_period;

    throughput_send_last = 0;
    throughput_recv_last = 0;
    latency_prev = 0;
    samples_tot = 0;
    reqs_last = 0;
    loops = 0;

    cur = end = NULL;

    if (statsv && statsc > 0) {
        bzero(statsv, sizeof(*statsv));
        end = statsv + duration * samples_per_sec;
        cur = statsv + 1;
    }

    tsc_start = tsc_cur = tsc_last = rdtsc();
    if (statsv)
        statsv->xsr_duration = tsc_cur;

    sample_period = (sample_period_usec * tsc_freq) / 1000000ul;
    if (print_period >= sample_period)
        print_period -= (sample_period / 2);

    while (1) {
        char lat_min_buf[32], lat_max_buf[32], lat_avg_buf[32];
        uint64_t latency_min, latency_max;
        long tgt, delta;

        ++samples_tot;

        tgt = tsc_start + samples_tot * sample_period;

#ifdef USE_TSC
        delta = ((tgt - tsc_cur) * 1000000ul) / tsc_freq;
#else
        delta = tgt - tsc_cur;
#endif

        if (delta > 999)
            usleep(delta - 999);

        tsc_cur = rdtsc();

        pthread_spin_lock(&mnt->mnt_stats_spin);
        latency_min = mnt->mnt_stats.latency_min;
        latency_max = mnt->mnt_stats.latency_max;

        mnt->mnt_stats.latency_min = UINT64_MAX;
        mnt->mnt_stats.latency_max = 0;

        latency_cur = mnt->mnt_stats.latency_cum;
        reqs_cur = mnt->mnt_stats.requests;
        throughput_send_cur = mnt->mnt_stats.thruput_send;
        throughput_recv_cur = mnt->mnt_stats.thruput_recv;
        pthread_spin_unlock(&mnt->mnt_stats_spin);

        if (cur) {
            cur->xsr_time = tsc_cur;
            cur->xsr_sample = samples_tot;
            cur->xsr_requests = reqs_cur;
            cur->xsr_throughput_send = throughput_send_cur;
            cur->xsr_throughput_recv = throughput_recv_cur;
            cur->xsr_latency = latency_cur;

            if (++cur >= end)
                cur = NULL;
        }

        if (!mark) {
            if (__atomic_load_n(&mnt->mnt_jobs_cnt, __ATOMIC_SEQ_CST) < 1)
                break;
            continue;
        }

        if (tsc_cur - tsc_last < print_period)
            continue;

        tsc_interval = ((tsc_cur - tsc_last) * 1000000ul) / tsc_freq;

        if (reqs_cur > reqs_last) {
            uint64_t lat;

            snprintf(lat_min_buf, sizeof(lat_min_buf), "%6.1lf",
                     (latency_min * 1000000.0) / (tsc_cur - tsc_last));
            snprintf(lat_max_buf, sizeof(lat_max_buf), "%6.1lf",
                     (latency_max * 1000000.0) / (tsc_cur - tsc_last));

            lat = (latency_cur - latency_prev) / (reqs_cur - reqs_last);
            snprintf(lat_avg_buf, sizeof(lat_avg_buf), "%6.1lf",
                     (lat * 1000000.0) / (tsc_cur - tsc_last));

            throughput_send_avg = throughput_send_cur - throughput_send_last;
            throughput_send_avg /= 1024 * 1024;
            throughput_recv_avg = throughput_recv_cur - throughput_recv_last;
            throughput_recv_avg /= 1024 * 1024;
        } else {
            if (__atomic_load_n(&mnt->mnt_jobs_cnt, __ATOMIC_SEQ_CST) < 1) {
                break;
            }

            snprintf(lat_avg_buf, sizeof(lat_avg_buf), "%10s", "stalled");
            throughput_send_avg = 0;
            throughput_recv_avg = 0;
        }

        if ((loops++ % 22) == 0) {
            printf("\n%8s %9s %8s %7s %7s %7s %7s %7s\n",
                   "SAMPLES", "DURATION", "OPS", "TXMB", "RXMB",
                   "LATMIN", "LATAVG", "LATMAX");
        }

        printf("%8ld %9lu %8lu %7.2lf %7.2lf %7s %7s %7s\n",
               samples_tot, tsc_interval, reqs_cur - reqs_last,
               throughput_send_avg, throughput_recv_avg,
               lat_min_buf, lat_avg_buf, lat_max_buf);

        throughput_send_last = throughput_send_cur;
        throughput_recv_last = throughput_recv_cur;
        latency_prev = latency_cur;
        reqs_last = reqs_cur;
        tsc_last = tsc_cur;
    }

    if (statsv && statsc > 0 && outdir) {
        uint64_t throughput_send_min, throughput_send_max, throughput_send_tot, throughput_send;
        uint64_t throughput_recv_min, throughput_recv_max, throughput_recv_tot, throughput_recv;
        uint64_t requests_min, requests_max, requests_tot, requests;
        double latency_min, latency_max;
        uint64_t latency_tot, latency;
        nct_statsrec_t *tail;
        FILE *fpraw;
        time_t now;

        fpraw = fopen("raw", "w");
        if (!fpraw) {
            eprint("unable to open [%s/raw]: %s\n", outdir, strerror(errno));
            return;
        }

        time(&now);
        fprintf(fpraw, "# Created on %s", ctime(&now));
        fprintf(fpraw, "# %ld samples\n", samples_tot);
        fprintf(fpraw, "# %ld samples/sec\n", samples_per_sec);
        fprintf(fpraw, "# %ld sample period (usecs)\n", sample_period_usec);
        fprintf(fpraw, "# time, duration, and latency in usecs\n");
        fprintf(fpraw, "# send and recv in bytes\n");
        fprintf(fpraw, "#\n");
        fprintf(fpraw, "# %8s %10s %10s %8s %8s %10s %10s %8s %10s %10s\n",
                "SAMPLE", "TIME", "DURATION", "LATENCY",
                "OPS", "SEND", "RECV",
                "OPSRA", "SENDRA", "RECVRA");

        if (cur)
            end = cur - 1;      // Ignore the last sample

        cur = statsv + 1;
        prev = statsv;
        tail = cur;

        requests_min = latency_min = throughput_send_min = throughput_recv_min = ULONG_MAX;
        requests_max = latency_max = throughput_send_max = throughput_recv_max = 0;
        requests_tot = latency_tot = throughput_send_tot = throughput_recv_tot = 0;

        while (cur < end) {
            uint64_t requests_ra, thruput_send_ra, thruput_recv_ra;

            requests = cur->xsr_requests - prev->xsr_requests;
            requests_tot += requests;

            throughput_send = cur->xsr_throughput_send - prev->xsr_throughput_send;
            throughput_send_tot += throughput_send;

            throughput_recv = cur->xsr_throughput_recv - prev->xsr_throughput_recv;
            throughput_recv_tot += throughput_recv;

            latency = cur->xsr_latency - prev->xsr_latency;
            latency_tot += latency;

            if (requests > 0) {
                if (latency / requests > latency_max)
                    latency_max = latency / requests;
                if (latency / requests < latency_min)
                    latency_min = latency / requests;
            }

            /* Compute n-point running average (where n is samples_per_second).
             */
            if (cur - tail >= samples_per_sec) {
                requests_ra = cur->xsr_requests - tail->xsr_requests;
                thruput_send_ra = cur->xsr_throughput_send - tail->xsr_throughput_send;
                thruput_recv_ra = cur->xsr_throughput_recv - tail->xsr_throughput_recv;

                if (requests_ra > requests_max)
                    requests_max = requests_ra;
                if (requests_ra < requests_min)
                    requests_min = requests_ra;

                if (thruput_send_ra > throughput_send_max)
                    throughput_send_max = thruput_send_ra;
                if (thruput_send_ra < throughput_send_min)
                    throughput_send_min = thruput_send_ra;

                if (thruput_recv_ra > throughput_recv_max)
                    throughput_recv_max = thruput_recv_ra;
                if (thruput_recv_ra < throughput_recv_min)
                    throughput_recv_min = thruput_recv_ra;

                ++tail;
            }
            else {
                requests_ra = cur->xsr_requests;
                thruput_send_ra = cur->xsr_throughput_send;
                thruput_recv_ra = cur->xsr_throughput_recv;
            }

            cur->xsr_time -= tsc_start;

            fprintf(fpraw, "  %8u %10lu %10lu %8lu %8lu %10lu %10lu %8lu %10lu %10lu\n",
                    cur->xsr_sample,
                    (cur->xsr_time * 1000000ul) / tsc_freq,
                    ((cur->xsr_time - prev->xsr_time) * 1000000ul) / tsc_freq,
                    (latency * 1000000ul) / tsc_freq,
                    requests, throughput_send, throughput_recv,
                    requests_ra, thruput_send_ra, thruput_recv_ra);

            prev = cur;
            ++cur;
        }

        /* Print summary statistics...
         */
        samples_tot = cur - statsv;

        printf("\n%12s %12s %12s %15s  %s\n", "MIN", "AVG", "MAX", "TOTAL", "DESC");

        uint64_t requests_avg = (requests_tot * samples_per_sec) / samples_tot;

        printf("%12lu %12lu %12lu %15lu  bytes transmitted per second\n",
               throughput_send_min,
               (throughput_send_tot * samples_per_sec) / samples_tot,
               throughput_send_max,
               mnt->mnt_stats.thruput_send);

        printf("%12lu %12lu %12lu %15lu  bytes received per second\n",
               throughput_recv_min,
               (throughput_recv_tot * samples_per_sec) / samples_tot,
               throughput_recv_max,
               mnt->mnt_stats.thruput_recv);

        printf("%12.1lf %12.1lf %12.1lf %15lu  latency per request (usecs)\n",
               (latency_min * 1000000.0) / tsc_freq,
               ((latency_tot * 1000000.0 * samples_per_sec) / (tsc_freq * samples_tot)) / requests_avg,
               (latency_max * 1000000.0) / tsc_freq,
               mnt->mnt_stats.latency_cum);

        printf("%12lu %12lu %12lu %15lu  requests per second\n",
               requests_min, requests_avg, requests_max,
               mnt->mnt_stats.requests);

        printf("%12s %12s %12s %15lu  updates\n",
               "-", "-", "-", mnt->mnt_stats.updates);

        printf("%12s %12s %12s %15lu  marks\n",
               "-", "-", "-", mnt->mnt_stats.marks);

        printf("%12s %12s %12s %15u  threads\n",
               "-", "-", "-", mnt->mnt_tds_max);

        printf("%12s %12s %12s %15u  jobs\n",
               "-", "-", "-", mnt->mnt_jobs_max);


        fclose(fpraw);

        char ylabel[128], using[128];

        /* Create the gnuplot files...
         */
        snprintf(ylabel, sizeof(ylabel), "MB / second");
        snprintf(using, sizeof(using),
                 "($2 / %d):(($10 * %d) / (1024 * 1024))",
                 1000000, 1);
        nct_gplot(samples_tot, samples_per_sec, term, using,
                  "recv", "seconds", ylabel, "green");

        snprintf(using, sizeof(using),
                 "($2 / %d):(($9 * %d) / (1024 * 1024))",
                 1000000, 1);
        nct_gplot(samples_tot, samples_per_sec, term, using,
                  "send", "seconds", ylabel, "red");

        snprintf(using, sizeof(using),
                 "($2 / %d):($4 / $5)",
                 1000000);
        nct_gplot(samples_tot, samples_per_sec, term, using,
                  "latency", "seconds", "usec/request", "black");

        snprintf(ylabel, sizeof(ylabel), "requests / seconds");
        snprintf(using, sizeof(using),
                 "($2 / %d):($8 * %d)",
                 1000000, 1);
        nct_gplot(samples_tot, samples_per_sec, term, using,
                  "requests", "seconds", ylabel, "blue");
    }
}
