/*
 * Copyright (c) 1992, 1993 by the University of Southern California
 *
 * For copying and distribution information, please see the file
 * <usc-license.h>
 *
 * Written  by bcn 1991     as get_next_request in rdgram.c (Prospero)
 * Modified by bcn 1/93     modularized and incorporated into new ardp library
 * Modified by Katia 1/96   added get_nxt_all_timeout
 */

#include <usc-license.h>

#include <stdio.h>
#include <memory.h>		/* MEMSET() prototype */
#include <sys/time.h>
#include <string.h>
#ifdef AIX
#include <sys/select.h>
#endif
#include <ardp.h>
#include "ardp__int.h"		/* for UFACTOR */
#include <ardp_time.h>

extern int	pQNlen;
extern int	pQlen;
extern int	ardp_srvport;
extern int	ardp_prvport;

extern int      ardp_port;    /* Opened client UDP port */

#ifndef max
#define max(x,y) (x > y ? x : y)
#endif

/*
 * ardp_get_nxt - return next request for server
 *
 *   ardp_get_nxt returns the next request to be processed by the server.
 *   If no requests are pending, ardp_get_nxt waits until a request
 *   arrives, then returns it.
 */
/* XXX DOES NOT DO ANY RETRANSMIT QUEUE TIMEOUTS --swa, katia, 2/97 */
RREQ 
ardp_get_nxt(void)
{
    RREQ	nextreq;
    fd_set	readfds;
    int		tmp;

 tryagain:
    if ((nextreq = ardp_get_nxt_nonblocking()))
        return nextreq;
    /* if queue is empty, then wait till something comes */
    /* in, then go back to start                          */
    FD_ZERO(&readfds);
    if(ardp_srvport != -1) FD_SET(ardp_srvport, &readfds);
    if(ardp_prvport != -1) FD_SET(ardp_prvport, &readfds); 
    tmp = select(max(ardp_srvport,ardp_prvport) + 1, &readfds, 
		 (fd_set *)0, (fd_set *)0, NULL);
    goto tryagain;
}


/* XXX DOES NOT DO ANY RETRANSMIT QUEUE TIMEOUTS --swa, katia, 2/97 */
/*
 * Nonblocking version of the above.
 * Returns  NULL if no pending items.
 */
RREQ
ardp_get_nxt_nonblocking(void)
{
    ardp_accept();

    /* return next message in queue */
    if (ardp_pendingQ) {
        /* The above is an atomic test; it saves ourselves the trouble of going
	   through the multithreaded kernel if there is nothing in the
	   queue. */ 
        EXTERN_MUTEXED_LOCK(ardp_pendingQ);
        if(ardp_pendingQ) {
            RREQ nextreq = ardp_pendingQ;
            
            EXTRACT_ITEM(nextreq, ardp_pendingQ);
            pQlen--;if(nextreq->priority > 0) pQNlen--;
            EXTERN_MUTEXED_UNLOCK(ardp_pendingQ);
	    
            nextreq->svc_start_time = ardp__gettimeofday();
            EXTERN_MUTEXED_LOCK(ardp_runQ);
            APPEND_ITEM(nextreq,ardp_runQ);
            EXTERN_MUTEXED_UNLOCK(ardp_runQ);
	    /* Now we process the security context */

            return(nextreq);
        }
        EXTERN_MUTEXED_UNLOCK(ardp_pendingQ);
    }
    return NOREQ;
}


/*
 *   ardp_get_nxt_all - return next request for server or the next     
 *   reply to the request made by the server.  If no requests or 
 *   replies are pending, ardp_get_nxt_all waits until a request 
 *   or reply arrives, then returns it.
 *
 *   If you call ardp_get_nxt_all_timeout() without having either
 *   bound a server port (via ardp_bind_port()) or having bound a client
 *   port (by sending a message with ardp_send(), which calls ardp_init()),
 *   then there will be no file descriptors for ardp_get_nxt_all_timeout() to
 *   look for messages on.  In this case, its behavior will depend on how
 *   select() is implemented on your system.   Since it would be meaningless
 *   to call ardp_get_nxt_all_timeout() in such a context, we haven't
 *   written special-case code to test for this condition and return
 *   some appropriate error.
 *
 *   Sets req_type to 1 if it's a new incoming request, if it's a
 *   response to an outstanding request, sets it to 0.        
 */

