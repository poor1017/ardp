/* -*-c-*- */
/*
 * Copyright (c) 1991-1997 by the University of Southern California
 *
 * For copying and distribution information, please see the file
 * <usc-license.h>.
 */

#include <usc-license.h>

#include <gl_parse.h>

/* This actually won't compile. */
#error This should not be compiled yet.
/* # of bytes available to read */
size_t 
in_nunread(constINPUT in)
{
    if (in->sourcetype == GL__IO_STRING) {
        linebuflen = eol->u.s.s - in->u.s.s; /* string type doesn't use offset
						member. */
    } else {
        linebuflen = eol->offset - in->offset; /* size that strlen() would
                                                  return if it worked on these
						  types. */ 
    }

}
