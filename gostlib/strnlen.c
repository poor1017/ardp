/* -*-c-*- */
/*
 * Copyright (c) 1997-1998 by the University of Southern California
 *
 * For copying and distribution information, please see the file
 * <usc-license.h>.
 */

#include <usc-license.h>

#include <gostlib.h>		/* Actually declared in gl_strings.h */

/* Take the length of a string.

   Limit it just in case the string isn't null terminated.  

   We use this in the ARDP security routines, to make sure that we don't go too
   far when reading string arguments; this handles mis-formatted messages. */


size_t
strnlen(const char s[], const int maxdist)
{
    const char *sp = s;
    while (*sp && (sp < s + maxdist))
	++sp;
    return sp - s;
}

