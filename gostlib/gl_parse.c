/*
 * Copyright (c) 1993, 1994, 1995, 1997 by the University of Southern California
 *
 * For copying and distribution information, please see the file
 * <usc-license.h>.
 */

/* Authors: Chen Yu */

#include <usc-license.h>


#include <gostlib.h>
#include <gl_parse.h>
#include <perrno.h>		/* p_err_string, p_warn_string */

/* Returns the distinguished value (from <stdio.h>) EOF if the end of the input
   is detected. */
int 
NO_SIDE_EFFECTS
in_readc(constINPUT in)
{
    switch(in->sourcetype) {
    case GL__IO_BSTRING:
	return in->u.s.s < in->u.s.pastend ? *(in->u.s.s) : EOF;
    case GL__IO_CALLERDEF:
	return (in->u.c.readc)(in);
    default:
        internal_error("invalid in->sourcetype");
    }
    /* NOTREACHED */
    return EOF; /* Unreached - keeps gcc happy */
}


/* This function may legally be called on a stream which has already reached
   EOF.  In that case, it's a no-op. */
void 
in_incc(INPUT in)
{
    switch(in->sourcetype) {
    case GL__IO_BSTRING:
	++in->u.s.s;		/* ok to increment past the end. */
        break;
    case GL__IO_CALLERDEF:
	(in->u.c.incc)(in);
	break;
    default:
        internal_error("invalid in->sourcetype");
    }
}


int
NO_SIDE_EFFECTS
in_eof(constINPUT in)
{
    return (in_readc(in) == EOF) ? EOF : 0;
}
