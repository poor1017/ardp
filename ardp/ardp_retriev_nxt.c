/*
 * Copyright (c) 1993 by the University of Southern California
 *
 * For copying and distribution information, please see the file
 * <usc-license.h>.
 *
 * Written  by bcn 1/93     to check and/or wait for completion of request
 */

#include <usc-license.h>
#include <errno.h>
#include <stdio.h>

#ifdef AIX
#include <sys/select.h>
#endif

#include <ardp.h>
#include "ardp__int.h"
#include <ardp_time.h>
#include <perrno.h>
#include <pmachine.h>		/* why?  I forget... */


#ifndef min
#define min(x,y) ((x) < (y) ? (x) : (y))
#endif /*min */ 

/*#define GDEBUG */

/*
 * ardp_retrieve_next - check and/or wait for completion of any request
 *
 *   ardp_retrieve_nxt takes a request and a time to wait.  It will check to
 *   see if the request is complete and if so, returns ARDP_SUCCESS.
 *   If not complete,  ardp_retrieve will wait up till the time to wait
 *   before returning.  If still incomplete it will return ARDP_PENDING.
 *   A time to wait of -1 indicates that one should not return until
 *   complete, or until a timeout occurs. On any failure (other than
 *   still pending), an error code is returned.
 *
 */

RREQ				/* request we found */
ardp_retrieve_nxt(ttwait_arg)
    int			ttwait_arg;   /* Time to wait in microseconds  */
{
    fd_set		readfds;  /* Used for select               */
    struct timeval	selwait_st; /* Time to wait for select       */
    int			tmp;	  /* Hold value returned by select */

    struct timeval	cur_time; /* in microseconds */  
    struct timeval	start_time; /* in microseconds */  
    struct timeval	time_elapsed = zerotime; /* time elapsed so far */           	
    struct timeval ttwait;


    p_clear_errors();

    if (ttwait_arg < 0) {
	ttwait = infinitetime;
    } else {
	ttwait.tv_sec = ttwait_arg / UFACTOR;
	ttwait.tv_usec = ttwait_arg % UFACTOR;
    }
    if (ttwait_arg > 0){	
	start_time = ardp__gettimeofday();
#ifdef GDEBUG
	rfprintf(stderr, "Start time %d sec %d usec\n", 
			start_time.tv_sec, start_time.tv_usec);   
#endif /*GDEBUG */
    }	

    if(!ardp_activeQ && !ardp_completeQ) {
	perrno = ARDP_BAD_REQ;
	return NULL;
    }

 check_for_more:

    ardp_process_active();
    if (ardp_completeQ) {
	RREQ req;		/* request to return */
	req = ardp_completeQ;
	EXTRACT_ITEM(req,ardp_completeQ);
	/* XXX fix later -- should lock this so that only one is writing to 
	   stderr at a time */
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
	if(req->status == ARDP_STATUS_COMPLETE) {
	    perrno = ARDP_SUCCESS; /* here we use perrno to indicate
				      ARDP_SUCCESS or failure. */
	} else {
	    perrno = req->status;   /* Specific error */
	}
	return req;
    }

 restart_select:
    /* recompute (or initially compute) the select wait time; we jump here if 
       the select() was interrupted before it naturally timed out. */
    if (ttwait_arg == 0) {
	perrno = ARDP_PENDING;
	return NULL;
    }
    cur_time = ardp__gettimeofday();

#ifdef GDEBUG
    rfprintf(stderr, "Current time %d.%06ld sec\n"
	    cur_time.tv_sec, (long) cur_time.tv_usec);   
#endif /*GDEBUG */

    time_elapsed = subtract_timeval(cur_time, start_time);

#ifdef GDEBUG
    rfprintf(stderr, "Time elapsed %d usec\n", time_elapsed);
#endif /*GDEBUG */

    /* If ttwait == infinitetime, then this test will always fail. */
    if (time_is_later(time_elapsed, ttwait)) {
	perrno = ARDP_PENDING;
	return NULL;
    }

    /* Here we should figure out how long to wait, a minimum of */
    /* ttwait, or the first retry timer for any pending request */

    /* XXX (However, we in fact do not check all pending requests; we only 
       check the current request. -- this bug will be fixed really soon -- Sung
       has requested it) */

    if (ardp_debug >= 6)
	rfprintf(stderr,"Waiting for reply...");

    FD_ZERO(&readfds);
    FD_SET(ardp_port, &readfds);

    /* We must copy the select wait, since some UNIX-like systems (such as
       LINUX) modify the last argument to select(). */

    assert(ardp_activeQ);	/* if not, why are we running select()? */
    
    selwait_st = min_timeval(ardp__next_activeQ_timeout(cur_time),
			     subtract_timeval(ttwait, time_elapsed));
#ifdef GDEBUG
    rfprintf(stderr, "Time select will wait %d.%06ld sec\n", 
	    selwait_st.tv_sec, (long) selwait_st.tv_usec);
#endif /*GDEBUG */

    /* select - either recv is ready, or timeout */
    /* see if timeout or error or wrong descriptor */
    tmp = select(ardp_port + 1, &readfds, (fd_set *)0, (fd_set *)0, &selwait_st);

    /* Packet received, or timeout - both handled by ardp_process_active */
    if(tmp >= 0) goto check_for_more;

    /* This is a fix to a bug that Santo found. When an interrupt is detected,
       the select loop fails. In this case, we want to re-start it. */
    /* Note that some systems (notably, LINUX) update selwait_st when interrupted, others (notably, SunOS 4.1.3) don't. 
       Therefore, we make sure select is re-started with the time
       remaining. */  

    if ((tmp == -1) && (errno == EINTR)) 
      goto restart_select;
    
    if (ardp_debug) 
	rfprintf(stderr, "select failed: returned %d\n", tmp);
    perrno = ARDP_SELECT_FAILED;
    return NULL;
}
