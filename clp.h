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
 * $Id: clp.h 384 2016-01-25 11:48:25Z greg $
 */
#ifndef CLP_H
#define CLP_H

#define CLP_ERRBUFSZ        (128)

#define CLP_OPTION_END      { .optopt = 0 }
#define CLP_PARAM_END       { .name = NULL }

#define CLP_OPTION(xtype, xoptopt, xargname, xexcl, xhelp)          \
    { CLP_OPTION_TMPL((xoptopt), #xargname, (xexcl), NULL,          \
                      clp_convert_ ## xtype, &(xargname), 0,        \
                      NULL, NULL, NULL, (xhelp)) }

#define CLP_OPTION_HELP                                             \
    { CLP_OPTION_TMPL('h', NULL, "^v", "help",                      \
                      NULL, NULL, 0,                                \
                      NULL, clp_help, clp_posparam_none,            \
                      "print this help list") }

#define CLP_OPTION_VERSION(xversion)                                \
    { CLP_OPTION_TMPL('V', NULL, "*", #xversion,                    \
                      NULL, &(xversion), 0,                         \
                      NULL, clp_version, clp_posparam_none,         \
                      "print version") }

#define CLP_OPTION_VERBOSE(xverbose)                                \
    { CLP_OPTION_TMPL('v', NULL, NULL, #xverbose,                   \
                      clp_convert_incr, &(xverbose), 0,             \
                      NULL, NULL, NULL,                             \
                      "increase verbosity") }

#define CLP_OPTION_DRYRUN(xdryrun)                                  \
    { CLP_OPTION_TMPL('n', NULL, NULL, #xdryrun,                    \
                      clp_convert_incr, &(xdryrun), 0,              \
                      NULL, NULL, NULL,                             \
                      "trace execution but do not change anything") }

#define CLP_OPTION_CONF(xconf)                                      \
    { CLP_OPTION_TMPL('C', #xconf, NULL, #xconf,                    \
                      clp_convert_file, &(xconf), 0,                \
                      NULL, NULL, NULL,                             \
                      "specify a configuration file") }

#define CLP_OPTION_TMPL(xoptopt, xargname, xexcludes, xlongopt,     \
                        xconvert, xresult, xcvtarg,                 \
                        xbefore, xafter, xparamv, xhelp)            \
    .optopt = (xoptopt),        \
    .argname = (xargname),      \
    .excludes = (xexcludes),    \
    .longopt = (xlongopt),      \
    .convert = (xconvert),      \
    .result = (xresult),        \
    .cvtarg = (xcvtarg),        \
    .before = (xbefore),        \
    .after = (xafter),          \
    .paramv = (xparamv),        \
    .help = (xhelp),            \



/* By default dprint() and eprint() print to stderr.  You can change that
 * behavior by simply setting these variables to a different stream.
 */
extern FILE *clp_dprint_fp;
extern FILE *clp_eprint_fp;
extern FILE *clp_vprint_fp;

extern int verbosity;

extern void clp_printf(FILE *fp , const char *file, int line, const char *fmt, ...);


struct clp_s;
struct clp_option_s;
struct clp_posparam_s;

typedef int clp_convert_t(void *arg, const char *str, void *dst);

typedef void clp_option_cb_t(struct clp_option_s *option);

typedef void clp_posparam_cb_t(struct clp_posparam_s *param);

typedef struct clp_posparam_s {
    const char         *name;           // Name shown by help for the parameter
    const char         *help;           // One line that descibes this parameter
    clp_convert_t      *convert;        // Called for each positional argument
    void               *cvtarg;         // Argument to convert()
    void               *result;         // Where convert() stores its output
    clp_posparam_cb_t  *before;         // Called before positional argument distribution
    clp_posparam_cb_t  *after;          // Called after positional argument distribution
    void               *priv1;          // Free for use by caller of clp_parse()

    /* The following fields are used by the option parser, whereas the above
     * fields are supplied by the user.
     */
    struct clp_s       *clp;
    int                 posmin;         // Min number of positional parameters
    int                 posmax;         // Max number of positional parameters
    int                 argc;           // Number of arguments assigned to this parameter
    char              **argv;           // Ptr to arguments assigned to this parameter
} clp_posparam_t;

typedef struct clp_option_s {
    const int           optopt;         // Option letter for getopt optstring
    const char         *argname;        // Name of option argument (if any)
    const char         *excludes;       // List of options excluded by optopt
    const char         *longopt;        // Long option name for getopt
    const char         *help;           // One line that describes this option
    clp_convert_t      *convert;        // Function to convert optarg
    void               *cvtarg;         // Argument to convert()
    void               *result;         // Where convert() stores its result
    clp_option_cb_t    *before;         // Called before getopt processing
    clp_option_cb_t    *after;          // Called after getopt processing
    clp_posparam_t     *paramv;         // Option specific positional parameters
    void               *priv1;          // Free for use by caller of clp_parse()

    /* The following fields are used by the option parser, whereas the above
     * fields are supplied by the user.
     */
    struct clp_s       *clp;
    int                 given;          // Count of time this option was given
    int                 longidx;        // Index into cli->longopts[]
} clp_option_t;

typedef struct clp_s {
    const char         *basename;       // From argv[0] of clp_parsev()
    clp_option_t       *optionv;        // Argument from clp_parsev()
    clp_posparam_t     *paramv;         // Argument from clp_parsev()
    int                 optionc;        // Count of elements in optionv[]
    char               *optstring;      // The optstring for getopt
    struct option      *longopts;       // Table of long options for getopt_long()
    int                 opthelp;        // The option tied to opt_help()
    char               *errbuf;
} clp_t;

extern clp_posparam_t clp_posparam_none[];

extern clp_option_t *clp_option_find(clp_option_t *optionv, int optopt);

extern void clp_option_priv1_set(clp_option_t *option, void *priv1);

extern clp_convert_t clp_convert_bool;

extern clp_convert_t clp_convert_string;
extern clp_convert_t clp_convert_file;
extern clp_convert_t clp_convert_incr;

extern clp_convert_t clp_convert_int;
extern clp_convert_t clp_convert_u_int;
extern clp_convert_t clp_convert_long;
extern clp_convert_t clp_convert_u_long;

extern clp_convert_t clp_convert_int8_t;
extern clp_convert_t clp_convert_uint8_t;
extern clp_convert_t clp_convert_int16_t;
extern clp_convert_t clp_convert_uint16_t;
extern clp_convert_t clp_convert_int32_t;
extern clp_convert_t clp_convert_uint32_t;
extern clp_convert_t clp_convert_int64_t;
extern clp_convert_t clp_convert_uint64_t;

extern clp_option_cb_t clp_help;
extern clp_option_cb_t clp_version;

extern int clp_parsev(int argc, char **argv,
                      clp_option_t *optionv,
                      clp_posparam_t *paramv,
                      char *errbuf, int *optindp);

extern int clp_parsel(const char *line,
                      clp_option_t *optionv,
                      clp_posparam_t *paramv,
                      char *errbuf);

#endif /* CLP_H */
