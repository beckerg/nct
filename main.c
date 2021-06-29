/*
 * Copyright (c) 2001-2006,2011,2014-2017,2019 Greg Becker.  All rights reserved.
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

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>
#include <assert.h>
#include <signal.h>
#include <sysexits.h>
#include <ctype.h>
#include <time.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <pthread.h>
#include <limits.h>
#include <dirent.h>

#include <rpc/types.h>
#include <rpc/auth.h>
#include <rpc/rpc.h>

#ifdef __FreeBSD__
#include <sys/sysctl.h>
#endif

#include "clp.h"
#include "main.h"
#include "nct.h"
#include "nct_shell.h"
#include "nct_nfs.h"
#include "nct_getattr.h"
#include "nct_read.h"
#include "nct_null.h"

char version[] = NCT_VERSION;
char *progname;
int verbosity;

FILE *dprint_fp;
FILE *eprint_fp;

unsigned int jobs_max = 1;
unsigned int tds_max = 1;
in_port_t port = 2049;
char *term = "png";         // Term type for gnuplot
char *outdir = NULL;
time_t duration = 60;
char *command = NULL;
char *args = NULL;
u_int mark = 0;

bool have_tsc __read_mostly;
uint64_t tsc_freq __read_mostly;

static struct clp_posparam posparamv[] = {
    CLP_POSPARAM("command", string, command, NULL, NULL, "command to run [getattr,read,shell]"),
    CLP_POSPARAM("[args...]", string, args, NULL, NULL, "command arguments"),
    CLP_POSPARAM_END
};

static struct clp_option optionv[] = {
    CLP_OPTION('d', time_t, duration, NULL, "duration of the test (in seconds)"),
    CLP_OPTION('j', u_int, jobs_max, NULL, "max number of NFS request threads"),
    CLP_OPTION('m', u_int, mark, NULL, "print status every mark seconds"),
    CLP_OPTION('o', string, outdir, NULL, "directory in which to store results"),
    CLP_OPTION('p', uint16_t, port, NULL, "remote NFSd port"),
    CLP_OPTION('T', string, term, NULL, "terminal type for gnuplot"),
    CLP_OPTION('t', u_int, tds_max, NULL, "max number of NFS reply threads"),

    CLP_OPTION_VERBOSITY(verbosity),
    CLP_OPTION_VERSION(version),
    CLP_OPTION_HELP,
    CLP_OPTION_END
};

static bool
given(int c)
{
    return !!clp_given(c, optionv, NULL);
}

int
main(int argc, char **argv)
{
    char state[256];
    char *envopts;
    char *pc;
    int rc, i;

    progname = strrchr(argv[0], '/');
    progname = (progname ? progname + 1 : argv[0]);

    dprint_fp = stderr;
    eprint_fp = stderr;
    have_tsc = false;

    initstate((u_long)time(NULL), state, sizeof(state));

    /* TODO: Get options from the environment.
     */
    {
        char progname_uc[MAXNAMLEN + 1];
        char *uc = progname_uc;

        for (pc = progname; *pc; ++pc) {
            *uc++ = toupper(*pc);
        }
        *uc = '\000';

        envopts = getenv(progname_uc);
        if (envopts) {
            eprint("getenv %s=\"%s\" ignored\n", progname_uc, envopts);
        }
    }

    rc = clp_parsev(argc, argv, optionv, posparamv);
    if (rc) {
        exit(rc);
    }

    if (given('h') || given('V'))
        return 0;

    argc -= optind;
    argv += optind;

#if __amd64__
#if __FreeBSD__
    uint64_t val;
    size_t valsz;

    valsz = sizeof(val);
    rc = sysctlbyname("kern.timecounter.invariant_tsc", (void *)&val, &valsz, NULL, 0);
    if (rc) {
        eprint("sysctlbyname(kern.timecounter.invariant_tsc): %s\n", strerror(errno));
        exit(EX_OSERR);
    }

    if (val) {
        valsz = sizeof(tsc_freq);
        rc = sysctlbyname("machdep.tsc_freq", (void *)&val, &valsz, NULL, 0);
        if (rc) {
            eprint("sysctlbyname(machdep.tsc_freq): %s\n", strerror(errno));
        } else {
            have_tsc = true;
            tsc_freq = val;
            dprint(1, "machedep.tsc_freq: %lu\n", tsc_freq);
        }
    }
