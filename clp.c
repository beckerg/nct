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
 * $Id: clp.c 386 2016-01-27 13:25:47Z greg $
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <sysexits.h>
#include <ctype.h>
#include <getopt.h>
#include <math.h>
#include <sys/file.h>
#include <sys/param.h>

#include "clp.h"

//#define CLP_DEBUG


__attribute__((weak)) char *progname;
__attribute__((weak)) int verbosity;

clp_posparam_t clp_posparam_none[] = {
    { .name = NULL }
};

/* dprint() prints a message if (lvl >= verbosity).  'verbosity' is
 * increased by one each time the -v option is given on the command
 * line.
 */
#ifdef CLP_DEBUG
#define dprint(lvl, ...)                                                \
do {                                                                    \
    if (verbosity >= (lvl)) {                                           \
        clp_printf(stdout, __func__, __LINE__, __VA_ARGS__);            \
    }                                                                   \
} while (0);

/* Called via the dprint() macro..
 */
void
clp_printf(FILE *fp, const char *func, int line, const char *fmt, ...)
{
    char msg[256];
    int msglen;
    va_list ap;

    if (func) {
        snprintf(msg, sizeof(msg), "%4d %-12s ", line, func);
    } else {
        snprintf(msg, sizeof(msg), "%s: ", progname);
    }
    msglen = strlen(msg);

    va_start(ap, fmt);
    vsnprintf(msg + msglen, sizeof(msg) - msglen, fmt, ap);
    va_end(ap);

    fputs(msg, fp ? fp : stderr);
}
#else
#define dprint(lvl, ...)
#endif /* CLP_DEBUG */


clp_option_t *
clp_option_find(clp_option_t *optionv, int optopt)
{
    if (optionv && optopt > 0) {
        while (optionv->optopt > 0) {
            if (optionv->optopt == optopt) {
                return optionv;
            }
            ++optionv;
        }
    }

    return NULL;
}

void
clp_option_priv1_set(clp_option_t *option, void *priv1)
{
    if (option) {
        option->priv1 = priv1;
    }
}

/* Format and save an error message for retrieval by the caller
 * of clp_parse().
 */
void
clp_eprint(clp_t *clp, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(clp->errbuf, CLP_ERRBUFSZ, fmt, ap);
    va_end(ap);
}

/* An option's conversion procedure is called each time the
 * option is seen on the command line.
 *
 * Note:  These functions are not type safe.
 */
int
clp_convert_bool(void *cvtarg, const char *str, void *dst)
{
    bool *result = dst;

    *result = ! *result;

    return 0;
}

int
clp_convert_string(void *cvtarg, const char *str, void *dst)
{
    char **result = dst;

    if (!result) {
        errno = EINVAL;
        return EX_DATAERR;
    }
    
    *result = strdup(str);

    return *result ? 0 : EX_OSERR;
}

int
clp_convert_file(void *cvtarg, const char *str, void *dst)
{
    char *mode = cvtarg ? cvtarg : "r";
    FILE **result = dst;

    if (!result) {
        errno = EINVAL;
        return EX_DATAERR;
    }
    
    *result = fopen(str, mode);

    return *result ? 0 : EX_NOINPUT;
}

int
clp_convert_inc(void *cvtarg, const char *str, void *dst)
{
    int *result = dst;

    if (!result) {
        errno = EINVAL;
        return EX_DATAERR;
    }

    ++(*result);

    return 0;
}

