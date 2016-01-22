/*
 * Copyright (c) 1991, 1992, 1993, 1997 by the University of Southern California
 *
 * For copying and distribution information, please see the file
 * <usc-license.h>
 *
 * Written  by bcn 1989-92  as part of dirsend.c in the Prospero distribution
 * Modified by bcn 1/93     moved to ardp library - added asynchrony 
 */

#include <usc-license.h>

#include <stdio.h>
#include <memory.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <memory.h>		/* for memset() */
#ifdef AIX
#include <sys/select.h>
#endif

#include <pmachine.h>		/* for memset(), if not prototyped */
#include <ardp.h>
#include <ardp_time.h>		/* for 'struct timeval' math routines. */
#include <ardp_sec.h>		/* prototypes ardp__sec_process_contexts() */
#include "ardp__int.h"		/* ardp__adjust_backoff() */
#include <perrno.h> 

#ifndef max
#define max(x,y) ((x) > (y) ? (x) : (y))
#endif

/* DEFINITIONs for variables DECLARED in ardp.h */
EXTERN_MUTEXED_DEF(RREQ,ardp_activeQ);
int	    ardp_activeQ_len;

EXTERN_MUTEXED_DEF(RREQ, ardp_completeQ);

static enum ardp_errcode  ardp__pr_ferror(int errcode);
static void do_retransmit_unacked_packets(RREQ req);
/* these are local; used only by ardp_pr_actv, to specify context */
enum ardp_bad_version_response_flag {VERSION_INDEPENDENT_BAD_VERSION,
				     PRE_ARDP_V0_RESPONSE, 
				     ARDP_V0_CID_0_UNMATCHED,
				     VERSION_SPECIFIC_BAD_VERSION_RESPONSE };
/* Here's an additional flag for RREQ->flags:*/
static const ARDP__ALREADY_GOT_BOGUS_V0_CID0_RESPONSE_TO_ARDP_V1_REQUEST = 4;

				     
static enum ardp_errcode handle_bad_version_response(enum ardp_bad_version_response_flag,
				       RREQ req,  const char *ctlptr, int nbytes);


/* This is a function-valued variable which is set in lib/ardp/ardp_srv_ini.c.
   Because we use it, we do not need to link ardp_process_active() with
   ardp_accept() at compile time.  The binding happens at run time; this means
   that a program which is only a client never needs to be linked with
   ardp_accept().  This, in turn, reduces the executable's size. --katia & swa,
   2/97 */

enum ardp_errcode (*ardp__accept)(void) = NULL;

/*
 * ardp_process_active - process active requests
 *
 *   ardp_process_active takes no arguments.  It checks to see if any responses
 *   are pending on the UDP port, and if so processes them, adding them
 *   to the request structures on ardp_activeQ.  If no requests are pending
 *   ardp_process_active processes any requests requiring retries then
 *   returns.
 *
 */
/* This client code does not know how to parse pre-ARDP responses; since it
   sends only ARDP-V0 and ARDP-V1 requests, presumably any pre-ARDP servers
   responding would be sending back bogus data. */
/* If we get a pre-ARDP response, and there is only one request outstanding, we
   immediately mark that request as completed with an error. 
   If we get a pre-ARDP response and have more than one request outstanding, we
   don't know which request would have caused the error -- pre-ARDP responses
   don't include the CID of the request that caused the problems.  Therefore,
   we don't remove any active requests, even the (unidentifiable) one that
   caused the problem -- instead, we let all the requests try to complete.  
   This means that the request which got the pre-ARDP response will time
   out. 
   */ 
