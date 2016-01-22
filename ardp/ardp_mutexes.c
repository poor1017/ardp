/*
 * Copyright (c) 1993-1994 by the University of Southern California
 *
 * For copying and distribution information, please see the file
 * <usc-license.h>.
 */

#include <usc-license.h>

#include <gl_threads.h>
#include <ardp.h>
#include <ardp_sec.h>

#include "dnscache_alloc.h"     /* for DNSCACHE_MAX, if defined. */

#ifdef GL_THREADS
pthread_mutex_t p_th_mutexARDP_ACCEPT; /* declaration */
#ifndef ARDP_NO_SECURITY_CONTEXT
#if defined(ARDP_SEC_HAVE_LABELS_CLASS_OF_SERVICE)
pthread_mutex_t p_th_mutexARDP_COS; /* declaration */
#endif /* defined(ARDP_SEC_HAVE_LABELS_CLASS_OF_SERVICE) */
pthread_mutex_t p_th_mutexARDP_SECTYPE; /* declaration */
#endif /* ndef ARDP_NO_SECURITY_CONTEXT */
pthread_mutex_t p_th_mutexPTEXT; /* declared in ardp_mutexes.c */
pthread_mutex_t p_th_mutexARDP_RQALLOC; /* declared in ardp_mutexes.c */
pthread_mutex_t p_th_mutexARDP_SELFNUM; /* declared in pfs_mutexes.c */
#ifdef DNSCACHE_MAX
pthread_mutex_t p_th_mutexDNSCACHE; /* declared in pfs_mutexes.c */
pthread_mutex_t p_th_mutexALLDNSCACHE; /* declared in pfs_mutexes.c */
#endif
pthread_mutex_t p_th_mutexFILES; /* declared on p__self_num.c */
pthread_mutex_t p_th_mutexFILELOCK; /* declared in flock.c */
#endif /* GL_THREADS */

#if defined(GL_THREADS)
void
ardp__diagnose_mutexes(void)
{
    fprintf(stderr, "You need to write ardp__diagnose_mutexes().");
}
#endif

void
ardp_init_mutexes(void)
{
    myaddress();                /* Calling myaddress() will initialize this
                                   function, which is good, since it's not
                                   inherently multithreaded when called for the
                                   1st time.  */
#ifdef GL_THREADS
    EXTERN_MUTEXED_INIT_MUTEX(ardp_doneQ);
    EXTERN_MUTEXED_INIT_MUTEX(ardp_runQ);
    EXTERN_MUTEXED_INIT_MUTEX(ardp_pendingQ);
    EXTERN_MUTEXED_INIT_MUTEX(ardp_activeQ);
    EXTERN_MUTEXED_INIT_MUTEX(ardp_completeQ);

    pthread_mutex_init(&(p_th_mutexARDP_ACCEPT), NULL);
#ifndef ARDP_NO_SECURITY_CONTEXT
#if defined(ARDP_SEC_HAVE_LABELS_CLASS_OF_SERVICE)
    pthread_mutex_init(&(p_th_mutexARDP_COS), NULL); /* declaration */
#endif
    pthread_mutex_init(&(p_th_mutexARDP_SECTYPE), NULL); /* declaration */
#endif
    pthread_mutex_init(&(p_th_mutexPTEXT), NULL);
    pthread_mutex_init(&(p_th_mutexARDP_RQALLOC), NULL);
    pthread_mutex_init(&(p_th_mutexARDP_SELFNUM), NULL);
#ifdef DNSCACHE_MAX
    pthread_mutex_init(&(p_th_mutexDNSCACHE), NULL);
    pthread_mutex_init(&(p_th_mutexALLDNSCACHE), NULL);
#endif
    pthread_mutex_init(&(p_th_mutexFILES), NULL);
    pthread_mutex_init(&(p_th_mutexFILELOCK), NULL);
#endif /* GL_THREADS */
}

