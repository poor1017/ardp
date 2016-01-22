/*
 * Copyright (c) 1991-1994 by the University of Southern California
 *
 * For copying and distribution information, please see the file
 * <usc-license.h>
 *
 * Filename       Lines   Module   Authors
 * --------       -----   ------   -------
 *                       GOSTLIB   bcn(), swa(100%), srao(), ari(), cheang()
 *
 */
#include <usc-license.h>


#include <stddef.h>             /* for NULL */
#include <stdarg.h>             /* for va_list */
#include <gostlib.h>                /* for vqsprintf_stcopyr() prototype */
#include <gl_internal_err.h>

/* This function should be called internally by a Gostlib or higher-level
   Prospero function whenever the function detects that it has been invoked in
   a way that is inconsistent with its arguments.  It is possible that the
   error is actually in the way a higher-level function was called, and that
   the higher-level function then passed its malformed argument to the
   lower-level one.  

   It is especially useful to set a breakpoint on this function when running
   under the debugger.  Since this function will normally cause a core image to
   be dumped, that core image is also useful for debugging.
*/


void 
gl_function_arguments_error(const char *format, ...)
{
    char *buf1 = NULL;
    char *buf2 = NULL;
    va_list ap;
    
    va_start(ap, format);
    buf1 = vqsprintf_stcopyr(buf1, format, ap);
    buf2 = qsprintf_stcopyr(buf2, "A function was called with inappropriate arguments: %s\n", buf1);
    gl__function_internal_error_helper((char *) NULL, 0, buf2); /* need a better name for this interface */
    /* NOTREACHED */
    va_end(ap);
}