enum ardp_errcode
ardp_process_active()
{
    enum ardp_errcode	errcode; /* Temporarily stores error return values. */ 
    fd_set		readfds;     /* Used for select		      */
    struct sockaddr_in	from;	     /* Reply received from	      */
    unsigned int	from_sz;     /* Size of address structure     */
    unsigned int	hdr_len;     /* Header Length                 */
    unsigned char	firstbyte; /* first byte of the message. */
    int			peer_ardp_version; /* 1 or 0; The client doesn't
					      differentiate between old-v0 and
					      new-v0, */ 
    /* We initialize this to NULL just to shut up GCC -W.  We have checked, and
       this is indeed being set before use.  --swa & katia */
    char		*ctlptr = NULL; /* Pointer to control field.      */
    u_int16_t	cid = 0; /* Connection ID from rcvd pkt   */
    unsigned char 	rdflag11; /* First byte of flags (bit vect)*/
    unsigned char 	rdflag12; /* Second byte of flags (int)    */
    unsigned char	tchar;	/* For decoding extra fields     */
    u_int16_t		stmp;	/* Temp short for conversions    */
    u_int32_t		ltmp;	/* Temp long for converions      */
    int			select_retval;	/* Return value from select() */
    int			nr;	/* Number of octets received     */
    RREQ	req = NOREQ;	/* To store request info         */
    PTEXT	pkt = NOPKT;	/* Packet being processed        */
    PTEXT	ptmp = NOPKT;	/* Packet being processed        */
    int retransmit_unacked_packets = FALSE; /* Used as a boolean flag while
                                               processing header octets 9 and
                                               11.   Tested later. */  
    struct timeval  select_zerotimearg; /* Used by select        */
    struct timeval now = zerotime; /* current time of day, if needed; zero if
				      not. */ 

check_for_pending:

    /* Set list of file descriptors to be checked by select */
    FD_ZERO(&readfds);
    if(ardp_port != -1) FD_SET(ardp_port, &readfds);

    /* Check the server ports as well, in case the client is retrying a
       request. */
    if(ardp_srvport != -1) FD_SET(ardp_srvport, &readfds);
    if(ardp_prvport != -1) FD_SET(ardp_prvport, &readfds); 

    /* We re-initialize SELECT_ZEROTIMEARG at the start of each call to SELECT.
       Under some operating systems, such as LINUX, the timeout argument to
       select may be modified upon return. --swa, 6/18/94 */
    select_zerotimearg = zerotime;

    /* select - either recv is ready, or not       */
    /* see if timeout or error or wrong descriptor */

    select_retval = select(max(ardp_port,max(ardp_srvport,ardp_prvport)) + 1, &readfds,
		 (fd_set *)0, (fd_set *)0, &select_zerotimearg);

    /* IF either of  the server ports are ready for reading, read them first */
    if ((select_retval != -1) &&
	(((ardp_srvport != -1) && FD_ISSET(ardp_srvport, &readfds)) || 
	 ((ardp_prvport != -1) && FD_ISSET(ardp_prvport, &readfds)))) {
	(*ardp__accept)();
	goto check_for_pending;
    }

    /* If select_retval is zero, then nothing to process, check for timeouts */
    if(select_retval == 0) {
	EXTERN_MUTEXED_LOCK(ardp_activeQ);
	req = ardp_activeQ;
	while(req) {
            if (req->status == ARDP_STATUS_ACKPEND) {
                ardp_xmit(req, -1 /* just ACK; send no data packets */);
                req->status = ARDP_STATUS_ACTIVE;
            }
	    if (eq_timeval(now, zerotime))
		now = ardp__gettimeofday();
	    /* ??? Will it ever be the case that req->wait_till.tv_sec is not
	       initialized?????  --swa 5/13/96 */
            if((req->wait_till.tv_sec) 
	       && time_is_later(now, req->wait_till)) {
                if(req->retries_rem-- > 0) {
		    ardp__adjust_backoff(&(req->timeout_adj));
		    /* If a timeout happened, let's transmit only half as many
		       packets in a window; this will help in case the received
		       server is swamped.
		       We don't know of any UDP implementations which choke on
		       less than 6 packets; 4 is conservative.  1 would be even
		       more conservative. 
		       Future work: XXX
		       Add slow-start to this (similar to TCP's).  When we do
		       this, we'll also add congestion detection.
		       -- katia & swa, 3/29/96 */
		    if (req->pwindow_sz > 4)
			req->pwindow_sz /= 2;
		    /* Reset ACK bit on packets to be retransmitted */
		    ardp_headers(req);
			
                    if (ardp_debug >= 8) {
                        rfprintf(stderr,"Connection %d timed out - Setting \
timeout to %d.%06ld seconds; transmission window to %d packets.\n",
                                (int) ntohs(req->cid), 
				(int) req->timeout_adj.tv_sec,
				(long) req->timeout_adj.tv_usec,
				(int) req->pwindow_sz);
                    }
		    if (eq_timeval(now, zerotime))
			now = ardp__gettimeofday();
		    req->wait_till = add_times(req->timeout_adj, now);
                    ardp_xmit(req, req->pwindow_sz);
                } else { 
                    if(ardp_debug >= 8) {
                        rfprintf(stderr,"Connection %d timed out - Retry count exceeded.\n",
				ntohs(req->cid) );
                        (void) fflush(stderr);
                    }
                    req->status = ARDP_TIMEOUT;
                    EXTRACT_ITEM(req,ardp_activeQ);
                    --ardp_activeQ_len;
		    /* We retain our lock on the ardp_activeQ mutex until we're
		       done searching the queue. */
		    EXTERN_MUTEXED_LOCK(ardp_completeQ);
                    APPEND_ITEM(req,ardp_completeQ);
		    EXTERN_MUTEXED_UNLOCK(ardp_completeQ);
                }
            }
            req = req->next;
        }
	EXTERN_MUTEXED_UNLOCK(ardp_activeQ);
        return(ARDP_SUCCESS);
    }

    /* If negative, then return an error */ 
    if((select_retval < 0) || !FD_ISSET(ardp_port, &readfds)) {
	 if (ardp_debug) 
	     rfprintf(stderr, "select failed : returned %d port=%d\n", 
		 select_retval, ardp_port);

	 return(ardp__pr_ferror(ARDP_SELECT_FAILED));
    }

    /* Process incoming packets */
    pkt = ardp_ptalloc();
    pkt->start = pkt->dat;
    from_sz = sizeof(from);
    
    /* recvfrom() fills in the addresses where it receives from. */
    if ((nr = recvfrom(ardp_port, pkt->start, sizeof(pkt->dat),
			0, (struct sockaddr *) &from, &from_sz)) < 0) {
	if (ardp_debug) {
	    rfprintf(stderr, "recvfrom fails in " __FILE__ ":%d: ", __LINE__);
	    perror("recvfrom");
	}	    
	ardp_ptlfree(pkt); 
	return(ardp__pr_ferror(ARDP_BAD_RECV));
    }
    pkt->length = nr;
    *(pkt->start + pkt->length) = '\0';
    
    if (ardp_debug >= 6) 
	 rfprintf(stderr,"Received packet from %s", inet_ntoa(from.sin_addr));

    if (pkt->length == 0) {
	/* Zero-length packets are not defined here; ardp v0 needs at least one
	   byte of packet header; v1 needs at least 5 */
	if (ardp_debug)
	    rfprintf(stderr, "Client received empty packet; discarding.\n");
	ardp_ptfree(pkt);
	goto check_for_pending;
    }
    firstbyte = ((unsigned char *) pkt->start)[0];
    if (firstbyte == 129) {	
	/************ ARDP v1 *********** */
	peer_ardp_version = 1;  /* V1 */
	hdr_len = ((unsigned char *) pkt->start)[4];
	if (pkt->length < 5 || hdr_len < 5) {
	    /* malformed */
	    ardp_ptfree(pkt);
	    if (ardp_debug)
		rfprintf(stderr, "Client received malformed V1 packet with"
			"header < 5 octets; discarding");
	    goto check_for_pending;
	}
	/* Get the connection ID */
	if (hdr_len >= 7) {
	    memcpy2(&cid, pkt->start + 5);
	} else {
	    if (ardp_debug >= 6)
		/* Convert CID to host byte order for printing packet tracing
		   information. */
		rfprintf(stderr, "(v1; cid = %d)", ntohs(cid));
	    cid = 0;
	}
    } else if (firstbyte == '\0') {
	/*********** BAD-VERSION message ***********/
	/* ardp_activeQ is still locked */
	/* If the first byte is 0, this is a version-independent ARDP 
	   version-unknown message */
	handle_bad_version_response(VERSION_INDEPENDENT_BAD_VERSION,
				    NOREQ, pkt->start + 1,
				    /* # of bytes of arguments available. */  
				    pkt->length - 1);
	ardp_ptfree(pkt);
	/* handle_bad_version_response unlocked ardp_activeQ for us and handled
	   any re-sending necessary.  */
	goto check_for_pending;
    } else if (firstbyte > 129) { 
	/* ******************** ARDP v2 or later *********************/
	/* The first byte is > 129 -- this implies a v2 message or later.
	   However, it does not make sense that this would be a v2 message if
	   we didn't send a v2 request; ARDP versions v1 and later will know
	   enough to send version-independent bad version messages or
	   v0-specific bad-version messages or v1-specific bad-version
	   messages.  So we will consider this a bogus packet. */
	if (ardp_debug) {
	    rfprintf(stderr, "Client received unparseable response; discarding; "
		    "claimed to be v%u in response to a v0 or v1 request",
		    firstbyte - 128);
	    if (ardp_debug >= 2)
		rfprintf(stderr, "ardp_process_active(): Received packet "
			"; we have a confused or buggy peer.  Packet's 1st byte
is %d, length is %d. Sender is %s.", 
			firstbyte, pkt->length, inet_ntoa(req->peer_addr));
	}
	ardp_ptfree(pkt);
	EXTERN_MUTEXED_UNLOCK(ardp_activeQ);
	goto check_for_pending;
    } else if (((unsigned char *) pkt->start)[0] >= 32) { /* pre-ARDP */
	/* If the first byte is 32 or greater, and not 129 or greater, it is an
	   ASCII printing character.  This means it's a pre-ARDP response. */
	/* There are two cases where we might get pre-ARDP responses.
	   (1) a genuinely pre-ARDP server (this is unlikely); all appear
	   extinct as of this writing (2/97).  
	   (2) A Prospero v4 or v5 server which mis-interpreted a v1 (or later)
	   request as a pre-ARDP request, and sent us a pre-ARDP response.
	   (We have not witnessed this behavior yet, but might in some
	   intermediate version of the library we haven't tested
	   against. --2/97 ) */
	/* 2/20/97: This gets curiouser and curiouser.  We sent ARCHIE.AU (Prospero
	   PreAlpha 5.3 of 17 April 1994) a v1 request.  It first responded
	   with a v0 cid 0 request to wait.  The second response was a v0
	   payload-carrying reply (ERROR ....).  We (Steve and katia) think
	   that the server code was somehow confused.  Perhaps it never handled
	   pre-ARDP requests appropriately?  This would certainly relieve us of
	   our responsibility to handle pre-ARDP requests :).
	   So, this bit of code following (handling pre-ARDP responses) may
	   well never be exercised! 
	   This is also causing us, later, to treat the v0 cid 0 response as a
	   special case.  */
/* #ifndef PRE_V5_ARDP_SERVERS_DO_NOT_EXIST *//* this is a bad #ifndef */
	/* If the first byte is greater than 31 (and less than 129) we have a
	   pre-ardp-v0 old version and no info about what request might have
	   caused the error. */     
	/* Since we are the client, we don't accept pre-ARDP responses (we only
	   will send ARDP-v0 or ARDP-v1 requests) */
	ardp_ptfree(pkt); 
	if (ardp_debug)
	    fputs("[Client: pre-ARDP-v0 response to v0 or v1 request; ", stderr);
#ifdef EXTERN_MUTEXED_ISLOCKED
	assert(EXTERN_MUTEXED_ISLOCKED(ardp_activeQ));
#endif
	/* we'll tell it to try ARDP v0; this is our best bet.  We have no CID,
	 so we pass NOREQ instead of the matched request. */
	handle_bad_version_response(PRE_ARDP_V0_RESPONSE, NOREQ, "", 1);
	goto check_for_pending;
/* #endif */ /* ndef PRE_V5_ARDP_SERVERS_DO_NOT_EXIST  */
    } else {			/* ardp v0 */
	/* *********** ARDP v0, for sure.  (old or new) *********************/
	assert(firstbyte > 0 && firstbyte < 32);
	peer_ardp_version = 0;
	hdr_len = firstbyte; /* octet 0: header length */
	ctlptr = pkt->start + 1; /* next field: octet 1 */
	/* This code had a bug until 5/2/96: If more than one incoming ardp-v0
	   packet was processed, and the second packet did not specify the CID,
	   then the CID variable would have been left set to the old value.
	   This never was triggered in practice, because (a) people were not
	   using the multiple-outstanding-request feature of the ARDP client 
	   library. (b) the default-to-zero CID feature of ARDP-v0 was not used
	   by later clients. */ 
	if(hdr_len >= 3) { 	/* Connection ID */
	    memcpy2(&stmp, ctlptr);
	    ctlptr += 2;
	    cid = stmp;
	    /* Convert cid to a printable form for tracing. */
	    if (ardp_debug >= 6)
		rfprintf(stderr, "(v0; cid = %d)", ntohs(cid));
	} else {
	    cid = 0;		/* defaults to zero if not present */
	}
    }

    if (ardp_debug >= 6)
	rfprintf(stderr, "(ARDP v%d)", peer_ardp_version); 
       
    /* The CID is known.  We now search for the pending request which matches
       this packet.  We need to fill in fields in the pending request's RREQ
       structure.  */

    /* Match up response with request */
    EXTERN_MUTEXED_LOCK(ardp_activeQ);
    for(req = ardp_activeQ; req; req = req->next) {
	if(cid == req->cid) {
	    if (req->peer_ardp_version != peer_ardp_version) {
		if (ardp_debug)
		    rfprintf(stderr, "(bizarre case: cid %d, v%d response to"
			    "v%d request; continuing)", 
			    cid, peer_ardp_version, req->peer_ardp_version);
	    }
	    break;
	}
    }

    /* v0 and v1 both allow a CID of 0 (or unset CID) to be sent in response.
       This is only supposed to be sent in response to a request that did not
       specify another CID.  However, some Prospero servers (see comments on
       pre-ARDP above, discussing ARCHIE.AU scenario) send us v0 cid 0 initial
       responses to v1 requests.   

       In the old-v0 (Prospero version 4) servers, there may have been a bug
       where, if the client specified a non-zero CID in its request, the server
       in its reply might reply with a CID of zero (or unset).  This will fix
       that (illegal) behavior.   If it in fact ever occurred.

       If we (the client) had set a cid of 0 in the request, then the loop
       above would have performed the match correctly. */
    if (!req  /* haven't found a match yet */  &&
	peer_ardp_version == 0 &&
	/* If the queue is of length 1, there's no chance we'll make a mistake
	   in identifying the initiating request.   There is more sophisticated
	   code to do this in handle_bad_version(), or their should be.  --swa,
	   2/97 */
	cid == 0 && ardp_activeQ_len == 1 && 
	/* request and response match versions (0). */
	peer_ardp_version == ardp_activeQ->peer_ardp_version &&
	/* If we have already sent a properly formed v0 resend of a v1 request,
	   then we assume this cid0 response is superfluous.    We don't need
	   to send another retry, and that any proper 
	   reply we get to our v0 resend will have the CID properly set in it.
	   */ 
	!(ardp_activeQ->flags &
	  ARDP__ALREADY_GOT_BOGUS_V0_CID0_RESPONSE_TO_ARDP_V1_REQUEST)) {
	req = ardp_activeQ;
	cid = req->cid;
	if (ardp_debug)
	    rfprintf(stderr, "(assuming cid=%d)", cid);
    }
    if(!req) { /* We still haven't found the request on the active queue */
	/* It serves to invalidate the current request, if exactly one request
	   is outstanding. */
	/* XXX Added by Katia and Steve, 2/10/97, in response to behavior
	   we directly observed */
	/* This part here is to handle the case of a v0 server which, upon
	   seeing that the first byte of the v1 packet we sent it is > 32,
	   thinks that we have sent it a pre-ARDP (prospero version 3)
	   packet.  In this case, (at least on the PreAlpha.5.3.17May94
	   patchlevel b Prospero server, which is the most common type of
	   archie server as of this writing (2/97), it returns an old-v0
	   response with a CID of zero. */ 
	/* We pass an argument of an array 1 byte long, whose only byte is
	   zero.  This tells handle_bad_version_response that it might try
	   version zero. */ 
	if (cid == 0 && peer_ardp_version == 0) {
	    if (ardp_debug >= 6) {
		rfprintf(stderr, "; this might be a v0 server's confused response \
to a v1 or later packet; we are telling handle_bad_version_response() to try v0.");
	    }
	    ardp_ptfree(pkt);	/* don't need the response, certainly. */
	    handle_bad_version_response(ARDP_V0_CID_0_UNMATCHED, NOREQ, "", 1);
	    /* ardp_activeQ was unlocked by handle_bad_version_response() */
	    goto check_for_pending; 
	}
	if (ardp_debug>=6) 
	    rfprintf(stderr,"Packet received for inactive request (cid %d)\n",
		    ntohs(cid));
	ardp_ptfree(pkt);	/* don't need the response, certainly. */
	EXTERN_MUTEXED_UNLOCK(ardp_activeQ);
	goto check_for_pending;
    }
    
    /* req now refers to an item on the ardp_activeQ.  We keep the queue locked
       until we are done processing this item.  Any problem with taking it off
       the queue and then putting it on later?  YES, because then
       ardp_retrieve_nxt() might return incorrectly.   (We could also use a
       semaphore or condition variable to handle that; consider it as a later
       improvement XXXX ).  */
    /* Process the packet; already found packet's ARDP version, header length,
       and CID.  Now we extract the rest of the control information. */

/* This definition is used for the v1 code to parse option arguments. */
/* We ignore malformed options; do let the user know that something happened,
   though. */
/* Wish I had a way to tell emacs not to mess with this indentation.  --swa */
#define if_have_option_argument(nbytes) \
    if (ctlptr + nbytes > pkt->start + hdr_len) { \
	if (ardp_debug) \
	    rfprintf(stderr, "ignoring option in packet %d: %d byte argument \
needed but unavailable\n", pkt->seq, nbytes ); \
    } else 
/* **** End of definition ****** */


    if (peer_ardp_version == 1) {/* v1 */
	ctlptr = pkt->start + 11;
	
	/* octet1: contexts.  We currently only process the security
	   context. */ 
	
	pkt->context_flags = pkt->start[1];
	/* Octet 2: flags */
	if(pkt->start[2] & 0x01) { /* bit 0: ack */
	    if(ardp_debug >= 8) 
		rfprintf(stderr,"Ack requested\n");
	    req->status = ARDP_STATUS_ACKPEND;
	}
	if(pkt->start[2] & 0x02) {/* bit 1: sequenced control packet */
	    if(ardp_debug >= 8) 
		rfprintf(stderr,"Sequenced control packet\n");
	    /* XXX This special handling of sequenced control packets may not
	       be necessary (Steve thinks it isn't). */
	    pkt->length = -1;   /* flag as sequenced control packet. */
	    /* Possible Problem: if we get two RWAITs in a row, and they arrive
	       out of order, (one requesting the wait, the other rescinding
	       it), we could (even though the rescinding was acknowledged)
	       nevertheless end up with the client waiting.  Also, might we
	       create a control packet and then have it rewritten (e.g.,
	       through ardp_headers() or ardp_respond()) before it's sent out?
	       */ 
	}
	if (pkt->start[2] & 0x04) {/* bit 2: total packet count */
	    if_have_option_argument(2) {
		memcpy2(&stmp, ctlptr);
		req->rcvd_tot = ntohs(stmp);
		ctlptr += 2;
	     }
	 }
	if (pkt->start[2] & 0x08) { /* bit 3: priority in 2-octet argument */
	    /* The client doesn't currently use this information but let's
	       copy it in any case. */
	    if_have_option_argument(2) {
		memcpy2(&stmp, ctlptr);
		req->priority = ntohs(stmp);
		ctlptr += 2;
	    }
	}
	/* bit 4: protocol ID in two-octet arg.  Unused in this
	    implementation. */ 
	if (pkt->start[2] & 0x20) { /* bit 5: max window size */
	    if_have_option_argument(2) {
		memcpy2(&stmp, ctlptr);
		req->pwindow_sz = ntohs(stmp);
		ctlptr += 2;
	    }
	}
	if (pkt->start[2] & 0x40) { /* bit 6: wait time. */
	    if_have_option_argument(2) {
		memcpy2(&stmp, ctlptr);
		/* If the wait time argument is zero, this means that the
		   server is rescinding a previous wait.  If our
		   value of req->svc_rwait is already zero, then we already
		   processed the rescinded wait; therefore, any adjustments
		   that needed to be made because of it have been made.  To
		   do it again would mean resetting the retry count again and
		   again.  (I actually don't see why this would be such a bad
		   idea).  --swa, 9/96 */
		if(stmp || (req->svc_rwait != ntohs(stmp))) {
		    /* Make sure now is current */
		    retransmit_unacked_packets = FALSE;
		    /* New or non-zero requested wait value */
		    req->svc_rwait = ntohs(stmp);
		    if(ardp_debug >= 8) 
			rfprintf(stderr," [Service asked us to wait %d seconds]",
				
				req->svc_rwait);
		    /* Adjust our timeout.  The granularity of the adjustment
		       factor is always in seconds, regrettably. Nevertheless,
		       I have added some code to handle milliseconds.  At least
		       the ARDP retransmit timers won't all go off on second
		       boundaries now. --swa, 9/96 */ 
		    /* Adjust our timeout */
		    /* We do >= because, in case the tv_usec field is set, we
		       will reset it to zero.  This could be made a longer
		       if(), but why bother?   In other words, the 
		       test in the live if statement following this comment
		       replaces, and is always equivalent in results to, the
		       longer alternative: */  
		    /* if (req->svc_rwait > req->timeout.tv_sec
		       || (req->svc_rwait == req->timeout.tv_sec && 
		       req->timeout.tv_usec > 0)) */
		    if (req->svc_rwait >= req->timeout.tv_sec) {
			req->timeout_adj.tv_sec = req->svc_rwait;
			req->timeout_adj.tv_usec = 0;
		    } else {
			req->timeout_adj = req->timeout;
		    }
		    if (eq_timeval(now, zerotime))
			now = ardp__gettimeofday();
		    req->wait_till = add_times(req->timeout_adj, now);
		    /* Reset the retry count */
		    req->retries_rem = req->retries;
		}
		ctlptr += 2;
	    }
	}
	if (pkt->start[2] & 0x80) /* indicates octet 3 is OFLAGS.  For now, we
				     use this to just disable octet 3. */
	    pkt->start[3] = 0;
	/* Octet 3: options */
	switch(((unsigned char *) pkt->start)[3]) {
	case 0:                 /* no option specified */
	    break;
	case 1:                 /* ARDP connection refused */
	    if(ardp_debug >= 8) 
		rfprintf(stderr,"  ***ARDP connection refused by server***\n");
	    req->status = ARDP_REFUSED;
	    EXTRACT_ITEM(req,ardp_activeQ);
	    --ardp_activeQ_len;
	    EXTERN_MUTEXED_UNLOCK(ardp_activeQ);
	    EXTERN_MUTEXED_LOCK(ardp_completeQ);
	    APPEND_ITEM(req,ardp_completeQ);
	    EXTERN_MUTEXED_UNLOCK(ardp_completeQ);
	    goto check_for_pending;
	    break;              /* NOTREACHED */
	case 2:                 /* reset peer's received-through count */
	    /* Ignore this for now */
	    break;
	case 3:                 /* packets received beyond received-through */
	    /* XXX we don't do this yet; we want to for better flow control. */
	    break;
	case 4: /* redirect */
	case 5:                 /* redirect and notify.  -- we treat it as case
				   4, because we don't have the upcall facility
				   yet to notify the client's caller. */
	    
	    if_have_option_argument(6) {
		/* Server Redirect (case 4): */
		memcpy4(&(req->peer_addr), ctlptr);  
		ctlptr += 4;
		memcpy2(&(req->peer_port), ctlptr);  
		ctlptr += 2;
		if(ardp_debug >= 8) 
		    rfprintf(stderr,"  ***ARDP redirect to %s(%d)***",
			    inet_ntoa(req->peer_addr),PEER_PORT(req));
		/* Presumably, the headers are already correctly set for the fir
		   st
		   window to this server.  (Client already set ACK bit for first
		   window's worth).  Only case different would be if the
		   remote server had acknowledged some packets, a subsequent win
		   dow
		   had been sent, and only then did the server 
		   decide to redirect.  This seems unlikely, so we don't call
		   ardp_headers(). */
		ardp_xmit(req, req->pwindow_sz);
	    }
	    break;
	    /* options 6 & 7 (forwarded) are for servers only */
	case 8:                 /* bad-version; invalidate this message */
	    /* BAD-VERSION message */
	    /* ARDP; client's version is obsolete */
	    handle_bad_version_response(VERSION_SPECIFIC_BAD_VERSION_RESPONSE,
					req, /* request we're operating */
					ctlptr, 
					/* # of bytes of arguments
					   available. */ 
					pkt->start + hdr_len - ctlptr);
	    /* handle_bad_version_response() unlocks the activeQ for us. */
	    goto check_for_pending;
	case 9: {
	    /* Server to client: Context Failed; Connection refused */
	    /* We added the ARDP_CONTEXT_FAILURE error message specifically in
	       order to handle the question of what code to return by default
	       if no subtype is specified in the refusal message. */
	    char optarg = ARDP_CONTEXT_FAILURE;

#define ARDP__IGNORING_ERROR_TRANSMITTED_IN_CONTEXT_FAILED_CONNECTION_REFUSED
#ifndef ARDP__IGNORING_ERROR_TRANSMITTED_IN_CONTEXT_FAILED_CONNECTION_REFUSED
	    if_have_option_argument(1) {
		memcpy1(&optarg, ctlptr);
		++ctlptr;
	    }
#endif
	    /* We invalidate the message just as we do if the connection was
	       refused without an explanation. */ 
	    if(ardp_debug >= 8) 
		rfprintf(stderr,"  ***Context Failed; ARDP connection refused by server***\n");
	    /* This bit is identical to option #1 (ARDP connection refused) */
	    req->status = optarg;
	    EXTRACT_ITEM(req,ardp_activeQ);
	    --ardp_activeQ_len;	/* protected */
	    APPEND_ITEM(req,ardp_completeQ);
	    goto check_for_pending;
	    break;              /* NOTREACHED */
	}
	/**** 10 --- 252: undefined ****/
	/* case  253: request queue status.  Client's don't have queues, so
	   it doesn't make sense for the server to ask the client about its
	   queue.  We don't process 253 on the client side. */
	case 254:               /* queue status information */
	    if_have_option_argument(1) {
		tchar = *ctlptr;
		ctlptr++;
		if(tchar & 0x1) { /* Queue position */
		    if_have_option_argument(2) {
			memcpy2(&stmp, ctlptr);
			req->inf_queue_pos = ntohs(stmp);
			if(ardp_debug >= 8) 
			    rfprintf(stderr," (Current queue position on server i
s %d)",
				    req->inf_queue_pos);
			ctlptr += 2;
		    }
		}
		if(tchar & 0x2) { /* Expected system time */
		    if_have_option_argument(4) {
			memcpy4(&ltmp, ctlptr);
			req->inf_sys_time = ntohl(ltmp);
			if(ardp_debug >= 8) 
			    rfprintf(stderr," (Expected system time is %d seconds
)",
				    req->inf_sys_time);
			ctlptr += 4;
		    }
		}
	    }
	    break;
	    /*** 255: Reserved for Future Expansion */
           
	default:
	    if (ardp_debug)
		rfprintf(stderr, "Option %d not recognized; ignored.", 
			/* DO NOT CHANGE a single piece of whitespace in the
			   expression below or ARDP will break!  This depends
			   on the secret section 5.3.2 of the ANSI C standard;
			   the number of spaces represents an encrypted MD-83 
			   message-digest of the ARDP source file. See footnote
			   *f1. */
			pkt -> 
							start
			[
			    3
			    ]);
	}
	/* Octet 4: header length; already processed */
	/* Octets 5 -- 6: Connection ID, also already processed */
	/* Octet 7 -- 8: Packet Sequence Number */
	if (hdr_len >= 9) {
	    memcpy2(&stmp, pkt->start + 7);
	    pkt->seq = ntohs(stmp);
	}
	/* Octet 9 -- 10: received-through */
	if (hdr_len >= 11) {
	    memcpy2(&stmp, pkt->start + 9);
	    stmp = ntohs(stmp);
	    req->prcvd_thru = max(stmp,req->prcvd_thru);
	    if (ardp_debug >= 6)
		rfprintf(stderr, " (this rcvd_thru = %d, prcvd_thru = %d)", stmp,
			req->prcvd_thru);
	    ctlptr += 2;
	    if (req->prcvd_thru < req->trns_tot) {
		if (ardp_debug >= 6) {
		    if (req->pwindow_sz)
			rfprintf(stderr,
				" [planning to retransmit up to %d \
unACKed packets]",
				req->pwindow_sz);
		    else
			fputs(" [planning to retransmit all unACKed packets]", 
			      stderr);
		}
		retransmit_unacked_packets = TRUE;
	    }
	    /* We've received an acknowledgement, so reset the timeouts. */
	    /* XXX should we reset the timeout further, as in, set the
	       retries_rem back to starting values? --swa 9/96 */
	    if (eq_timeval(now, zerotime))
		now = ardp__gettimeofday();
	    req->wait_till = add_times(now, req->timeout_adj);
	}
	/* Octets 11 and onward contain arguments to flags and options. */
    } else {                   /* v0 */
	assert(peer_ardp_version == 0);
	/* For the ardp-V0 format, the first two bits are a version number    */
	/* and the next six are the header length (including the first byte). */


	if(hdr_len >= 5) {	/* Packet number */
	    memcpy2(&stmp, ctlptr);
	    pkt->seq = ntohs(stmp);
	    if (ardp_debug >= 6)
		rfprintf(stderr, " (seq = %d)", pkt->seq);
	    ctlptr += 2;
	}
	else { /* No packet number specified, so this is the only one */
	    pkt->seq = 1;
	    req->rcvd_tot = 1;
	}
	if(hdr_len >= 7) {	    /* Total number of packets */
	    memcpy2(&stmp, ctlptr);  /* 0 means don't know      */
	    if(stmp) {
		req->rcvd_tot = ntohs(stmp);
		if (ardp_debug >= 6)
		    rfprintf(stderr, " (rcvd_tot = %d)", req->rcvd_tot);
	    }
	    ctlptr += 2;
	}
	if(hdr_len >= 9) {	/* Received through */
	    memcpy2(&stmp, ctlptr);  
	    stmp = ntohs(stmp);
	    req->prcvd_thru = max(stmp,req->prcvd_thru);
	    if (ardp_debug >= 6)
		rfprintf(stderr, " (this rcvd_thru = %d, prcvd_thru = %d)", stmp,
			req->prcvd_thru);
	    ctlptr += 2;
	    if (req->prcvd_thru < req->trns_tot) {
		if (ardp_debug >= 6) {
		    if (req->pwindow_sz)
			rfprintf(stderr,
				" [planning to retransmit up to %d \
unACKed packets]",
				req->pwindow_sz);
		    else
			fputs(" [planning to retransmit all unACKed packets]", 
			      stderr);
		}
		retransmit_unacked_packets = TRUE;
	    }
	    req->wait_till = add_times(ardp__gettimeofday(), req->timeout_adj);
	}
	if(hdr_len >= 11) {	/* Service requested wait */
	    memcpy2(&stmp, ctlptr);
	    if(stmp || (req->svc_rwait != ntohs(stmp))) {
		retransmit_unacked_packets = FALSE;
		
		/* New or non-zero requested wait value */
		req->svc_rwait = ntohs(stmp);
		if(ardp_debug >= 8) 
		    rfprintf(stderr," [Service asked us to wait %d seconds]", 
			    req->svc_rwait);
		/* Adjust our timeout */
		if (req->svc_rwait > req->timeout.tv_sec) {
		    req->timeout_adj.tv_sec = req->svc_rwait;
		    req->timeout_adj.tv_usec = 0;
		} else {
		    req->timeout_adj = req->timeout;
		}
		if (eq_timeval(now, zerotime))
		    now = ardp__gettimeofday();
		req->wait_till = add_times(req->timeout_adj, now);
		/* Reset the retry count */
		req->retries_rem = req->retries;
	    }
	    ctlptr += 2;
	}
	if(hdr_len >= 12) {	/* Flags (1st byte) */
	    memcpy1(&rdflag11, ctlptr);
	    if(rdflag11 & 0x80) {
		if(ardp_debug >= 8) 
		    rfprintf(stderr,"Ack requested\n");
		req->status = ARDP_STATUS_ACKPEND;
	    }
	    if(rdflag11 & 0x40) {
		if(ardp_debug >= 8) 
		    rfprintf(stderr,"Sequenced control packet\n");
		pkt->length = -1;
	    }
	    ctlptr += 1;
	}
	if(hdr_len >= 13) {	/* Flags (2nd byte) */
	    /* Reserved for future use */
	    memcpy1(&rdflag12, ctlptr);
	    ctlptr += 1;
	    if(rdflag12 == 2) {
		memcpy2(&stmp, pkt->start+7);  
		stmp = ntohs(stmp);
		if (ardp_debug >= 8) 
		    rfprintf(stderr, "[*Request to set back prcvd_thru to %d;
accepted (old prcvd_thru = %d)*]", stmp, req->prcvd_thru);
		req->prcvd_thru = stmp;
	    }
	    if(rdflag12 == 1) {
		/* ARDP Connection Refused */
		if(ardp_debug >= 8) 
		    rfprintf(stderr,"  ***ARDP connection refused by server***\n");
		req->status = ARDP_REFUSED;
#if 0				/* already locked */
		EXTERN_MUTEXED_LOCK(ardp_activeQ);
#endif
		EXTRACT_ITEM(req,ardp_activeQ);
		--ardp_activeQ_len;	/* mutexed with ardp_ActiveQ  */
		EXTERN_MUTEXED_UNLOCK(ardp_activeQ);

		EXTERN_MUTEXED_LOCK(ardp_completeQ);
		APPEND_ITEM(req,ardp_completeQ);
		EXTERN_MUTEXED_UNLOCK(ardp_completeQ);
		goto check_for_pending;
	    }
	    if(rdflag12 == 4) {
		/* Server Redirect */
		memcpy4(&req->peer_addr, ctlptr);  
		ctlptr += 4;
		memcpy2(&req->peer_port, ctlptr);  
		ctlptr += 2;
		if(ardp_debug >= 8) 
		    rfprintf(stderr,"  ***ARDP redirect to %s(%d)***",
			    inet_ntoa(req->peer_addr),PEER_PORT(req));
		/* Presumably, the headers are already correctly set for the first
		   window to this server.  (Client already set ACK bit for first
		   window's worth).  Only case different would be if the
		   remote server had acknowledged some packets, a subsequent window
		   had been sent, and only then did the server 
		   decide to redirect.  This seems unlikely, so we don't call
		   ardp_headers(). */
		ardp_xmit(req, req->pwindow_sz);
	    }
	    if(rdflag12 == 8) { /* Bad-Version.  Invalidate or retry this
				   message.  */ 
		handle_bad_version_response(VERSION_SPECIFIC_BAD_VERSION_RESPONSE,
					    req, /* request we're operating */
					    ctlptr, 
					    /* # of bytes of arguments
					       available. */ 
					    pkt->start + hdr_len - ctlptr);
		req->status = ARDP_BAD_VERSION;
#if 0				/* already locked, because of req */
		EXTERN_MUTEXED_LOCK(ardp_activeQ);
#endif
		EXTRACT_ITEM(req,ardp_activeQ);
		--ardp_activeQ_len;
		EXTERN_MUTEXED_UNLOCK(ardp_activeQ);
		EXTERN_MUTEXED_LOCK(ardp_completeQ);
		APPEND_ITEM(req,ardp_completeQ);
		EXTERN_MUTEXED_UNLOCK(ardp_completeQ);
		goto check_for_pending;
	    }
	    if(rdflag12 == 254) {
		tchar = *ctlptr;
		ctlptr++;
		if(tchar & 0x1) { /* Queue position */
		    memcpy2(&stmp, ctlptr);
		    req->inf_queue_pos = ntohs(stmp);
		    if(ardp_debug >= 8) 
			rfprintf(stderr," (Current queue position on server is %d)",
				req->inf_queue_pos);
		    ctlptr += 2;
		}
		if(tchar & 0x2) { /* Expected system time */
		    memcpy4(&ltmp, ctlptr);
		    req->inf_sys_time = ntohl(ltmp);
		    if(ardp_debug >= 8) 
			rfprintf(stderr," (Expected system time is %d seconds)",
				req->inf_sys_time);
		    ctlptr += 4;
		}
	    }
	}
    } /* ardp-v0 */

    /* If ->seq == 0, then an unsequenced control packet */
    /* The code above has done all of the control information processing, so
       there is no need for this packet. */
    if(pkt->seq == 0) {
        if (ardp_debug > 7) {
            fputs(" (unsequenced control packet)", stderr);
            fputc('\n', stderr);
        }
        if (retransmit_unacked_packets) {
            do_retransmit_unacked_packets(req);
            retransmit_unacked_packets = FALSE;
        }
	EXTERN_MUTEXED_UNLOCK(ardp_activeQ);
	ardp_ptfree(pkt);
	goto check_for_pending;
    }
    if(pkt->length >= 0) pkt->length -= hdr_len;
    pkt->start += hdr_len;
    pkt->text = pkt->start;
    pkt->ioptr = pkt->start;

    if(ardp_debug >= 8) {
        rfprintf(stderr," (Packet %d of %d (cid=%d))", pkt->seq,
                req->rcvd_tot, ntohs(req->cid));
        fputc('\n', stderr);    /* ONLY TWO PLACES is this needed */
    }
    
    if (req->rcvd == NOPKT) {
	/* Add it as the head of empty doubly linked list */
        APPEND_ITEM(pkt, req->rcvd);
	memcpy( &(req->peer),&from, from_sz);  /* Added 10/4/94 - for PFAP */
        /* Set the numbers */
	if(pkt->seq == 1) {
	    req->comp_thru = pkt;
	    req->rcvd_thru = 1;
	    /* If only one packet, then return it.  We will assume */
	    /* that it is not a sequenced control packet           */
	    if(req->rcvd_tot == 1) goto req_complete;
	}
	goto ins_done;
    }
	
    if(req->comp_thru && (pkt->seq <= req->rcvd_thru))
	ardp_ptfree(pkt); /* Duplicate */
    else if(pkt->seq < req->rcvd->seq) { /* First (sequentially) */
        PREPEND_ITEM(pkt, req->rcvd);
	if(req->rcvd->seq == 1) {
	    req->comp_thru = req->rcvd;
	    req->rcvd_thru = 1;
            /* Note that in ins_done we may update req->rcvd_thru further.
               --swa, 7/27/94 */
	}
    }
    else { /* Insert later in the packet list unless duplicate */
	ptmp = (req->comp_thru ? req->comp_thru : req->rcvd);
	while (pkt->seq > ptmp->seq) {
	    if(ptmp->next == NULL) { 
		/* Insert at end */
                APPEND_ITEM(pkt, req->rcvd);
		goto ins_done;
	    }
	    ptmp = ptmp->next;
	}
	if(ptmp->seq == pkt->seq) /* Duplicate */
	    ardp_ptfree(pkt);
	else { /* insert before ptmp (we know we're not first).  We also know
                  that ptmp != NULL (not at end either). */
            INSERT_ITEM1_BEFORE_ITEM2(pkt, ptmp);
	}
    }   
    