#define CLP_CONVERT_INTXX(xtype, xmin, xmax, xvaltype, xvalcvt)         \
    int                                                                 \
    clp_convert_ ## xtype(void *cvtarg, const char *str, void *dst)     \
    {                                                                   \
        xtype *result = dst;                                            \
        xvaltype val;                                                   \
        char *end;                                                      \
        int base;                                                       \
                                                                        \
        if (!result) {                                                  \
            errno = EINVAL;                                             \
            return EX_DATAERR;                                          \
        }                                                               \
                                                                        \
        base = cvtarg ? (intptr_t)cvtarg : 0;                           \
        if (base < 0 || base == 1 || base > 36) {                       \
            errno = EINVAL;                                             \
            return EX_DATAERR;                                          \
        }                                                               \
                                                                        \
        end = NULL;                                                     \
        errno = 0;                                                      \
                                                                        \
        val = xvalcvt(str, &end, base);                                 \
                                                                        \
        if (errno == 0 && end != str && *end == '\000') {               \
            if (val >= xmin && val <= xmax) {                           \
                *result = val;                                          \
                return 0;                                               \
            }                                                           \
                                                                        \
            errno = ERANGE;                                             \
        }                                                               \
                                                                        \
        return EX_DATAERR;                                              \
    }

CLP_CONVERT_INTXX(char, CHAR_MIN, CHAR_MAX, quad_t, strtoq);
CLP_CONVERT_INTXX(u_char, 0, UCHAR_MAX, u_quad_t, strtouq);
CLP_CONVERT_INTXX(short, SHRT_MIN, SHRT_MAX, quad_t, strtoq);
CLP_CONVERT_INTXX(u_short, 0, USHRT_MAX, u_quad_t, strtouq);
CLP_CONVERT_INTXX(int, INT_MIN, INT_MAX, quad_t, strtoq);
CLP_CONVERT_INTXX(u_int, 0, UINT_MAX, u_quad_t, strtouq);
CLP_CONVERT_INTXX(long, LONG_MIN, LONG_MAX, quad_t, strtoq);
CLP_CONVERT_INTXX(u_long, 0, ULONG_MAX, u_quad_t, strtouq);

CLP_CONVERT_INTXX(int8_t, INT8_MIN, INT8_MAX, quad_t, strtoq);
CLP_CONVERT_INTXX(uint8_t, 0, UINT8_MAX, u_quad_t, strtouq);
CLP_CONVERT_INTXX(int16_t, INT16_MIN, INT16_MAX, quad_t, strtoq);
CLP_CONVERT_INTXX(uint16_t, 0, UINT16_MAX, u_quad_t, strtouq);
CLP_CONVERT_INTXX(int32_t, INT32_MIN, INT32_MAX, quad_t, strtoq);
CLP_CONVERT_INTXX(uint32_t, 0, UINT32_MAX, u_quad_t, strtouq);
CLP_CONVERT_INTXX(int64_t, INT64_MIN, INT64_MAX, quad_t, strtoq);
CLP_CONVERT_INTXX(uint64_t, 0, UINT64_MAX, u_quad_t, strtouq);
CLP_CONVERT_INTXX(quad_t, QUAD_MIN, QUAD_MAX, quad_t, strtoq);
CLP_CONVERT_INTXX(u_quad_t, 0, UQUAD_MAX, u_quad_t, strtouq);


/* Return true if the two specified options are mutually exclusive.
 */
int
clp_excludes2(const clp_option_t *l, const clp_option_t *r)
{
    if (l && r) {
        if (l->excludes) {
            if (l->excludes[0] == '^') {
                if (!strchr(l->excludes, r->optopt)) {
                    return true;
                }
            } else {
                if (l->excludes[0] == '*' || strchr(l->excludes, r->optopt)) {
                    return true;
                }
            }
        }
        if (r->excludes) {
            if (r->excludes[0] == '^') {
                if (!strchr(r->excludes, l->optopt)) {
                    return true;
                }
            } else {
                if (r->excludes[0] == '*' || strchr(r->excludes, l->optopt)) {
                    return true;
                }
            }
        }
        if (l->paramv && r->paramv && l->paramv != r->paramv) {
            return true;
        }
    }

    return false;
}

/* Return the opt letter of any option that is mutually exclusive
 * with the specified option and was appeared on the command line
 * at least the specified number of times.
 */
clp_option_t *
clp_excludes(clp_option_t *first, const clp_option_t *option, int given)
{
    clp_option_t *o;

    for (o = first; o->optopt > 0; ++o) {
        if (o->given >= given && clp_excludes2(option, o)) {
            return o;
        }
    }

    return NULL;
}

