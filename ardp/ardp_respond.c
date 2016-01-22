/*

 * Copyright (c) 1992 -- 1998 by the University of Southern California
 *
 * For copying and distribution information, please see the file
 *  <usc-license.h>.
 *
 * Written  by bcn 1991     as transmit in rdgram.c (Prospero)
 * Modified by bcn 1/93     modularized and incorporated into new ardp library
 * Modified by swa and katia 1997	security context added.
 */


#include <usc-license.h>

#include <stdio.h>
#include <ardp.h>
#include <ardp_sec.h>
#include <ardp_time.h>
#include "ardp__int.h"		/* ardp__gettimeofday() prototype */
#include <gl_log_msgtypes.h>
#include <memory.h>		/* memset() */

int             ardp_clear_doneQ_loginfo = 0; /* Clear log info from the doneQ.
                                                 Set by the -n option to
                                                 dirsrv. */


/*
 * ardp_respond - respond to request
 *
 *    ardp_respond takes a request block whose ->outpkt element is a pointer
 *    to a list of packets that have not yet been returned.  It moves the
 *    packets to the transmission queue of the request (->trns) and assigns a
 *    sequence number to each packet.  If options is ARDP_R_COMPLETE, it
 *    also sets the ->trns_tot field of the request indicating the total 
 *    number of packets in the response.   if the ARDP_R_NOSEND option is 
 *    not specified, the current packet is also sent to the client.
 *    
 *    ardp_respond() expects the text of the packet to have a trailing null
 *    at the end of it, which is transmitted for safety's sake.  
 *    (noted: swa, 8/5/93).  This explains some bugs...
 */
