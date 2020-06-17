# Copyright (c) 2011,2014-2017,2019 Greg Becker.  All rights reserved.
#
# $Id: GNUmakefile 393 2016-04-14 09:21:59Z greg $


# The only variables you might need to change in this makefile are:
# PROG, SRC, HDR, LDLIBS, VPATH, and CDEFS.
#
PROG	:= nct

SRC	:= nct_req.c nct.c nct_xdr.c nct_nfs.c nct_rpc.c nct_mount.c nct_vnode.c
SRC	+= nct_read.c nct_getattr.c nct_null.c nct_shell.c
SRC	+= main.c clp.c

HDR	:= ${patsubst %.c,%.h,${SRC}}
HDR	+= nct_nfstypes.h

LDLIBS	:= -lpthread
VPATH	:=

NCT_VERSION	:= $(shell git describe --abbrev=10 --dirty --always --tags)
PLATFORM	:= $(shell uname -s | tr 'a-z' 'A-Z')

INCLUDE 	:= -I. -I../lib -I../../src/include
CDEFS 		:= -DNCT_VERSION=\"${NCT_VERSION}\"

ifneq ($(wildcard /usr/include/tirpc/rpc/rpc.h),)
	INCLUDE := -I/usr/include/tirpc ${INCLUDE}
	CDEFS += -DHAVE_TIRPC
	LDLIBS += -ltirpc
endif

CFLAGS		+= -Wall -g -O2 ${INCLUDE}
DEBUG		:= -O0 -DDEBUG -UNDEBUG -fno-omit-frame-pointer
CPPFLAGS	:= ${CDEFS}
OBJ		:= ${SRC:.c=.o}

CSCOPE_DIRS	?= \
	. ${VPATH} \
	$(patsubst %, /usr/src/%, sys include sbin lib/libc) \
	$(patsubst %, /usr/src/%, lib/libthr lib/libthread_db) \
	$(patsubst %, /usr/src/%, usr.bin) \
	$(patsubst %, /usr/src/cddl/contrib/opensolaris/%, cmd/zfs lib/libzfs) \
	$(patsubst %, /usr/src/cddl/lib/%, libzfs)

CSCOPE_EXCLUDE	?= '^/usr/src/sys/(arm|i386|ia64|mips|powerpc|sparc64|sun4v|pc98|xen|gnu|netatalk|coda|dev/sound|dev/firewire|dev/digi|dev/cardbus|dev/bktr|dev/w[il]|dev/usb/wlan|dev/xen|contrib/altq|contrib/ia64|contrib/ngatm|contrib/octeon-sdk|boot/(arm|i386|ia64|mips|powerpc|sparc64|sun4v|pc98))/.*'

# Always delete partially built targets.
#
.DELETE_ON_ERROR:

.PHONY:	all asan clean clobber cscope debug etags native tags


all: ${PROG}

asan: CFLAGS += ${DEBUG}
asan: CFLAGS += -fsanitize=address -fsanitize=undefined
asan: LDLIBS += -fsanitize=address -fsanitize=undefined
asan: ${PROG}

clean:
	rm -f ${PROG} ${OBJ} *.core
	rm -f $(patsubst %.c,.%.d,${SRC})

cleandir clobber distclean: clean
	rm -f cscope.files cscope*.out TAGS

cscope: cscope.out

cscope.out: cscope.files
	cscope -bukq

cscope.files: GNUmakefile ${HDR} ${SRC}
	find ${CSCOPE_DIRS} -name \*.[chsSylx] -o -name \*.cpp > $@.tmp
	if [ -n "${CSCOPE_EXCLUDE}" ] ; then \
		egrep -v ${CSCOPE_EXCLUDE} $@.tmp > $@.tmp2 ;\
		mv $@.tmp2 $@.tmp ;\
	fi
	mv $@.tmp $@

debug: CFLAGS += ${DEBUG}
debug: ${PROG}

native: CFLAGS += -march=native
native: ${PROG}

tags etags: TAGS

TAGS: cscope.files
	cat cscope.files | xargs etags -a --members --output=$@

# Use gmake's link rule to produce the target.
#
${PROG}: ${OBJ}
	$(LINK.o) $^ $(LOADLIBES) $(LDLIBS) -o $@


# We make ${OBJ} depend on the GNUmakefile so that all objects are rebuilt
# if the makefile changes.
#
${OBJ}: GNUmakefile

# Automatically generate/maintain dependency files.
#
.%.d: %.c
	@set -e; rm -f $@; \
	$(CC) -M $(CPPFLAGS) ${INCLUDE} $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

-include $(patsubst %.c,.%.d,${SRC})