/* Option after() procedures are called after option processing
 * for each option that was given on the command line.
 */
void
clp_version(clp_option_t *option)
{
    printf("%s\n", (char *)option->result);
}

/* Return the count of leading open brackets, and the given
 * name stripped of white space and brackets in buf[].
 */
int
clp_unbracket(const char *name, char *buf, size_t bufsz)
{
    int nbrackets = 0;

    if (!name || !buf || bufsz < 1) {
        abort();
    }

    // Eliminate white space around open brackets
    while (isspace(*name) || *name == '[') {
        if (*name == '[') {
            ++nbrackets;
        }
        ++name;
    }

    strncpy(buf, name, bufsz - 1);
    buf[bufsz - 1] = '\000';

    // Terminate buf at first white space or bracket
    while (*buf && !isspace(*buf) && *buf != ']' && *buf != '[') {
        ++buf;
    }
    *buf = '\000';

    return nbrackets;
}

/* Print lines of the form "usage: progname [options] args...".
 */
void
clp_usage(clp_t *clp, const clp_option_t *limit, FILE *fp)
{
    clp_posparam_t *paramv = clp->paramv;
    char excludes_buf[clp->optionc + 1];
    char optarg_buf[clp->optionc * 16];
    char opt_buf[clp->optionc + 1];
    clp_posparam_t *param;
    char *pc_excludes;
    clp_option_t *o;
    char *pc_optarg;
    char *pc_opt;
    char *pc;

    if (limit) {
        if (!limit->paramv) {
            return;
        }
        paramv = limit->paramv;
    }

    pc_excludes = excludes_buf;
    pc_optarg = optarg_buf;
    pc_opt = opt_buf;

    /* Build three lists of option characters:
     *
     * 1) excludes_buf[] contains all the options that might exclude or be
     * excluded by another option.
     *
     * 2) optarg_buf[] contains all the options not in (1) that require
     * an argument.
     *
     * 3) opt_buf[] contains all the rest not covered by (1) or (2).
     *
     * Note: if 'limit' is not NULL, then only options that share
     * the same paramv or have a NULL paramv may appear in one of
     * the three lists.
     */
    for (o = clp->optionv; o->optopt > 0; ++o) {
        if (limit) {
            if (clp_excludes2(limit, o)) {
                continue;
            }
        } else if (o->paramv) {
            continue;
        }

        if (o != limit) {
            if (isprint(o->optopt)) {
                if (o->excludes) {
                    *pc_excludes++ = o->optopt;
                } else if (o->argname) {
                    *pc_optarg++ = o->optopt;
                } else {
                    *pc_opt++ = o->optopt;
                }
            }
        }
    }

    *pc_excludes = '\000';
    *pc_optarg = '\000';
    *pc_opt = '\000';

    dprint(1, "option -%c:\n", limit ? limit->optopt : '?');
    dprint(1, "  excludes: %s\n", excludes_buf);
    dprint(1, "  optarg:   %s\n", optarg_buf);
    dprint(1, "  opt:      %s\n", opt_buf);

    /* Now print out the usage line in the form of:
     *
     * usage: basename [mandatory-opt] [bool-opts] [opts-with-args] [exclusive-opts] [parameters...]
     */
    fprintf(fp, "usage: %s", clp->basename);
    if (limit) {
        fprintf(fp, " -%c", limit->optopt);
    }

    if (opt_buf[0]) {
        fprintf(fp, " [-%s]", opt_buf);
    }

    for (pc = optarg_buf; *pc; ++pc) {
        clp_option_t *o = clp_option_find(clp->optionv, *pc);

        if (o) {
            fprintf(fp, " [-%c %s]", o->optopt, o->argname);
        }
    }
    

    /* Generate the mutually exclusive option usage message...
     */
    char *listv[clp->optionc + 1];
    int listc = 0;
    char *cur;
    int i;

    /* Build a vector of strings where each string contains
     * mutually exclusive options.
     */
    for (cur = excludes_buf; *cur; ++cur) {
        clp_option_t *l = clp_option_find(clp->optionv, *cur);
        char buf[1024], *pc_buf;

        pc_buf = buf;

        for (pc = excludes_buf; *pc; ++pc) {
            if (cur == pc) {
                *pc_buf++ = *pc;
            } else {
                clp_option_t *r = clp_option_find(clp->optionv, *pc);

                if (clp_excludes2(l, r)) {
                    *pc_buf++ = *pc;
                }
            }
        }

        *pc_buf = '\000';

        listv[listc++] = strdup(buf);
    }

    /* Eliminate duplicate strings.
     */
    for (i = 0; i < listc; ++i) {
        int j;

        for (j = i + 1; j < listc; ++j) {
            if (listv[i] && listv[j]) {
                if (0 == strcmp(listv[i], listv[j])) {
                    free(listv[j]);
                    listv[j] = NULL;
                }
            }
        }
    }

    /* Ensure that all options within a list are mutually exclusive.
     */
    for (i = 0; i < listc; ++i) {
        if (listv[i]) {
            for (pc = listv[i]; *pc; ++pc) {
                clp_option_t *l = clp_option_find(clp->optionv, *pc);
                char *pc2;

                for (pc2 = listv[i]; *pc2; ++pc2) {
                    if (pc2 != pc) {
                        clp_option_t *r = clp_option_find(clp->optionv, *pc2);

                        if (!clp_excludes2(l, r)) {
                            free(listv[i]);
                            listv[i] = NULL;
                            goto next;
                        }
                    }
                }
            }
        }

    next:
        continue;
    }
    
    /* Now, print out the remaining strings of mutually exclusive options.
     */
    for (i = 0; i < listc; ++i) {
        if (listv[i]) {
            char *bar = "";

            fprintf(fp, " [");

            for (pc = listv[i]; *pc; ++pc) {
                fprintf(fp, "%s-%c", bar, *pc);
                bar = " | ";
            }

            fprintf(fp, "]");
        }
    }

    /* Finally, print out all the positional parameters.
     */
    if (paramv) {
        int noptional = 0;

        for (param = paramv; param->name; ++param) {
            char namebuf[128];
            int isopt;

            isopt = clp_unbracket(param->name, namebuf, sizeof(namebuf));

            if (isopt) {
                ++noptional;
            }

            fprintf(fp, "%s%s", isopt ? " [" : " ", namebuf);

            if (param[1].name) {
                isopt = clp_unbracket(param[1].name, namebuf, sizeof(namebuf));
            }

            /* If we're at the end of the list or the next parameter
             * is not optional then print all the closing brackets.
             */
            if (!param[1].name || !isopt) {
                for (; noptional > 0; --noptional) {
                    fputc(']', fp);
                }
            }
        }
    }

    fprintf(fp, "\n");
}