enum ardp_errcode
ardp_respond(RREQ req, int options)
{
    char 		buf[ARDP_PTXT_DSZ];
    int			retval = ARDP_SUCCESS; /* Return value */
    u_int16_t	cid = htons(req->cid); /* This (and all other functions
						in this file (and on the server
						side)) are assuming that
						req->cid is in HOST byte
						order.  (This is not how the
						client handles it).  */
    u_int16_t	nseq = 0;	/* <NB> Sequence number of this
				   packet.  Version of
				   req->outpkt->seq.  */
    u_int16_t	ntotal = 0;	/* <NB> total # of packets.  <NB> version of 
				   req->trns_tot */  
    u_int16_t	rcvdthru = 0;	/* <NB> */
    u_int16_t	bkoff = 0;
    int			stamp_clear_wait = 0; /* flag: stamp packet with clear
						 wait */ 
    int			stamp_request_ack = 0; /* flag */
    int			stamp_total = 0; /* flag */
    PTEXT			tpkt; /* Temporary pkt pointer */

    *buf = '\0';

#ifdef ARDP___CANONICALIZE_OUTPKT
    ardp___canonicalize_ptexts(req->outpkt);
#endif
#ifndef ARDP_NO_SECURITY_CONTEXT
    if (options & ARDP_R_COMPLETE) {
	ardp_sec_commit(req);
    }
#endif /* ndef ARDP_NO_SECURITY_CONTEXT */

    if(req->peer_ardp_version < 0) cid = 0;

    if(req->status == ARDP_STATUS_FREE)
        internal_error("Attempt to respond to free RREQ\n");

    if(!req->outpkt) return(ARDP_BAD_REQ);

 more_to_process:

    if(!req->trns) req->outpkt->seq = 1;
    else req->outpkt->seq = req->trns->previous->seq + 1;


    if((options & ARDP_R_COMPLETE) && (req->outpkt->next == NOPKT)) {
        /* It's the last packet to send. */
	req->trns_tot = req->outpkt->seq; /* set the total # of packets; now
                                             known.  */
    
        /* Test this condition later to see if the server (us) needs to request
           that the client ACK.  The server will request an ACK if we had
           previously told the client to back off and now are rescinding that.
           */
    }
    nseq = htons((u_short) req->outpkt->seq);

    if((req->peer_ardp_version < 0)&&(req->peer_ardp_version != -2)) {
	/* PRE-ARDP version only. */
	if(req->trns_tot) sprintf(buf, "MULTI-PACKET %d OF %d",
				  req->outpkt->seq, req->trns_tot);
	else sprintf(buf,"MULTI-PACKET %d\n",req->outpkt->seq);
    }

    /* Start writing the headers for each packet in the response (each packet
       hanging off of req->outpkt) */
    /* Version-independent stuff that will affect what we put into the header:
     */ 

    /* Do we need to clear a service requested wait? */
    /* RATIONALE: If we haven't heard back from the client since we last modified the
       wait, then every packet we send will be stamped with the wait time.  We
       do this so that even if a lot of packets are lost, one of them will
       still get through (it only costs us 2 bytes to piggyback the stamp on
       our V1 packets -- costs us 7 bytes (or 0 bytes if ack was requested
       anyway) on v0.  */
    /* ARDP-old-v0:, assume that svc_rwait of > 5 is too long. */ 
    /* ARDP-new-v0: ( and subsequent including ARDP-V1, though we don't do that
       here), if we set the svc_rwait before to any non-default value, now set
       it to 0. (client's default). */  
    /* IMPLEMENTATION: XXX Note that when all clients V5, new wait will be 0
     */ 
    if ((req->peer_ardp_version < 0 && req->svc_rwait > 5)
	|| (req->peer_ardp_version >= 0 && req->svc_rwait)) {
	stamp_clear_wait = 1;
	/* set the new wait */
	if(req->peer_ardp_version < 0) 
	    req->svc_rwait = 5;
	else
	    req->svc_rwait = 0;
	/* Remember when we cancelled the old wait */
	req->svc_rwait_seq = req->outpkt->seq; /* This was the sequence
						  number it was reset
						  on. */ 
	/* We'll ask for an acknowledgement when we send the last
	   packet of the response. */
    } else if (req->svc_rwait_seq > req->prcvd_thru) 
	stamp_clear_wait = 1;
    else
	stamp_clear_wait = 0;


    /* Send an ACK request either
       (a) if we need to clear a service requested wait or
       (b) if we are sending the last packet of a window (in this case, the
       client should ACK (or request a retry) before the next one goes. */
    if ((/* (A1) It's the last packet of the message */
	(req->trns_tot && req->outpkt->seq == req->trns_tot)
	&& /* (A2) If we reduced a previously requested wait, and haven't
	      yet had that reduction acknowledged, (or haven't even sent that
	      reduction yet, but will in this packet) then ask for an
	      acknowledgement. */ 
	stamp_clear_wait)
	|| /* OR: Case (B) -- sending last packet of a window */
	(req->outpkt->seq == req->prcvd_thru + req->pwindow_sz)) {
	req->retries_rem = 4;
	req->wait_till = add_times(ardp__gettimeofday(), req->timeout_adj);
	ardp__log(L_NET_INFO, req, "Pkt %d will request an ACK", 
		  req->outpkt->seq);
	stamp_request_ack = 1;
    } else {
	stamp_request_ack = 0;
    }
    /* Stamp the total # of packets iff it's the last packet. 
       This is only tested by the ardp-v1 code. */
    if (req->trns_tot && (req->outpkt->seq == req->trns_tot)) {
	stamp_total = 1;
	ntotal = htons((u_short) req->trns_tot);
    } else {
	stamp_total = 0;
    }
    /* Note that in the following, we don't have to make sure  */
    /* there is room for the header in the packet because we   */
    /* are the only one that moves back the start, and ptalloc */
    /* allocates space for us in all packets it creates        */
    

    /* V0: If a single message and no connection ID to return, */
    /* and no need to stamp with wait or acknowledgement */
    /* then   */
    /* we can leave the control fields out                 */
    /* If we just need the CID, we can still leave out most of the
       control fields. */ 
    if ((req->trns_tot == 1 && !stamp_clear_wait && !stamp_request_ack)
	&& (req->peer_ardp_version == ARDP_VERSION_NEW_V0
	    || req->peer_ardp_version == ARDP_VERSION_OLD_V0)) {
	/* ARDP new-v0 or old-v0 only */
	if(req->cid == 0) {
	    req->outpkt->start -= 1;
	    req->outpkt->length += 1;
	    *req->outpkt->start = (char) 1;
	}
	else {
	    req->outpkt->start -= 3;
	    req->outpkt->length += 3;
	    *req->outpkt->start = (char) 3;
	    memcpy2(req->outpkt->start+1, &cid);     /* Conn ID */
	}
    } /* Fill in the control fields */
    else if (req->peer_ardp_version < 1) {	    
	/* ARDP-new-v0, ARDP-old-v0, pre-ARDP */
	/* pre-ARDP:  Note that this code will cheerfully write headers
	   (requesting that 
	   waits be cleared or acks be sent) onto pre-ARDP packets.  This is
	   clearly bogus, since PRE-ARDP lacks a header; however, it was what
	   the code was doing before we started messing with it, so we're not
	   changing this.  --swa & katia 5/1/96 */

	/* If we have to set octet 11 flags (in this case, the ACK flag),
           then we need to send an 12-byte request. 
	   */
	if (stamp_request_ack) {
            req->outpkt->start -= 12;
            req->outpkt->length += 12;
	    req->outpkt->start[0] = 12;
	} else if(stamp_clear_wait) {
	    /* If we need to clear a service-requested wait, we need octets 9 &
	       10. */
            req->outpkt->start -= 11;
            req->outpkt->length += 11;
	    req->outpkt->start[0] = 11;
	} else if (stamp_total) {
	    req->outpkt->start -= 7;
	    req->outpkt->length += 7;
	    req->outpkt->start[0] = 7;
	} else {
	    req->outpkt->start -= 5;
	    req->outpkt->length += 5;
	    req->outpkt->start[0] = 5;
	}

	memcpy2(req->outpkt->start+1, &cid);     /* Conn ID */
	memcpy2(req->outpkt->start+3, &nseq);     /* Pkt no  */
	
	if (req->outpkt->start[0] >= 7) 
	    memcpy2(req->outpkt->start+5, &ntotal);   /* Total   */

	if (req->outpkt->start[0] >= 9) {
	    rcvdthru = htons((u_short) req->rcvd_thru);
	    memcpy2(req->outpkt->start+7, &rcvdthru); /* Recvd Through   */
	} 
	if (req->outpkt->start[0] >= 11) {
	    bkoff = htons((u_short) req->svc_rwait);
	    memcpy2(req->outpkt->start+9, &bkoff);     /* New ttwait  */
	}
	if (req->outpkt->start[0] >= 12) {
	    assert (stamp_request_ack) ;
	    req->outpkt->start[11] = 0x80;          /* Request ack */
	}
    } else {			/* v1 */
	unsigned char header_len;
	char *ctlptr;

	assert(req->peer_ardp_version == 1);
	/* Stamp header size */
	if (stamp_clear_wait) {
	    /* Room for wait-time argument */
	    header_len = 13;
	} else {
	    header_len = 11;
	}	    
	if (stamp_total) {
	    header_len += 2;
	}
	req->outpkt->start -= header_len;
	req->outpkt->length += header_len;
	req->outpkt->start[4] = (char) header_len;

	req->outpkt->start[0] = (char) 129; /* version # */
	req->outpkt->start[1] = req->outpkt->context_flags;	/* no contexts here (yet) */
	req->outpkt->start[2] = 0;	/* flags */
	if (stamp_request_ack)
	    req->outpkt->start[2] |= 0x01;	/* flags: please-ack bit on. */
	if (stamp_total)
	    req->outpkt->start[2] |= 0x04;
	if (stamp_clear_wait) 
	    req->outpkt->start[2] |= 0100; /* bit 6: clear wait (2 octet arg. too) */
	req->outpkt->start[3] = 0;	/* no options */
	memcpy2(req->outpkt->start+5, &cid);		/* Connection ID     */
	memcpy2(req->outpkt->start+7, &nseq);		/* Packet sequence number */
	rcvdthru = htons(req->rcvd_thru);
	memcpy2(req->outpkt->start+9, &rcvdthru); /* Received Through   */
	/* Possible Optimization: We might be able to leave out the
	   received-through under some circumstances, and thereby save
	   ourselves 2 bytes.  */
	/* Write options */
	ctlptr = req->outpkt->start + 11;
	if (stamp_total) {
	    memcpy2(ctlptr, &ntotal);
	    ctlptr += 2;
	}
	if (stamp_clear_wait) {
	    bkoff = htons((u_short) req->svc_rwait);
	    memcpy2(ctlptr, &bkoff);
	    ctlptr += 2;
	}
    }    
    /* The end of writing the packet's header */

#if 0
    /* commented out by swa@isi.edu, since nulls in packets are now significant
       to the ARDP library.  Therefore, one should NOT be appended to each
       packet in the data area. */
    /* Make room for the trailing null */
    req->outpkt->length += 1;
#endif

    /* Only send if packet not yet received. */
    /* Do honor the window of req->pwindow_sz packets.  */
    if(!(options & ARDP_R_NOSEND) && 
       (req->outpkt->seq <= (req->prcvd_thru + req->pwindow_sz)) &&
       (req->outpkt->seq > req->prcvd_thru) /* This packet not yet received */
                                               ) { 
	retval = ardp_snd_pkt(req->outpkt,req);
    }


    /* Add packet to req->trns */
    tpkt = req->outpkt;
    EXTRACT_ITEM(tpkt,req->outpkt);
    APPEND_ITEM(tpkt,req->trns);

    if(req->outpkt) goto more_to_process;

    /* If complete then add request structure to done  */
    /* queue in case we have to retry.                 */
    if(options & ARDP_R_COMPLETE) {
        RREQ match_in_runQ;     /* Variable for indexing ardp_runQ */
        
	/* Request has been processed, here you can accumulate */
	/* statistics on system time or service time           */
	ardp__log(L_QUEUE_COMP, req, "Requested service completed"); 

        EXTERN_MUTEXED_LOCK(ardp_runQ);
        for (match_in_runQ = ardp_runQ; match_in_runQ; match_in_runQ = match_in_runQ->next) {
            if(match_in_runQ == req) {
                EXTRACT_ITEM(req, ardp_runQ);
                break;
            }
        }
        /* At this point, 'req' is the completed request structure.  It is
           definitely not on the ardp_runQ, and if it was, it's been removed.
           */ 
        EXTERN_MUTEXED_UNLOCK(ardp_runQ);
	if((req->cid == 0) || (dQmaxlen <= 0)) {
            /* If request has no CID (can't be matched on a retry) or
               if done-Q max length is <= 0 (i.e., don't keep a doneQ), then
               just throw away the request now that it's done. --swa */
#if 0                           /* ask BCN about this. Should never be
                                   necessary, since req->outpkt is always going
                                   to be NULL or we wouldn't be here. */
	    req->outpkt = NULL;
#endif
	    ardp_rqfree(req);
	}
	else {
            /* This item should be put on the doneQ; don't just throw it away.
               */
            /* Note that this code will not handle a reduction in the size of
               dQmaxlen; if anywhere you reset it to a smaller value, that code
               will have to truncate the queue. */
            EXTERN_MUTEXED_LOCK(ardp_doneQ);
            if(dQlen <= dQmaxlen) {
                /* Add to start */
#ifndef NDEBUG                  /* this helps debugging and slightly cuts down
                                 on memory usage of the doneq. */
                if (ardp_clear_doneQ_loginfo)
                    ardp_rq_partialfree(req);
#endif
                PREPEND_ITEM(req, ardp_doneQ);
                dQlen++;
            } else {
                /* The last item in the queue is ardp_doneQ->previous. */
                /* Use a variable to denote it, so that the EXTRACT_ITEM macro
                   doesn't encounter problems (since it internally modifies the
                   referent of the name ardp_doneQ->previous). */
                register RREQ doneQ_lastitem = ardp_doneQ->previous;
                /* Add new request to start */
#ifndef NDEBUG                  /* this helps debugging and slightly cuts down
                                   on memory usage of the doneq. */
                if (ardp_clear_doneQ_loginfo)
                    ardp_rq_partialfree(req);
#endif
                PREPEND_ITEM(req, ardp_doneQ);

                /* Now, prepare to remove the last item in the queue */
                /* If the request to remove was not acknowledged by the
                   clients, log the fact */ 
                if(doneQ_lastitem->svc_rwait_seq > 
                   doneQ_lastitem->prcvd_thru) {
                    ardp__log(L_QUEUE_INFO,doneQ_lastitem,
                         "Requested ack never received (%d of %d/%d ack)",
                         doneQ_lastitem->prcvd_thru, 
                         doneQ_lastitem->svc_rwait_seq, 
                         doneQ_lastitem->trns_tot);
                }
                /* Now do the removal and free it. */
                EXTRACT_ITEM(doneQ_lastitem, ardp_doneQ);
                ardp_rqfree(doneQ_lastitem);
            }
            EXTERN_MUTEXED_UNLOCK(ardp_doneQ);
        }
    }
    return(retval);
}