RREQ 
ardp_get_nxt_all(enum ardp_gna_rettype * req_type)
{
    RREQ	nextreq;
    fd_set	readfds;
    int		tmp;
    int		port;
    static int order = 0;	/* we use 'order' to ensure that neither
				   requests nor responses will be starved; of
				   course, if we're this heavily loaded, some
				   of each will end up starving. */

    while(1){

	if (order){
	    order = 0;	
        		    /* ardp replies */
            if ((nextreq = ardp_retrieve_nxt_nonblocking())) {
		 *req_type = ARDP_CLIENT_PORT;
	   	 return nextreq;
	    }	
            		    /* ardp requests */    
            if ((nextreq = ardp_get_nxt_nonblocking())) {
		 *req_type = ARDP_SERVER_PORT;
	   	 return nextreq;
	    }
	}else{
	    order = 1;	
            		    /* ardp requests */    
            if ((nextreq = ardp_get_nxt_nonblocking())) {
		*req_type = ARDP_SERVER_PORT;
	        return nextreq;
	    }
                            /* ardp replies */
            if ((nextreq = ardp_retrieve_nxt_nonblocking())) {
		*req_type = ARDP_CLIENT_PORT;
	   	return nextreq;
	    }
        } 

        /* if queue is empty, then wait till somethings comes */
        /* in, then go back to start                          */
        FD_ZERO(&readfds);
         /* server ports */
        if(ardp_srvport != -1) FD_SET(ardp_srvport, &readfds);
        if(ardp_prvport != -1) FD_SET(ardp_prvport, &readfds); 

        /* client ports */ 
        if(ardp_port != -1) FD_SET(ardp_port, &readfds);

	port = max(ardp_port, max(ardp_srvport ,ardp_prvport));  
	
        tmp = select(port + 1, &readfds, 
		 (fd_set *)0, (fd_set *)0, NULL);
    }
}


/*
 *    ardp_get_nxt_all_timeout - returns a request or a reply.  If no
 *    requests or replies are pending, ardp_get_nxt_all_timeout waits
 *    until a timeout or until a request or reply arrives, then
 *    returns it. The order in which it checks for replies or requests
 *    is defined by priority, which can be either ARDP_CLIENT_PORT or 
 *    ARDP_SERVER_PORT depending on whether to check for replies or 
 *    requests first, respectively.
 *
 *    If you call ardp_get_nxt_all_timeout() without having either
 *    bound a server port (via ardp_bind_port()) or having bound a client
 *    port (by sending a message with ardp_send(), which calls ardp_init()),
 *    then there will be no file descriptors for ardp_get_nxt_all_timeout() to
 *    look for messages on.  In this case, its behavior will depend on how
 *    select() is implemented on your system.   Since it would be meaningless
 *    to call ardp_get_nxt_all_timeout() in such a context, we haven't
 *    written special-case code to test for this condition and return
 *    some appropriate error.
 *
 *    Sets ret_type to 2 if it's a new incoming request. If it's a
 *    reply to an outstanding request, sets it to 1. If it's a
 *    select error, sets it to 0 and returns NOREQ. If a timeout occurs, 
 *    it returns NOREQ and sets ret_type to 3. */

