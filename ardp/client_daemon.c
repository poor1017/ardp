/*
 * Copyright (c) 1998 by the University of Southern California
 *
 * Written  by Nader Salehi 1998
 *
 * For copying and distribution information, please see the file
 * <usc-license.h>.
 */

#include <usc-license.h>

#ifdef GL_THREADS
#ifdef PURIFY
#include <purify.h>
#endif

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/uio.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>

#include <ardp.h>
#include <ardp_sec.h>		/* ardp_sec_commit() prototype. */
#include <ardp_time.h>		/* internals routines; add_times() */
#include "ardp__int.h"		/* ardp__gettimeofday() */
#include <perrno.h>

#ifndef max
#define max(x,y) ((x) > (y) ? (x) : (y))
#endif

static void daemon_cleanup_handler(void *args);
static RREQ match_req_for_resp(u_int16_t *cid, int peer_ardp_version);
static void process_packet_header(RREQ req, PTEXT pkt, int peer_ardp_verions,
				  int hdr_len, char *ctrlptr, 
				  int *xmit_unacked_pkts);
static int read_into_pkt(int sockfd, PTEXT *pkt, u_int16_t *cid, 
			 int *peer_ardp_version, int *hdr_len, char **ctrlptr,
			 struct sockaddr_in *serv_addr);
static void retrieval_mgmt(int ardp_port);
static void timeout_mgmt(RREQ req);
static void xmit_mgmt(void);

DAEMON daemon;
int selectPred;

/* daemon_cleanup_handler():
	This function is the clean-up handler for daemon thread.  It looks into
	the active queue for any outstanding request, and for each one of them,
	sends an abort control packet to the appropriate server.  This is
	merely a performance optimization since it frees up the server from
	some unnecessary tasks.  --Nader Salehi 5/98
 */
static void daemon_cleanup_handler(void *args)
{
    char *datastart;
    int length;
    PTEXT ptmp = NOPKT;		/* Abort packet to be sent */
    RREQ req;
    void *dummy;

    dummy = args;

    /* Now for each request send an abort packet to the associated server. */
    ptmp = ardp_ptalloc();
    datastart = ptmp->start;
    for (req = ardp_activeQ; req; req = req->next) {
	if (req->peer_ardp_version == 1) { /* v1 */
	    ptmp->start = datastart - 9;
	    ptmp->length = 9;
	    ptmp->start[0] = (unsigned char) 129; /* version # */
	    memset(ptmp->start + 1, '\000', 8); /* zero out rest of header
						 */ 
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
	} /* else */
	ardp__bwrite_cid(req->cid, ptmp);
	if (ardp_debug) {
	    if(req->peer.sin_family == AF_INET) 
		rfprintf(stderr,
			 "\nSending abort message (cid=%d) to %s(%d)...", 
			 ntohs(req->cid), inet_ntoa(req->peer_addr),
			 PEER_PORT(req));
	    else 
		rfprintf(stderr, "\nSending abort message...");
	    fflush(stderr);
	} /* if */
	length = sendto(ardp_port, (char *)(ptmp->start), ptmp->length, 0, 
			(struct sockaddr *) &(req->peer), S_AD_SZ);
	if (ardp_debug)
	    if (length != ptmp->length) {
		rfprintf(stderr, "length: %d\t ptmp->length: %d\n", length,
			 ptmp->length);
	    } /* if */
    } /* for */
    exit(0);
    
    
} /* daemon_cleanup_handler */

/* daemon_thread():
	This function could be viewed as a combination of ardp_process_active()
	and ardp_xmit().  The function, along with select_thread(), are the
	client's `engine'.  It handles transmission and retransmission of
	packets.  It also takes the apporpriate actions in case of timeout.

	Input:
	Output:	None
	Return:	Does not return
 */