/*
 * ardp_rwait - sends a message to a client requesting
 *   that it wait for service.  This message indicates that
 *   there may be a delay before the request can be processed.
 *   The recipient should not attempt any retries until the
 *   time specified in this message has elapsed.  Depending on the
 *   protocol version in use by the client, the additional
 *   queue position and system time arguments may be returned.
 */
enum ardp_errcode
ardp_rwait(RREQ		req,        /* Request to which this is a reply  */
	   int		timetowait, /* How long client should wait, in seconds
				     */ 
	   u_int16_t	qpos,       /* Queue position                    */
/* Note stime is a System call in solaris, changed to systemtime */
	   u_int32_t	systemtime)      /* System time                  */
{
    PTEXT	r = ardp_ptalloc();
    short	cid = htons(req->cid);
    short	nseq = 0;
    short	zero = 0;
    short	backoff;
    short	stmp;
    u_int32_t	ltmp;
    int		tmp;
    
    if(req->peer_ardp_version < 0) /* if ARDP old-v0; (also if pre-ardp).
				      Pre-ARDP clients might not
				      know how to handle this request in any
				      case.;  I'm not positive --swa, 5/1/96 */
	cid = 0;

    /* Remember that we sent a server requested wait and assume  */
    /* it took effect, though request will be unsequenced        */
    /* This seems bogus to me.  Rather, more appropriate would be to send an
       unsequenced control packet if the requested wait is being extended and
       a sequenced packet if the wait is being shortened, since the shortening
       of the wait is an event which requires an acknowledgement. */
    /* Note, however, that this shortening is a way the current function is not
       used. */
    req->svc_rwait_seq = req->prcvd_thru; /* Mark the latest requested wait as
                                             definitely having been received.
                                             This is not necessary, actually --
                                             leaving this set to zero is ok. */
    req->svc_rwait = timetowait;
    
    backoff = htons((u_short) timetowait);
    if (req->peer_ardp_version <= 0) {
	*r->start = (char) 11;
	r->length = 11;

	memcpy2(r->start+1, &cid);		/* Connection ID     */
	memcpy2(r->start+3, &nseq);		/* Packet number     */
	memcpy2(r->start+5, &zero);		/* Total packets     */
	memcpy2(r->start+7, &zero);		/* Received through  */
	memcpy2(r->start+9, &backoff);	/* Backoff           */

	if((req->peer_ardp_version >= 0) && (qpos || systemtime)) {
	    r->length = 14;
	    bzero3(r->start+11); 
	    *(r->start+12) = (unsigned char) 254;
	    if(qpos) *(r->start+13) |= (unsigned char) 1;
	    if(systemtime) *(r->start+13) |= (unsigned char) 2;
	    if(qpos) {
		stmp = htons(qpos);
		memcpy2(r->start+r->length, &stmp);
		r->length += 2;
	    }
	    if(systemtime) {
		ltmp = htonl(systemtime);
		memcpy4(r->start+r->length, &ltmp);
		r->length += 4;

	    }
	    *r->start = (char) r->length;
	}
    } else {
	short rcvd_thru = htons(req->rcvd_thru);
	assert(req->peer_ardp_version == 1); /* ARDP v1 */
		r->length = 13;	/* include 2 bytes for WAIT-TIME argument */
	r->start[0] = (char) 129;
	r->start[1] = 0;	/* XXX context later */
	r->start[2] = 0100;	/*  Turn on WAIT-TIME flag */
	if (qpos || systemtime)
	    r->start[3] = (char) 254;	/* queue status option */
	else
	    r->start[3] = 0;
	/* Octet 4 (header length) is set once we're done. */
	memcpy2(r->start+5, &cid);  	/* Connection ID */
	memcpy2(r->start+7, &zero);  	/* Packet Sequence Number
					   (unseq. control pkt.) */ 
	/* EXPERIMENTAL STRATEGY/RATIONALE:
	   We don't know why the v0 code says it received through zero packets;
	   we're returning the real packet count for v1, but leaving the v0
	   code the old way. -- steve & katia. */
	memcpy2(r->start + 9, &rcvd_thru); /* received through this. */
	/* Start option data */
	/* WAIT-TIME flag */
	memcpy2(r->start + 11, &backoff);
	if (systemtime)
	    r->start[r->length++] = 0;
	if(qpos) {
	    r->start[13] = (char) 0x01;
	    stmp = htons(qpos);
	    memcpy2(r->start + r->length, &stmp);
	    r->length += 2;
	}
	if(systemtime) {
	    r->start[13] |= (char) 0x02;
	    ltmp = htonl(systemtime);
	    memcpy4(r->start + r->length, &ltmp);
	    r->length += 4;
	}
	r->start[4] = (char) r->length;	/* header length */
    }

    tmp = ardp_snd_pkt(r,req);
    ardp_ptfree(r);
    return(tmp);
}

