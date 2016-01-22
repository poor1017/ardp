/*
 * Copyright (c) 1993, 1997 by the University of Southern California
 *
 * For copying and distribution information, please see the file
 * <usc-license.h>.
 *
 * Written  by bcn 1/93     to check and/or wait for completion of request
 * Microsecond granularity changes: swa, katia, , 2/97
 */

#include <usc-license.h>
#include <errno.h>
#include <stdio.h>

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

/*#define GDEBUG */
/*#define ARDP_RETRIEV_AVOID_CALLING_ARDP_GETTIMEOFDAY*/

/*
 * ardp_retrieve - check and/or wait for completion of request
 *
 *   ardp_retrieve takes a request and a time to wait.  It will check to
 *   see if the request is complete and if so, returns ARDP_SUCCESS.
 *   If not complete,  ardp_retrieve will wait up till the time to wait
 *   before returning.  If still incomplete it will return ARDP_PENDING.
 *   A time to wait of -1 indicates that one should not return until
 *   complete.
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
ardp_retrieve(req,ttwait_arg)
    RREQ		req;	  /* Request to wait for           */
    int			ttwait_arg;   /* Time to wait in microseconds  */
{
#ifndef GL_THREADS
    fd_set		readfds;  /* Used for select               */
    struct timeval	selwait_st; /* Time to wait for select       */
    int			tmp;	  /* Hold value returned by select */

    /* absolute time (to microsecond precision) */   
    struct timeval	cur_time = ardp_bogustime;
    /* absolute time (to microsecond precision) */   
    struct timeval	start_time = ardp_bogustime;
    /* elapsed time so far, in microseconds (this is a difference, not an
       absolute time).  */ 
    struct timeval	time_elapsed = ardp_bogustime;
    struct timeval	ttwait = ardp_bogustime; /* time to wait requested by
						    caller.  */
#endif /* ndef GL_THREADS */

#ifdef GL_THREADS
    return ardp_thr_retrieve(req, ttwait_arg);
#else /* GL_THREADS */
    
    if (ttwait_arg < 0)
	ttwait = infinitetime;
    else {
	ttwait.tv_sec = ttwait_arg / UFACTOR;
	ttwait.tv_usec = ttwait_arg % UFACTOR;
    }

#if 0				/* shouldn't need this --swa, 9/96 */
    p_clear_errors();
#endif


#ifdef ARDP_RETRIEV_AVOID_CALLING_ARDP_GETTIMEOFDAY
    /* We won't need the start time if it's zero or infinite. */
    if
#if 1
	(!eq_timeval(ttwait, infinitetime) && !eq_timeval(ttwait, zerotime))
#else
	/* SOFTWARE ENGINEERING NOTE:
	 The test (ttwait_arg > 0) is a considerably shorter version of the
	 above.  But it's less clear to understand.  A refinement process here
	 would be useful (more of my software engineering talk --swa 2/97) */ 
	(ttwait_arg > 0)
#endif
#endif
	{
	    start_time = ardp__gettimeofday();


#ifdef GDEBUG
	rfprintf(stderr, "Start time %d sec %d usec\n", 
		start_time.tv_sec, start_time.tv_usec);   
#endif /*GDEBUG */
    }

    if (!req && !ardp_activeQ && !ardp_completeQ) {
	p_clear_errors();
	return ARDP_BAD_REQ;
    }

    if(req && req->status == ARDP_STATUS_FREE) {
	if (ardp_debug)
	    rfprintf(stderr,"Attempt to retrieve free RREQ\n");
	/* this might be a good candidate for gl_function_arguments_error()
	   --swa, 9/96 */ 
	p_clear_errors();
	return(perrno = ARDP_BAD_REQ);
    }

    if(req && req->status == ARDP_STATUS_NOSTART) {
	p_clear_errors();
	return perrno = ARDP_BAD_REQ;
    }

 check_for_more:

    ardp_process_active();

    if (!req && ardp_completeQ) 
      return PSUCCESS;
    if (!req)
      goto restart_select;

    if((req->status == ARDP_STATUS_COMPLETE) || (req->status > 0)) {
	EXTERN_MUTEXED_LOCK(ardp_completeQ);
	EXTRACT_ITEM(req,ardp_completeQ);
	EXTERN_MUTEXED_UNLOCK(ardp_completeQ);

	if(ardp_debug >= 9) {
            PTEXT		ptmp;	  /* debug-step through req->rcvd  */
	    if(req->status > 0) rfprintf(stderr,"Request failed (error %d)!",
					req->status);
	    else rfprintf(stderr,"Packets received...");
	    ptmp = req->rcvd;
	    while(ptmp) {
		rfprintf(stderr,"Packet %d%s:\n",ptmp->seq,
			(ptmp->seq <= 0 ? " (not rcvd, but constructed)" : ""));
                ardp_showbuf(ptmp->start, ptmp->length, stderr);
                putc('\n', stderr);
		ptmp = ptmp->next;
	    }
	    (void) fflush(stderr);
	}
	if(req->status == ARDP_STATUS_COMPLETE) return(ARDP_SUCCESS);
	else {
	    return perrno = req->status;   /* Specific error */
	}
    }

restart_select:
#ifndef ARDP_RETRIEV_AVOID_CALLING_ARDP_GETTIMEOFDAY
    /* Easy approach: always get the current time */
    cur_time = ardp__gettimeofday();
#endif

    /* recompute (or initially compute) the select wait time; we jump here if 
       the select() was interrupted before it naturally timed out. */
    /* We recompute the select wait each time.  Some UNIX-like systems (such as
       LINUX) modify the last argument to select().  Others, regrettably, do
       not.  In any case, the selwait time changes after packets are
       retransmitted by ardp_process_active().  */

    if (eq_timeval(ttwait, zerotime)) 
	return(ARDP_PENDING);
    else if (eq_timeval(ttwait,infinitetime)) {
#ifdef ARDP_RETRIEV_AVOID_CALLING_ARDP_GETTIMEOFDAY
	cur_time = ardp__gettimeofday();
#endif
        selwait_st = ardp__next_activeQ_timeout(cur_time);
    } else {
	assert(!eq_timeval(ttwait, infinitetime) && 
	       !eq_timeval(ttwait, zerotime));

#ifdef ARDP_RETRIEV_AVOID_CALLING_ARDP_GETTIMEOFDAY /* SOFTWARE ENGINEERING DECISION: */
	/* If cur_time is unset, we can save a call to ardp__gettimeofday() by 
	   using the value cached in start_time.  We can do this because we
	   know we've never been through the loop.  However, this may be bogus;
	   it depends upon whether ardp_process_active() runs in a significant
	   amount of time.  Also depends on how expensive it is for us to check
	   the time of day on our particular operating system. */
	if (eq_timeval(cur_time, ardp_bogustime)) {
	    assert(!eq_timeval(start_time, ardp_bogustime));
	    cur_time = start_time;
	} else {
	    cur_time = ardp__gettimeofday();
	}
#endif

#ifdef GDEBUG
	rfprintf(stderr, "Current time %d sec %d usec\n", 
			cur_time.tv_sec, cur_time.tv_usec);   
#endif /*GDEBUG */

	time_elapsed = subtract_timeval(cur_time, start_time);

#ifdef GDEBUG
	rfprintf(stderr, "Time elapsed %d.%06d sec\n", 
		time_elapsed.tv_sec, time_elapsed.tv_usec);
#endif /*GDEBUG */

	if (time_is_later(time_elapsed, ttwait))
	    return ARDP_PENDING;

	/* Here we figure out how long to wait, a minimum of */
	/* ttwait, or the first retry timer for any pending request */

	selwait_st = min_timeval(ardp__next_activeQ_timeout(cur_time),
				 subtract_timeval(ttwait, time_elapsed));
    }	
    

    if (ardp_debug >= 6) 
	rfprintf(stderr,"Waiting %ld.%06ld seconds for reply...",
		selwait_st.tv_sec, selwait_st.tv_usec); 

    FD_ZERO(&readfds);
    FD_SET(ardp_port, &readfds);

    /* select - either recv is ready, or timeout */
    /* see if timeout or error or wrong descriptor */
    /* XXX in a multi-threaded mode, we may find out that something has already
       arrived and been processed since we checked last... XXX --swa, 9/96 and
       earlier */
    tmp = select(ardp_port + 1, &readfds, (fd_set *)0, (fd_set *)0, &selwait_st);

    /* Packet received, or timeout - both handled by ardp_process_active */
    if(tmp >= 0) 
	goto check_for_more;

    /* SELECT FAILED */

    /* This is a fix to a bug that Santo found. When an interrupt is detected,
       the select loop fails. In this case, we want to re-start it. */
    /* Note that some systems (notably, LINUX) update selwait_st when 
       interrupted, others (notably, SunOS 4.1.3) don't.  Therefore, we make
       sure select is re-started with the time remaining. */  

    if ((tmp == -1) && (errno == EINTR)) 
	goto restart_select;
    
    p_clear_errors();
    p_err_string = 
	qsprintf_stcopyr(p_err_string,
			 "select() failed: %s (UNIX error #%d)\n",
			 unixerrstr(), errno);
    if (ardp_debug) {
	fputs(p_err_string, stderr);
    }
    return perrno = ARDP_SELECT_FAILED;
#endif /* GL_THREADS */
}

/*
 * ardp_retrieve_nxt_nonblocking - check for completion of any request
 *
 *   ardp_retrieve_nxt_nonblocking will check to see if any request
 *   is complete and if so, return the request.  If not complete, 
 *   ardp_retrieve_nxt_nonblocking will return NOREQ (NULL).
 *
 */

RREQ
ardp_retrieve_nxt_nonblocking(void)
{
    RREQ	req; 

    p_clear_errors();

    ardp_process_active();

    if ((req = ardp_completeQ)) { 

	EXTRACT_ITEM(req,ardp_completeQ);
	if(ardp_debug >= 9) {
            PTEXT		ptmp;	  /* debug-step through req->rcvd  */
	    if(req->status > 0) rfprintf(stderr,"Request failed (error %d)!",
					req->status);
	    else rfprintf(stderr,"Packets received...");
	    ptmp = req->rcvd;
	    while(ptmp) {
		rfprintf(stderr,"Packet %d:\n",ptmp->seq);
                ardp_showbuf(ptmp->start, ptmp->length, stderr);
                putc('\n', stderr);
		ptmp = ptmp->next;
	    }
	    (void) fflush(stderr);
	}

	return req;
    }else{
	return NOREQ;
    }
}