static int
clp_help_cmp(const void *lhs, const void *rhs)
{
    clp_option_t * const *l = lhs;
    clp_option_t *const *r = rhs;

    int lc = isupper((*l)->optopt) ? tolower((*l)->optopt) : (*l)->optopt;
    int rc = isupper((*r)->optopt) ? tolower((*r)->optopt) : (*r)->optopt;
    
    return (lc - rc);
}

/* Print the entire help message, for example:
 *
 * usage: prog [-v] [-i intarg] src... dst
 * usage: prog -h [-v]
 * usage: prog -V
 * -h         print this help list
 * -i intarg  specify an integer argument
 * -V         print version
 * -v         increase verbosity
 * dst     specify destination directory
 * src...  specify one or more source files
 */
void
clp_help(clp_option_t *opthelp)
{
    const clp_posparam_t *param;
    const clp_option_t *option;
    clp_posparam_t *paramv;
    int longhelp;
    int optionc;
    clp_t *clp;
    int width;
    FILE *fp;
    int i;

    /* opthelp is the option that triggered clp into calling clp_help().
     * Ususally -h, but the user could have changed it...
     */
    if (!opthelp) {
        return;
    }

    fp = opthelp->priv1 ? opthelp->priv1 : stdout;
    longhelp = (opthelp->longidx >= 0);

    clp = opthelp->clp;
    optionc = clp->optionc;
    paramv = clp->paramv;

    /* Create an array of pointers to options and sort it.
     */
    clp_option_t *optionv[optionc];

    for (i = 0; i < optionc; ++i) {
        optionv[i] = clp->optionv + i;
    }
    qsort(optionv, optionc, sizeof(optionv[0]), clp_help_cmp);

    /* Print the default usage line.
     */
    clp_usage(clp, NULL, fp);

    /* Print usage lines for each option that has positional parameters
     * different than the default usage.
     * Also, determine the width of the longest combination of option
     * argument and long option names.
     */
    width = 0;
    for (i = 0; i < optionc; ++i) {
        option = optionv[i];

        if (!option->help) {
            continue;
        }

        clp_usage(clp, option, fp);

        if (option->argname) {
            int len = strlen(option->argname) + 1;

            if (longhelp && option->longopt) {
                len += strlen(option->longopt) + 4;
            }

            if (len > width) {
                width = len;
            }
        }
    }

    /* Print a line of help for each option.
     */
    for (i = 0; i < optionc; ++i) {
        char buf[width + 8];

        option = optionv[i];

        if (!option->help) {
            continue;
        }

        if (!isprint(option->optopt) && !longhelp) {
            continue;
        }

        buf[0] = '\000';

        if (longhelp && option->longopt) {
            if (isprint(option->optopt)) {
                strcat(buf, ",");
            }
            strcat(buf, " --");
            strcat(buf, option->longopt);
        }
        if (option->argname) {
            strcat(buf, " ");
            strcat(buf, option->argname);
        }

        if (isprint(option->optopt)) {
            fprintf(fp, "-%c%-*s  %s\n", option->optopt, width, buf, option->help);
        } else {
            fprintf(fp, "   %-*s  %s\n", width, buf, option->help);
        }
    }

    /* Determine the wdith of the longest positional parameter name.
     */
    if (paramv) {
        width = 0;

        for (param = paramv; param->name; ++param) {
            char namebuf[32];
            int namelen;

            clp_unbracket(param->name, namebuf, sizeof(namebuf));
            namelen = strlen(namebuf);

            if (namelen > width) {
                width = namelen;
            }
        }

        /* Print a line of help for each positional paramter.
         */
        for (param = paramv; param->name; ++param) {
            char namebuf[32];

            clp_unbracket(param->name, namebuf, sizeof(namebuf));

            fprintf(fp, "%-*s  %s\n",
                    width, namebuf, param->help ? param->help : "");
        }
    }

    /* Print detailed help if -v was given.
     */
    option = clp_option_find(clp->optionv, 'v');

    if (option && option->given == 0) {
#if 0
        fprintf(fp, "\nUse -%c for more detail", option->optopt);
        if (!longhelp && opthelp->longopt) {
            fprintf(fp, ", use --%s for long help", opthelp->longopt);
        }
        fprintf(fp, "\n");
#endif
    }

    fprintf(fp, "\n");
}