void *
daemon_thread(void *args)
{
    int retval;
    RREQ req;
    struct timespec timeout;	/* Time out counter */
    void *dummy;

#ifdef PURIFY
    purify_name_thread("ARDP daemon thread");
#endif
    dummy = args;		/* This is merely to suppress "unused parameter
				   `args' warninig */

    if (ardp_debug) {
	rfprintf(stderr, "daemon_thread(): Initializing data strutcture ...");
    } /* if */

    /* Initialization routine */
    daemon.mutex = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
    retval = pthread_mutex_init(daemon.mutex, NULL);
    if (retval) {
	rfprintf(stderr, "%s\n", strerror(retval));
	internal_error("daemon_once_routine():  The module failed to"
		       "initialize daemon's mutex");
    } /* if */
    
    daemon.cond = (pthread_cond_t *)malloc(sizeof(pthread_cond_t));
    retval = pthread_cond_init(daemon.cond, NULL);
    if (retval) {
	rfprintf(stderr, "%s\n", strerror(retval));
	internal_error("daemon_once_routine():  The module failed to"
		       "initialize daemon's condition variable");
    } /* if */

    pthread_cleanup_push(daemon_cleanup_handler, NULL);
    pthread_mutex_lock(daemon.mutex);
    daemon.inputReady = daemon.outputReady = daemon.timeout = 0;
    
    if (ardp_debug) {
	rfprintf(stderr, "Done\n");
	fflush(stderr);
    } /* if */

    /* Now it is time to wait for an event to happen */
    while (1) {
	req = first_timeout();
	if (req) {
	    timeout.tv_sec = req->wait_till.tv_sec;
	    timeout.tv_nsec = req->wait_till.tv_usec * 1000;
	} /* if */
	else {
	    timeout.tv_sec = time(NULL) + ONE_YEAR;
	    timeout.tv_nsec = 0;
	} /* else */

	while (!daemon.inputReady && !daemon.outputReady && !daemon.timeout &&
	       !daemon.timerUpdate) {
	    retval = pthread_cond_timedwait(daemon.cond, daemon.mutex, 
					    &timeout);
	    if (retval) {
		if (retval == ETIMEDOUT) 
		    daemon.timeout = 1;
		else {
		    rfprintf(stderr, "%s\n", strerror(retval));
		    internal_error("daemon_thread():  Unable to wait on a"
				   " condition variable");
		} /* else */
	    } /* if */
	} /* while */
	/* NOTE:  It is very possible daemon thread wakes up with multiple
	   events (i.e., two request to send, a request and a response, a
	   request and timeout, etc.).  Therefore, the thread must check for
	   all possibilities.  --Nader Salehi 4/98 */
	EXTERN_MUTEXED_LOCK(ardp_activeQ);
	if (ardp_debug)
	    rfprintf(stderr, 
		     "daemon(): timeout: %d\t"
		     " inputReady: %d\toutputReady: %d\n",
		     daemon.timeout, daemon.inputReady, daemon.outputReady);
	if (daemon.timerUpdate) {
	    /* This predicate means that retrieval_thread has removed a request
	       from the active queue and wants to notify daemon thread of the
	       change.  This will eliminate the timing out on a non-exsiting
	       request.  Therefore, the daemon should ignore the possible
	       timeout and  out --Nader Salehi 5/98 */
	    daemon.timeout = 0;
	    daemon.timerUpdate = 0;
	    rfprintf(stderr, "daemon_thread(): timerUpdate\n");
	} /* if */
	if (daemon.timeout) {
	    daemon.timeout = 0;
	    req = first_timeout();
	    /* NOTE:  It is possible that the timeout being invoked even though
	       there is nothing in the active queue.  As an example, consider a
	       single request being retrieved by retrieval_thread while at the
	       same time daemon_thread is waiting for the timeout. --Nader
	       Salehi, Steve Augart 7/1/98*/
	    if (req)
		timeout_mgmt(req);
	} /* if */
	
	if (daemon.inputReady) {
	    internal_error("daemon_thread():  Daemon thread is processing"
			   " input_ready condition.  This should never "
			   " happen!");
	} /* if */

	if (daemon.outputReady) { 
	    xmit_mgmt();
	    daemon.outputReady = 0;
	} /* while */
	EXTERN_MUTEXED_UNLOCK(ardp_activeQ);
    } /* while */
    pthread_cleanup_pop(1);
} /* daemon_thread */

/* retrieval_thread():
	Listens to ardp_port for any incoming packet.  In case of an input, it
	reads the input and places it into a packet.  The thread will then find
	a match in the request queue.  This thread is a substitute for the
	retrieval management of daemon thread.  It is suggested by Steven
	Berson as an optimization to the {select, daemon} thread pair.  --Nader
	Salehi 5/98
 */
void *retrieval_thread(void *args)
{
    fd_set readfds;
    int	retval;
    void *dummy;

#ifdef PURIFY
    purify_name_thread("ARDP retrieval thread");
#endif
    dummy = args;		/* This is mereley to suppress "unused
				   parameter `args' warning */
    ardp_init();
    FD_ZERO(&readfds);
    while (1) {			/* Forever */
	FD_SET(ardp_port, &readfds);
	retval = select(ardp_port + 1, &readfds, NULL, NULL, NULL);
	if (retval > 0) {	/* input ready */
	    if (FD_ISSET(ardp_port, &readfds)) {
		EXTERN_MUTEXED_LOCK(ardp_activeQ);
		retrieval_mgmt(ardp_port);
		EXTERN_MUTEXED_UNLOCK(ardp_activeQ);
#if 0
		/* The request is complete and out of the active queue, so we
		   must nofify the daemon that the request's timer should not
		   be taken into an account */ 
		rfprintf(stderr, 
			 "retrieval_mgmt(%d): Locking daemon mutex ...\n",
			 __LINE__);
		pthread_mutex_lock(daemon.mutex);
		rfprintf(stderr, "retrieval_mgmt(%d): Daemon mutex locked\n",
			 __LINE__);
		daemon.timerUpdate = 1;
		pthread_cond_signal(daemon.cond);
		rfprintf(stderr, 
			 "retrieval_mgmt(%d): Unlocking daemon mutex ...\n",
			 __LINE__);
		pthread_mutex_unlock(daemon.mutex);
		rfprintf(stderr, "retrieval_mgmt(%d): Daemon mutex unlocked\n",
			 __LINE__);
#endif /* 0 */
	    } /* if */
	    else {
		if (ardp_debug)
		    rfprintf(stderr, "select_thread(%d): select failed\n",
			     __LINE__); 
	    } /* else */
	} /* if */
	else if (retval == 0) { /* Timeout */
	} /* else */
	else {			/* Error */
	    if (ardp_debug) 
		rfprintf(stderr, "select_thread(%d): select failed: %s\n",
			 __LINE__, strerror(errno));
	} /* else */
    } /* while */
} /* retrieval_thread */

/* select_thread():
	This thread is merely a messenger for daemon thread.  It listens to the
	ardp port and in case of an input, it notifies daemon immediately.
	Daemon will then read from the socket.

	Note:  As of 5/14/98 we no longer use this module.  It was replaced by
	retrieval_thread().  -- Nader Salehi
 */
void *select_thread(void *args)
{
    fd_set readfds;
    int	retval;
    void *dummy;		

    dummy = args;		/* This is merely to suppress "unused parameter
				   `args' warninig */

    /* The thread MUST call ardp_init to have the ARDP port number handy before
       it calls `select()'.  Otherwise it will not detect any incoming response
       from the server. */ 
    ardp_init();
    if (ardp_debug)
	rfprintf(stderr, "select_thread(): ardp_port: %d\n", ardp_port);
    FD_ZERO(&readfds);
    while (1) {
	FD_SET(ardp_port, &readfds);
	retval = select(ardp_port + 1, &readfds, NULL, NULL, NULL);
	if (retval > 0) {	/* input ready */
	    if (FD_ISSET(ardp_port, &readfds)) {
		pthread_mutex_lock(daemon.mutex);
		rfprintf(stderr, "select_thread(%d): InputReady = %d\n",
			 pthread_self(), daemon.inputReady);
		daemon.inputReady = 1;
		selectPred = 1;
		pthread_cond_signal(daemon.cond);
		pthread_mutex_unlock(daemon.mutex);
	    } /* if */
	    else {
		if (ardp_debug)
		    rfprintf(stderr, "select_thread(%d): select failed\n",
			     __LINE__); 
	    } /* else */
	} /* if */
	else if (retval == 0) { /* Timeout */
	} /* else */
	else {			/* Error */
	    if (ardp_debug) 
		rfprintf(stderr, "select_thread(%d): select failed: %s\n",
			 __LINE__, strerror(errno));
		
	} /* else */
    } /* while */
} /* select_thread */

