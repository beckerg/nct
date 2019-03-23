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
 * $Id: nct_shell.c 392 2016-04-13 11:21:51Z greg $
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
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

#include "clp.h"
#include "main.h"
#include "nct_shell.h"
#include "nct_nfs.h"
#include "nct_getattr.h"
#include "nct_read.h"
#include "nct.h"

typedef int cmdfunc_t(int argc, char **argv, char *errbuf, size_t errbufsz);

static cmdfunc_t help;
static cmdfunc_t ls;
static cmdfunc_t mount;

struct {
    const char *name;
    cmdfunc_t *func;
    const char *help;
} cmd[] = {
    { "help",   help,   "print this help list" },
    { "ls",     ls,     "list files" },
    { "mount",  mount,  "mount an nfs file system" },
    { }
};

static int
help(int argc, char **argv, char *errbuf, size_t errbufsz)
{
    int width = 7;
    int i;

    printf("  %*s  %s\n", width, "Command", "Description");

    for (i = 0; cmd[i].name; ++i) {
        printf("  %-*s  %s\n", width, cmd[i].name, cmd[i].help);
    }

    printf("\n");

    return 0;
}

static int
ls(int argc, char **argv, char *errbuf, size_t errbufsz)
{
    snprintf(errbuf, errbufsz, "%s command not yet implemented\n", argv[0]);

    return ENOTSUP;
}

static int
mount(int argc, char **argv, char *errbuf, size_t errbufsz)
{
    snprintf(errbuf, errbufsz, "%s command not yet implemented\n", argv[0]);

    return ENOTSUP;
}


int
nct_shell(int argc, char **argv)
{
    const size_t errbufsz = 128;
    char linebuf[1024], *line;
    char errbuf[errbufsz];
    int rc, i;

    while (1) {
        printf("> ");

        line = fgets(linebuf, sizeof(linebuf), stdin);
        if (!line) {
            break;
        }

        rc = clp_breakargs(line, NULL, errbuf, errbufsz, &argc, &argv);
        if (rc) {
            printf("%s\n", errbuf);
            continue;
        }

        if (argc < 1) {
            free(argv);
            continue;
        }

        for (i = 0; cmd[i].name; ++i) {
            if (0 == strcasecmp(argv[0], cmd[i].name)) {
                break;
            }
        }

        if (!cmd[i].name) {
            if (argv[0][0]) {
                printf("invalid command %s, type 'help' for help\n\n", argv[0]);
            }
            free(argv);
            continue;
        }

        rc = cmd[i].func(argc, argv, errbuf, errbufsz);
        if (rc) {
            printf("%s\n", errbuf);
            continue;
        }

        free(argv);
    }

    return 0;
}