/* Determine the minimum and maximum number of arguments that the
 * given posparam vector could consume.
 */
void
clp_posparam_minmax(clp_posparam_t *paramv, int *posminp, int *posmaxp)
{
    clp_posparam_t *param;

    if (!paramv || !posminp || !posmaxp) {
        assert(0);
        return;
    }

    *posminp = 0;
    *posmaxp = 0;

    for (param = paramv; param->name; ++param) {
        char namebuf[128];
        int isopt;
        int len;

        isopt = clp_unbracket(param->name, namebuf, sizeof(namebuf));

        param->posmin = isopt ? 0 : 1;

        len = strlen(namebuf);
        if (len >= 3 && 0 == strncmp(namebuf + len - 3, "...", 3)) {
            param->posmax = 1024;
        } else {
            param->posmax = 1;
        }

        *posminp += param->posmin;
        *posmaxp += param->posmax;
    }
}

static int
clp_parsev_impl(clp_t *clp, int argc, char **argv, int *optindp)
{
    struct option *longopt;
    clp_posparam_t *paramv;
    size_t optstringsz;
    clp_option_t *o;
    int posmin;
    int posmax;
    char *pc;
    int rc;

    clp->longopts = calloc(clp->optionc + 1, sizeof(*clp->longopts));
    if (!clp->longopts) {
        clp_eprint(clp, "%s: unable to calloc longopts (%zu bytes)",
                   __func__, sizeof(*clp->longopts) * (clp->optionc + 1));
        return EX_OSERR;
    }
    longopt = clp->longopts;

    optstringsz = clp->optionc * 3 + 3;

    clp->optstring = calloc(1, optstringsz);
    if (!clp->optstring) {
        clp_eprint(clp, "%s: unable to calloc optstring (%zu bytes)",
                   __func__, optstringsz);
        return EX_OSERR;
    }

    pc = clp->optstring;

    *pc++ = '+';    // Enable POSIXLY_CORRECT sematics
    *pc++ = ':';    // Disable getopt error reporting

    /* Generate the optstring and the long options table from the options vector.
     */
    if (clp->optionv) {
        for (o = clp->optionv; o->optopt > 0; ++o) {
            if (isprint(o->optopt)) {
                *pc++ = o->optopt;

                if (o->convert == clp_convert_bool) {
                    o->argname = NULL;
                }

                if (o->argname) {
                    *pc++ = ':';
                }
            }

            if (o->longopt) {
                longopt->name = o->longopt;
                longopt->has_arg = no_argument;
                longopt->val = o->optopt;

                if (o->argname) {
                    longopt->has_arg = required_argument;
                }

                ++longopt;
            }
        }

        /* Call each option's before() procedure before option processing.
         */
        for (o = clp->optionv; o->optopt > 0; ++o) {
            if (o->before) {
                o->before(o);
            }
        }
    }

    paramv = clp->paramv;

    char usehelp[] = ", use -c for help";
    if (clp->opthelp > 0) {
        usehelp[7] = clp->opthelp;
    } else {
        usehelp[0] = '\000';
    }

    /* Reset getopt_long().
     * TODO: Create getopt_long_r() for MT goodness.
     */
    optreset = 1;
    optind = 1;

    while (1) {
        int curind = optind;
        int longidx = -1;
        clp_option_t *x;
        int c;

        c = getopt_long(argc, argv, clp->optstring, clp->longopts, &longidx);

        if (-1 == c) {
            break;
        } else if ('?' == c) {
            clp_eprint(clp, "invalid option %s%s", argv[curind], usehelp);
            return EX_USAGE;
        } else if (':' == c) {
            clp_eprint(clp, "option %s requires a parameter%s", argv[curind], usehelp);
            return EX_USAGE;
        }

        /* Look up the option.  This should not fail unless someone perturbs
         * the option vector that was passed in to us.
         */
        o = clp_option_find(clp->optionv, c);
        if (!o) {
            clp_eprint(clp, "%s: program error: unexpected option %s", __func__, argv[curind]);
            return EX_SOFTWARE;
        }

        /* See if this option is excluded by any other option given so far...
         */
        x = clp_excludes(clp->optionv, o, 1);
        if (x) {
            clp_eprint(clp, "option -%c excludes -%c%s", x->optopt, c, usehelp);
            return EX_USAGE;
        }

        ++o->given;
        o->longidx = longidx;
        if (o->paramv) {
            paramv = o->paramv;
        }

        if (o->convert) {
            rc = o->convert(o->cvtarg, optarg, o->result);

            if (rc) {
                char optbuf[] = { o->optopt, '\000' };

                clp_eprint(clp, "unable to convert '%s%s %s'%s%s",
                           (longidx >= 0) ? "--" : "-",
                           (longidx >= 0) ? o->longopt : optbuf,
                           optarg,
                           errno ? ": " : "",
                           errno ? strerror(errno) : "");

                return rc;
            }
        }
        else if (o->result && !o->argname) {

            /* Presume o->result points to an int and increment it.
             */
            clp_convert_inc(NULL, NULL, o->result);
        }
    }

    if (optindp) {
        *optindp = optind;
    }
    argc -= optind;
    argv += optind;

    posmin = 0;
    posmax = 0;

    /* Only check positional parameter counts if paramv is not NULL.
     * This allows the caller to prevent parameter processing by clp
     * and handle it themselves.
     */
    if (paramv) {
        clp_posparam_minmax(paramv, &posmin, &posmax);

        if (argc < posmin) {
            clp_eprint(clp, "mandatory positional parameters required%s", usehelp);
            return EX_USAGE;
        } else if (argc > posmax) {
            clp_eprint(clp, "extraneous positional parameters detected%s", usehelp);
            return EX_USAGE;
        }
    }

    /* Call each given option's after() procedure after all options have
     * been processed.
     */
    if (clp->optionv) {
        for (o = clp->optionv; o->optopt > 0; ++o) {
            if (o->given && o->after) {
                o->after(o);
            }
        }
    }

    if (paramv) {
        clp_posparam_t *param;
        int i;

        /* Call each parameter's before() procedure before positional parameter processing.
         */
        for (param = paramv; param->name; ++param) {
            if (param->before) {
                param->before(param);
            }
        }

        /* Distribute the given positional arguments to the positional parameters
         * using a greedy approach.
         */
        for (param = paramv; param->name && argc > 0; ++param) {
            param->argv = argv;
            param->argc = 0;

            if (param->posmin == 1) {
                param->argc = 1;
                if (param->posmax > 1) {
                    param->argc += argc - posmin;
                }
                --posmin;
            }
            else if (argc > posmin) {
                if (param->posmax > 1) {
                    param->argc = argc - posmin;
                } else {
                    param->argc = 1;
                }
            }

            dprint(1, "argc=%d posmin=%d argv=%s param=%s %d,%d,%d\n",
                   argc, posmin, *argv, param->name, param->posmin,
                   param->posmax, param->argc);

            argv += param->argc;
            argc -= param->argc;
        }

        if (argc > 0) {
            dprint(0, "args left over: argc=%d posmin=%d argv=%s\n",
                   argc, posmin, *argv);
        }

        /* Call each parameter's convert() procedure for each given argument.
         */
        for (param = paramv; param->name; ++param) {
            if (param->convert) {
                for (i = 0; i < param->argc; ++i) {
                    param->convert(param->cvtarg, param->argv[i], param->result);
                }
            }
        }

        /* Call each filled parameter's after() procedure.
         */
        for (param = paramv; param->name; ++param) {
            if (param->argc > 0 && param->after) {
                param->after(param);
            }
        }
    }

    return 0;
}