/* read_into_pkt():
	This function is a subset of ardp_process_active().  Its main
	responsibility is to read information from a socket and put them in a
	packet.  It also returns some extra information back to the caller
	which are passed to other functions.  As I mentioned before, most of
	the code was "borrowed" from ardp_process_active() and for that reason,
	the interface is not as "optimized" as one would like

	Input:	sockfd	The socket to read from
	Output:	pkt	The created packet
		cid	The connection ID of the packet
		peer_ardp_version 
		hdr_len	The length of the packet header
		ctrlptr	Pointer to the first control character
		serv_addr The address of the responding peer
	Return:	0	on success
		!0	Otherwise
	Side:	A PTEXT data structure is created.
 */
static int read_into_pkt(int sockfd, PTEXT *pkt, u_int16_t *cid, 
			 int *peer_ardp_version, int *hdr_len, char **ctrlptr,
			 struct sockaddr_in *serv_addr)
{
    unsigned char firstbyte;
    int	len,
	retval = PSUCCESS,
	serv_len;
    PTEXT packet;
    u_int16_t		stmp;	/* Temp short for conversions    */

    /* Initialization part */
    *cid = 0;
    *ctrlptr = NULL;
    *pkt = packet = ardp_ptalloc();
    packet->start = packet->dat;

    /* Read the packet */
    serv_len = sizeof(*serv_addr);
    len = recvfrom(sockfd, packet->start, sizeof(packet->dat), 0,
		   (struct sockaddr *)serv_addr, &serv_len);
    if (len < 0) {
#if 1
	fd_set readfds;
	struct timeval timeout;

	rfprintf(stderr, "%s(%d): recvfrom fails: %s\n", __FILE__,
		 __LINE__, strerror(errno)); 
	FD_ZERO(&readfds);
	FD_SET(ardp_port, &readfds);
	timeout.tv_sec = 0;
	timeout.tv_usec = 10;
	retval = select(ardp_port + 1, &readfds, NULL, NULL, &timeout);
	if (retval > 0) {
	    if (FD_ISSET(ardp_port, &readfds))
		rfprintf(stderr, 
			 "read_into_packet(): Input is really ready\n");
	    else
		rfprintf(stderr, "read_into_packet(%d): select failed\n",
			 __LINE__);
	} /* if */
	else if (retval == 0) 
	    rfprintf(stderr, "read_into_packet(%d): Nothing actually to read."
		     " No wonder that operation would block!!\n", __LINE__);
	else
	    rfprintf(stderr, "read_into_pkt(%d): select failed: %s\n",
		     __LINE__, strerror(errno));
	rfprintf(stderr, "read_into_pkt(%d): selectPred: %d\n", __LINE__,
		 selectPred);
	exit(0);
#endif
	retval = ARDP_BAD_RECV;
	if (ardp_debug)
	    rfprintf(stderr, "%s(%d): recvfrom fails: %s\n", __FILE__,
		     __LINE__, strerror(errno)); 
    } /* if */

    selectPred = 0;
    if (retval == PSUCCESS) {
	packet->length = len;
	*(packet->start + packet->length) = '\0';

	if (packet->length == 0) {
	    retval = ARDP_BAD_RECV;
	} /* if */
    } /* if */

    if (retval == PSUCCESS) {
	firstbyte = ((unsigned char *)packet->start)[0];
	if (firstbyte == 129) {	/* ARDP V1 */
	    *peer_ardp_version = 1;
	    *hdr_len = ((unsigned char *)packet->start)[4];
	    if ((packet->length < 5) || (*hdr_len < 5)) { 
		/* Malformed pakcet */
		retval = ARDP_BAD_RECV;
		if (ardp_debug) 
		    rfprintf(stderr, "%s(%d): Client received malformed V1"
			     " packet with header < 5 octets; discarding\n",
			     __FILE__, __LINE__);
	    } /* if */
	    else {		/* Get the connection ID */
		if (*hdr_len >= 7)
		    memcpy2(cid, packet->start + 5);
		else
		    *cid = 0;
	    } /* else */
	} /* if */
	else if (firstbyte == '\0') { /* BAD-VERSION message */
	    retval = ARDP_BAD_VERSION;
	} /* else */
	else if (firstbyte > 129) { /* ARDP v2 or later */
	    retval = ARDP_BAD_VERSION;
	} /* else */
	else if (((unsigned char *)packet->start)[0] >= 32) { /* pre-ARDP */
	    /* Since client does not accept pre-ARDP responses and it only
	       sends V0 or V1 request, then it is a bad version */
	    retval = ARDP_BAD_VERSION;
	} /* else */
	else {			/* ARDP V0 */
	    *peer_ardp_version = 0;
	    *hdr_len = firstbyte;
	    *ctrlptr = packet->start + 1;
	    if (*hdr_len >= 3) {
		memcpy2(&stmp, *ctrlptr);
		*ctrlptr += 2;
		*cid = stmp;
	    } /* if */
	    else 
		*cid = 0;
	} /* else */
    } /* if */

    if (retval != PSUCCESS) {	/* Clean up the packet */
	ardp_ptlfree(packet);
	*pkt = NULL;
    } /* if */
    return retval;
} /* read_into_pkt */