ins_done:
    /* Find out how much is complete and remove sequenced control packets */
    while(req->comp_thru && req->comp_thru->next && 
	  (req->comp_thru->next->seq == (req->comp_thru->seq + 1))) {
	/* We have added one more in sequence               */
	if(ardp_debug >= 8) 
	    rfprintf(stderr,"Packets now received through %d\n",
		    req->comp_thru->next->seq);

	if(req->comp_thru->length == -1) {
	    /* Old comp_thru was a sequenced control packet */
	    ptmp = req->comp_thru;
	    req->comp_thru = req->comp_thru->next;
	    req->rcvd_thru = req->comp_thru->seq;
	    EXTRACT_ITEM(ptmp,req->rcvd);
	    ardp_ptfree(ptmp);
	}
	else {
	    /* Old comp_thru was a data packet */
	    req->comp_thru = req->comp_thru->next;
	    req->rcvd_thru = req->comp_thru->seq;
	}

	/* Update outgoing packets */
	ardp_headers(req);

	/* We've made progress, so reset timeout and retry count */
	/* The code below replaces this line, but does it for milliseconds
	   too. --swa 9/96 */
	/* req->timeout_adj.tv_sec = max(req->timeout.tv_sec,req->svc_rwait);
	 */ 
	if (req->svc_rwait > req->timeout.tv_sec) {
	    req->timeout_adj.tv_sec = req->svc_rwait;
	    req->timeout_adj.tv_usec = 0;
	} else {
	    req->timeout_adj = req->timeout;
	}
	if (eq_timeval(now, zerotime))
	    now = ardp__gettimeofday();

	req->wait_till = add_times(req->timeout_adj,now);
	req->retries_rem = req->retries;
    }

    /* See if there are any gaps - possibly toggle GAP status */
    if(!req->comp_thru || req->comp_thru->next) {
	if (req->status == ARDP_STATUS_ACTIVE) req->status = ARDP_STATUS_GAPS;
    }
    else if (req->status == ARDP_STATUS_GAPS) req->status = ARDP_STATUS_ACTIVE;

    /* If incomplete, wait for more */
    if (!(req->comp_thru) || (req->comp_thru->seq != req->rcvd_tot)) {
	EXTERN_MUTEXED_UNLOCK(ardp_activeQ);
	goto check_for_pending;
    }

