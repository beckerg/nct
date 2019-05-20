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

#include <rpc/types.h>
#include <rpc/auth.h>
#include <rpc/rpc.h>

#ifdef __FreeBSD__
#include <sys/sysctl.h>
#endif

#include "clp.h"
#include "main.h"
#include "nct_shell.h"
#include "nct_nfs.h"
#include "nct_getattr.h"
#include "nct_read.h"
#include "nct_null.h"
#include "nct.h"

char version[] = NCT_VERSION;
char *progname;
int verbosity;

FILE *dprint_fp;
FILE *eprint_fp;

unsigned int jobs = 1;
in_port_t port = 2049;
char *term = "png";         // Term type for gnuplot
char *outdir = NULL;
long duration = 60;
char *command = NULL;
char *args = NULL;
bool mark = false;

static clp_posparam_t posparamv[] = {
    { .name = "command",
      .help = "command to run [getattr,read,shell]",
      .convert = clp_cvt_string, .cvtdst = &command, },

    { .name = "[args...]",
      .help = "command arguments",
      .convert = clp_cvt_string, .cvtdst = &args, },

    CLP_PARAM_END
};

static clp_option_t optionv[] = {
    CLP_OPTION_VERBOSE(verbosity),
    CLP_OPTION_VERSION(version),
    CLP_OPTION_HELP,

    CLP_OPTION(long, 'd', duration, NULL, NULL, "duration of the test (in seconds)"),
    CLP_OPTION(u_int, 'j', jobs, NULL, NULL, "max number of concurrent jobs (worker threads)"),
    CLP_OPTION(bool, 'm', mark, NULL, NULL, "print a status mark once per second"),
    CLP_OPTION(string, 'o', outdir, NULL, NULL, "directory in which to store results"),
    CLP_OPTION(uint16_t, 'p', port, NULL, NULL, "remote NFSd port"),
    CLP_OPTION(string, 'T', term, NULL, NULL, "terminal type for gnuplot"),

    CLP_OPTION_END
};

static bool
given(int c)
{
    clp_option_t *opt = clp_option_find(optionv, c);

    return (opt && opt->given);
}

int
main(int argc, char **argv)
{
    char errbuf[128];
    char state[256];
    char *envopts;
    int optind;
    char *pc;
    int rc;
    int i;

    progname = strrchr(argv[0], '/');
    progname = (progname ? progname + 1 : argv[0]);

    dprint_fp = stderr;
    eprint_fp = stderr;

    initstate((u_long)time(NULL), state, sizeof(state));

    /* TODO: Get options from the environment.
     */
    {
        char PROGNAME[1024];
        char *uc = PROGNAME;

        for (pc = progname; *pc; ++pc) {
            *uc++ = toupper(*pc);
        }
        *uc = '\000';

        envopts = getenv(PROGNAME);
        if (envopts) {
            eprint("environment options %s=\"%s\" ignored\n", PROGNAME, envopts);
        }
    }

    rc = clp_parsev(argc, argv, optionv, posparamv, errbuf, sizeof(errbuf), &optind);
    if (rc) {
        eprint("%s\n", errbuf);
        exit(rc);
    }

    if (given('h') || given('V'))
        return 0;

    argc -= optind;
    argv += optind;

#ifdef USE_TSC
#ifdef __FreeBSD__
    size_t sz = sizeof(tsc_freq);
    int ival;

    sz = sizeof(ival);
    rc = sysctlbyname("kern.timecounter.smp_tsc", (void *)&ival, &sz, NULL, 0);
    if (rc) {
        eprint("sysctlbyname(kern.timecounter.smp_tsc): %s\n", strerror(errno));
        exit(EX_OSERR);
    }

    if (!ival) {
        dprint(0, "unable to determine if the TSC is SMP safe, "
               "output will likely be incorrect\n");
    }

    sz = sizeof(tsc_freq);
    rc = sysctlbyname("machdep.tsc_freq", (void *)&tsc_freq, &sz, NULL, 0);
    if (rc) {
        eprint("sysctlbyname(machdep.tsc_freq): %s\n", strerror(errno));
        exit(EX_OSERR);
    }

    dprint(1, "machedep.tsc_freq: %lu\n", tsc_freq);
#else
#error "Don't know how to determine the TSC frequency on this platform"
#endif
#endif

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

    mnt = nct_mount(rhostpath, port);
    if (!mnt) {
        eprint("mount %s failed\n", rhostpath);
        abort();
    }

    for (i = 0; i < jobs; ++i) {
        req = nct_req_alloc(mnt);

        req->req_priv = priv;
        req->req_argc = argc;
        req->req_argv = argv;

        nct_worker_create(mnt, start, req);
    }

    nct_stats_loop(mnt, duration, mark, outdir, term);

    nct_umount(mnt);

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
