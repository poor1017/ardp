/*
 * Copyright (c) 1991-1996 by the University of Southern California
 *
 * For copying and distribution information, please see the file
 * <usc-license.h>.
 */

#include <usc-license.h>

#include <pmachine.h>
#include <ardp.h>
#include "ardp_sec.h"		/* provides ARDP_NO_SECURITY_CONTEXT */

#ifndef ARDP_NO_SECURITY_CONTEXT

#include <gl_log_msgtypes.h>

#ifndef min
#define min(x,y) (x < y ? x : y)
#endif


static enum ardp_errcode do_security_context(
    RREQ req, PTEXT pkt_context_started_in, const char *buf, int buflen,
    ardp_sectype **failed_ctxtp);
static enum ardp_errcode match_responses_with_originals(ardp_sectype *secq);
static enum ardp_errcode check_orphaned_originals(ardp_sectype *secq,
						  ardp_sectype **failed);

/* SERVER-only function. */
/* This logs errors to the logfile; other than that, it's a wrapper around
   ardp__sec_process_contexts() */
enum ardp_errcode
ardp__sec_server_process_received_security_contexts(RREQ creq)
{
    enum ardp_errcode errcode = ARDP_SUCCESS; /* error code returned. */
    ardp_sectype *failed_ctxt = NULL;
    
    /* A complete multi-packet request has been received or a single-packet
       request (always complete) has been received. */
    /* At this point, creq refers to an RREQ structure that has either just
       been removed from the ardp_partialQ or was never put on a queue. */

    if (ardp_debug >= 3) {
	rfprintf(stderr, "ardp server: entering server_process_received_"
		"security_contexts()\n");
    }
    if((errcode = ardp__sec_process_contexts(creq, &failed_ctxt))) {
	if (failed_ctxt) {
	    /* If we know which context failed, send a rejection message. */
	    ardp__sec_reject_failed_ardp_context(
		creq, failed_ctxt->service, failed_ctxt->mechanism, 
		failed_ctxt->error_code);
	}

	/* Here we log the errors so the server knows what to do. */
	if (errcode == ARDP_CRITICAL_CONTEXT_NOT_SUPPORTED)
	    /* Failed to recognize security context */
	    ardp__log(L_CONTEXT_ERR, creq, "An ARDP critical context \
(probably the security context) was received but not understood by \
the server.");
	if (errcode == ARDP_CRITICAL_SERVICE_FAILED)
	    ardp__log(L_CONTEXT_ERR, creq, "Critical security service failed");
	/* Should never get a failure-to-match error
	   (ARDP_CRITICAL_SERVICE_UNRECIPROCATED), since the server does not
	   initiate requests, it just replies to them. */
	if (errcode == ARDP_CRITICAL_SERVICE_UNRECIPROCATED)
	    ardp__log(L_CONTEXT_ERR, creq, "Critical security service unreciprocated");
    }
    if (errcode) {
	if(ardp_debug) {
	    rfprintf(stderr, "ardp server:"
		    " server_process_received_security_contexts: returning"
		    " ARDP error %d\n", errcode);
	}
    } else {			/* Status: clean.  */
	if (ardp_debug >= 3) {
	    rfprintf(stderr, "ardp server: server_process_received_security_"
		    "contexts: returning ARDP_SUCCESS (no critical failures)\n");
	}
    }
    return errcode;
}

/* Process the security contexts on the client and server sides 
   Notifies the peer if errors occur. */
/* This function reads the protocol form of the context from the PTEXT
   directly. It pastes the complete security context together; the context
   might have been spread across separate packets.  It does not break the
   context into individual security context blocks.  It then hands the
   constructed complete block to do_security_context(), which in turn
   dispatches to the appropriate processing functions. */ 