/* process_packet_header():
	As the name suggests, processes the packet header and see how it
	affects the request.
 */
static void process_packet_header(RREQ req, PTEXT pkt, int peer_ardp_version, 
				  int hdr_len, char *ctrlptr, 
				  int *xmit_unacked_pkts)
{
    struct timeval now;
    u_int16_t stmp;		/* Temp short for conversions    */
    u_int32_t ltmp;		/* Temp long for converions      */
    unsigned char rdflag11;	/* First byte of flags (bit vect) */
    unsigned char rdflag12;	/* Second byte of flags (int)    */
    unsigned char tchar;	/* For decoding extra fields     */

    *xmit_unacked_pkts = FALSE;
    /* NOTE:  At this time only version 1 is implemented.  Support for V0
       will be provided later */
    if (peer_ardp_version == 1) {
	ctrlptr = pkt->start + 11;
	
	/* Octet1; context.  We currently only process the security context
	 */ 
	pkt->context_flags = pkt->start[1];
	/* Octet 2: flags */
	if (pkt->start[2] & 0x01) { /* bit 0: ack */
	    if (ardp_debug >= 8) 
		rfprintf(stderr,"Ack requested\n");
	    req->status = ARDP_STATUS_ACKPEND;
	} /* if */
	if (pkt->start[2] & 0x02) {/* bit 1: sequenced control packet */
	    if(ardp_debug >= 8) 
		rfprintf(stderr,"Sequenced control packet\n");
	    pkt->length = -1;   /* flag as sequenced control packet. */
	} /* if */
	if (pkt->start[2] & 0x04) {/* bit 2: total packet count */
	    if (ctrlptr + 2 <= pkt->start + hdr_len) {
		memcpy2(&stmp, ctrlptr);
		req->rcvd_tot = ntohs(stmp);
		ctrlptr += 2;
	    } /* if */
	} /* if */
	if (pkt->start[2] & 0x08) { /* bit 3: priority in 2-octet argument
				     */ 
	    /* The client doesn't currently use this information but let's
	       copy it in any case. */
	    if (ctrlptr + 2 <= pkt->start + hdr_len) {
		memcpy2(&stmp, ctrlptr);
		req->priority = ntohs(stmp);
		ctrlptr += 2;
	    } /* if */
	} /* if */
	
	/* Bit 4: Protocol ID in two-octet arg. Unused in this
	   implementation. */
	if (pkt->start[2] & 0x20) { /* bit 5: max window size */
	    if (ctrlptr + 2 <= pkt->start + hdr_len) {
		memcpy2(&stmp, ctrlptr);
		req->pwindow_sz = ntohs(stmp);
		ctrlptr += 2;
	    } /* if */
	} /* if */
	
	if (pkt->start[2] & 0x40) { /* bit 6: wait time. */
	    if (ctrlptr + 2 <= pkt->start + hdr_len) {
		memcpy2(&stmp, ctrlptr);
		if(stmp || (req->svc_rwait != ntohs(stmp))) {
		    /* Make sure now is current */
		    *xmit_unacked_pkts = FALSE;
		    /* New or non-zero requested wait value */
		    req->svc_rwait = ntohs(stmp);
		    if (req->svc_rwait >= req->timeout.tv_sec) {
			req->timeout_adj.tv_sec = req->svc_rwait;
			req->timeout_adj.tv_usec = 0;
		    } /* if */
		    else {
			req->timeout_adj = req->timeout;
		    } /* else */
		    now = ardp__gettimeofday();
		    req->wait_till = add_times(req->timeout_adj, now);
		    /* Reset the retry count */
		    req->retries_rem = req->retries;

		    /* Modify the time priority queue. */
		} /* if */
		ctrlptr += 2;
	    } /* if */
	} /* if */
	if (pkt->start[2] & 0x80) /* indicates octet 3 is OFLAGS.  For now,
				     we use this to just disable octet
				     3. */ 
	    pkt->start[3] = 0;
	
	/* Octet 3: options */
	switch(((unsigned char *) pkt->start)[3]) {
	case 0:		/* no option specified */
	    break;
	case 1:		/* ARDP connection refused */
	    if(ardp_debug >= 8) 
		rfprintf(stderr,"  ***ARDP connection refused by"
			" server***\n");
	    req->status = ARDP_REFUSED;
	    break;
	case 2:		/* reset peer's received-through count */
	    /* Ignore this for now */
	    break;
	case 3:		/* packets received beyond received-through */
	    /* XXX we don't do this yet; we want to for better flow control. */
	    break;
	case 4:		/* redirect */
	case 5:		/* redirect and notify.  -- we treat it as case
			   4, because we don't have the upcall facility
			   yet to notify the client's caller. */ 
	    if (ctrlptr + 6 <= pkt->start + hdr_len) {
		/* Server Redirect (case 4): */
		memcpy4(&(req->peer_addr), ctrlptr);  
		ctrlptr += 4;
		memcpy2(&(req->peer_port), ctrlptr);  
		ctrlptr += 2;
		if(ardp_debug >= 8) 
		    rfprintf(stderr,"  ***ARDP redirect to %s(%d)***",
			    inet_ntoa(req->peer_addr),PEER_PORT(req));
		/* NOTE:  Here the client is asked to transmit the request
		   once again.  I am not sure if that should happen within
		   this function of the function should notify the daemon
		   --Nader Salehi 4/98 */
		req->status = ARDP_STATUS_NOSTART;
		daemon.outputReady = 1;
	    } /* if */
	    break;
	case 8:		/* bad-version; invalidate this message
			   BAD-VERSION message ARDP; client's version
			   is obsolete */
	    if (ardp_debug)
		rfprintf(stderr, "Option %d not recognized; ignored.", 
			 pkt->start[3]);
	} /* switch */
	/* Octet 4: header length; already processed */
	/* Octets 5 -- 6: Connection ID, also already processed */
	/* Octet 7 -- 8: Packet Sequence Number */
	if (hdr_len >= 9) {
	    memcpy2(&stmp, pkt->start + 7);
	    pkt->seq = ntohs(stmp);
	} /* if */

	/* Octet 9 -- 10: received-through */
	if (hdr_len >= 11) {
	    memcpy2(&stmp, pkt->start + 9);
	    stmp = ntohs(stmp);
	    req->prcvd_thru = max(stmp, req->prcvd_thru);
	    ctrlptr += 2;
	    *xmit_unacked_pkts = TRUE;
	} /* if */
	/* We've received an acknowledgement, so reset the timeouts. */
	/* XXX should we reset the timeout further, as in, set the
	   retries_rem back to starting values? --swa 9/96 */
	now = ardp__gettimeofday();
	req->wait_till = add_times(now, req->timeout_adj);
    } /* if */
    else {			/* V0 */
	if (hdr_len >= 5) {	/* Pakcet number */
	    memcpy2(&stmp, ctrlptr);
	    pkt->seq = ntohs(stmp);
	    if (ardp_debug)
		rfprintf(stderr, " (seq = %d)", pkt->seq);
	    ctrlptr += 2;
	} /* if */
	else {			/* No packet number specified, so this is the
				   only one */ 
	    pkt->seq = 1;
	    req->rcvd_tot = 1;
	} /* else */

	if(hdr_len >= 7) {	/* Total number of packets */
	    memcpy2(&stmp, ctrlptr); /* 0 means don't know      */
	    if(stmp) {
		req->rcvd_tot = ntohs(stmp);
		if (ardp_debug)
		    rfprintf(stderr, " (rcvd_tot = %d)", req->rcvd_tot);
	    } /* if */
	    ctrlptr += 2;
	} /* if */

	if(hdr_len >= 9) {	/* Received through */
	    memcpy2(&stmp, ctrlptr);  
	    stmp = ntohs(stmp);
	    req->prcvd_thru = max(stmp,req->prcvd_thru);
	    ctrlptr += 2;
	    if (req->prcvd_thru < req->trns_tot) {
		*xmit_unacked_pkts = TRUE;
	    } /* if */
	    req->wait_till = add_times(ardp__gettimeofday(), req->timeout_adj);
	} /* if */

	if(hdr_len >= 11) {	/* Service requested wait */
	    memcpy2(&stmp, ctrlptr);
	    if(stmp || (req->svc_rwait != ntohs(stmp))) {
		*xmit_unacked_pkts = FALSE;
		
		/* New or non-zero requested wait value */
		req->svc_rwait = ntohs(stmp);

		/* Adjust our timeout */
		if (req->svc_rwait > req->timeout.tv_sec) {
		    req->timeout_adj.tv_sec = req->svc_rwait;
		    req->timeout_adj.tv_usec = 0;
		} /* if */
		else {
		    req->timeout_adj = req->timeout;
		} /* else */
		now = ardp__gettimeofday();
		req->wait_till = add_times(req->timeout_adj, now);

		/* Reset the retry count */
		req->retries_rem = req->retries;
	    } /* if */
	    ctrlptr += 2;
	} /* if */

	if(hdr_len >= 12) {	/* Flags (1st byte) */
	    memcpy1(&rdflag11, ctrlptr);
	    if(rdflag11 & 0x80) {
		req->status = ARDP_STATUS_ACKPEND;
	    } /* if */
	    if(rdflag11 & 0x40) {
		pkt->length = -1;
	    } /* if */
	    ctrlptr += 1;
	} /* if */

	if(hdr_len >= 13) {	/* Flags (2nd byte) */
	    /* Reserved for future use */
	    memcpy1(&rdflag12, ctrlptr);
	    ctrlptr += 1;
	    if(rdflag12 == 2) {
		memcpy2(&stmp, pkt->start+7);  
		stmp = ntohs(stmp);
		if (ardp_debug >= 8) 
		    rfprintf(stderr, "[*Request to set back prcvd_thru to %d;"
			     " accepted (old prcvd_thru = %d)*]", 
			     stmp, req->prcvd_thru);
		req->prcvd_thru = stmp;
	    } /* if */
	    if(rdflag12 == 1) {
		/* ARDP Connection Refused */
		if(ardp_debug >= 8) 
		    rfprintf(stderr,"  ***ARDP connection refused by"
			     " server***\n");
		req->status = ARDP_REFUSED;
	    } /* if */
	    if(rdflag12 == 4) {
		/* Server Redirect */
		memcpy4(&req->peer_addr, ctrlptr);
		ctrlptr += 4;
		memcpy2(&req->peer_port, ctrlptr);  
		ctrlptr += 2;
		if(ardp_debug >= 8) 
		    rfprintf(stderr,"  ***ARDP redirect to %s(%d)***",
			     inet_ntoa(req->peer_addr),PEER_PORT(req));
		/* NOTE:  Here the client is asked to transmit the request
		   once again.  I am not sure if that should happen within
		   this function of the function should notify the daemon
		   --Nader Salehi 4/98 */
		req->status = ARDP_STATUS_NOSTART;
		daemon.outputReady = 1;
	    } /* if */
	    if(rdflag12 == 8) { /* Bad-Version.  Invalidate or retry this
				   message.  */ 
		req->status = ARDP_BAD_VERSION;
	    } /* if */

	    if(rdflag12 == 254) {
		tchar = *ctrlptr;
		ctrlptr++;
		if(tchar & 0x1) { /* Queue position */
		    memcpy2(&stmp, ctrlptr);
		    req->inf_queue_pos = ntohs(stmp);
		    if(ardp_debug >= 8) 
			rfprintf(stderr," (Current queue position on"
				 " server is %d)", req->inf_queue_pos);
		    ctrlptr += 2;
		} /* if */
		if(tchar & 0x2) { /* Expected system time */
		    memcpy4(&ltmp, ctrlptr);
		    req->inf_sys_time = ntohl(ltmp);
		    if(ardp_debug >= 8) 
			rfprintf(stderr," (Expected system time is"
				 "%d seconds)", req->inf_sys_time);
		    ctrlptr += 4;
		} /* if */
	    } /* if */
	} /* if */
    } /* if */
} /* process_packet_header */