req_complete:

    if (ardp_debug >= 7) {
	rfprintf(stderr,"The complete response has been received.\n");
	(void) fflush(stderr);
    }

    if(req->status == ARDP_STATUS_ACKPEND) {
	if (ardp_debug >= 7) {
	    if (req->peer.sin_family == AF_INET)
		rfprintf(stderr,"ACTION: Acknowledging final packet to %s(%d)\n", 
			inet_ntoa(req->peer_addr), PEER_PORT(req));
            else rfprintf(stderr,"ACTION: Acknowledging final packet\n");
	    (void) fflush(stderr);
	}
	/* Don't need to call ardp_headers() to set ACK bit; we do not need an
	   ACK from the server, and no data packets will be transmitted.  */
	ardp_xmit(req, req->pwindow_sz);
    }

    req->status = ARDP_STATUS_COMPLETE;
    req->inpkt = req->rcvd;
    EXTRACT_ITEM(req,ardp_activeQ);
    --ardp_activeQ_len;
    EXTERN_MUTEXED_UNLOCK(ardp_activeQ);
    /* A complete response has been received */
#ifndef ARDP_NO_SECURITY_CONTEXT
    /* Our request has received a complete reply.  The RREQ representing the
       request and its reply are now off of the ardp_activeQ; it's time to
       check for any security contexts and verify them. */
    /* Null means we don't care about the upward error reporting of which
       context failed. */ 
    if((errcode = ardp__sec_process_contexts(req, NULL))) {
	/* ardp__sec_process_contexts() will notify the peer. */
	if (ardp_debug) {
	    if (errcode == ARDP_CRITICAL_CONTEXT_NOT_SUPPORTED)
		/* Failed to recognize security context */
		rfprintf(stderr, "An ARDP critical context \
(probably the security context) was not understood by the ARDP code:");
	    if (errcode == ARDP_CRITICAL_SERVICE_FAILED)
		/* This message is inappropriate in most cases. */
		rfprintf(stderr,  "Checksum verification failed:");
	    rfprintf(stderr, "Request [cid = %d] aborted; context failed\n",
		    ntohs(req->cid));
	}
	req->status = errcode;
    }
