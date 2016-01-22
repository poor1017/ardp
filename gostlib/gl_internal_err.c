/*
 * Copyright (c) 1992,1994 by the University of Southern California
 *
 * For copying and distribution information, please see the file
 * <usc-license.h>.
 */

#include <usc-license.h>

#include <unistd.h>		/* write()  prototype */
#include <gostlib.h>
#include <gl_internal_err.h>
#include <stdio.h>

/* A function version of the gl__macro_internal_error() macro.  Interchangeable
   with that, except for behavior in stack mess-up situations and MALLOC
   errors. */
/* See gl_internal_err.h */

void
NEVER_RETURNS
#ifdef __GNUC__
gl__function_internal_error_helper_gcc(
    const char file[], int line, const char funcname[], const char msg[])
#else
gl__function_internal_error_helper(const char file[], int line, const char msg[])
#endif
{
    if (internal_error_handler)   
        internal_error_handler(file, line, msg);   
     /* If the internal_error_handler() returns, or was not defined, then we
	display a message to stderr and abort execution. */
    fputs("Internal error", stderr);
#ifdef __GNUC__
    if (funcname || file || line)
	fputs(" in", stderr);
#else
    if (file || line)
	fputs(" in", stderr);
#endif

#ifdef __GNUC__
    if (funcname)
	rfprintf(stderr, " function %s,", funcname);
#endif
    if (file)
	rfprintf(stderr, " file %s", file);
    if (line)
	rfprintf(stderr, " (line %d)", line);
    if (msg)
	rfprintf(stderr, ": %s", msg);
    fputc('\n', stderr);
    /* internal_error() must never return; should break.
     */
    abort();
    /* NOTREACHED */
}


/* This is set by gl__fout_of_memory().  It is looked at by the server restart
   code in server/dirsrv.c and server/restart_srv.c. */

int gl__is_out_of_memory = 0;

void NEVER_RETURNS (*gl_out_of_memory_handler)(const char file[], int line) = NULL;


void
NEVER_RETURNS
gl__fout_of_memory(const char file[], int line) 
{
    if (gl_out_of_memory_handler)
	(*gl_out_of_memory_handler)(file, line);
    gl__is_out_of_memory++;
    /* If we are out of memory, then let's try to display an error message
       right now.  This uses a few bytes more than the minimum possible
       necessary amount of stack space.   The minimum possible would be if this
       were part of the out_of_memory() macro; that would save the call to
       gl__fout_of_memory(). --swa, salehi 5/98 */
#define alert_string "gostlib: Out of Memory!\n"
    write(2, alert_string, sizeof alert_string);
    gl__function_internal_error_helper(file, line, "Out of Memory");
}

