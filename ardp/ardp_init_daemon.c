/*
 * Copyright (c) 1998 by the University of Southern California
 *
 * For copying and distribution information, please see the file
 * <usc-license.h>.
 */

/* Author: Nader Salehi */

#ifdef GL_THREADS

#include <usc-license.h>
#include <stdlib.h>		/* for getenv() */
#include <string.h>
#include <pthread.h>

#include <ardp.h>

pthread_t daemon_id;

/* init_daemon():
	Creates daemon and select threads to handle transmission and timeout
	issues.  The two threads are joinable, although they never terminate.
 */
void
ardp_init_daemon(void)
{
    int retval;
    pthread_t select_id;

    /* First, it is time to create the 'select thread'.  The thread initializes
       ardp_port. */
    retval = pthread_create(&select_id, NULL, retrieval_thread, NULL);
    if (retval) {
	rfprintf(stderr, "%s\n", strerror(retval));
	internal_error("ardp_initialize(): Unable to create select thread");
    } /* if */
    sched_yield();		/* Give retrieval_thread() the chance to
				   execute.  XXX Can delete this. */
    /* Create daemon thread in order to handle transmission -- Nader Salehi
       4/98 */
    retval = pthread_create(&daemon_id, NULL, daemon_thread, NULL);
    if (retval) {
	rfprintf(stderr, "%s\n", strerror(retval));
	internal_error("ardp_initialize(): Unable to create daemon thread");
    } /* if */

    sched_yield();		/* In order allow the two threads to execute */
}
#endif /* GL_THREADS */
