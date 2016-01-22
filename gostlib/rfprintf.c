/*
 * Copyright (c) 1993-1994, 1998 by the University of Southern California
 *
 * For copying and distribution information, please see the file
 * <usc-license.h>.
 */

#include <usc-license.h>

#ifdef GL_THREADS
#include <stdio.h>
#include <stdarg.h>
#include <sys/varargs.h>

#include <gostlib.h>
/* rfprintf()
	Works like fprintf but works better in multi-threaded environments.
 */
int 
rfprintf(FILE *fp, char *format, ...)
{
#ifdef PURIFY
    register int len;
    va_list args;

    va_start(args, format);
    len = vfprintf(fp, format, args);
    va_end(args);
    return len;
#else
    
#ifdef GL_THREADS
    register int i;
    char buf[65536];
#endif
    register int len;
    va_list args;

    va_start(args, format);

#ifdef GL_THREADS
    flockfile(fp);
    len = vsprintf(buf, format, args);
    for (i = 0; i < len; i++)
	putc_unlocked((int)*(buf + i), fp);
    funlockfile(fp);
#else
    len = vfprintf(fp, format, args);
#endif
    va_end(args);
    return len;
#endif /* PURIFY */
} /* rfprintf */
#endif /* GL_THREADS */