/* XXX somebody cleaning up this code could make ardp_get_nxt_all() call
   ardp_get_nxt_all_timeout().   We haven't done so because both are working
   right now.  --steve & katia, 3/22/96
*/
RREQ
ardp_get_nxt_all_timeout(enum ardp_gna_rettype *ret_type, int ttwait, int priority)
{
    RREQ next_req;
    fd_set readfds;
    struct timeval time_before_select; /* set before calling select() */
    struct timeval time_after_select; /* set after calling select() */
    struct timeval time_we_did_wait; /* how long have we waited so far?
					(difference between above two) */
    /* This is the ttwait argument, transformed into the 'timeval' format that
       select() requires */
    struct timeval ttwait_requested;
    /* This variable is set and immediately used.  It is the shortest timeout
       we need in order to check for anything on the activeQ (transmission
       queue).  It will be infinite if we have no undelivered messages on the
       ardp_activeQ. */
    struct timeval next_activeQ_timeout;
    /* This is the minimum of the above two values.  It is passed directly to
       select() right after it is set */
    struct timeval timeout;

    int order;
    int tmp;
    
    /* Set order based on priority */
    
    if (priority == ARDP_CLIENT_PORT) 
	order = 1;
    else if ((priority == ARDP_SERVER_PORT))
	order = 0;
    else{
	*ret_type = 0;
	return NOREQ;
    }
   
    /* Do we already have any requests or replies available? */
    if (order) { /* check first for replies and then requests*/
	if ((next_req = ardp_retrieve_nxt_nonblocking())) {
	    *ret_type = ARDP_CLIENT_PORT; 
	    return next_req;
	}
	if ((next_req = ardp_get_nxt_nonblocking())) {
	    *ret_type = ARDP_SERVER_PORT;
	    return next_req;
	}
    } else {                          /* check for requests */
	if ((next_req = ardp_get_nxt_nonblocking())) {
	    *ret_type = ARDP_SERVER_PORT; 
	    return next_req;
	}
	if ((next_req = ardp_retrieve_nxt_nonblocking())) {
	    *ret_type = ARDP_CLIENT_PORT;
	    return next_req;
	}
    }
    /* If there are no requests or replies, then wait till 
       something comes in, or timeout. */
    
    /* Set list of file descriptors to be checked by select. */
    
    FD_ZERO(&readfds);
    /* client port; bound by ardp_init() */
    if(ardp_port != -1) FD_SET(ardp_port, &readfds);
    
    if(ardp_srvport != -1) FD_SET(ardp_srvport, &readfds);
    if(ardp_prvport != -1) FD_SET(ardp_prvport, &readfds);
    
    /* Set ttwait_requested based upon 'ttwait' arg. */
    if (ttwait >= 0) {
	/* ttwait is in microseconds */
	ttwait_requested.tv_sec = ttwait/UFACTOR;
	ttwait_requested.tv_usec = ttwait%UFACTOR;
    } else {
	/* -1 (ARDP_WAIT_TILL_TO) implies blocking behavior (infinite
	   timeout).   We just test for any negative number. */ 
	ttwait_requested = infinitetime;
	assert(ttwait < 0);
    }

restart_select:		
    time_before_select = ardp__gettimeofday();
    next_activeQ_timeout = ardp__next_activeQ_timeout(time_before_select);
    timeout = min_timeval(next_activeQ_timeout, ttwait_requested);

    /* Select port that is ready to read. */
    tmp = select(max(max(ardp_srvport, ardp_prvport), ardp_port)+1, 
		 &readfds, (fd_set *)0, (fd_set *) 0, 
		 eq_timeval(timeout, infinitetime) ? NULL : &timeout); 
    if (tmp < 0){
      *ret_type = ARDP_GNA_SELECT_FAILED;
      return NOREQ;
    }
    time_after_select = ardp__gettimeofday();
    /* Either (a) there is a message waiting or (b) select timed out.
       Case (b) can be subdivided into 
       (b1): ttwait has been exceeded
       (b2) we have a retransmission/failure timeout in the ardp_activeQ to
       process.  We could return in case (b1), but we are checking for 
       messages anyway. */
    /* We want to call ardp_process_active(), but not wastefully --
       ardp_retrieve_nxt_nonblocking() calls it for us;
       ardp_get_nxt_nonblocking() doesn't call it for us. */
    
    if (order) { /* check first for replies and then requests*/
	/* ardp_retrieve_nxt_nonblocking() calls ardp_process_active()  */
	if ((next_req = ardp_retrieve_nxt_nonblocking())) {
	    *ret_type = ARDP_CLIENT_PORT; 
	    return next_req;
	}
	if ((next_req = ardp_get_nxt_nonblocking())) {
	    *ret_type = ARDP_SERVER_PORT;
	    return next_req;
	}
    } else {                          /* check for requests */
	if ((next_req = ardp_get_nxt_nonblocking())) {
	    ardp_process_active(); /* so it gets called before we return from
				      this function; retransmissions are done.
				      */ 
	    *ret_type = ARDP_SERVER_PORT; 
	    return next_req;
	}
	if ((next_req = ardp_retrieve_nxt_nonblocking())) {
	    *ret_type = ARDP_CLIENT_PORT;
	    return next_req;
	}
    }
    /* When we reach here, we've just called ardp_process_active() in the above
       checking for replies and requests. */

    time_we_did_wait = subtract_timeval(time_after_select, time_before_select);
    /* Cut down ttwait_requested by the amount we have waited so far. */
    /* subtract_timeval() will never return negative values; zero only. */
    /* infinity minus anything is infinity. */
    ttwait_requested = subtract_timeval(ttwait_requested, time_we_did_wait);
    if (eq_timeval(ttwait_requested,zerotime)) /* done waiting! */ {
	*ret_type = ARDP_GNA_TIMEOUT;
	return NOREQ;
    } else {
	goto restart_select;
    }
}
