/* qsscanf.c
   Author: Steven Augart (swa@isi.edu)
   Designed, Documented, and Written: 7/18/92 -- 7/27/92
   Ported from Gnu C to full ANSI C & traditional C, 10/5/92
   & modifier added, detraditionalized: 2/16/93.
   Made a wrapper around qscanf(), 3/2/93
*/
/* Edited for GL__STRING_PARSE redevelopment/experiment, 9/25/97 */
/* Copyright (c) 1992, 1993 by the University of Southern California. */
/* For copying and distribution information, please see the file
 * <usc-license.h> */

#include <usc-license.h>
#include <stddef.h>		/* good luck :) */
#include <stdarg.h>             /* ANSI variable arguments facility. */

#include <gostlib.h>		/* prototype for qsscanf() */
#include <gl_parse.h>

int
qsscanf(const char *s, const char *fmt, ...)
/* s: source string
   fmt: format describing what to scan for.
   remaining args: pointers to places to store the data we read, or 
      integers (field widths).
*/
{
    va_list ap;                 /* for varargs */
    int     retval;

#ifndef GL__STRING_PARSE
    INPUT_ST in_st;
    INPUT in = &in_st;
#endif

    /* Otherwise vqscanf will fail an assertion.  This is not good code, by the
       way; should be fixed. --swa */
    if (!s || s[0] == '\0') 
	return 0;

    va_start(ap, fmt);    

#ifdef GL__STRING_PARSE
    retval = vqslscanf(s, strlen(s), fmt, ap);
#else
    gl_string_to_in(s, in);
    in->flags = GL__IO_PERCENT_R_TARGET_IS_STRING;
    retval = vqscanf(in, fmt, ap);
#endif

    va_end(ap);
    return retval;
}