enum ardp_errcode
ardp__sec_process_contexts(RREQ req, ardp_sectype **failed_ctxtp) 
{
    PTEXT tpkt;
    PTEXT pkt_context_started_in = NOPKT; /* Implementation trivia: doesn't
					     need initialization; we initialize
					     it because "gcc -Wall" doesn't
					     follow the code through to
					     determine this.  */ 
    char *buf = NULL;
    int context_sz = 0;		/* Size of the security context (# of bytes in
				   the context that haven't been eaten up
				   yet).  This is decremented as we go along */
    int offset = 0;		/* index into *bufp; how many bytes of data
				   have we copied so far?  */
    enum ardp_errcode errcode;
    ardp_sectype *dummy_context; /* Used for problem reporting */

    if (!failed_ctxtp)
	failed_ctxtp = &dummy_context;

    for (tpkt = req->rcvd; tpkt; tpkt = tpkt->next) {
	/* Was a new context initiated in this packet? */
	/* For each context initiated (there is currently only one ever
	   initiated), this loop pastes together physical context blocks into a
	   single long string.  The large block will then be handed to
	   do_security_context(). */ 
	u_int32_t ltmp;

	/* On the listening sides, tpkt->start refers to the packet's
	   payload (text after the headers).  We come here only on the
	   listening side.  (On the sending sides, it refers to the packet's 
	   first header byte.)  Right now, the payload is considered to
	   include the security context; this will be stripped out before we
	   exit from this function. */
	tpkt->ioptr = tpkt->start;

	if (tpkt->context_flags & 0x04) { /* context flag 0x04: Security Context present.  */
	    if (context_sz) {
		if (ardp_debug) {
		    rfprintf(stderr, "ardp: pkt #%d: protocol error: The"
			    " requestor just initiated a"
			    " new security context, but the current context"
			    " still expects %d more bytes.  Throwing away"
			    " current context.", tpkt->seq, context_sz);
		}
	    }
	    pkt_context_started_in = tpkt;
	   
	    memcpy4(&ltmp, tpkt->ioptr); /* length of context, including these
					    4 bytes. */
	    context_sz = ntohl(ltmp);
	    offset = 0;
	    context_sz -= 4;	/* eat up the 4 bytes we just used.  */
	    tpkt->ioptr += 4;
	    /* Make sure there's enough room for the context */
	    if ((int) p__bstsize(buf) < context_sz) {
		stfree(buf);
		buf = stalloc(context_sz);
	    }
	}
	if (context_sz) {	/* processing a context? */
	    /* zero-length contexts won't be processed; this follows as part of the general case :) */ 
	    /* Explicit temporary to improve legibility :(  I love long code strings.*/
	    /* # of bytes unread from this packet. */
	    int pkt_unreadbytes = tpkt->start + tpkt->length - tpkt->ioptr;
	    /* # of bytes left to read in context */
	    int cbytes= min(pkt_unreadbytes, context_sz);
	    memcpy(buf + offset, tpkt->ioptr, cbytes);
	    context_sz -= cbytes;
	    offset += cbytes;
	    tpkt->ioptr += cbytes;
	    /* Make the ->text member indicate the start of this packet's
	       payload.  We make it point right after the security context area
	       for this packet.  It might point to the start of the 
	       naming or message contexts, if they are present.  */
	    tpkt->text = tpkt->ioptr;

	    if (!context_sz) {
		/* We have retrieved the concatenated series of security
		   context blocks that started in the packet
		   PKT_CONTEXT_STARTED_IN.  This consists of one or more
		   'security context blocks', as defined in our base document 
		   <URL:
		   http://gost.isi.edu/info/ardp/ardp_protocol_sec_context.html
		   > */   
		/* BUF points to an entire block of the security context.
		   OFFSET is the buffer's data length now */		
		if((errcode = do_security_context(req, pkt_context_started_in,
						 buf, offset, failed_ctxtp))) {
		    stfree(buf);
		    /* Failed to process a critical context */
		    /* do_security_context() has notified the peer. */
		    return errcode;
		}
		offset = 0;
	    }
	}
#if 0
	/* XXX Need to process messsage and naming contexts still. */
	message_context = pkt->start[1] & 0x08;
	naming_context = pkt->start[1] & 0x10;
#endif
    } /* for */
    /* All contexts processed. */
    errcode = match_responses_with_originals(req->secq);
    if (!errcode) {
	errcode = check_orphaned_originals(req->secq, failed_ctxtp);
    }
    if (errcode && failed_ctxtp && *failed_ctxtp) {
	/* Not needed, but keeps the structure pretty, in case we debug. */
	(*failed_ctxtp)->error_code = errcode;
    }
    stfree(buf);
    /* Any rejection messages handled by the higher levels. */
    return errcode;		/* probably ARDP_SUCCESS */
}


/* This server function accepts a just-received security context as a
   contiguous extent of memory.  From this extent, it parses each security
   context block into an 'ardp_sectype' structure.  It does the receiver's
   work, as well as setting up the callbacks for the reply. */ 