int
clp_parsev(int argc, char **argv,
           clp_option_t *optionv, clp_posparam_t *paramv,
           char *errbuf, int *optindp)
{
    char _errbuf[128];
    clp_t clp;
    int rc;

    if (argc < 1) {
        return 0;
    }

    memset(&clp, 0, sizeof(clp));

    clp.optionv = optionv;
    clp.paramv = paramv;
    clp.errbuf = errbuf ? errbuf : _errbuf;

    clp.basename = __func__;
    if (argc > 0) {
        clp.basename = strrchr(argv[0], '/');
        clp.basename = (clp.basename ? clp.basename + 1 : argv[0]);
    }

    /* Validate options.
     */
    if (optionv) {
        clp_option_t *o;

        for (o = optionv; o->optopt > 0; ++o) {
            o->clp = &clp;

            if (o->argname && !o->convert) {
                clp_eprint(&clp, "%s: option -%c requires an argument"
                           " but has no conversion function", __func__, o->optopt);
                return EX_DATAERR;
            }

            if (o->convert && !o->result) {
                clp_eprint(&clp, "%s: option -%c has a conversion function "
                           "but no value ptr", __func__, o->optopt);
                return EX_DATAERR;
            }

            if (o->after == clp_help) {
                clp.opthelp = o->optopt;
            }

            ++clp.optionc;
        }
    }

    /* Validate positional parameters.
     */
    if (paramv) {
        clp_posparam_t *param;

        for (param = paramv; param->name > 0; ++param) {
            if (param->convert && !param->result) {
                clp_eprint(&clp, "%s: parameter %s has a conversion function "
                           "but no value ptr", __func__, param->name);
                return EX_DATAERR;
            }

        }
    }

    rc = clp_parsev_impl(&clp, argc, argv, optindp);

    if (rc && !errbuf) {
        fprintf(stderr, "%s: %s\n", clp.basename, clp.errbuf);
    }

    if (clp.optstring) {
        free(clp.optstring);
    }
    if (clp.longopts) {
        free(clp.longopts);
    }

    return rc;
}

