/*
 * Copyright (c) 2001-2006,2011,2014-2016 Greg Becker.  All rights reserved.
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
 * $Id: main.c 392 2016-04-13 11:21:51Z greg $
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
#include "nct.h"

static char svnid[] = "$Id: main.c 392 2016-04-13 11:21:51Z greg $";

char *progname;

FILE *dprint_stream;
FILE *eprint_stream;

unsigned int nthreads = 1;
in_port_t port = 2049;
char *term = "png";     // Term type for gnuplot
char *outdir = NULL;
long duration = 60;
char *command = NULL;
char *args = NULL;
int mark = 0;

#if 0
    /* Print more detailed help if -v was given.
     */
    if (verbosity > 0) {
        printf("  COMMAND  ARGS...\n");
        printf("  getattr  [user@]rhost:path\n");
        printf("  read     [user@]rhost:path [length]\n");
        printf("  shell\n");
    } else {
        printf("Use -hv for more detailed help\n");
    }
#endif

static clp_posparam_t posparamv[] = {
    {
        .name = "command",
        .help = "command to run [getattr,read,shell]",
        .convert = clp_convert_string, .result = &command,
    },

    {
        .name = "[args...]",
        .help = "command arguments",
        .convert = clp_convert_string, .result = &args,
    },

    { .name = NULL }
};

static clp_option_t optionv[] = {
    CLP_OPTION_VERBOSE(&verbosity),
    CLP_OPTION_VERSION(svnid),
    CLP_OPTION_HELP,

    {
        .optopt = 'd', .argname = "duration",
        .help = "duration of the test (in records)",
        .convert = clp_convert_long, .result = &duration, .cvtarg = (void *)10,
    },

    {
        .optopt = 'm',
        .help = "print status once per second",
        .result = &mark,
    },

    {
        .optopt = 'o', .argname = "outdir",
        .help = "directory in which to store results file",
        .convert = clp_convert_string, .result = &outdir,
    },

    {
        .optopt = 'p', .argname = "port",
        .help = "NFS port",
        .convert = clp_convert_int, .result = &port, .cvtarg = (void *)10,
    },

    {
        .optopt = 'T', .argname = "term",
        .help = "terminal type for gnuplot",
        .convert = clp_convert_string, .result = &term,
    },

    {
        .optopt = 't', .argname = "threads",
        .help = "number of worker threads",
        .convert = clp_convert_uint, .result = &nthreads, .cvtarg = (void *)10,
    },

    { .optopt = 0 }
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
    char errbuf[CLP_ERRBUFSZ];
    char state[256];
    char *envopts;
    int optind;
    char *pc;
    int rc;
    int i;

    progname = strrchr(argv[0], '/');
    progname = (progname ? progname + 1 : argv[0]);

    dprint_stream = stderr;
    eprint_stream = stderr;

    (void)initstate((unsigned long)time((time_t *)0), state, sizeof(state));

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

    rc = clp_parsev(argc, argv, optionv, posparamv, errbuf, &optind);
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
            eprint("mkdir(%s) failed: %s\n", strerror(errno));
            exit(EX_OSERR);
        }

        rc = chdir(outdir);
        if (rc) {
            eprint("chdir(%s) failed: %s\n", strerror(errno));
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
        eprint("%s: mount %s failed\n", rhostpath);
        abort();
    }

    for (i = 0; i < nthreads; ++i) {
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

    if (verbosity >= lvl) {
        (void)snprintf(msg, sizeof(msg), "%s(%d): %s:%d ",
                       progname, getpid(), func, line);

        va_start(ap, fmt);
        vsnprintf(msg+strlen(msg), sizeof(msg)-strlen(msg), fmt, ap);
        va_end(ap);

        fputs(msg, dprint_stream);
    }
}


/* Error print.
 */
void
eprint(const char *fmt, ...)
{
    char msg[256];
    va_list ap;

    (void)snprintf(msg, sizeof(msg), "%s: ", progname);

    va_start(ap, fmt);
    vsnprintf(msg+strlen(msg), sizeof(msg)-strlen(msg), fmt, ap);
    va_end(ap);

    fputs(msg, eprint_stream);
}
