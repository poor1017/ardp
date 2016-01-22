/*
 * Copyright (c) 1993-1994 by the University of Southern California
 *
 * For copying and distribution information, please see the file
 * <usc-license.h>.
 */

#include <usc-license.h>

#include <gl_threads.h>
#include <gostlib.h>

#ifdef GL_THREADS
pthread_mutex_t        p_th_mutexPFS_VQSCANF_NW_CS;
pthread_mutex_t        p_th_mutexPFS_VQSPRINTF_NQ_CS;

static pthread_t gl__master_thread; 
static int gl__master_thread_initialized;
#endif

void
gl__init_mutexes(void)
{
#ifdef GL_THREADS
    assert(!gl__master_thread_initialized);
    gl__master_thread = pthread_self();
    gl__master_thread_initialized = 1;
    pthread_mutex_init(&(p_th_mutexPFS_VQSCANF_NW_CS), NULL);
    pthread_mutex_init(&(p_th_mutexPFS_VQSPRINTF_NQ_CS), NULL);
#endif
}


/* Return 1 if not running multi-threaded or if this is the same thread that
   called gl_initialize(); */
int
gl_is_this_thread_master(void)
{
#ifdef GL_THREADS
    if (!gl__master_thread_initialized)
	return 1;
    return pthread_equal(pthread_self(), gl__master_thread);
#else
    return 1;
#endif
}

#ifndef NDEBUG
void
gl__diagnose_mutexes(void)
{
#if defined(DIAGMUTEX)
    DIAGMUTEX(PFS_VQSCANF_NW_CS,"PFS_VQSCANF_NW_CS");
    DIAGMUTEX(PFS_VQSPRINTF_NQ_CS,"PFS_VQSPRINTF_NQ_CS");
#endif
}
#endif /*NDEBUG*/


