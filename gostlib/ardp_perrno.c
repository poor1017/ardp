/*
 * Copyright (c) 1993, 1998 by the University of Southern California
 *
 * For copying and distribution information, please see the file
 * <usc-license.h>.
 *
 * Written  by swa 11/93     to handle error reporting consistently in libardp
 */

#include <usc-license.h>

#include <gostlib.h>
#include <gl_threads.h>
#include <perrno.h>

/* perrno is declared in ardp_perrno.c, separately from p_err_string,  in case
   some older applications have explicitly declared their own perrno variable
   (a practice we now discourage). */
#ifndef GL_THREADS
enum p_errcode perrno = 0;                 /* Declare it. */
#endif
