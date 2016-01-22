/* vqsprintf.c
   Author: Steven Augart <swa@isi.edu>
   Written: 7/18/92 -- 7/24/92
   Long support added, 10/2/92
   vqsprintf() added, 10/6/92
   Moved to gostlib 8/97
   I am really interested in comments on this code, suggestions for making it
   faster, and criticism of my style.  Please send polite suggestions for
   improvement to swa@isi.edu.
*/

/*
 * Copyright (c) 1991-1997 by the University of Southern California
 *
 * For copying and distribution information, please see the file
 * <usc-license.h>.
 */

#include <usc-license.h>

#include <gostlib.h>

/* See vqsprintf() for documentation.
   This is slightly inefficient, since it wastes a function call.  Oh well.
   */
size_t
qsprintf(char *buf, size_t buflen, const char *fmt, ...)
{
    va_list ap;
    size_t retval;

    va_start(ap,fmt);
    retval = vqsprintf(buf, buflen, fmt, ap);
    va_end(ap);
    return retval;

}