/*
 * ardp_refuse - sends a message to the recipient indicating
 *   that its connection attempt has been refused.
 */
enum ardp_errcode
ardp_refuse(RREQ req)
{
    PTEXT	r;
    short	cid = htons(req->cid);
    short	zero = 0;
    int		tmp;
    
    /* If old version, it won't understand the REFUSED option. */
    if(req->peer_ardp_version < 0) return(ARDP_FAILURE);

    r = ardp_ptalloc();

    if (req->peer_ardp_version <= 0) {
	*r->start = (char) 13;
	r->length = 13;

	memcpy2(r->start+1, &cid);		/* Connection ID     */
	memcpy2(r->start+3, &zero);		/* Packet number     */
	memcpy2(r->start+5, &zero);		/* Total packets     */
	memcpy2(r->start+7, &zero);		/* Received through  */
	memcpy2(r->start+9, &zero);		/* Backoff           */
	bzero2(r->start+11);		/* Flags             */
	*(r->start + 12) = (unsigned char) 1; /* Refused         */
    } else {
	assert(req->peer_ardp_version == 1); /* ARDP v1 */
	r->length = 9;
	r->start[0] = (char) 129;
	r->start[1] = 0;	/* XXX context later */
	r->start[2] = 0;	/* flags */
	r->start[3] = '\001';	/* REFUSED option */
	r->start[4] = (char) 9;	/* header length */
	memcpy2(r->start+5, &cid);  	/* Connection ID */
	memcpy2(r->start+7, &zero);  	/* Packet Sequence Number
					   (unseq. control pkt.) */ 
    }    
    tmp = ardp_snd_pkt(r,req);
    ardp_ptfree(r);
    return(tmp);
}


