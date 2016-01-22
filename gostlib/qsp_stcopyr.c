/*
 * Copyright (c) 1993 by the University of Southern California
 *
 * For copying and distribution information, please see the file
 * <usc-license.h>.
 */

#include <usc-license.h>
#include <gostlib.h>

char *
qsprintf_stcopyr(char *buf, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    buf = vqsprintf_stcopyr(buf, fmt, ap);
    va_end(ap);
    return buf;
}


/*
 * Instructions to user: 
   Calling vqsprintf_stcopyr with the same string as the string being
   written to [buf] and as one of the strings being copied from can lead to
   nasty problems.   Don't do it.
   Example of what not to do:
	qsprintf_stcopyr(a, "%s%s", "FOO+%s", a);
   Rationale:
   (1) [buf] can be freed by vqsprintf_stcopyr() before it is
   used.  Correcting this is not enough, because
   (2) Due to the iterative behavior of vqsprintf_stcopyr, and depending on the
   amount of space available in the buffer being copied from, 
   we may restart the iteration (goto again) at any time.  At that point, the
   old string [buf] may be partially written to.
   
   This may be obvious, but not knowing it led to a bug in an application
   program I was working on -- a bug that took me an hour+ to fix.
   */
   
   
char *
vqsprintf_stcopyr(char *buf, const char *fmt, va_list ap)
{
    int tmp;
 again:
    tmp = vqsprintf(buf, p__bstsize(buf), fmt, ap);
    if (tmp > p__bstsize(buf)) {
        stfree(buf);
        buf = stalloc(tmp);
        goto again;
    }
    /* The count returned by vqsprintf includes a trailing null. */
    /* Mark this so that vqsprintf_stcopyr() returns a properly sized bstring.
       */ 
    p_bst_set_buffer_length_nullterm(buf, tmp - 1);
    return buf;
}

