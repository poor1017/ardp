#ifndef GL_THREADS_H
#define GL_THREADS_H

/*
 * Copyright (c) 1993-1994, 1998 by the University of Southern California
 *
 * For copying and distribution information, please see the file
 * <usc-license.h>.
 */

#include <usc-license.h>

/* This is the GOST project include file to supplement the POSIX thread
   standard API.  POSIX supplemented with this package is used by the ARDP
   client and server code when they are used in a multi-threaded mode.  
*/

/* GL_THREADS must be defined if you want any multi-threading to occur. */
/* GL_THREADS is usually defined in the Makefile.config.gostlib configuration
   Makefile. */ 

/*#define GL_THREADS */               /* Part of the external interface.  */

#include <gl_internal_err.h>		/* assert */

#ifdef GL_THREADS

/* Add here any special #defines your system needs for multi-threaded code */
#ifdef SOLARIS
/* These need to be defined to compile multi-threaded with Solaris 2.5 */
/* You should also put these definitions on the command line when compiling, so
   that the proper #include prototypes are included. */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE=199506L
#endif /* ndef _POSIX_C_SOURCE */
#ifndef _REENTRANT
#define _REENTRANT
#endif /* ndef _REENTRANT */
#endif /* def SOLARIS */


#include <pthread.h>
#endif /* GL_THREADS */

/* XXX MUST excise this: search for uses of "p_th_ar" */
/* In any case, follow definitions with a:
   #define VARNAME p_th_arVARNAME[p__th_self_num()] */


#ifdef GL_THREADS
/* Handling of global shared variables (e.g., ardp_runQ) */
/* Mutex initialized in ardp_mutexes.c  (ardp_init_mutexes()). */
/* Variable defined and declared as mutexed. */
#define EXTERN_MUTEX_DECL(VARNAME) \
    extern pthread_mutex_t p_th_mutex##VARNAME
#define EXTERN_MUTEX_DEF(VARNAME) \
    pthread_mutex_t p_th_mutex##VARNAME

/* Initialize the mutex (in ardp_init_mutexes(), pfs_init_mutexes(),
   dirsrv_init_mutexes()) */