#elif __linux__
    const char cmd[] = "lscpu | sed -En 's/^Model name.*([0-9]\\.[0-9][0-9])GHz$/\\1/p'";
    char buf[32];
    FILE *fp;

    fp = popen(cmd, "r");
    if (fp) {
        if (fgets(buf, sizeof(buf), fp)) {
            tsc_freq = strtod(buf, NULL) * 1000000000;
            have_tsc = true;
        }
        pclose(fp);
    }
#endif
#endif

    if (!have_tsc)
        tsc_freq = 1000000;
    dprint(1, "have_tsc %d, tsc_freq %lu\n", have_tsc, tsc_freq);

    if (outdir) {
        rc = mkdir(outdir, 0755);
        if (rc && errno != EEXIST) {
            eprint("mkdir(%s) failed: %s\n", outdir, strerror(errno));
            exit(EX_OSERR);
        }

        rc = chdir(outdir);
        if (rc) {
            eprint("chdir(%s) failed: %s\n", outdir, strerror(errno));
            exit(EX_OSERR);
        }
    }

    rc = setpriority(PRIO_PROCESS, 0, -15);
    if (rc && errno != EACCES) {
        eprint("unable to set priority: %s\n", strerror(errno));
    }

    char *rhostpath = NULL;
    start_t *start;
    nct_mnt_t *mnt;
    nct_req_t *req;
    void *priv;

    if (0 == strcmp("getattr", argv[0])) {
        priv = test_getattr_init(argc, argv, duration, &start, &rhostpath);
    }
    else if (0 == strcmp("read", argv[0])) {
        priv = test_read_init(argc, argv, duration, &start, &rhostpath);
    }
    else if (0 == strcmp("null", argv[0])) {
        priv = test_null_init(argc, argv, duration, &start, &rhostpath);
    }
    else if (0 == strcmp("shell", argv[0])) {
        return nct_shell(argc, argv);
    }
    else {
        eprint("invalid command [%s], use -h for help\n", argv[0]);
        exit(EX_USAGE);
    }

    if (!priv || !start || !rhostpath) {
        abort();
    }

    mnt = nct_mount(rhostpath, port, tds_max, jobs_max);
    if (!mnt) {
        eprint("mount %s failed\n", rhostpath);
        abort();
    }

    const long sample_period = 100 * 1000;
    nct_statsrec_t *statsv = NULL;
    u_int statsc = 0;

    if (outdir) {
        long samples_per_sec = 1000000 / sample_period;
        statsc = (duration + 1) * samples_per_sec;

        statsv = malloc(sizeof(*statsv) * statsc);
        if (!statsv)
            abort();
    }

    for (i = 0; i < jobs_max; ++i) {
        __atomic_add_fetch(&mnt->mnt_jobs_cnt, 1, __ATOMIC_SEQ_CST);

        req = nct_req_alloc(mnt);
        req->req_priv = priv;
        req->req_argc = argc;
        req->req_argv = argv;

        start(req);
    }

    nct_stats_loop(mnt, mark, sample_period, duration,
                   statsv, statsc, outdir, term);

    nct_umount(mnt);

    free(statsv);

    return 0;
}


/* Debug print.  Usually called indirectly via the dprint() macro.
 */
void
dprint_func(int lvl, const char *func, int line, const char *fmt, ...)
{
    char msg[256];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);

    if (verbosity > 1)
        fprintf(dprint_fp, "%s: %16s %4d:  %s", progname, func, line, msg);
    else
        fprintf(dprint_fp, "%s", msg);
}


/* Error print.
 */
void
eprint(const char *fmt, ...)
{
    char msg[256];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);

    fprintf(eprint_fp, "%s: %s", progname, msg);
}
