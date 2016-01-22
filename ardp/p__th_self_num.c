/*
 * Copyright (c) 1991-1994 by the University of Southern California
 *
 * For copying and distribution information, please see the file
 * <usc-license.h>.
 */

#include <usc-license.h>

#include <pfs_threads.h>
#include <ardp.h>                /* for internal error stuff. */

/* This is the only section of code which calls the pthread_ routines directly.
   Everything else is through the interface in pfs_threads.h. */

#ifdef PFS_THREADS
/* Don't compile this if !PFS_THREADS, because it overrides macro definitions
   */ 
static struct {
    p_th_t thread;
    int       inuse;
} thread_map[P_MAX_NUM_THREADS];

#ifdef PFS_THREADS_SOLARIS
thread_key_t thread_map_key;
#endif

int
p__th_self_num(void)
{
/* XXX for later: use SETSPECIFIC and GETSPECIFIC  in the POSIX implementation
   as well. */
#if defined(PFS_THREADS_POSIX) || defined(PFS_THREADS_FLORIDA) || defined(PFS_THREADS_PROVEN) || !defined(NDEBUG)
    p_th_t self = p_th_self();
#endif

#if defined(PFS_THREADS_POSIX) || defined(PFS_THREADS_FLORIDA) || defined(PFS_THREADS_PROVEN)
    int i;

    for (i = 0; i < P_MAX_NUM_THREADS; ++i) {
        if (thread_map[i].inuse 
            && p_th_equal(thread_map[i].thread, self))
            return i;
    }
#endif
#ifdef PFS_THREADS_SOLARIS
	long val;

	if (!thread_map_key) return 0;	/* Assume not threading */
	if (!thr_getspecific(thread_map_key,(void *)&val)) {
		assert(p_th_equal(thread_map[val].thread, self));
		assert(val < P_MAX_NUM_THREADS);    /* i.e. 0 .. MAX-1 */
		return val;
	}
#endif /*PFS_THREADS_SOLARIS*/
    internal_error("p__th_self_num() called for a thread that didn't have its \
number set with p__th_allocate_self_num() or p__th_set_self_master()");
}


void
p__th_set_self_master(void)
{
    assert(!thread_map[0].inuse);
    thread_map[0].inuse = 1;
    thread_map[0].thread = p_th_self();
#ifdef PFS_THREADS_SOLARIS
	assert(!thread_map_key);
	thr_keycreate(&thread_map_key, NULL);
	thr_setspecific(thread_map_key, 0);
#endif
}
    


/*
 * This allocates a number to this thread.
 * It also does consistency checking to make sure the function is called
 * only on a thread that doesn't have a number already allocated.
 */
void
p_th_allocate_self_num(void)
{
#ifndef NDEBUG
    int allocated = 0;
#endif
    p_th_t self = p_th_self();
    int i;

#ifdef PFS_THREADS_SOLARIS
    assert(thread_map_key);	/* Better be initialized*/
#endif
    /* I think this needs mutexing */
    p_th_mutex_lock(p_th_mutexARDP_SELFNUM);
    for (i = 0; i < P_MAX_NUM_THREADS; ++i) {
        if (!thread_map[i].inuse) {
#ifndef NDEBUG
            if (!allocated) {
#endif
                thread_map[i].thread = self;
                thread_map[i].inuse = 1;
#ifdef PFS_THREADS_SOLARIS
		thr_setspecific(thread_map_key,(void *)i);
#endif
#ifdef NDEBUG
	     break;
	    }
#else
	     ++allocated;
            }
        } else {
            if(p_th_equal(thread_map[i].thread, self)) {
                internal_error("Shouldn't allocate a number to a thread twice.");
            }
	}
#endif
    } /* for */
    p_th_mutex_unlock(p_th_mutexARDP_SELFNUM);
}

void
p_th_deallocate_self_num(void)
{
    int i = p__th_self_num();
    thread_map[i].inuse = 0;
}

#ifndef NDEBUG
char mutex_locked_msg[] = "Mutex %s locked\n";
#endif

#ifdef PFS_THREADS_SOLARIS
/* Return true if fails to lock - i.e was already locked */
int
p_th_solaris_mutex_islocked(mutex_t *mp)
{
  int retval;
  if ((retval = mutex_trylock(mp))) {
    return retval;
  } else {
    return mutex_unlock(mp);
  }
    
}

#endif /* PFS_THREADS_SOLARIS */

#endif /*PFS_THREADS*/