/*
 * ardp_redirect - sends a message to the recipient indicating that
 * all unacknowledged packets should be sent to a new target destination.
 * The target specifies the host address and the UDP port in network
 * byte order.  For now, redirections should only occur before any
 * request packets have been acknowledged, or response packets sent.
 */
enum ardp_errcode
ardp_redirect(RREQ 		     req,     /* Request to be redirected */
	      struct sockaddr_in    *target)  /* Address of new server    */
{
    PTEXT	r;
    short	cid = htons(req->cid);
    short	zero = 0;
    int		tmp;

    /* Old versions don't support redirection */
    if(req->peer_ardp_version < 0) return(ARDP_FAILURE);

    r = ardp_ptalloc();

    if (req->peer_ardp_version <= 0) { /* ardp V0 or pre-v0 */
	*r->start = (char) 19;
	r->length = 19;

	memcpy2(r->start+1, &cid);  	/* Connection ID                     */
	memset(r->start+3, '\000', 9);	/* Pktno, total, rcvd, swait, flags1 */
	*(r->start + 12) = '\004';  /* Redirect option                   */
	memcpy4(r->start+13, &(target->sin_addr));
	memcpy2(r->start+17, &(target->sin_port));
    } else {
	assert(req->peer_ardp_version == 1); /* ARDP v1 */
	r->length = 17;
	r->start[0] = (char) 129;
	r->start[1] = 0;	/* XXX context later */
	r->start[2] = 0;	/* flags */
	r->start[3] = '\004';	/* redirect option */
	r->start[4] = (char) 17;	/* header length */
	memcpy2(r->start+5, &cid);  	/* Connection ID */
	memcpy2(r->start+7, &zero);  	/* Packet Sequence Number
					   (unseq. control pkt.) */ 
	/* We should not acknowledge receipt of any packets received from the
	   peer. 	   
	   Rationale: We haven't done any processing (presumably) on the
	   packets we got. 
	   (Forwarded (option 6) is not yet implemented. If it were, then we
	   could forward the packets received and acknowledge them to our
	   peer; then the client would not have to resend the request to the
	   new server.)  There are a number of possibilities in the ARDP
	   options. */ 
	memcpy2(r->start+9, &zero);  	/* # packets Received from peer. */
	memcpy4(r->start+11, &(target->sin_addr));
	memcpy2(r->start+15, &(target->sin_port));
    }
    tmp = ardp_snd_pkt(r,req);
    ardp_ptfree(r);
    return(tmp);
}