/* Create a vector of strings from src broken up by spaces.
 * Spaces between arguments are elided.
 */
int
clp_breakargs(const char *src, const char *sep, char *errbuf, int *argcp, char ***argvp)
{
    char _errbuf[CLP_ERRBUFSZ];
    bool backslash = false;
    bool dquote = false;
    bool squote = false;
    size_t argvsz;
    int argcmax;
    char **argv;
    int srclen;
    char *prev;
    char *dst;
    int argc;

    errbuf = errbuf ?: _errbuf;

    if (!src) {
        snprintf(errbuf, CLP_ERRBUFSZ, "%s: invalid input: src may not be NULL", __func__);
        return EX_SOFTWARE;
    }

    srclen = strlen(src);
    argcmax = (srclen / 2) + 1;

    /* Allocate enough space to hold the maximum needed pointers
     * plus the entire source string.  This will generally waste
     * a bit of space, but it greatly simplifies cleanup.
     */
    argvsz = sizeof(*argv) * argcmax + (srclen + 1);

    argv = malloc(argvsz);
    if (!argv) {
        snprintf(errbuf, CLP_ERRBUFSZ, "%s: unable to allocate %zu bytes",
                 __func__, argvsz);
        return ENOMEM;
    }

    dst = (char *)(argv + argcmax);
    prev = dst;
    argc = 0;

    while (*src) {
        if (backslash) {
            backslash = false;
            *dst++ = *src;
        } else if (*src == '\\') {
            backslash = true;
        } else if (*src == '"') {
            if (squote) {
                *dst++ = *src;
            } else {
                dquote = !dquote;
            }
        } else if (*src == '\'') {
            if (dquote) {
                *dst++ = *src;
            } else {
                squote = !squote;
            }
        } else if (dquote || squote) {
            *dst++ = *src;
        } else if ((!sep && isspace(*src)) || (sep && strchr(sep, *src))) {
            if (dst > prev) {
                argv[argc++] = prev;
                *dst++ = '\000';
                prev = dst;
            }
        } else {
            *dst++ = *src;
        }

        ++src;
    }

    if (dquote || squote) {
        snprintf(errbuf, CLP_ERRBUFSZ, "%s: unterminated %s quote",
                 __func__, dquote ? "double" : "single");
        free(argv);
        return EX_DATAERR;
    }
    
    if (dst > prev) {
        argv[argc++] = prev;
        *dst++ = '\000';
    }

    if (argcp) {
        *argcp = argc;
    }
    if (argvp) {
        *argvp = argv;
    } else {
        free(argv);
    }

    return 0;
}

int
clp_parsel(const char *line, clp_option_t *optionv, clp_posparam_t *paramv, char *errbuf)
{
    char **argv;
    int optind;
    int argc;
    int rc;
    int i;

    rc = clp_breakargs(line, NULL, errbuf, &argc, &argv);
    if (!rc) {
        for (i = 0; i < argc; ++i) {
            printf("%d %s\n", i, argv[i]);
        }
        rc = clp_parsev(argc, argv, optionv, paramv, errbuf, &optind);
    }

    free(argv);

    return rc;
}