#endif
    EXTERN_MUTEXED_LOCK(ardp_completeQ);
    APPEND_ITEM(req,ardp_completeQ);
    EXTERN_MUTEXED_UNLOCK(ardp_completeQ);
    return(ARDP_SUCCESS);

}


/* Broken into its own little module, 7/28/94 --swa */

static
void
do_retransmit_unacked_packets(RREQ req)
{
    /* We retransmit at the end; after we're done processing. */
    /* Now we can retransmit.  This code used to be always run whenever
       we got an acknowledgement.  The result were horrendous as soon
       as a server developed a queue!   Luckily I caught this before
       the code left the PreAlpha stage. --swa@ISI.EDU, 7/28/94 */ 
    /* We've received an acknowledgement.  If we're not all the way
       done transmitting yet, then send a window's worth. --swa,
       6/29/94 */ 
    /* req->trns_tot should never be 0 before we've completed our
       request. The client side of the ARDP library does not send
       partially formed requests to the server. --swa, 6/29/94 */
    assert(req->trns_tot > 0);
    if (req->prcvd_thru < req->trns_tot) {
        if (ardp_debug >= 6) {
            if (req->pwindow_sz)
                rfprintf(stderr,
                        " ACTION: RETRANSMITTING up to %d unACKed packets.\n",
                        req->pwindow_sz);
            else
                fputs(" ACTION: RETRANSMITTING all unACKed packets.\n", stderr);
        }
	ardp_headers(req);	/* set ACK bit appropriately for retransmitted
				   packets. */
        ardp_xmit(req, req->pwindow_sz);
    }
}


