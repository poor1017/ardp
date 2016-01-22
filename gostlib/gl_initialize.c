/*
 * Copyright (c) 1993, 1994, 1995, 1997 by the University of Southern California
 *
 * For copying and distribution information, please see the file
 * <usc-license.h>.
 */

/* Authors: Sio-Man Cheang and Steven Augart */

#include <usc-license.h>


#include <gostlib.h>
#include <perrno.h>		/* p_err_string, p_warn_string */


void
gl_initialize(void)
{
#ifdef GL_THREADS
    gl__init_mutexes();        /* need not be called if not running threaded.
                                   Won't hurt though. */ 
#endif
    /* Initialize error messages to null string, if they're currently set to
       NULL ptr.  This means that it will never be wrong to dereference
       p_err_string.   This is not strictly necessary, but it makes the coding
       easier. */
    if(!p_err_string)
	p_err_string = stcopy("");
    if(!p_warn_string)
	p_warn_string = stcopy("");
}



