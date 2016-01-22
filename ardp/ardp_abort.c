/*
 * Copyright (c) 1993, 1997 by the University of Southern California
 *
 * For copying and distribution information, please see the file
 * <usc-license.h>.
 *
 * Written  by bcn 1/93     to abort pending requests
 * Updated: swa 5/97: portability
 */
#define GL_UNUSED_C_ARGUMENT __attribute__((unused))


#include <usc-license.h>

#include <stdio.h>
#include <memory.h>		/* MEMSET() prototype on Solaris.*/
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>		/* inet_nota() */
#include <unistd.h>
#include <sched.h>
#include <signal.h>

#include <gl_threads.h>
#include <pmachine.h>		/* for signal handling (SIGNAL_RET_TYPE) */

#include <list_macros.h>
#include <ardp.h>

/*
 * ardp_abort - abort a pending request
 *
 *   ardp_abort takes a pointer to a request structure of a request to 
 *   be aborted, sends an abort request to the server handling the pending,
 *   request, and immediately returns.  If the request is null (NOREQ), then
 *   all requests on the ardp_activeQ are aborted.
 *
 *   All of the aborted requests are immediately freed; it is assumed the user
 *   has no further interest in them.  (It is not clear whether this or
 *   not-freeing is the obvious behavior.  Should this be a flag?   See the
 *   rationale/implementation discussion below. --swa, 9/96) 
 */
enum ardp_errcode
ardp_abort(req)
    RREQ		req;		/* Request to be aborted         */
{
    RREQ		rtmp = NOREQ;	/* Current request to abort      */
#ifndef GL_THREADS
    RREQ		nextreq = NOREQ; /* Next time through loop, abort this
					    one. */ 
    PTEXT		ptmp = NOPKT;    /* Abort packet to be sent       */
    int			ns;		/* Number of bytes actually sent */
    char		*datastart;
    enum		ardp_errcode retval = ARDP_SUCCESS; /* return value, in
							       some cases.  */
#endif /* GL_THREADS */

    if(req && (req->status != ARDP_STATUS_ACTIVE)) 
        return(ARDP_BAD_REQ);

#ifdef GL_THREADS
    EXTERN_MUTEXED_LOCK(ardp_activeQ);
    daemon.outputReady = 1;
    if (req) {
	req->status = ARDP_STATUS_ABORTED;
    } /* if */
    else {
	for (rtmp = ardp_activeQ; rtmp; rtmp = rtmp->next) {
	    rtmp->status = ARDP_STATUS_ABORTED;
	} /* for */
    } /* else */
    pthread_cond_signal(daemon.cond);
    EXTERN_MUTEXED_UNLOCK(ardp_activeQ);
    return(ARDP_SUCCESS);
#else
    if(req) {
	rtmp = req;
    } else {
	EXTERN_MUTEXED_LOCK(ardp_activeQ);
	rtmp = ardp_activeQ;
    }    
    ptmp = ardp_ptalloc();

    datastart = ptmp->start;

    /* Note: we only go through this loop once if a req was specified as the
       argument.  'rtmp' is re-assigned-to later on in this loop. */
    for( ; rtmp; rtmp = nextreq) {
	/* Add header */
	/* We used to add the header (except for the cid) before this loop.  We
          add the header inside the loop now so that we can support aborting
          the entire ardp_activeQ when there are both v0 and v1 requests on
          it. */ 
       if (rtmp->peer_ardp_version == 1) { /* v1 */
           /* Notice how pretty and easy to read the v1 code is.  We like
              it. */ 
           ptmp->start = datastart - 9;
           ptmp->length = 9;
           ptmp->start[0] = (unsigned char) 129; /* version # */
           memset(ptmp->start + 1, '\000', 8); /* zero out rest of header */
           ptmp->start[3] = 0x01; /* Cancel Request flag */
           ptmp->start[4] = (unsigned char) 9;
           /* CID set later (5 & 6) */
           /* Sequence # explicitly zero (unseq. control pkt.) */
       } else {                /* v0 */
            /* Add header */
           ptmp->start = datastart - 13;
           ptmp->length = 13;
           *(ptmp->start) = (char) 13; /* length of packet */
	   /* An unsequenced control packet */
	   memset(ptmp->start+3, '\000', 10);
	   /* Cancel flag */
	   *(ptmp->start+12) = 0x01;
       }
      
       ardp__bwrite_cid(rtmp->cid, ptmp);
       if (ardp_debug >= 6) {
	   if(rtmp->peer.sin_family == AF_INET) 
	       rfprintf(stderr,"\nSending abort message (cid=%d) to %s(%d)...", 
		       ntohs(rtmp->cid),inet_ntoa(rtmp->peer_addr),PEER_PORT(rtmp));
	   else rfprintf(stderr,"\nSending abort message...");
	   (void) fflush(stderr);
       }
       /* rtmp->peer is a (struct sockaddr_in), which doesn't match the Solaris
	  prototype for sendto(), which expects a (struct sockaddr *) */
	ns = sendto(ardp_port,(char *)(ptmp->start), ptmp->length, 0, 
		    (struct sockaddr *) &(rtmp->peer), S_AD_SZ);
	if(ns != ptmp->length) {
	    if (ardp_debug) {
		rfprintf(stderr,"\nsent only %d/%d: ",ns, ptmp->length);
		perror("");
	    }
	    retval = ARDP_NOT_SENT;
	    goto cleanup;
	}
	if (ardp_debug >= 6) 
	    rfprintf(stderr,"Sent.\n");

	rtmp->status = ARDP_STATUS_ABORTED; 

	/* where do we go next? req is set iff we are not looping through the
	   ardp_activeQ */ 
	if (req) /* If only one request needs aborting, only go through this
		    loop once */ 
	    nextreq = NOREQ;
	else
	    nextreq = rtmp->next;
	EXTRACT_ITEM(rtmp,ardp_activeQ);
	--ardp_activeQ_len;
	/* IMPLEMENTATION RATIONALE/DISCUSSION:
	   There are several ways this can be handled.  If we don't free
	   the items we extract, then there will be a memory leak if you didn't
	   have another handle to each RREQ being extracted.
	   If we free them, then the higher level must be aware of this so
	   we don't do double freeing.
	   We might also put these items on the completeQ and let the higher
	   level extract them.
	   Usually it doesn't matter; a call to ardp_abort() is usually
	   immediately followed by program termination.
	   */
	ardp_rqfree(rtmp);
    } /* end of while loop */

cleanup:
    ardp_ptfree(ptmp);
    if (! req)		/* If req is NULL, we've been while'ing through
			   the ardp_activeQ, and we now unlock it. */
	EXTERN_MUTEXED_UNLOCK(ardp_activeQ);
    return(retval);
#endif /* GL_THREADS */
}