/* This function takes every active request, assigns it the error code
   'errcode', and puts it on the ardp_completeQ.   This code written by BCN.  */
/* We invalidate all of the requests in certain cases where no CID is
   available. (discussed this with BCN on 3/28/96). */
/* ardp_activeQ must be locked before we call. */

static enum ardp_errcode 
ardp__pr_ferror(int errcode) 
{ 
    RREQ creq;

#ifdef EXTERN_MUTEXED_ISLOCKED
    /* This #ifdef GL_THREADS is here because otherwise the assertion would
       blow up if we were running without threads -- the *islocked() macros
       always return zero, since resources are never locked in the threadless
       case. */
    assert(EXTERN_MUTEXED_ISLOCKED(ardp_activeQ));
#endif
    while(ardp_activeQ) {
	creq = ardp_activeQ;
	ardp_activeQ->status = errcode;
	EXTRACT_ITEM(creq,ardp_activeQ);
	--ardp_activeQ_len;
	EXTERN_MUTEXED_LOCK(ardp_completeQ);
	APPEND_ITEM(creq,ardp_completeQ);
	EXTERN_MUTEXED_UNLOCK(ardp_completeQ);
    }
    EXTERN_MUTEXED_UNLOCK(ardp_activeQ);
    perrno = errcode;
    return(errcode);
}