/* match_req_for_resp():
	Tries to find a match in the active queue with the same connection ID
	and ardp version as in cid and peer_ardp_version, respectively.

	Input:	cid	Connection ID
		peer_..	The peer's ARDP version
	Output:	cid	Only when the client had set a cid of 0 in the request.
	Return:	RREQ	The pointer to the request.
	Side:	It assumes that ardp_activeQ is locked.
 */
static RREQ match_req_for_resp(u_int16_t *cid, int peer_ardp_version)
{
    RREQ req;

    for (req = ardp_activeQ; req && (req->cid != *cid) &&
	     (req->peer_ardp_version != peer_ardp_version);
	 req = req->next);
    if (!req) {
	/* v0 and v1 both allow a CID of 0 (or unset CID) to be sent in
	   response. This is only supposed to be sent in response to a request
	   that did not specify another CID.  However, some Prospero servers
	   (see comments on pre-ARDP above, discussing ARCHIE.AU scenario) send
	   us v0 cid 0 initial responses to v1 requests. 

	   In the old-v0 (Prospero version 4) servers, there may have been a
	   bug where, if the client specified a non-zero CID in its request,
	   the server in its reply might reply with a CID of zero (or unset).
	   This will fix that (illegal) behavior.  If it in fact ever occurred.
       
	   If we (the client) had set a cid of 0 in the request, then the loop
	   above would have performed the match correctly. */
	if ((peer_ardp_version == 0) && (*cid == 0) && 
	    (ardp_activeQ_len == 1) && 
	    (ardp_activeQ->peer_ardp_version == 0)) {
	    req = ardp_activeQ;
	    *cid = req->cid;
	    if (ardp_debug)
		rfprintf(stderr, "(assuming cid=%d)", cid);
	} /* if */
    } /* if */
    return req;
} /* match_req_for_resp */