#define EXTERN_MUTEXED_INIT_MUTEX(VARNAME) \
        pthread_mutex_init(&p_th_mutex##VARNAME, NULL)
#else
/* No-ops if not threaded. */
#define EXTERN_MUTEX_DECL(VARNAME)
#define EXTERN_MUTEX_DEF(VARNAME)
#define EXTERN_MUTEXED_INIT_MUTEX(VARNAME) do {} while(0) /* no-op */
#endif /* GL_THREADS */

#define EXTERN_MUTEXED_DECL(TYPE,VARNAME) \
    EXTERN_MUTEX_DECL(VARNAME); \
    extern TYPE VARNAME
#define EXTERN_MUTEXED_DEF(TYPE,VARNAME) \
    EXTERN_MUTEX_DEF(VARNAME); \
    TYPE VARNAME
#define EXTERN_MUTEXED_DEF_INITIALIZER(TYPE,VARNAME,INITIALIZER) \
    EXTERN_MUTEX_DEF(VARNAME); \
    TYPE VARNAME = (INITIALIZER)

/* Lock and unlock a mutexed external variable.  */
/* In order to avoid potential deadlock situations, all variables must be
   locked in alphabetical order. */
   
#ifdef GL_THREADS
#if 0
#define EXTERN_MUTEXED_LOCK(VARNAME)          do { \
    int status; \
    if ((strcmp(#VARNAME, "ardp_activeQ") == 0) || \
	(strcmp(#VARNAME, "ardp_completeQ") == 0)) { \
	rfprintf(stderr, "Locking %s by %d\n", #VARNAME, pthread_self()); \
	fflush(stderr); \
    } \
    status = pthread_mutex_lock(&(p_th_mutex##VARNAME)); \
    if (status) { \
	rfprintf(stderr, "Thread (%d) Unable to lock %s: %s\n" #VARNAME, \
		 strerror(status)); \
    } \
    if ((strcmp(#VARNAME, "ardp_activeQ") == 0) || \
	(strcmp(#VARNAME, "ardp_completeQ") == 0)) { \
	rfprintf(stderr, "Thread(%d) Locking done!\n", pthread_self());} \
	fflush(stderr); \
    } \
while (0)
#define EXTERN_MUTEXED_UNLOCK(VARNAME)        do { \
    int status; \
    if ((strcmp(#VARNAME, "ardp_activeQ") == 0) || \
	(strcmp(#VARNAME, "ardp_completeQ") == 0)) { \
	rfprintf(stderr, "Unlocking %s by %d\n", #VARNAME, pthread_self()); \
	fflush(stderr); \
    } \
    status = pthread_mutex_unlock(&(p_th_mutex##VARNAME)); \
    if (status) { \
	rfprintf(stderr, "Thread (%d) Unable to unlock %s: %s\n" #VARNAME, \
		 strerror(status)); \
    } \
    if ((strcmp(#VARNAME, "ardp_activeQ") == 0) || \
	(strcmp(#VARNAME, "ardp_completeQ") == 0)) { \
	rfprintf(stderr, "Thread(%d) Unlocking done!\n", pthread_self());} \
	fflush(stderr); \
    } \
while (0)
#else
#define EXTERN_MUTEXED_LOCK(VARNAME) \
    pthread_mutex_lock(&(p_th_mutex##VARNAME));
#define EXTERN_MUTEXED_UNLOCK(VARNAME) \
    pthread_mutex_unlock(&(p_th_mutex##VARNAME));
#endif /* 0 */
#else
#define EXTERN_MUTEXED_LOCK(VARNAME) do {} while (0)
#define EXTERN_MUTEXED_UNLOCK(VARNAME) do {} while (0)
#endif /* GL_THREADS */

/* Return 1 if not running multi-threaded or if this is the same thread that
   called gl_initialize() or if we're calling this before we ever call
   gl_initialize(); */ 
int gl_is_this_thread_master(void);

/* There are additional facilities we can make available for diagnosing
   the state of various mutexes.  These were used during development, although
   they have not been recently enabled. */

#if 0 && defined(GL_THREADS)
/* This is not defined for POSIX right now.  --swa, salehi 4/98 */
/* This is used exclusively for assertions. */
#define EXTERN_MUTEXED_ISLOCKED(VARNAME)      pthread_mutex_trylock(&p_th_mutex##VARNAME)
#endif /* GL_THREADS */


#if defined(GL_THREADS) && !defined(NDEBUG) && defined(EXTERN_MUTEXED_ISLOCKED)
#define DIAGMUTEX(MX1,MX2)  if (EXTERN_MUTEXED_ISLOCKED(##MX1 )) { printf(mutex_locked_msg,MX2); }
extern char mutex_locked_msg[];
#else
#define DIAGMUTEX(MX1,MX2) do { } while(0)
#endif

/****************************************************************************
 *****                       Keyed Data Items                           *****
 ****************************************************************************/

/* Here is an interface which we may use to conserve previously-allocated
   buffers for reuse. The theory here is that hanging onto a
   previously-allocated  buffer in some fashion (such as per-thread data, or a
   static variable in the single-threaded case) may be more efficient than
   calling malloc() and free() on every call to the function.
   
   With this abstraction, we can later implement a space-for-time tradeoff by
   altering the implementation of this macro; at the moment, these are just
   aliases for stalloc() and stfree(). --swa, 6/98 */

/* All variable names are their own keys, preceded with "gl___key".  */
/* This is the sleazy interface, which doesn't do anything fancy. */
#define GL_KEYED_STRING_DECL(variable) char *variable = 0
#define GL_KEYED_STRING_DONE(variable) GL_STFREE(variable)


#endif /* ndef GL_THREADS_H */



