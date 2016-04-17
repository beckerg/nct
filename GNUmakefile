# Copyright (c) 2011,2014-2015 Greg Becker.  All rights reserved.
#
# $Id: GNUmakefile 393 2016-04-14 09:21:59Z greg $


# The only variables you might need to change in this makefile are:
# PROG, SRC, HDR, LDLIBS, and VPATH.
#
PROG	:= nct

HDR	:= main.h nct_req.h nct.h nct_xdr.h nct_nfs.h nct_rpc.h nct_mount.h nct_vnode.h
HDR	+= nct_vnode.h nct_nfstypes.h
HDR	+= nct_shell.h
HDR	+= nct_read.h nct_getattr.h
HDR	+= clp.h

SRC	:= main.c nct_req.c nct.c nct_xdr.c nct_nfs.c nct_rpc.c nct_mount.c nct_vnode.c
SRC	+= nct_shell.c
SRC	+= nct_read.c nct_getattr.c
SRC	+= clp.c

#LDLIBS	:= -lm -lpthread -lrt
LDLIBS	:= -lthr

VPATH	:=

# Uncomment USE_TSC if you have a P-state invariant TSC that is synchronized
# across all cores.  By default we use gettimeofday().
#
#CDEFS	+= -DUSE_TSC


# You probably don't need to change anything below this line.


VERSION		:= $(shell git describe --abbrev=4 --dirty --always --tags)

INCLUDE 	:= -I. -I../lib -I../../src/include
CFLAGS 		+= -Wall -g -O2 ${INCLUDE}
CDEFS 		:= -DVERSION=\"${VERSION}\"
DEBUG 		:= -g -O0 -DDEBUG -UNDEBUG
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

.PHONY:	all clean clobber cscope tags etags debug


all: ${PROG}

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

debug: CFLAGS+=${DEBUG}
debug: ${PROG}

tags etags: TAGS

TAGS: cscope.files
	cat cscope.files | xargs etags -a --members --output=$@


# Use gmake's link rule to produce the target.
#
${PROG}: ${OBJ}
	$(LINK.o) $^ $(LOADLIBES) $(LDLIBS) -o $@


# We make ${OBJ} depend on the makefile so that all objects are rebuilt
# if the makefile changes.
#
${OBJ}: GNUmakefile

# Automatically generate/maintain dependency files.
#
.%.d: %.c
	@set -e; rm -f $@; \
	$(CC) -M $(CPPFLAGS) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

-include $(patsubst %.c,.%.d,${SRC})
