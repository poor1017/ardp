				# Makefile for Prospero Directory Service ARDP library. 
GOST_LIB = libgost.a
DATA_FILE_TARGETS = $(GOST_LIB) 
GOSTLIB_DIRECTORY=	# unset -- current directory here.
# Def. when part of Prospero:
# SOURCEBASE = ..
# include $(SOURCEBASE)/Makefile.config
include $(GOSTLIB_DIRECTORY)Makefile.config.gostlib

# because <gostlib.h> is currently in the gostlib directory.
CFLAGS += -I.

CFILES = ardp_debug.c \
	ardp_int_err.c \
	ardp_perrno.c \
	ardp_priority.c \
	bstring_offset_to_in.c \
	func_arg_err.c \
	gl__input_copy.c \
	gl__perrno.c \
	gl_bstcopy.c \
	gl_initialize.c \
	gl_internal_err.c \
	gl_mutexes.c \
	gl_set_bstring_length.c \
	gl_set_err.c \
	in_readc.c \
	p_config.c \
	p_err_string.c \
	pfs_debug.c \
	qscanf.c \
	qsp_stcopyr.c \
	qsprintf.c \
	qsprintf_GSP.c	\
	rfprintf.c \
	stcopy.c \
	stcopy_GSP.c \
	string_to_in.c \
	stringlen_to_in.c \
	strnlen.c \
	uid_to_name.c \
	usc_lic_str.c \
	vqscanf.c \
	vqsprintf.c \
	wholefiltoin.c

#DEV MODIFIED for the 'retro' (qsscanf.prev.c) implementation
#DEV Here in the other (non-retro) implementation : 	qsscanf.c vqsscanf.c

## RETRO (1994) 
## Retro is impermissible for production use, since (although faster) it does not
## allow one to read protocol lines containing null bytes.
# If you turn this on, turn off GL__STRING_PARSE
#CFILES += qsscanf.prev.c
## MODERN (1997)
## This is good good good.  Works well.
CFILES += vqsscanf.c qsscanf.c
CFLAGS += -DGL__STRING_PARSE



OBJECTS =	$(CFILES:.c=.o)

all:	${DATA_FILE_TARGETS}

install:
	cp ${GOST_LIB} ${P_BINARIES}/${GOST_LIB}
	$(RANLIB) ${P_BINARIES}/${GOST_LIB}

clean:
	rm -f $(OBJECTS) $(GOSTLIB)

touch:
	$(MAKE) $(MFLAGS) -t

.PHONY: all install clean touch
${GOST_LIB}: ${OBJECTS} Makefile
	rm -f ${GOST_LIB}
	$(AR) r${AR_FLAGS} ${GOST_LIB} ${OBJECTS}
	$(RANLIB) ${GOST_LIB}

# Dependencies
include Makefile.dependencies
