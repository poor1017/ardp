/*
 * Copyright (c) 1993, 1998 by the University of Southern California
 *
 * For copying and distribution information, please see the file
 * <usc-license.h>.
 */

#include <usc-license.h>

#include <stddef.h>		/* for NULL */

#include <perrno.h>
#include <gl_threads.h>

/* These are initialized in gl_initialize(). */

/* In multi-threaded mode, these are defined as macros in <perrno.h>, and
   backed-up by functions in gostlib/gl__perrno.c.

   These definitions are for single-threaded mode. */

#ifndef GL_THREADS
/* perrno already defined in gostlib/ardp_perrno.c. */
/* These are prototyped in <perrno.h> */
enum p_warncode	pwarn = 0;
char	*p_warn_string = NULL;
char	*p_err_string = NULL;
#endif

/* This variable is never referred to outside of this file. */
const char *ardp___please_do_not_optimize_me_out_of_existence; 

/* This duplicates a declaration in usc-license.h */
extern const char usc_license_string[];


void
p_clear_errors(void)
{
    /* This is tied with the internals of gostlib/usc_lic_str.c */
    /* See gostlib/usc_lic_str.c for why this assignment is here. */
    ardp___please_do_not_optimize_me_out_of_existence = usc_license_string;

    /* Actually clear the errors.  */
    perrno = PSUCCESS; if (p_err_string) p_err_string[0] = '\0';
    pwarn = PNOWARN; if (p_warn_string) p_warn_string[0] = '\0';
}