/* timeout_mgmt():
	Responds to a timed out request.  The function is invoked due to
	failure in receiving an acknowledgment from the server.  It first
	begins to recover from the loss by retransmitting the unacknowledged
	packets.  Once tried for several times, the function terminates the
	transmission and places the request in the complete queue.

	NOTE:  The function assumes the ardp_activeQ is locked.  Failure to
	lock the mutex before calling the function will result in race
	situations.
 */
static void timeout_mgmt(RREQ req)
{
    struct timeval now;		/* Holds the current time */

    if (req->status == ARDP_STATUS_ACKPEND) {
	ardp_xmit(req, -1);	/* Just ACK; send no data packets */
	req->status = ARDP_STATUS_ACTIVE;
    } /* if */
    now = ardp__gettimeofday();
    if ((req->wait_till.tv_sec) &&
	(time_is_later(now, req->wait_till))) {
	if (req->retries_rem-- > 0) {
	    ardp__adjust_backoff(&(req->timeout_adj));
	    if (req->pwindow_sz > 4)
		req->pwindow_sz /= 2;
	    /* Reset ACK bit on packets to be retransmitted */
	    ardp_headers(req);
	    if (ardp_debug >= 8) 
		rfprintf(stderr,"Connection %d timed out - Setting timeout to"
			 " %d.%06ld seconds; transmission window to %d"
			 " packets.\n", (int) ntohs(req->cid), 
			 (int) req->timeout_adj.tv_sec,
			 (long) req->timeout_adj.tv_usec,
			 (int) req->pwindow_sz);
	    now = ardp__gettimeofday();
	    req->wait_till = add_times(req->timeout_adj, now);
	    ardp_xmit(req, req->pwindow_sz);
	    /* The new time out ==> the new queue line up --Nader
	       Salehi 4/98 */
	} /* if */
	else {	/* No more chances */
	    if (ardp_debug) 
		rfprintf(stderr, "Time out occured for the request. The"
			 " request is moved to the complete queue.\n");
	    req->status = ARDP_TIMEOUT;
	    EXTERN_MUTEXED_LOCK(ardp_completeQ);
	    EXTRACT_ITEM(req, ardp_activeQ);
	    --ardp_activeQ_len;
	    APPEND_ITEM(req, ardp_completeQ);
	    if ((req->mutex) && (req->cond)) { 
		/* If a blocking mode transmission.  Note that req->status is
		   the predicate and, therefore, no need to introduce a new one
		*/ 
		pthread_mutex_lock(req->mutex);
		pthread_cond_signal(req->cond);
		pthread_mutex_unlock(req->mutex);
	    } /* if */
	    EXTERN_MUTEXED_UNLOCK(ardp_completeQ);
	} /* else */
    } /* if */
} /* timeout_mgmt */

/* retrieval_mgmt():
	This function is a "deciphered" version of a portion of
	ardp_process_active().  It is responsible to retrieve a packet from the
	socket and find a match in the request queue.  It then processes the
	packet and takes the right action to the packet (e.g., stores the
	packet,  sends an ACK, etc.).

	NOTE:  Although tried hard, the "decryption" algorithm did not work
	properly.  In some parts I was forced to "glue" the code!!  
	--Nader Salehi 4/98
 */
