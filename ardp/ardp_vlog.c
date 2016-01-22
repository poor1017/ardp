/*
 * Copyright (c) 1991-1995 by the University of Southern California
 *
 * For copying and distribution information, please see the file
 * <usc-license.h>.
 */
#include <usc-license.h>

/* Written by Steven Augart 4/27/95 ; adapted from old plog support by Cliff
   Neuman and Steven Augart */


#include <ardp.h>
#include <gl_log_msgtypes.h>

/* external interface; set by caller */

void          (*ardp_vlog)(int, RREQ, const char *, va_list) = NULL;

/* internal interface used by other functions in the ARDP library. */
/* The ARDP library itself never examines or makes use of the return value of
   ardp__log(). */


void
ardp__log(int type, RREQ req, const char * format, ...)
{
    va_list ap;

     /* Call ardp_vlog if the ardp_vlog interface is set to something
	(presumably a function to call) */
     /* If  debugging output has been requested, log it to stderr. */
     if (ardp_debug) {
	 char *logtxt = NULL;
	 va_start(ap, format);
	 logtxt = vqsprintf_stcopyr(logtxt, format, ap);
	 va_end(ap);
	 fputs("ardp__log: ", stderr);
	 fputs(logtxt, stderr);
	 putc('\n', stderr);
	 stfree(logtxt);
     }
     if (ardp_vlog) {
	 va_start(ap, format);
	 (*ardp_vlog)(type, req, format, ap);
	 va_end(ap);
     }
}

