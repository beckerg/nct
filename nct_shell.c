/*
 * Copyright (c) 2015-2016,2019 Greg Becker.  All rights reserved.
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
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <limits.h>

#include <rpc/types.h>
#include <rpc/auth.h>
#include <rpc/rpc.h>

#include "clp.h"
#include "main.h"
#include "nct_shell.h"
#include "nct.h"

struct cmd;
typedef int cmdfunc_t(struct cmd *cmd);

static cmdfunc_t help;
static cmdfunc_t ls;
static cmdfunc_t mount;
static cmdfunc_t nyi;

struct cmd {
    const char *name;
    cmdfunc_t *func;
    const char *help;
    const char *line;
};

static struct cmd cmdv[] = {
    { "cd",       nyi,    "change current working directory" },
    { "getattr",  nyi,    "run NFS getattr" },
    { "help",     help,   "print this help list" },
    { "ls",       ls,     "list files" },
    { "mkdir",    nyi,    "make directories" },
    { "mount",    mount,  "mount an nfs file system" },
    { "rm",       nyi,    "remove directory entries" },
    { "rmdir",    nyi,    "remove directory" },
    { "umount",   nyi,    "unmount an nfs file system" },
    { }
};

static char errbuf[128];

static int
nyi(struct cmd *cmd)
{
    snprintf(errbuf, sizeof(errbuf), "command not yet implemented\n");

    return ENOTSUP;
}

static int
help(struct cmd *cmd)
{
    int width = 7;
    int i;

    printf("  %*s  %s\n", width, "Command", "Description");

    for (i = 0; cmdv[i].name; ++i) {
        printf("  %-*s  %s\n", width, cmdv[i].name, cmdv[i].help);
    }

    printf("\n");

    return 0;
}

static int
ls(struct cmd *cmd)
{
    return nyi(cmd);
}

static in_port_t port = 2049;
static char *rhostpath, *mntnode;
static size_t readsize = 131072;
static size_t writesize = 131072;

static struct clp_posparam mount_posparamv[] = {
    CLP_POSPARAM("rhostpath", string, rhostpath, NULL, NULL, "[user@]rhost:path"),
    CLP_POSPARAM("node", string, mntnode, NULL, NULL, "mount point"),
    CLP_POSPARAM_END
};

static struct clp_option mount_optionv[] = {
    CLP_OPTION('p', uint16_t, port, NULL, "remote NFSd port"),
    CLP_OPTION('r', size_t, readsize, NULL, "read size"),
    CLP_OPTION('w', size_t, writesize, NULL, "write size"),

    CLP_OPTION_VERBOSITY(verbosity),
    CLP_OPTION_VERSION(version),
    CLP_OPTION_HELP,
    CLP_OPTION_END
};

static bool
mount_given(int c)
{
    return !!clp_given(c, mount_optionv, NULL);
}

static int
mount(struct cmd *cmd)
{
    int argc, rc;
    char **argv;

    rc = clp_breakargs(cmd->line, NULL, &argc, &argv);
    if (rc)
        return rc;

    rc = clp_parsev(argc, argv, mount_optionv, mount_posparamv);
    if (rc) {
        free(argv);
        return rc;
    }

    if (mount_given('h') || mount_given('V')) {
        free(argv);
        return 0;
    }

    //argc -= optind;
    //argv += optind;

    free(argv);

    return nyi(cmd);
}

int
nct_shell(int argc, char **argv)
{
    char linebuf[1024], *line, *pc;
    int len, rc, i;

    while (1) {
        const char *errmsg = "invalid";
        struct cmd *cmd = NULL;

        printf("> ");

        line = fgets(linebuf, sizeof(linebuf), stdin);
        if (!line)
            break;

        while (*line && isspace(*line))
            ++line;

        pc = line;
        while (*pc && !isspace(*pc))
            ++pc;

        len = pc - line;
        if (len < 1)
            continue;

        for (i = 0; cmdv[i].name; ++i) {
            if (strncasecmp(line, cmdv[i].name, len))
                continue;

            if (cmd) {
                errmsg = "ambiguous";
                cmd = NULL;
                break;
            }

            cmd = cmdv + i;
            cmd->line = line;

            if (strlen(cmd->name) == len)
                break; /* exact match */
        }

        if (!cmd) {
            printf("%s command '%.*s', type 'help' for help\n\n", errmsg, len, line);
            continue;
        }

        rc = cmd->func(cmd);
        if (rc)
            printf("%s\n", errbuf);
    }

    return 0;
}
