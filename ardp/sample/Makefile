### Makefile for ARDP sample programs (client and server), updated 1/98

### You must configure ARDP properly in ardp/Makefile.config.ardp-dist
### in order to use this file.  

### Here, you may need to update the locations of the ARDP library directory
### and the ARDP include directory.
ARDP_DIRECTORY=../
include $(ARDP_DIRECTORY)Makefile.config.ardp
purify = $(PURIFY_HOME)
## Configure the location of the GOSTLIB directory appropriately,
## relative to this directory ("sample") .  This  default definition
## is usually correct. 
GOSTLIB_DIRECTORY=$(ARDP_DIRECTORY)../gostlib/
include $(GOSTLIB_DIRECTORY)Makefile.config.gostlib

# This line means: the .o files are just the C files with .o instead of .c
OBJECTS =	$(CFILES:.c=.o)

## HERE TURN ON OPTIONS SPECIFIC TO THIS PROGRAM For this program
## only, we added a flag: DEMONSTRATE_SECURITY_CONTEXT.  Leave this
## on, unless you wish to test a version of ARDP without the security
## context.  (You can turn on and off the security context with the
## ARDP_NO_SECURITY_CONTEXT definition in Makefile.config.ardp.  The
## security context is on by default.)
CFLAGS += -DDEMONSTRATE_SECURITY_CONTEXT

## Link with the ARDP library before the GOST library; ARDP routines
## use function in GOSTLIB.
ARDP_AND_GOST_LIBS = $(ARDP_DIRECTORY)/libardp.a $(GOSTLIB_DIRECTORY)/libgost.a 
ARDP_AND_GOST_INCLUDES = -I$(GOSTLIB_DIRECTORY) -I$(ARDP_DIRECTORY)

CFILES	= client.c server.c share.c mt-client.c mt-server.c
CC=gcc
## Do this if using Purify
#CC_LINK = purify $(CC)
## For no purify
CC_LINK=$(CC)

# This line means we need to include the appropriate include files
# This includes Kerberos include files.
CFLAGS += $(ARDP_AND_GOST_INCLUDES) $(K_INC)

all: client server

## MT-CLIENT and MT-SERVER don't compile without threads.
mt-all: mt-client mt-server all

nthreads: nthreads.o
	${CC_LINK} ${CFLAGS}  -o $@ nthreads.o $(LIBS) $(THREAD_LIBS)

mt-client: mt-client.o share.o $(ARDP_AND_GOST_LIBS)
	${CC_LINK} ${CFLAGS}  -o $@ mt-client.o share.o $(ARDP_AND_GOST_LIBS) $(KRB_LIBS) $(LIBS) $(THREAD_LIBS)

client: client.o share.o $(ARDP_AND_GOST_LIBS)
	${CC_LINK} ${CFLAGS}  -o $@ client.o share.o $(ARDP_AND_GOST_LIBS) $(KRB_LIBS) $(LIBS) $(THREAD_LIBS)

server: server.o share.o $(ARDP_AND_GOST_LIBS)
	${CC_LINK} ${CFLAGS}  -o $@ server.o share.o $(ARDP_AND_GOST_LIBS) $(KRB_LIBS) $(LIBS) $(THREAD_LIBS)

mt-server: mt-server.o share.o $(ARDP_AND_GOST_LIBS)
	${CC_LINK} ${CFLAGS}  -o $@ mt-server.o share.o $(ARDP_AND_GOST_LIBS) $(KRB_LIBS) $(LIBS) $(THREAD_LIBS)

clean:
	$(RM) client server $(OBJECTS)

$(OBJECTS): 	sample.h

## This is to generate C preprocessor output; helpful for seeing how 
## certain macros expand.
client.i: client.c
	$(CC) $(CFLAGS) -E -o $@ client.c

# Since this is a sample client, we won't bother listing additional dependencies
# here; that will keep this Makefile simple.
# Dependencies
include Makefile.dependencies