/*
 * ardp_ack - sends an acknowledgment message to the client indicating
 * that all packets through req->rcvd_thru have been recived.  It is
 * only called on receipt of a multi-packet server request.
 */
enum ardp_errcode
ardp_acknowledge(RREQ	req)        /* Request to which this is a reply  */
{
    PTEXT	r = ardp_ptalloc();
    short	cid = htons(req->cid);
    short	rcvd = htons(req->rcvd_thru);
    short	zero = 0;
    int		tmp;
    
    if (req->peer_ardp_version <= 0) { /* ardp V0 or pre-v0 */
	*r->start = (char) 9;
	r->length = 9;

	memcpy2(r->start+1, &cid);		/* Connection ID     */
	memcpy2(r->start+3, &zero);		/* Packet number     */
	memcpy2(r->start+5, &zero);		/* Total packets     */
	memcpy2(r->start+7, &rcvd);		/* Received through  */
    } else {
	assert(req->peer_ardp_version == 1); /* ARDP v1 */
	r->length = 11;	/* writing an 11 byte header; don't need to move back
			   the start ptr (plenty of room to write where we
			   already are) */ 
	r->start[0] = (char) 129; /* version # */
	r->start[1] = 0;	/* no contexts here (yet) */
	r->start[2] = 0x01;	/* flags: please-ack bit on. */
	r->start[3] = 0;	/* no options */
	r->start[4] = 11;	/* header length */
	memcpy2(r->start+5, &cid);		/* Connection ID     */
	memcpy2(r->start+7, &zero);		/* Packet sequence number */
	memcpy2(r->start+9, &rcvd);		/* Received through  */
    }

    tmp = ardp_snd_pkt(r,req);
    ardp_ptfree(r);
    return(tmp);
}

