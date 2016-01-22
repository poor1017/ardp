## Makefile for Prospero Directory Service ARDP library. 
## Enhanced to work with the ARDP Version 1 Security Context
##
## Copyright (c) 1991-1997 by the University of Southern California
##
## For copying and distribution information, please see the file
## <gostlib/usc-license.h>.

## Lines starting with #DEV are useful if you are developing ARDP; 
## we do not ship with them enabled -- Steve & Katia, 12/13/96



RDP_LIB = libardp.a
DATA_FILE_TARGETS = $(RDP_LIB) 

## Read this only if you are compiling ARDP as part of the Prospero system:
# Old def for Prospero:
# SOURCEBASE = ../..
# include $(SOURCEBASE)/Makefile.config

## Note: We do not include Makefile.boilerplate, even when compiling under Prospero.
## This is because we want ARDP to be able to be compiled without reference to anything
## but GOSTlib.

include Makefile.config.ardp
include $(GOSTLIB_DIRECTORY)Makefile.config.gostlib

# Document the machine type.  This is used by the include file 'pmachine.h'
CFLAGS += -D$(P_MACHINE_TYPE) -D$(P_OS_TYPE)


# The include files for ARDP (standalone or otherwise) are in the ARDP
# directory and in the GOSTLIB directory.
CFLAGS += -I. -I$(GOSTLIB_DIRECTORY)

CFILES = ardp_abort.c \
	ardp_accept.c \
	ardp_add2req.c \
	ardp_breply.c \
	ardp_get_nxt.c \
	ardp_errmesg.c \
	ardp_headers.c \
	ardp_initialize.c \
	ardp_init_daemon.c \
	ardp_ioctls.c \
	ardp_labels_cos_alloc.c \
	ardp_mutexes.c \
	ardp_pr_actv.c \
	ardp_ptalloc.c \
	ardp_reply.c \
	ardp_req_security.c \
	ardp_respond.c \
	ardp_retriev.c \
	ardp_retriev_nxt.c \
	ardp_rqalloc.c \
	ardp_rreq_to_in.c \
	ardp_sealloc.c \
	ardp_sec_commit.c \
	ardp_sectype.c \
	ardp_sectype_ioctl.c \
	ardp_send.c \
	ardp_showbuf.c \
	ardp_snd_pkt.c \
	ardp_srv_ini.c \
	ardp_srvport.c \
	ardp_thr_retrieve.c \
	ardp_timeouts.c \
	ardp_vlog.c \
	ardp_xmit.c \
	authentication_asrthost.c \
	authentication_kerberos.c \
	class_of_service_tags.c \
	client_daemon.c \
	client_util.c \
	context_failed.c \
	dispatch_svc.c \
	dnscache_alloc.c \
	find_krbauth_secref.c \
	hostname2adr.c \
	myhost.c \
	next_activeQ_timeout.c \
	ardp_add2secdata4req.c \
	integrity_checksum.c \
	integrity_kerberos.c \
	make_PTEXTs_from_krb5_data.c \
	make_krb5_data_from_PTEXTs.c \
	pfs_debug.c \
	privacy_kerberos.c \
	process_contexts.c \
	processing_state_str.c \
	standalone.c \
	stcaseequal.c \
	time.c \
	unixerrstr.c

OBJECTS =	$(CFILES:.c=.o)

## Here are various warnings.  We don't need this stuff now, since it's 
## mostly set in ../gostlib/Makefile.config.compiler-warning-flags.
## We can add ARDP-specific optional warning flags.  This can be useful if you 
## want to test compilation options but don't want to enable them for the 
## entire build. 
#	CFLAGS += -D__USE_FIXED_PROTOTYPES__ -O -Wall -Wno-implicit -W
# -O sometimes makes it hard to debug.
#DEV CFLAGS += -D__USE_FIXED_PROTOTYPES__ -Wall -Wno-implicit -W
#$(OBJECTS):
#	${CC} -c ${CFLAGS} -O -D__USE_FIXED_PROTOTYPES__ -Wall $*.c
CFLAGS += -D__USE_FIXED_PROTOTYPES__ -Wall  -W

## This is for the security context.  Kerberos.  K_INC is 
## an empty macro if you aren't using it (defined in Makefile.config.ardp).

CFLAGS += $(K_INC) # $(NCC_INCS) 

### Distributed version. 
all: ${DATA_FILE_TARGETS}
### Development version
#all: ${DATA_FILE_TARGETS} 	sample/client

### DEVELOPMENT, MOSTLY.  Makes the sample client and server.
#sample/client: $(RDP_LIB) # XXX dev only
#	cd sample; $(MAKE) 

### Development only
## This generates a preprocessed file ending in .i from a .c file.  
## It is only invoked by explicit request.  We keep it in order to use the
## hack properly.

## Implementation note: We are using the traditional suffix rule instead of the
## GNU Make "implicit rules" feature so that this file will be more portable.
.SUFFIXES: .i .cppstdout
.c.i:
	$(CC) -E $(CFLAGS) $(CPPFLAGS) -o $@ $<

## Invoke this with, e.g.: gmake ardp_accept.cppstdout.
## This dumps the preprocessor output to stdout.
.c.cppstdout:
	$(CC) -E $(CFLAGS) $(CPPFLAGS) $<

#TAGS: $(CFILES) $(HFILES)
#	sh /nfs/tonga/prospero/misc/gost_etags $(CFILES) $(HFILES)
TAGS: $(CFILES) *.h
	sh /nfs/tonga/prospero/misc/gost_etags $(CFILES) *.h

#Makefile.hfile-list:
#	

clean:
	rm -f $(OBJECTS) $(RDP_LIB)

touch:
	$(MAKE) $(MFLAGS) -t 

## XXX this 'install' target  needs to be fixed; P_LIBRARIES is another higher-level 
## config file.  It is not currently used by Prospero, in any case.  --swa, srao 11/96

install:
	cp ${RDP_LIB} ${P_LIBRARIES}/${RDP_LIB}
	$(RANLIB) ${P_LIBRARIES}/${RDP_LIB}

.PHONY: all clean touch install

${RDP_LIB}: ${OBJECTS}
	rm -f ${RDP_LIB}
	ar r${AR_FLAGS} ${RDP_LIB} ${OBJECTS}
	$(RANLIB) ${RDP_LIB}

#include Makefile.hfile-list
# Dependencies
include Makefile.dependencies
