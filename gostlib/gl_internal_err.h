/*
 * Copyright (c) 1991-1994 by the University of Southern California
 *
 * For copying and distribution information, please see the file
 * <usc-license.h>.
 */

#include <usc-license.h>

#ifndef GL_INTERNAL_ERR_H
#include "gl_function_attributes.h" /* for NEVER_RETURNS and others */
/* This file is intended for inclusion by ardp or pfs or any other GOSTlib
   library. */
/* NOTE: It is *not* dependent on pmachine.h */

/* Internal error handling routines used by the GOST group code.  This file was
   formerly part of the regular Prospero Directory Service distribution.  */
/* It includes our own version of the assert() */
/* macro, and an interface for internal error handling, better        */
/* documented in internal_error.c                                     */


/* OUT OF MEMORY */
extern int gl__is_out_of_memory;    /* used internally by gl__fout_of_memory() */
extern void NEVER_RETURNS gl__fout_of_memory(const char file[], int lineno);
#define out_of_memory() \
    gl__fout_of_memory(__FILE__, __LINE__); 

extern void NEVER_RETURNS (*gl_out_of_memory_handler)(const char file[], int line);



/* BUFFER FULL */
/* This routine will be soon removed; eventually, there will be no fixed-size
   buffers in Prospero. */
#define interr_buffer_full() \
    gl__function_internal_error_helper(__FILE__, __LINE__, "A buffer filled up"); 
/*********************/

/* ASSERT */
#ifdef assert                  /* In case ASSERT.H was already included, we
                                  want to over-ride it.  This is compliant with
				  the definition of the assert() macro in ANSI
				  C, where NDEBUG can be defined or undefined
				  and then assert.h re-included. */
#undef assert
#endif /* assert */

/* Here, we explicitly request the function version.  That is because we don't 
   need to worry (much) about the case of the stack being trashed. */
#if 1				/* Making ASSERT() into a void-returning
				   function, just like the ANSI one is. */
#ifndef NDEBUG

#define assert(expr)				    \
    ((expr) ? ((void) 0) :			    \
     gl__function_internal_error_helper(	    \
	 __FILE__, __LINE__, "assertion violated: " #expr))

#else /* NDEBUG */
#define assert(expr) ((void) 0)
#endif /* NDEBUG */

#else  /* 1 */

#ifndef NDEBUG

#define assert(expr) do { \
    if (!(expr)) \
          gl__function_internal_error_helper(__FILE__, __LINE__, "assertion violated: " #expr); \
} while(0)

#else /* NDEBUG */
#define assert(expr) do {;} while(0)
#endif /* NDEBUG */

#endif /* 1 */
/*****************************************/


/* INTERNAL ERROR handling */
/* internal_error() */
/* This is the main macro we call when an 'internal error' has occurred.
   This is usually a "can't happen" condition. */

#define internal_error(msg) \
    gl__function_internal_error_helper(__FILE__, __LINE__, msg)

/* Function form of internal_error.  Shrinks code size.  Same interface as the
   macro version. */

#ifdef __GNUC__

#define gl__function_internal_error_helper(file, lineno, msg)		\
    (gl__function_internal_error_helper_gcc(				\
	file, lineno, __PRETTY_FUNCTION__, msg))			\

void NEVER_RETURNS gl__function_internal_error_helper_gcc(
    const char file[], int linenumber, const char funcname[], 
    const char mesg[]);

#else  /* __GNUC__ */

void NEVER_RETURNS gl__function_internal_error_helper(const char file[], int linenumber, const char mesg[]);

#endif /* __GNUC__ */

/************************************************************************/
/* ***** gl_macro_internal_error_helper() ********/
/* This has an amusing series of comments following it, so I am preserving it.
   However, it is no longer in active use.  Left around in case someone might
   have a need for it one day. --swa, 5/97 */
/* There are two helpers you can set this to.  */
/* The macro version might be useful in instances where we might have blown the
   stack.  The function version is used instead of the macro version in order
   to save a bit of code space (one function call instead of that inline code).
   Each has a description below. */

/* The macro version currently (8/9/96) displays errors of the form:
    Internal error in file foo.c (line __LINE__): strange error */
/* We are trying to figure out how to handle this one.  --steve 8/9/96  */
/* 8/9/96: I don't know under what circumstances we would have LINE be zero.
   Must've happened oor I wouldn't have written it.  --swa */
/* 8/9/96: using gl__function_internal_error_helper() always now; this
   is (a) a way around the __LINE__ problem and (b) if the stack is
   really trashed (the rationale for making the internal error handler
   into inline code), then we won't be able to call write() or 
   abort() either, so the macro wouldn't buy us anything useful. */
/* I wish there were a way of getting rid of the strlen() and the
   write() in the macro version; don't think we can do this in a
   machine-independent way, though.  If you ever encounter a problem and need
   to enable this macro again to debug it, then I recommend using inline
   assembly code with the C ASM construct. */ 
/* I know I could find a way around the macro's problem in printing the
   __LINE__ appropriately, but I am not doing so, since this code is not in
   use; we use the function version exclusively -- reaffirmed 5/97, swa*/
#define gl__macro_internal_error_helper(File, linenumber,msg) \
do { \
     /* If LINE is ZERO, then print no prefatory message. */              \
     if (linenumber) { \
        write(2, "Internal error in file " File " (line " #linenumber "): ",\
        sizeof "Internal error in file " File " (line " #linenumber "): " -1);\
     }                                                  \
     write(2, msg, strlen(msg)); \
     /* If LINE is ZERO, then print no terminal \n. */              \
     if (linenumber)                  \
        write(2, "\n", 1);        \
     if (internal_error_handler)   \
         (*internal_error_handler)(File, linenumber, msg);   \
     /* If the internal_error_handler() ever returns, we should not continue.
        */ \
     abort(); \
} while(0)

/* This function-valued variable may be set to provide additional handling for
   internal errors.  Dirsrv handles them in this way, by logging to plog.   Its
   return type used to be int instead of void because older versions of the BSD
   PCC (Portable C Compiler) could not handle pointers to void functions. */ 
/* Should this variable be prefixed with "gl_"?  --swa, 10/4/94 */
#ifdef __GNUC__
extern void NEVER_RETURNS (*internal_error_handler_gcc)(const char file[], int linenumber, const char mesg[]);
#endif
extern void NEVER_RETURNS (*internal_error_handler)(const char file[], int linenumber, const char mesg[]);

/* Not really an internal error handling routine.  Part of GOSTlib. It uses
   these routines internally though, so we leave it here. --swa, 4/27/95 */
extern void NEVER_RETURNS gl_function_arguments_error(const char *format, ...);


#endif /* GL_INTERNAL_ERR_H */

