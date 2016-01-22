#ifndef GOSTLIB_H_INCLUDED
#define GOSTLIB_H_INCLUDED
/*
 * Copyright (c) 1991-1995 by the University of Southern California
 *
 * For copying and distribution information, please see the file
 * <usc-license.h>.
 */

#include <usc-license.h>

#ifdef GL_THREADS
#include <stdio.h>
#include <sys/varargs.h>
#endif /* GL_THREADS */

#include <sys/types.h>		/* for uid_t */

#include <gl_threads.h>        /* PFS threads package.  Needed for
				   definitions of pthread_mutex_t.*/ 
#include <gl_strings.h>		/* string handling routine; users of 
				   this file currently expect it to be 
				   there.   Also consistency checks.*/
/* These constants are used when returning values from functions or setting
   variables explicitly.  They shouldn't be tested against (at least TRUE
   shouldn't), since C true is any non-zero value. */
#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

/* A minor C extension that handles a thorn in my side.  --swa */
#ifndef strequal
#define strequal(s1, s2) (strcmp(s1, s2) == 0)
#endif
#ifndef strnequal
#define strnequal(s1, s2, n) (strncmp(s1, s2, n) == 0)
#endif /*  strnequal */


/* ************************************************************ */
/* Alphabetically ordered list of published interfaces to gostlib. */
/* ************************************************************ */

extern int ardp_debug;		/* only set by gostlib, but included */
extern int ardp_priority;	/* only set by gostlib; but included */
extern void gl_initialize(void);
/* Returns a count of the # of matches. */
/* qsscanf() for a BSTRING. */
extern int gl_qbscanf(const char *bst, const char *format, ...);
/* qsscanf() for a length-tagged data buffer. */
#define qslscanf gl_qslscanf
extern int gl_qslscanf(const char *input, size_t len, const char *format, ...);
#define qsscanf gl_qsscanf
extern int gl_qsscanf(const char *input, const char *format, ...);

extern char * gl_uid_to_name_GSP(uid_t uid, char **namep);

extern void p_clear_errors(void); /* Clear perrno (used by ARDP library); 
				     clear pwarn and p_err_string and 
				     p_warn_string as well (used by 
				     PFS library). */ 

#ifdef GL_THREADS
int rfprintf(FILE *fp, char *format, ...);
#else
#define rfprintf fprintf
#endif /* GL_THREADS */

extern int pfs_debug;		/* only set by gostlib, but included */

/* ************************************************************ */
/* INTERNAL INTERFACES */
/* ************************************************************ */

/* THREADS-SPECIFIC stuff. */
#ifdef GL_THREADS
extern pthread_mutex_t        p_th_mutexPFS_VQSCANF_NW_CS;
extern pthread_mutex_t        p_th_mutexPFS_VQSPRINTF_NQ_CS;
#endif

/* These are actually only used when GL_THREADS is enabled.  We leave the
   prototypes present, because they won't hurt. --swa, 7/97 */
/* #ifdef GL_THREADS */
extern void gl__init_mutexes(void);
/* #ifndef NDEBUG */
extern void gl__diagnose_mutexes(void);
/* #endif */
/* #endif */



#endif /* GOSTLIB_H_INCLUDED */