#ifdef ARDP_PURE_V1_SERVER
/* Force clients to use v1 if they can */
static const unsigned char supported_server_versions[] = { 1};
#else
#ifdef ARDP_PURE_V0_SERVER
static const unsigned char supported_server_versions[] = { 0};
#else
static const unsigned char supported_server_versions[] = { 1, 0};
#endif
#endif


enum ardp_errcode
ardp_version_not_supported_by_server(RREQ req)
{
    PTEXT	r;
    short	cid = htons(req->cid);
    short	zero = 0;
    int		tmp;

    r = ardp_ptalloc();

    switch(req->peer_ardp_version) {
    case -1:			/* PRE-ARDP */
    case -2:			/* ARDP old-v0 */
	/* Old versions don't support this message */
	ardp_ptfree(r);
	return(ARDP_FAILURE);
	break;			/* NOTREACHED */
#ifndef ARDP_SEND_VERSION_INDEPENDENT_BAD_VERSION_ONLY
    case 0:			/* ARDP v0 */
	r->length = 13 + sizeof supported_server_versions;
	*r->start = (char) r->length;

	memcpy2(r->start+1, &cid);  	/* Connection ID                     */
	memset(r->start+3, '\000', 9);	/* Pktno, total, rcvd, swait, flags1 */
	*(r->start + 12) = (char) 8;  /* unknown version  option */
	memcpy(r->start + 13, supported_server_versions, sizeof supported_server_versions);
	break;
    case 1:			/* ardp V1 */
	r->length = 11 + sizeof supported_server_versions;
	r->start[0] = (char) 129;
	r->start[1] = 0;	/* XXX context later */
	r->start[2] = 0;	/* flags */
	r->start[3] = (char) 8;	/* unsupported version option */
	r->start[4] = (char) r->length;	/* header length */
	memcpy2(r->start+5, &cid);  	/* Connection ID */
	memcpy2(r->start+7, &zero);  	/* Packet Sequence Number
					   (unseq. control pkt.) */ 
	/* We should not acknowledge receipt of any packets received from the
	   peer.
	   Rationale: We haven't done any processing (presumably) on the
	   packets we got.  */
	memcpy2(r->start+9, &zero);  	/* # packets Received from peer. */
	memcpy(r->start + 11, supported_server_versions, sizeof supported_server_versions);
	break;
#endif
    default:			/* anything we don't have a version-specific
				   message for */
#ifndef ARDP_SEND_VERSION_INDEPENDENT_BAD_VERSION_ONLY
	assert(req->peer_ardp_version >= 2);
#endif
	/* VERSION-INDEPENDENT version unknown message */
	/* Generate the version-independent bad-version response. */
	/* This is a packet with a zero first byte. */

	/* Note that we bypassed ardp_respond(); it does more work than we need
	   and has inappropriate expectations about the packet format. */
	/* Set the fields that ardp_snd_pkt needs */
	*(r->start) = '\0';
	r->length = 1 + sizeof supported_server_versions;
	memcpy(r->start + 1, supported_server_versions, sizeof supported_server_versions);
	break;			/* unneeded */
    }
    tmp = ardp_snd_pkt(r,req);
    ardp_ptfree(r);
    return(tmp);
}