static void retrieval_mgmt(int ardp_port)
{
    enum ardp_errcode errcode;	/* Temporarily stores error return values. */ 
    char *ctrlptr;		/* Pointer to control field */
    int	hdr_len,		/* Header lenght */
	ins_done = 0,
	peer_ardp_version,	/* 1 or 0; The client doesn't differentiate
				   between old-v0 and new-v0, */ 
	req_complete = 0,
	xmit_unacked_pckts = FALSE, /* Used as a boolean flag while processing
				       header octets 9 and 11.  Tested
				       later. */    
	retval;
    PTEXT pkt = NOPKT;
    PTEXT ptmp = NOPKT;		/* Packet being processed */
    RREQ req;
    struct sockaddr_in from;
    struct timeval now;
    u_int16_t cid;		/* Connection ID from rcvd pkt */

    retval = read_into_pkt(ardp_port, &pkt, &cid, &peer_ardp_version,
			   &hdr_len, &ctrlptr, &from);  
    if (ardp_debug >= 11)
	rfprintf(stderr, "retrieval_mgmt(): packet [cid = %d] has been read\n",
		 cid);
    if (retval)	return;		/* Assume nothing has ever happened */

    req = match_req_for_resp(&cid, peer_ardp_version);
    if (!req) {	/* If there is no matching request */
	if (ardp_debug) 
	    rfprintf(stderr, "No match is found\n");
	ardp_ptlfree(pkt);
	return;
#if 0
	if ((pkt_info.cid == 0) && 
	    (pkt_info.peer_ardp_version == 0)) 
	    handle_bad_version_response(ARDP_V0_CID_0_UMATCHED, NOREQ, "", 1);
#endif
    } /* if */
    /* Time to process the packet header */
    process_packet_header(req, pkt, peer_ardp_version,
			  hdr_len, ctrlptr, &xmit_unacked_pckts);
    if ((req->status >= 0) || 
	(req->status == ARDP_STATUS_ABORTED) ||
	(req->status == ARDP_STATUS_FAILED)) {
	/* Problem with the packet.  Connection is closed. Remove the packet
	   from the active queue, put it on the complete queue, and in case the
	   owner is waiting for the response, inform the owner. */ 
	if (retval)
	    internal_error("retrieval_mgmt():  The request does"
			   " not seem to exist in the timer queue!");
	
	EXTRACT_ITEM(req, ardp_activeQ);
	--ardp_activeQ_len;
	EXTERN_MUTEXED_LOCK(ardp_completeQ);
	APPEND_ITEM(req, ardp_completeQ);
	if ((req->mutex) && (req->cond)) { 
	    /* If the request was in blocking mode */ 
	    pthread_mutex_lock(req->mutex);
	    /* The predicate is the request status itself.  No need to set
	       anything else. --Nader Salehi  */ 
	    pthread_cond_signal(req->cond);
	    pthread_mutex_unlock(req->mutex);
	} /* if */
	EXTERN_MUTEXED_UNLOCK(ardp_completeQ);
	return;
    } /* if */
    
    /* If ->seq == 0, then an unsequenced control packet.  The code above has
       done all of the control information processing, so there is no need for
       this packet */
    if (pkt->seq == 0) {
	if (xmit_unacked_pckts) {
	    ardp_headers(req);	/* set ACK bit appropriately for retransmitted
				   packets */ 
	    ardp_xmit(req, req->pwindow_sz);
	} /* if */
	ardp_ptfree(pkt);
	return;
    } /* if */
    else {
	if(pkt->length >= 0) 
	    pkt->length -= hdr_len;
	pkt->start += hdr_len;
	pkt->text = pkt->start;
	pkt->ioptr = pkt->start;
	if (req->rcvd == NOPKT) {
	    /* Add it as the head of empty doubly linked list */
	    APPEND_ITEM(pkt, req->rcvd);
	    memcpy(&(req->peer), &from, sizeof(from));

	    /* Set the numbers */
	    if(pkt->seq == 1) {
		req->comp_thru = pkt;
		req->rcvd_thru = 1;
		/* If only one packet, then return it.  We will assume that it
		   is not a sequenced control packet */  
		if(req->rcvd_tot == 1) 
		    req_complete = 1;
	    } /* if */
	    ins_done = 1;
	} /* if */
    } /* else */
    if (!ins_done & !req_complete) {
	if(req->comp_thru && (pkt->seq <= req->rcvd_thru))
	    ardp_ptfree(pkt); /* Duplicate */
	else if(pkt->seq < req->rcvd->seq) { /* First (sequentially) */ 
	    PREPEND_ITEM(pkt, req->rcvd);
	    ins_done = 1;
	    if(req->rcvd->seq == 1) {
		req->comp_thru = req->rcvd;
		req->rcvd_thru = 1;
		/* Note that in ins_done we may update req->rcvd_thru
		   further. --swa, 7/27/94 */  
	    } /* if */
	} /* if */
	else { /* Insert later in the packet list unless duplicate */ 
	    ptmp = (req->comp_thru ? req->comp_thru : req->rcvd);
	    while (pkt->seq > ptmp->seq) {
		if(ptmp->next == NULL) { 
		    /* Insert at end */
		    APPEND_ITEM(pkt, req->rcvd);
/* 		    if ((pkt->seq == 2) && (!req->comp_thru)) */
/* 			ins_done = 1; */
		    ins_done = 1;
		    break;
		} /* if */
		ptmp = ptmp->next;
	    } /* while */
	    if (!ins_done) {
		if(ptmp->seq == pkt->seq) /* Duplicate */
		    ardp_ptfree(pkt);
		else { 
		    /* insert before ptmp (we know we're not first).  We also
		       know that ptmp != NULL (not at end either). */ 
		    INSERT_ITEM1_BEFORE_ITEM2(pkt, ptmp);
		    ins_done = 1;
		} /* else */
	    } /* if */
	} /* else */
    } /* if */
    if (ins_done & !req_complete) {
	/* Find out how much is complete and remove sequenced control
	   packets */  
	while(req->comp_thru && req->comp_thru->next && 
	      (req->comp_thru->next->seq == 
	       (req->comp_thru->seq + 1))) {
	    /* We have added one more in sequence */
	    if(req->comp_thru->length == -1) {
		/* Old comp_thru was a sequenced control packet */ 
		ptmp = req->comp_thru;
		req->comp_thru = req->comp_thru->next;
		req->rcvd_thru = req->comp_thru->seq;
		EXTRACT_ITEM(ptmp,req->rcvd);
		ardp_ptfree(ptmp);
	    } /* if */
	    else {
		/* Old comp_thru was a data packet */
		req->comp_thru = req->comp_thru->next;
		req->rcvd_thru = req->comp_thru->seq;
	    } /* else */
	    
	    /* Update outgoing packets */
	    ardp_headers(req);
	    
	    /* We've made progress, so reset timeout and retry count.  The
	       code below replaces this line, but does it for milliseconds
	       too. --swa 9/96 
	       req->timeout_adj.tv_sec =
	       req->req->max(req->timeout.tv_sec,req->svc_rwait); */ 
	    if (req->svc_rwait > req->timeout.tv_sec) {
		req->timeout_adj.tv_sec = req->svc_rwait;
		req->timeout_adj.tv_usec = 0;
	    } /* if */
	    else {
		req->timeout_adj = req->timeout;
	    } /* else */
	    now = ardp__gettimeofday();
	    req->wait_till = add_times(req->timeout_adj, now);
	    req->retries_rem = req->retries;
	} /* while */
	
	/* See if there are any gaps - possibly toggle GAP status */ 
	if(!req->comp_thru || req->comp_thru->next) {
	    if (req->status == ARDP_STATUS_ACTIVE) 
		req->status = ARDP_STATUS_GAPS;
	} /* if */
	else if (req->status == ARDP_STATUS_GAPS) 
	    req->status = ARDP_STATUS_ACTIVE;
	/* Check whether the request is now complete. */ 
	if ((req->comp_thru) && (req->comp_thru->seq == req->rcvd_tot)) {
	    req_complete = 1;
	} /* if */
    } /* if */
    if (req_complete) {
	if(req->status == ARDP_STATUS_ACKPEND) {
	    /* Don't need to call ardp_headers() to set ACK bit; we do not
	       need an ACK from the server, and no data packets will be
	       transmitted. */  
	    ardp_xmit(req, req->pwindow_sz);
	} /* if */
	req->status = ARDP_STATUS_COMPLETE;
	req->inpkt = req->rcvd;
	EXTRACT_ITEM(req,ardp_activeQ);
	--ardp_activeQ_len;
#ifndef ARDP_NO_SECURITY_CONTEXT
	/* Our request has received a complete reply.  The RREQ
	   representing the request and its reply are now off of the
	   ardp_activeQ; it's time to check for any security contexts and
	   verify them. Null means we don't care about the upward error
	   reporting of which context failed. */   
	if((errcode = ardp__sec_process_contexts(req, NULL))) {
	    /* ardp__sec_process_contexts() will notify the
	       peer. */ 
	    req->status = errcode;
	} /* if */
#endif
	if (retval)
	    internal_error("retrieval_managment():  The request does"
			   " not seem to exist in the timer queue!");
	EXTERN_MUTEXED_LOCK(ardp_completeQ);
	APPEND_ITEM(req,ardp_completeQ);
	EXTERN_MUTEXED_UNLOCK(ardp_completeQ);
	if ((req->mutex) && (req->cond)) {
	    /* Blocking mode, so inform the owner.  NOTE: req->status is
	       the predicate.  No extra req->predicate is needed */ 
	    pthread_mutex_lock(req->mutex);
	    pthread_cond_signal(req->cond);
	    pthread_mutex_unlock(req->mutex);
	} /* if */
    } /* if */
} /* retrieval_mgmt */

