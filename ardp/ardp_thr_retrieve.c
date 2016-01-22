/*
 * Copyright (c) 1998 by the University of Southern California
 *
 * For copying and distribution information, please see the file
 * <usc-license.h>.
 *
 * Written  by Nader Salehi 4/98 to check and/or wait for completion of request
 * Microsecond granularity changes.  Portion of the code was stolen from
 * ardp_retrieve().
 *
 * This is the replacement for ardp_retrieve() in multi-threaded environments.
 */

#ifdef GL_THREADS
#include <usc-license.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#ifdef AIX
#include <sys/select.h>
#endif

#include <ardp.h>
#include "ardp__int.h"		/* UFACTOR */
#include <ardp_time.h>
#include <time.h>
#include <perrno.h>

#ifndef min
#define min(x,y) ((x) < (y) ? (x) : (y))
#endif /*min */ 


/*
 * ardp_threaded_retrieve - check and/or wait for completion of request
 *
 *   ardp_threaded_retrieve takes a request and a time to wait.  It will check
 *   to see if the request is complete and if so, returns ARDP_SUCCESS.
 *   If not complete,  it will wait up till the time to wait before returning.
 *   If still incomplete it will return ARDP_PENDING. A time to wait of -1
 *   indicates that one should not return until complete.
 *   Completion can mean either that we've successfully received a 
 *   complete response or that the request has timed out or that the request
 *   encountered some other fatal error.
 *
 *   On any failure (other than still pending), an error code is returned. 
 *
 *  
 *   If the REQ argument is NULL, then we test whether any request is
 *   complete.  However, we do not remove it from the ardp_completeQ.
 *   This allows us to poll to see whether any replies are ready to be looked 
 *   at. 
 *
 *   CONSISTENCY: the REQ argument, if non-NULL, should always be a member
 *   of the ardp_activeQ or ardp_completeQ.  (One could
 *   not otherwise wait for a request's completion, since ardp_pr_actv() would
 *   not be able to find the request to mark completion.)
 */
enum ardp_errcode
ardp_thr_retrieve(req,ttwait_arg)
    RREQ		req;		/* Request to wait for */
    int			ttwait_arg;	/* Time to wait in microseconds  */
{
    int retval;
    pthread_t thread_id;	/* The thread ID of the caller */
    RREQ req_index;
    struct timespec ttwait;	/* Absolute time (to nanosecond precision) */

    /* Just see if there is a complete request */
    if (!req) {
	if (!ardp_activeQ && !ardp_completeQ) {
	    p_clear_errors();
	    return ARDP_BAD_REQ;
	} /* if */
	else if (ardp_completeQ) {
	    /* Look for a complete response for the thread which has called
	       this function. */
	    EXTERN_MUTEXED_LOCK(ardp_completeQ);
	    req_index = ardp_completeQ;
	    thread_id = pthread_self();
	    while ((req_index) && (req_index->thread_id))
		req_index = req_index->next;
	    EXTERN_MUTEXED_UNLOCK(ardp_completeQ);
	    if (req_index)
		return ARDP_SUCCESS;
	    else		/* We assume that the message is in the active
				   queue, though it might not be a true
				   assumption.  --Nader Salehi 4/98 */
		return ARDP_PENDING;
	} /* else */
    } /* if */

    retval = pthread_mutex_lock(req->mutex);
    if (ttwait_arg) {		/* Non-blocking mode */
	if (ttwait_arg == -1) {
	    /* NOTE: In order to simplify coding, we force the system to wait
	       for one year.  This is longer than the system's practical
	       "infinity". */
	    ttwait.tv_sec = time(NULL) + ONE_YEAR;
	    ttwait.tv_nsec = 0;
	} /* if */
	else {
	    ttwait.tv_sec = time(NULL) + ttwait_arg / UFACTOR;
	    ttwait.tv_nsec = (ttwait_arg % UFACTOR) * 1000; /* Nanoseconds */
	} /* else */
	if (retval) {
	    rfprintf(stderr, "%s\n", strerror(retval));
	    internal_error("ardp_retrieve(): Unable to lock the mutex");
	} /* if */
	retval = 0;
	while ((req->status != ARDP_STATUS_COMPLETE) &&
	       (req->status <= 0) && (!retval)) {
	    retval = pthread_cond_timedwait(req->cond, req->mutex, &ttwait);
	    if (retval == ETIMEDOUT) { /* Timeout */
		pthread_mutex_unlock(req->mutex);
		return ARDP_PENDING;
	    } /* if */
	    else if (retval) {
		rfprintf(stderr, "%s\n", strerror(retval));
		internal_error("ardp_threaded_retrieve(): Failed");
	    } /* else */
	} /* while */
    } /* if */

    /* (req->status == ARPD_STATUS_COMPLETE) || (req->status > 0) */
    if (ardp_debug) 
	rfprintf(stderr, "ardp_thr_retrieve():Locking ardp_completeQ\n");
    EXTERN_MUTEXED_LOCK(ardp_completeQ);
    /* NOTE: I am not clear on whether I should decrement
       ardp_completeQ_len or this is not necessary.  --Nader Salehi 4/98 */
    EXTRACT_ITEM(req,ardp_completeQ);
    EXTERN_MUTEXED_UNLOCK(ardp_completeQ);
    if (req->status == ARDP_STATUS_COMPLETE)
	return ARDP_SUCCESS;
    else
	return perrno = req->status; /* specific error */
} /* ardp_threaded_retrieve */

#endif /* GL_THREADS */