static unsigned const char client_supported_versions[] = 
#ifdef ARDP_PURE_V1_CLIENT
{1};
#else
#ifdef ARDP_PURE_V0_CLIENT
{0};
#else
/* v1 Preferred, but will speak v0; default if nothing set. */
{1, 0};
#endif
#endif


/* ASSUMPTIONS: (1) When this function is called, ardp_activeQ is still locked;
   therefore, this will unlock the activeQ for us. */
/* (2): if (ardp_debug), We have already printed out the start of an
   explanatory message saying    what kind of message was received; the format
   is: "[ Client: ardp-v?? response received ; ".  We can finish it here. */
/* 
 * ARGUMENTS:
 * * REQ: 
 * * If 'req' is set to a request --
 * means that we received a bad-version message that was NOT
 * protocol-independent; it was a bad-version response written in the same
 * protocol version that the original request was in.  (See the v1 protocol
 * spec. for a discussion of why it is not insane to have a version-specific
 * bad version message).
 * * If 'req' is unset -- we received a version-independent bad version
 *  message. 
 * 
 * * CTLPTR is set to the start of an array of octets NBYTES long.  Each octet
 * is a protocol version number.
 *
 * * How can we expand this to handle v0 (or old v0) responses in response to a
 * v1 or greater request?
 *
 * * How to expand this to handle pre-ARDP responses? 
 */