/* buf points to one or more security context blocks */
/* buflen is the total size of buf */
static
enum ardp_errcode
do_security_context(RREQ req, PTEXT pkt_context_started_in, 
		    const char *buf, int buflen, ardp_sectype **failed_ctxtp)
{
    u_int32_t tmp32;		/* used to read network stuff. */
    u_int32_t seq;		/* Sequence position */
    unsigned int args_len;	/* length of this block's arguments. */
    int criticality;		/* boolean t/f; criticality bit */
    int peer_set_me_bit;	/* Was the ME bit set?  Did the peer send us a
				   block using the context, (if not, it was
				   presumably just a request for action)  */
    int peer_lusts_for_context;	/* should we reply to the peer with this
				   particular context type? */
    enum ardp__sec_service sec_service; /* type of security service */
    int sec_mechanism;		/* mechanism used to implement security service
				 */ 
    const char *blockstart = buf;
    enum ardp_errcode errcode;
    ardp_sectype *secp;	/* New element on the queue, the current
				   context (if recognized) */
    const char *argstart;		/* Where the argument block starts. */


    /* Loop through all of the security context blocks that have been strung
       together into the buffer 'buf' */ 
    while(blockstart < buf + buflen) {
	/* Criticality, receive, transmit, service, mechanism */
	criticality = blockstart[0] & 0x80;
	peer_set_me_bit = blockstart[0] & 0x40; /* ME */
	peer_lusts_for_context = blockstart[0] & 0x20; /* YOU */
	if (ardp_debug) {
	    if (!peer_set_me_bit && !peer_lusts_for_context) {
		rfprintf(stderr, "ardp: do_security_context(): Strange \
message: received security context block claiming neither ME nor YOU; \
continuing as best we can.\n");
	    }
	}
	sec_service = blockstart[0] & 0x1f;
	sec_mechanism = blockstart[1];
	
	/* Sequence Number */
	memcpy4(&tmp32, blockstart + 2);
	seq = ntohl(tmp32);

	/* Arguments length */
	memcpy4(&tmp32, blockstart + 6);
	args_len = ntohl(tmp32);

	/* All that remains are the arguments. */
	argstart = blockstart + 10;
	/* Here we need to add this request to the security context
	   deferred processing queue associated with the current RREQ */
	/* This queue also serves as a place to store the authentication
	   context or other useful information we may have received. */
	/* assign to existing sectype (need algorithm field filled in; have
	   info. for service and mechanism to fill in) */
	/* implementation note ardp_secopy() is specially designed to return
	   NULL if passed NULL. */
	if ((secp = ardp_secopy(ardp__sec_look_up_service(
		sec_service, sec_mechanism)))) {
	    /* Service lookup succeeded -- and we set the entire contents of
	       secp from it. */
	    secp->criticality = criticality;
	    /* stalloc() returns NULL if given a args_len of zero.  That's
	       OK.  --swa 7/97 */
	    if((secp->args = stalloc(args_len))) {
		memcpy(secp->args, argstart, args_len);
		p_bst_set_buffer_length_explicit(secp->args, args_len);
	    }
	    secp->requested_by_peer = peer_lusts_for_context;
	    secp->processing_state = ARDP__SEC_RECEIVED;
	    secp->peer_set_me_bit = peer_set_me_bit;
	    secp->seq = seq;
	    secp->pkt_context_started_in = pkt_context_started_in;
	    APPEND_ITEM(secp, req->secq);
	} else {
	    /* Didn't recognize the service. */
	    if (ardp_debug) {
		rfprintf(stderr, "ardp: Did not recognize the%s security"
			" context type (service = %d, mechanism = %d)\n",
			(criticality ? " critical" : ""), 
			sec_service, sec_mechanism);
	    }
	    /* If peer requests we send back a context with a critical
	       service, and we don't know what that context is, we must
	       fail. */
	    if (criticality) {
#if 1
		ardp_sectype *fc  = ardp_sealloc();

		fc->service = sec_service;
		fc->mechanism = sec_mechanism;
		fc->error_code = ARDP_CRITICAL_CONTEXT_NOT_SUPPORTED;
		
		*failed_ctxtp = fc;
		return fc->error_code;
#else
		/* Set failed_ctxtp to NULL so we notify the server that it
		   shouldn't send the message again. */
		/* The alternative would be to allocate a dummy context to hold
		   the error message. */
		*failed_ctxtp = NULL;
		
		ardp__sec_reject_failed_ardp_context( 
		    req, sec_service, sec_mechanism, 
		    ARDP_CRITICAL_CONTEXT_NOT_SUPPORTED);
		return ARDP_CRITICAL_CONTEXT_NOT_SUPPORTED;
#endif
	    }
	    /* In any case, we have no further use for this block.  Onward we
	       go! */
	    goto nextblock;	/* Increment onward. */
	}
	if (ardp_debug >= 3) {
	    rfprintf(stderr, "ardp: do_security_context():"
		    " received context #%u, mechanism %s: %s; %s\n",
		    secp->seq, ardp_sec_mechanism_name(secp),
		    (peer_set_me_bit ? "verifying." 
		     : "no need to verify"),
		    (peer_lusts_for_context ? "peer wants response." 
		     : "no response requested"));
	}

	/* Dispatch as appropriate.  Each service function checks
	   peer_set_me_bit before doing unnecessary work and reading possibly
	   nonexistent arguments! */
	secp->processing_state = ARDP__SEC_IN_PROCESS;
	if ((errcode = 
	    ardp__sec_dispatch_service_receiver(
		req, secp, secp->pkt_context_started_in, secp->service,
		secp->mechanism, secp->args, p_bstlen(secp->args)))) {
	    /* A problem here is that we may encounter processing success,
	       but preparation failure.  In this case, we end up trashing
	       the whole system. */
	    /* ardp_sec_dispatch_service_receiver() is responsible
	       for moving on to the next state. */
	    if (criticality) {
		/* Generate some sort of error message. */
#if 0
		/* only two errors that can occur.  Do we want
		   to do something with these specific error
		   codes? */
		if (errcode ==  ARDP_CONTEXT_NOT_UNDERSTOOD) {
		} 
		if (errcode == ARDP_SERVICE_FAILED) {
		}
#endif /* 0 */
		/* Set up failed_ctxtp so we can return a message to the
		   peer. */ 
		*failed_ctxtp = secp;
		if (secp->processing_state == ARDP__SEC_IN_PROCESS)
		    secp->processing_state = ARDP__SEC_PROCESSED_FAILURE;
		if (secp->error_code == ARDP_SUCCESS)
		    secp->error_code = errcode;
		if (ardp_debug)
		    rfprintf(stderr, "ardp: do_security_context():"
			    " critical failure\n");
		return errcode;
	    } else {
		if (secp->processing_state == ARDP__SEC_IN_PROCESS)
		    secp->processing_state = ARDP__SEC_PROCESSED_FAILURE;
		goto nextblock; /* onward. */
	    }
	} else {
	    /* Should've already been done, but most functions don't.
	       We're making sure. */ 
	    if (secp->processing_state == ARDP__SEC_IN_PROCESS)
		secp->processing_state = ARDP__SEC_PROCESSED_SUCCESS;
	    goto nextblock;	/* onward */
	}
	/* This is now dead code, not reached, unless the peer asks for
	   something back, of course... --- 1/98 */
	/* At this point, if we called a mechanism-specific function in
	   ardp__sec_dispatch_service_receiver(), then that function should
	   have set secp->processing_state to one of the following: */
	assert(secp->processing_state == ARDP__SEC_PROCESSED_SUCCESS
	       || secp->processing_state == ARDP__SEC_PROCESSED_FAILURE
	       || secp->processing_state == ARDP__SEC_IN_PROCESS_DEFERRED
	       || secp->processing_state == ARDP__SEC_PREP_FAILURE
	       || secp->processing_state == ARDP__SEC_PREP_SUCCESS);
    nextblock:
	if (ardp_debug >= 3) {
	    rfprintf(stderr, "ardp security: listener: block #%d, mech %s"
		    " verification status: %s\n",
		    secp->seq, ardp_sec_mechanism_name(secp),
		    ardp_sec_processing_state_str(secp));
	}
	/* We're done processing this block; on to the next one. */
	blockstart = argstart + args_len;
    }
    /* Now loop through everything again.  Look for deferrals. Oh dear. */
    for (secp = req->secq; secp; secp = secp->next) {
	if (secp->processing_state != ARDP__SEC_IN_PROCESS_DEFERRED)
	    continue;
	/* Here we duplicate some code from above, except for the two lines
	   marked DIVERGENCE
	   I have marked the code in this way to see about modularizing. */
	/* BEGIN DUPLICATE */
	/* DIVERGENCE -- but could be easily copied back. */
	if((errcode = 
	    ardp__sec_dispatch_service_receiver(
		req, secp, secp->pkt_context_started_in, secp->service,
		secp->mechanism, secp->args, p_bstlen(secp->args)))) {
	    /* A problem here is that the server may encounter errors preparing
	       a response to a successfully received message.
	       In this case, the server has no way of knowing that the request
	       was successfully received.  */
	    /* ardp_sec_dispatch_service_receiver() is responsible
	       for moving on to the next state. */
	    if (criticality) {
		/* Generate some sort of error message. */
#if 0
		/* only two errors that can occur.  Do we want
		   to do something with these specific error
		   codes? */
		if (errcode ==  ARDP_CONTEXT_NOT_UNDERSTOOD) {
		} 
		if (errcode == ARDP_SERVICE_FAILED) {
		}
#endif /* 0 */
		/* This just sends a message to the peer. */
		*failed_ctxtp = secp;
		if (secp->processing_state == ARDP__SEC_IN_PROCESS)
		    secp->processing_state = ARDP__SEC_PROCESSED_FAILURE;
		if (secp->error_code == ARDP_SUCCESS)
		    secp->error_code = errcode;
		if (ardp_debug)
		    rfprintf(stderr, "ardp: do_security_context():"
			    " critical failure\n");
		return errcode;
	    } else {
		/* Next line DIVERGENCE */
		if (secp->processing_state == ARDP__SEC_IN_PROCESS_DEFERRED)
		    secp->processing_state = ARDP__SEC_PROCESSED_FAILURE;
		goto deferred_showstatus;;  /* DIVERGENCE */
	    }
	} else {
	    /* Should've already been done, but most functions don't.
	       We're making sure. */ 
	    /* Next line DIVERGENCE */
	    if (secp->processing_state == ARDP__SEC_IN_PROCESS_DEFERRED)
		secp->processing_state = ARDP__SEC_PROCESSED_SUCCESS;
	    goto deferred_showstatus;		/* DIVERGENCE */
	}
	/* END DUPLICATE */
    deferred_showstatus:
	if (ardp_debug >= 3) {
	    rfprintf(stderr, "ardp security: listener: block #%d, mech %s"
		    " verification status: %s\n",
		    secp->seq, ardp_sec_mechanism_name(secp),
		    ardp_sec_processing_state_str(secp));
	}
    }
    
    return ARDP_SUCCESS;
}