/* xmit_mgmt():
	Transmits one or more requests to server(s).  The requests all are in
	the acitve queue.  The functions checks the status of each packet and
	sends them if they are ready to send.

	NOTE: The function assumes that ardp_activeQ mutex is locked!
 */
static void xmit_mgmt(void)
{
    char *datastart;
    enum ardp_errcode aerr;	/* To temporarily hold return values  */
    int length;			/* Number of bytes actually sent */
    PTEXT ptmp = ardp_ptalloc(); /* Abort packet to be sent */
    RREQ req,
	rtmp;

    req = ardp_activeQ;
    while (req) {
	/* Transmit as many new requests as there is in the active queue. */
	switch (req->status) {
	case ARDP_STATUS_NOSTART:
	    aerr = ardp_xmit(req, req->pwindow_sz);
	    if (aerr) {
		perrno = aerr;
		rfprintf(stderr, 
			 "Transmission problem at %s(%d): %s\n",
			 __FILE__, __LINE__, p_err_string);
	    } /* if */
	    req->status = ARDP_STATUS_ACTIVE;
	    /* Set the timer. */
	    break;
	case ARDP_STATUS_ABORTED:
	    /* Send an abort signal to the server.  Remove the request from the
	       active queue and notify the client thread.

	       NOTE:  The following block is stolen from ardp_abort().  If
	       there is a bug there, then there will be one here.  --Nader
	       Salehi 5/98 */
	    datastart = ptmp->start;
	    if (req->peer_ardp_version == 1) { /* v1 */
		ptmp->start = datastart - 9;
		ptmp->length = 9;
		ptmp->start[0] = (unsigned char) 129; /* version # */
		memset(ptmp->start + 1, '\000', 8); /* zero out rest of header
						     */ 
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
	    } /* else */
	    ardp__bwrite_cid(req->cid, ptmp);
	    length = sendto(ardp_port,(char *)(ptmp->start), ptmp->length, 0, 
			    (struct sockaddr *) &(req->peer), S_AD_SZ);
	    rtmp = req->previous;
	    EXTRACT_ITEM(req, ardp_activeQ);
	    --ardp_activeQ_len;
	    ardp_rqfree(req);
	    break;
	case ARDP_STATUS_ACKPEND:
	    /* Sends an ACK to the server and change the status to
	       ARDP_STATUS_ACITVE */
	    break;
	default:
	    break;
	} /* switch */
	req = req->next;
    } /* while */
    ardp_ptfree(ptmp);
} /* xmit_mgmt */
#endif /* GL_THREADS */
