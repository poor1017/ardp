/* -*-c-*- */
/*
 * Copyright (c) 1991-1998 by the University of Southern California
 *
 * For copying and distribution information, please see the file
 * <usc-license.h>.
 */
#include <usc-license.h>


#include <stdarg.h>		/* for va_list.  Also included in
				   <gl_strings.h> */ 

#include <perrno.h>		/* gl_set_err(), perrno prototypes */
#include <gl_strings.h>		/* for vqsprintf_GSP */

/* Pass NULL in FMT to set to nothing. */
enum p_errcode
gl_set_err(enum p_errcode perr, const char *fmt, /* Additional error text */ ...)
{
    if (fmt) {
	va_list ap;
	va_start(ap, fmt);
	vqsprintf_GSP(&p_err_string, fmt, ap);
	va_end(ap);
    } else {
	stcopy_GSP("", &p_err_string);
    }
    return perrno = perr;

}