#ifdef GL_THREADS
static void signal_handler(int nothing);
#else
/* Here, we get an unneeded Compiler warning from GCC -Wimplicit, so we quiet
   it down with this prototype. --swa, 7/97 */
static SIGNAL_RET_TYPE ardp_trap_int(int);
#endif /* GL_THREADS */


#ifdef GL_THREADS
/* signal_handler():
	This thread is in charge of shutting down the system when user presses
	^C.  The thread is assigned maximum priority at the time of creation.
	The thread first cancels daemon thread, gains control of the active
	queue, and sends an abort packet for each outstanding request. It will
	then exit.   

	NOTE: According to the manual page, Solaris 2.5 release does
	not support the Priority Scheduling option.  At the time of this
	writing siganl_handler() has the same priority as other threads.
	--Nader Salehi 5/98  
*/
void signal_handler(int nothing)
{
    int dummy,
	status;
    void *result;
    
    dummy = nothing;		/* This is only to avoid a compiler warning */

    /* Cancel daemon and wait till it dies! */
    if (ardp_debug)
	rfprintf(stderr, "signal_handler(%d): Canceling daemon(%d)...",
		 pthread_self(), daemon_id);
    status = pthread_cancel(daemon_id);
    if (status) {
	rfprintf(stderr, "signal_handler(): %s\n", strerror(status));
	internal_error("There is no way to cancel daemon.  Forced to use"
		       " emergency abortion");
    } /* if */
    status = pthread_join(daemon_id, &result);
    if (status) {
	rfprintf(stderr, "signal_handler(): %s\n", strerror(status));
	internal_error("Could not terminate daemon successfully.  Using"
		       " Emergency shutdown");
    } /* if */

    if ((int)result != PTHREAD_CANCELED) {
	internal_error("Could not terminate daemon successfully.  Using"
		       " Emergency shutdown");
    } /* if */
    exit(0);			/* This should never happen because daemon has
				   called exit by now */
} /* signal_handler */
#else
/*
 * ardp_trap_int - signal handler to abort request on ^C
 */
static
SIGNAL_RET_TYPE 
ardp_trap_int( int GL_UNUSED_C_ARGUMENT nothing)
{
    ardp_abort(NOREQ);
    exit(1);
}

#endif /* GL_THREADS */

/*
 * ardp_abort_on_int - set up signal handler to abort request on ^C
 */
enum ardp_errcode
ardp_abort_on_int(void)
{
#ifdef GL_THREADS
    /* If running multi-threaded, then there is one therad in charge of
       handling signals.  The thread is called signal_handler() */

    signal(SIGINT, signal_handler);
    return(ARDP_SUCCESS);
#else
    signal(SIGINT,ardp_trap_int);
    return(ARDP_SUCCESS);
#endif /* GL_THREADS */
}
