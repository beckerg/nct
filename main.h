/*
 * Copyright (c) 2001-2006,2011,2014-2016,2019 Greg Becker.  All rights reserved.
 */
#ifndef NCT_MAIN_H
#define NCT_MAIN_H

#ifndef __read_mostly
#define __read_mostly       __attribute__((__section__(".read_mostly")))
#endif

#ifndef likely
#define likely(_expr)       __builtin_expect(!!(_expr), 1)
#endif

#ifndef __aligned
#define __aligned(_n)       __attribute__((__aligned__(_n)))
#endif

#ifndef NELEM
#define NELEM(_a)           (sizeof((_a)) / sizeof((_a)[0]))
#endif

struct nct_req;
typedef int start_t(struct nct_req *req);

/* The command line parser set the following global variables:
 */
extern char version[];
extern char *progname;      // The programe name (i.e., the basename of argv[0])
extern int verbosity;       // The number of times -v appeared on the command line

/* By default dprint() and eprint() print to stderr.  You can change that
 * behavior by simply setting these variables to a different stream.
 */
extern FILE *dprint_stream;
extern FILE *eprint_stream;

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

extern bool have_tsc;
extern uint64_t tsc_freq;

#if __linux__ && __amd64__
static inline uint64_t
__rdtsc(void)
{
    uint32_t low, high;

    __asm __volatile("rdtsc" : "=a" (low), "=d" (high));

    return (low | ((u_int64_t)high << 32));
}
#endif

static inline uint64_t
rdtsc(void)
{
    struct timeval tv;

#if __amd64__
    if (likely(have_tsc))
        return __rdtsc();
#endif

    gettimeofday(&tv, NULL);

    return (tv.tv_sec * 1000000 + tv.tv_usec);
}

/* dprint() prints a message if (lvl >= verbosity).  'verbosity' is increased
 * by one each time the -v option is given on the command line.
 * Each message is preceded by: "progname(pid): func:line"
 */
#define dprint(lvl, ...)                                            \
do {                                                                \
    if ((lvl) <= verbosity) {                                       \
        dprint_func((lvl), __func__, __LINE__, __VA_ARGS__);        \
    }                                                               \
} while (0);

extern void dprint_func(int lvl, const char *func, int line, const char *fmt, ...)
    __attribute__((format (printf, 4, 5)));


/* You should call eprint() to print error messages that should always be shown.
 * It simply prints the given message preceded by the program name.
 */
extern void eprint(const char *fmt, ...)
    __attribute__((format (printf, 1, 2)));

#endif /* NCT_MAIN_H */
