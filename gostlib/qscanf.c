/* Copyright (c) 1993, 1995 by the University of Southern California.
 * For copying and distribution information, please see the file
 * <usc-license.h>
 */

#include <usc-license.h>

#include <gl_parse.h>

extern int
qscanf(INPUT in, const char fmt[], ...)
{
    int retval;
    va_list ap;
    va_start(ap, fmt);
#if 0				/* This will fail */
#ifdef GL__STRING_PARSE
    if (in->sourcetype == GL__IO_BSTRING)
	retval = vqslscanf(in->u.s.s, in->u.s.pastend - in->u.s.s, fmt, ap);
    else 
#endif
#endif
    {	
	in->flags |= GL__IO_PERCENT_R_TARGET_IS_INPUT;
	retval = vqscanf(in, fmt, ap);
    }
    va_end(ap);
    return retval;
}
