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

/* Create a worker thread.
 */
void
nct_worker_create(nct_mnt_t *mnt, start_t *start, void *arg)
{
    pthread_t td;
    int rc;

    __sync_fetch_and_add(&mnt->mnt_worker_cnt, 1);

    rc = pthread_create(&td, NULL, start, arg);
    if (rc) {
        eprint("pthread_create() failed: %s\n", strerror(errno));
        abort();
    }
}

void
nct_worker_exit(nct_mnt_t *mnt)
{
    __sync_fetch_and_add(&mnt->mnt_worker_cnt, -1);
}

static void
nct_gplot(int nsamples, int sampersec, const char *term, const char *using,
           const char *title, const char *xlabel, const char *ylabel, const char *color)
{
    char file[128], cmd[128];
    time_t now;
    FILE *fp;

    snprintf(file, sizeof(file), "%s.gnuplot", title);
    fp = fopen(file, "w");
    if (!fp) {
        eprint("fopen(%s) failed: %s\n", strerror(errno));
        return;
    }

    time(&now);
    fprintf(fp, "# Created on %s", ctime(&now));
    fprintf(fp, "# %d samples\n", nsamples);
    fprintf(fp, "# %d samples/sec\n", sampersec);

    fprintf(fp, "set title \"%s\"\n", title);
    fprintf(fp, "set output '%s.%s'\n", title, term);
    fprintf(fp, "set term %s size 3600,1200\n", term);
    fprintf(fp, "set size 1, 0.76\n");
    fprintf(fp, "set origin 0, 0.24\n");
    fprintf(fp, "set autoscale\n");
    fprintf(fp, "set grid\n");
    fprintf(fp, "set ylabel \"%s\"\n", ylabel);
    fprintf(fp, "set ytics autofreq\n");
    fprintf(fp, "set yrange [0:]\n");
    fprintf(fp, "set xlabel \"%s\"\n", xlabel);

#if 1
    /* Limit xtics to no more than 30 tics.
     */
    int m = (nsamples / sampersec) / 30;

    if (m <= 1) {
        m = 1;
    } else if (m <= 3) {
        m = 3;
    } else if (m <= 10) {
        m = 10;
    } else if (m <= 15) {
        m = 15;
    } else if (m <= 60) {
        m = 30;
    } else if (m <= 180) {
        m = 60;
    } else if (m <= 300) {
        m = 180;
    } else {
        m = 300;
    }

    fprintf(fp, "set xtics 0, %d\n", m);
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
        eprint("[%s] failed: %s\n", strerror(errno));
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
nct_stats_loop(nct_mnt_t *mnt, long duration, int mark,
                const char *outdir, const char *term)
{
    uint64_t throughput_send_cur, throughput_send_last;
    uint64_t throughput_recv_cur, throughput_recv_last;
    double throughput_send_avg, throughput_recv_avg;
    uint64_t latency_cur, latency_prev, latency_avg;
    uint64_t tsc_cur, tsc_last, tsc_interval;
    nct_statsrec_t *cur, *prev, *base, *end;
    uint64_t reqs_cur, reqs_last;
    uint64_t tsc_start;
    int samples_tot;
    long delay;
    long loops;

    const int sample_interval = 1000 * 100;     // Record sample at 100ms intervals
    const int samples_per_sec = 1000000 / sample_interval;

    const int print_interval = 1000 * 1000;     // Print sample at 1s intervals

    base = prev = cur = end = NULL;

    if (outdir) {
        const size_t srec_cnt = duration * samples_per_sec;

        base = malloc(sizeof(*base) * srec_cnt);
        if (!base) {
            abort();
        }

        prev = base;
        cur = base + 1;
        end = base + srec_cnt;
        bzero(prev, sizeof(*prev));

        prev->xsr_duration = rdtsc();
    }

    throughput_send_last = 0;
    throughput_recv_last = 0;
    latency_prev = 0;
    samples_tot = 0;
    reqs_last = 0;
    loops = 0;

    delay = sample_interval;
    tsc_start = rdtsc();
    tsc_last = tsc_start;

    while (1) {
        char latency_buf[32];
        long left;

        ++samples_tot;

        usleep(delay - 512);

        tsc_cur = rdtsc();
        latency_cur = mnt->mnt_stats_latency;
        reqs_cur = mnt->mnt_stats_requests;
        throughput_send_cur = mnt->mnt_stats_throughput_send;
        throughput_recv_cur = mnt->mnt_stats_throughput_recv;

        if (cur) {
            cur->xsr_time = tsc_cur;
            cur->xsr_sample = samples_tot;
            cur->xsr_requests = reqs_cur;
            cur->xsr_throughput_send = throughput_send_cur;
            cur->xsr_throughput_recv = throughput_recv_cur;
            cur->xsr_latency = latency_cur;

            if (++cur >= end) {
                cur = NULL;
            }
        }

        if (!mark) {
            if (mnt->mnt_worker_cnt < 1) {
                break;
            }
            continue;
        }

        if (tsc_cur - tsc_last < tsc_freq) {
            left = print_interval - ((tsc_cur - tsc_last) * 1000000ul) / tsc_freq;
            if (left > 512) {
                if (left < sample_interval) {
                    delay = left;
                }
                continue;
            }
        }

        delay = sample_interval;

        tsc_interval = ((tsc_cur - tsc_last) * 1000000ul) / tsc_freq;

        if (reqs_cur > reqs_last) {
            latency_avg = (latency_cur - latency_prev) / (reqs_cur - reqs_last);
            snprintf(latency_buf, sizeof(latency_buf), "%10.2f",
                     (latency_avg * (float)1000000) / (tsc_cur - tsc_last));
            throughput_send_avg = throughput_send_cur - throughput_send_last;
            throughput_send_avg /= 1024 * 1024;
            throughput_recv_avg = throughput_recv_cur - throughput_recv_last;
            throughput_recv_avg /= 1024 * 1024;
        } else {
            if (mnt->mnt_worker_cnt < 1) {
                break;
            }

            snprintf(latency_buf, sizeof(latency_buf), "%10s", "stalled");
            throughput_send_avg = 0;
            throughput_recv_avg = 0;
        }

        if ((loops++ % 22) == 0) {
            printf("\n%8s %9s %8s %7s %7s %10s\n",
                   "SAMPLES", "DURATION", "OPS", "TXMB", "RXMB", "LATENCY");
        }

        printf("%8u %9lu %8lu %7.2lf %7.2lf %s\n",
               samples_tot, tsc_interval, reqs_cur - reqs_last,
               throughput_send_avg, throughput_recv_avg, latency_buf);

        throughput_send_last = throughput_send_cur;
        throughput_recv_last = throughput_recv_cur;
        latency_prev = latency_cur;
        reqs_last = reqs_cur;
        tsc_last = tsc_cur;
    }

    if (outdir) {
        uint64_t throughput_send_min, throughput_send_max, throughput_send_tot, throughput_send;
        uint64_t throughput_recv_min, throughput_recv_max, throughput_recv_tot, throughput_recv;
        uint64_t requests_min, requests_max, requests_tot, requests;
        uint64_t latency_min, latency_max, latency_tot, latency;
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
        fprintf(fpraw, "# %d samples\n", samples_tot);
        fprintf(fpraw, "# %d samples/sec\n", samples_per_sec);
        fprintf(fpraw, "# %d sample interval (usecs)\n", sample_interval);
        fprintf(fpraw, "# time, duration, and latency in usecs\n");
        fprintf(fpraw, "# sent and rcvd in bytes\n");
        fprintf(fpraw, "#\n");
        fprintf(fpraw, "# %8s %10s %10s %8s %10s %10s %9s\n",
                "SAMPLE", "TIME", "DURATION", "OPS", "SENT", "RCVD", "LATENCY");

        if (cur) {
            end = cur - 1;      // Discard the last sample
        }

        cur = base + 1;         // Discard the first sample
        tail = cur;
        prev = base;

        requests_min = latency_min = throughput_send_min = throughput_recv_min = ULONG_MAX;
        requests_max = latency_max = throughput_send_max = throughput_recv_max = 0;
        requests_tot = latency_tot = throughput_send_tot = throughput_recv_tot = 0;

        while (cur < end) {
            requests = cur->xsr_requests - prev->xsr_requests;
            requests_tot += requests;

            throughput_send = cur->xsr_throughput_send - prev->xsr_throughput_send;
            throughput_send_tot += throughput_send;

            throughput_recv = cur->xsr_throughput_recv - prev->xsr_throughput_recv;
            throughput_recv_tot += throughput_recv;

            latency = cur->xsr_latency - prev->xsr_latency;
            latency_tot += latency;

            cur->xsr_time -= tsc_start;

            fprintf(fpraw, "  %8u %10lu %10lu %8lu %10lu %10lu %9lu\n",
                    cur->xsr_sample,
                    (cur->xsr_time * 1000000ul) / tsc_freq,
                    ((cur->xsr_time - prev->xsr_time) * 1000000ul) / tsc_freq,
                    requests,
                    throughput_send, throughput_recv,
                    (latency * 1000000ul) / tsc_freq);

            if (cur - tail >= samples_per_sec) {
                nct_statsrec_t *srec;

                requests = latency = throughput_send = throughput_recv = 0;

                for (srec = tail + 1; srec <= cur; ++srec) {
                    requests += srec->xsr_requests - (srec - 1)->xsr_requests;
                    latency += srec->xsr_latency - (srec - 1)->xsr_latency;
                    throughput_send += srec->xsr_throughput_send - (srec - 1)->xsr_throughput_send;
                    throughput_recv += srec->xsr_throughput_recv - (srec - 1)->xsr_throughput_recv;
                }

                if (requests > requests_max) {
                    requests_max = requests;
                }
                if (requests < requests_min) {
                    requests_min = requests;
                }

                if (throughput_send > throughput_send_max) {
                    throughput_send_max = throughput_send;
                }
                if (throughput_send < throughput_send_min) {
                    throughput_send_min = throughput_send;
                }

                if (throughput_recv > throughput_recv_max) {
                    throughput_recv_max = throughput_recv;
                }
                if (throughput_recv < throughput_recv_min) {
                    throughput_recv_min = throughput_recv;
                }

                if (latency > latency_max) {
                    latency_max = latency;
                }
                if (latency < latency_min) {
                    latency_min = latency;
                }

                ++tail;
            }

            prev = cur;
            ++cur;
        }

        /* Compute averages...
         */
        samples_tot = cur - base;

        printf("\n%12s %12s %12s  %s\n", "MIN", "AVG", "MAX", "DESC");

        uint64_t requests_avg = (requests_tot * samples_per_sec) / samples_tot;
        
        printf("%12lu %12lu %12lu  REQ/s (requests per second)\n",
               requests_min,
               requests_avg,
               requests_max);

        printf("%12lu %12lu %12lu  TX/s (bytes transmitted per second)\n",
               throughput_send_min,
               (throughput_send_tot * samples_per_sec) / samples_tot,
               throughput_send_max);

        printf("%12lu %12lu %12lu  RX/s (bytes received per second)\n",
               throughput_recv_min,
               (throughput_recv_tot * samples_per_sec) / samples_tot,
               throughput_recv_max);

        printf("%12lu %12lu %12lu  Latency (microseconds per request)\n",
               ((latency_min * 1000000ul) / tsc_freq) / requests_avg,
               ((latency_tot * 1000000ul * samples_per_sec) / (tsc_freq * samples_tot)) / requests_avg,
               ((latency_max * 1000000ul) / tsc_freq) / requests_avg);

        fclose(fpraw);

        /* Create the gnuplot files...
         */
        //int scale = samples_per_sec;
        int scale = 1;
        char ylabel[128];
        char using[128];
        char ydiv[128];

        if (scale == 1) {
            strcpy(ydiv, "sample");
        } else if (scale == samples_per_sec) {
            strcpy(ydiv, "sec");
        } else {
            snprintf(ydiv, sizeof(ydiv), "%d samples", scale);
        }

        snprintf(ylabel, sizeof(ylabel), "MB / %s", ydiv);
        snprintf(using, sizeof(using),
                 "($2 / %d):(($6 * %d) / (1024 * 1024))",
                 1000000, scale);
        nct_gplot(samples_tot, samples_per_sec, term, using,
                   "rcvd", "Seconds", ylabel, "green");

        snprintf(using, sizeof(using),
                 "($2 / %d):(($7 * %d) / (1024 * 1024))",
                 1000000, scale);
        nct_gplot(samples_tot, samples_per_sec, term, using,
                   "sent", "Seconds", ylabel, "red");

        snprintf(using, sizeof(using),
                 "($2 / %d):($7 / $4)",
                 1000000);
        nct_gplot(samples_tot, samples_per_sec, term, using,
                   "latency", "Seconds", "usec/request", "black");

        snprintf(ylabel, sizeof(ylabel), "requests / %s", ydiv);
        snprintf(using, sizeof(using),
                 "($2 / %d):($4 * %d)",
                 1000000, scale);
        nct_gplot(samples_tot, samples_per_sec, term, using,
                   "requests", "Seconds", ylabel, "blue");

        free(base);
    }
}