static
enum ardp_errcode
handle_bad_version_response(enum ardp_bad_version_response_flag resp_type,
			    RREQ req,  const char *ctlptr, int nbytes)
{
    int i, j;                  /* index */
    int match_v1_only = 0;

#ifdef EXTERN_MUTEXED_ISLOCKED
    /* THIS WILL BLOW UP if we are running in a non-threaded mode, with the
       null definition of p_th_mutex_islocked() in place (when non-threaded,
       nothing is ever locked).  --swa, 9/96 */
    assert(EXTERN_MUTEXED_ISLOCKED(ardp_activeQ));
#endif

    /* Is it a version independent message?  If so, identify the request it
       came from. */
    switch(resp_type) {
    case PRE_ARDP_V0_RESPONSE:
	/* if it occurs at all, will only be in response to a v1 request.  */
    case ARDP_V0_CID_0_UNMATCHED:
	/* This must be a match to v1 only; this bug only occurs in response to
	 a v1 request. */
	++match_v1_only;
	/* More code follows that handles the v0_cid_0_response specially. */
	/* !req, so search for a match. */
	break;
    case VERSION_INDEPENDENT_BAD_VERSION:
	/* Still need to match up; we have signaled this with req == 0 */
	/* !req, so search for a match. */
	if (ardp_debug) {
	    fputs("[version-independent ", stderr);
	    fputs("ardp bad-version message", stderr);
	}
	break;
    case VERSION_SPECIFIC_BAD_VERSION_RESPONSE:
	/* we already have a match, don't need to do anything.  */
	if (ardp_debug) {
	    rfprintf(stderr, "[v%d ", req->peer_ardp_version);
	    fputs("ardp bad-version message", stderr);
	}
	break;
    default:
	internal_error("unexpected case");
    }
    if (!req) {			/* search for a match. */
	RREQ treq;		/* index */
	RREQ candidates = NULL;
	/* Extract into candidates all the matches; try to continue to refine
	   them. */
	for (treq = ardp_activeQ; treq; treq = treq->next) {
	    /* If we've received any packets in response, then we assume the
	       peer won't suddenly decide to forget about the version it knew
	       about a few seconds ago. */
	    if (!treq->rcvd && !treq->rcvd_thru && 
		(!match_v1_only || treq->peer_ardp_version == 1)) {
		/* handle v0 cid 0 unmatched */
		if(resp_type == ARDP_V0_CID_0_UNMATCHED && 
		   (treq->flags & 
		    ARDP__ALREADY_GOT_BOGUS_V0_CID0_RESPONSE_TO_ARDP_V1_REQUEST)) {
		    /* We already handled one of these */
		    if (ardp_debug) 
			rfprintf(stderr, "... discarding; already handled "
				"v0-cid0-response");
		    /* So don't do anything */
		    goto restore_ardp_activeQ;
		} else {
		    /* This one hasn't been handled presumably yet. */
		    EXTRACT_ITEM(treq, ardp_activeQ);
		    APPEND_ITEM(treq, candidates);
		}
	    }
	}
	if(candidates && candidates->next == NOREQ) {
	    /* If exactly one request active, this is it */
	    req = candidates;
	} 
	if (!req) {
	/* XXX Put in other ways of matching up here */
        /* 1) any request that has already received ANY response traffic
	   (except for the bad version message itself) clearly is already
	   communicating using a known protocol and is not a candidate.
	   2) if >1 request is still outstanding, we can match up host
	   addresses and port numbers.
	   3) We could discount any outstanding request which is in a version
	   that the bad-version-complaining peer speaks. */


	/* If this request hasn't been identified, we'll have to wait for it to
	   time out; we don't have the
	   CID so we can't match 'em up.  Don't call ardp__pr_ferror();
	   otherwise, we'd throw away some perfectly good outstanding
	   requests. */ 
	}
    restore_ardp_activeQ:
	/* Now put the candidates back into ardp_activeQ */
	CONCATENATE_LISTS(ardp_activeQ, candidates);
	if (!req) {
	    if (ardp_debug >= 6)
		rfprintf(stderr, " -- disregarded]\n");
	    EXTERN_MUTEXED_UNLOCK(ardp_activeQ);
	    return ARDP_BAD_VERSION;
	} else {
	    if (ardp_debug >= 6)
		rfprintf(stderr, " -- matched with cid %d", ntohs(req->cid));
	}
    }
    assert(req);               /* we have identified the request */

    /* Do we speak a compatible version?  Identify it if so. */
    /* 'req' is still on the ardp_activeQ. */

   /* We may later choose a more sophisticated matching policy, based on what
       we know about inter-version compatibility, and what we know about the
       special requirements of this particular request. */
    for (i = 0; i < nbytes; ++i) { /* remote peer supported versions */
	for(j = 0; j < (int) sizeof client_supported_versions; ++j) {
	    if (((unsigned const char *) ctlptr)[i] == client_supported_versions[j]) {
		/* MATCHED.  Resend this request using a better version.  */
		enum ardp_errcode tmperr;
		if (req->peer_ardp_version == 1 && 
		    resp_type == ARDP_V0_CID_0_UNMATCHED && 
		    /* client_supported_versions[j] is the 'potential new
		       version for retransmission' */
		    client_supported_versions[j] == 0) {
		    assert(!(req->flags &
			     ARDP__ALREADY_GOT_BOGUS_V0_CID0_RESPONSE_TO_ARDP_V1_REQUEST));
		    req->flags |=
			ARDP__ALREADY_GOT_BOGUS_V0_CID0_RESPONSE_TO_ARDP_V1_REQUEST;
		}
		    
		req->peer_ardp_version = client_supported_versions[j];
	    retransmit_with_new_version:
		if (ardp_debug >= 6) {
		    rfprintf(stderr, " -- retransmitting using ardp v%d]\n",
			    req->peer_ardp_version);
		}
		ardp_headers(req);
		tmperr = ardp_xmit(req, req->pwindow_sz);

		EXTERN_MUTEXED_UNLOCK(ardp_activeQ);
		return tmperr;
	    }
	}               
    }
    /* No match. */
    /* Here is a specific case, to handle very recent ardp v0 servers (these
       were never released to the general public, but have been used at ISI).
       These servers knew about sending a single zero octet to indicate
       version-independent-bad-version, but they did not know enough to tell
       the peer what versions they did speak.  */
    if (resp_type == VERSION_INDEPENDENT_BAD_VERSION && 
	nbytes == 0 && req->peer_ardp_version == 1) {
	req->peer_ardp_version = 0;
	goto retransmit_with_new_version;
    }
    if (ardp_debug >= 6) {
	rfprintf(stderr, " -- no common version (peer speaks");
	for (i = 0; i < nbytes; ++i)
	    rfprintf(stderr, " %d", ctlptr[i]);
	rfprintf(stderr, ")]\n");
    }
    /* Take the dead request from the activeQ */
    EXTRACT_ITEM(req, ardp_activeQ);
    EXTERN_MUTEXED_UNLOCK(ardp_activeQ);
    /* Commit the dead request to the ardp_completeQ */
    req->status = ARDP_BAD_VERSION;
    EXTERN_MUTEXED_LOCK(ardp_completeQ);
    APPEND_ITEM(req,ardp_completeQ);
    EXTERN_MUTEXED_UNLOCK(ardp_completeQ);
    return ARDP_BAD_VERSION;
}



/* *f1: April fool. */