/* Match up by sequence numbers, service & mechanism, success status. */
/* We know that responses must come after the originals in the secq, since we
   use APPEND_ITEM() to put them on the queue. */
/* OK if we leave some amatched up. */

static enum ardp_errcode 
match_responses_with_originals(ardp_sectype *secq)
{
    ardp_sectype *resp, *orig;
    for (resp = secq; resp; resp = resp->next) {
	if (resp->processing_state != ARDP__SEC_PROCESSED_SUCCESS
	    /* It's actually not appropriate for there to ever be a
	       PREP_SUCCESS here in what we received from the client, but the
	       server does set the YOU bit sometimes in any case, and it's
	       easier to just add the test here than to impose the discipline
	       on the server.  Especially since some security contexts set YOU
	       by default. */
	    && resp->processing_state != ARDP__SEC_PREP_SUCCESS)
	    continue;
	for (orig = secq; orig != resp; orig = orig->next) {
	    if (orig->processing_state != ARDP__SEC_COMMITTMENT_SUCCESS)
		continue;
	    if (orig->service != resp->service 
		|| orig->mechanism != resp->mechanism)
		continue;
	    if (orig->seq != resp->seq)
		continue;
	    resp->mate = orig;
	    orig->mate = resp;
	    break;		/* On to the next resp. */
	}
    }
    return ARDP_SUCCESS;
}


static enum ardp_errcode 
check_orphaned_originals(ardp_sectype *secq, /* IN */
			 ardp_sectype **failedp /* OUT */)
{
    for (*failedp = secq; *failedp; *failedp = (*failedp)->next) {
	if ((*failedp)->reject_if_peer_does_not_reciprocate
	    && !(*failedp)->mate) {
	    if (ardp_debug) {
		rfprintf(stderr, 
			"ardp: no matching response found for security context"
			" block #%d, %s\n", (*failedp)->seq, 
			ardp_sec_mechanism_name(secq));
	    }
	    return ARDP_CRITICAL_SERVICE_UNRECIPROCATED;
	}
    }
    return ARDP_SUCCESS;
}



#endif /* ndef ARDP_NO_SECURITY_CONTEXT */
