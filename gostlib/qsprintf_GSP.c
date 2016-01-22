/* -*-c-*- */
/*
 * Copyright (c) 1997 by the University of Southern California
 *
 * For copying and distribution information, please see the file
 * <usc-license.h>.
 */

#include <usc-license.h>

#include <gl_strings.h>

char *
vqsprintf_GSP(char **bufp, const char *fmt, va_list ap)
{
    return *bufp = vqsprintf_stcopyr(*bufp, fmt, ap);
}

char *
qsprintf_GSP(char **bufp, const char *fmt, ...)
{
    va_list ap;
    char *dummy = NULL;
    if (!bufp)
	bufp = &dummy;
    va_start(ap, fmt);
    vqsprintf_GSP(bufp, fmt, ap);
    va_end(ap);
    return *bufp;
}

