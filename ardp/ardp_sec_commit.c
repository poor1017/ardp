/* ardp_sec_commit.c */
/*
 * Copyright (c) 1997 by the University of Southern California
 *
 * For copying and distribution information, please see the file
 * <usc-license.h>.
 */

#include <usc-license.h>

#include <ardp.h>
#include <ardp_sec.h>

/*
 * ardp_send will normally call ardp_sec_commit().
 * Additional error recovery may be obtained by an application calling it
 * directly. 
 */

#if !defined(ARDP_NO_SECURITY_CONTEXT)

static enum ardp_errcode do_commit(RREQ req, ardp_sectype *secp);
/* ardp_sec_commit() takes each ardp_sectype structure from the 'req->secq'
   queue, does whatever final processing it needs to generate the data it is
   sending, and then formats that data appropriately for an ARDP v1 protocol
   security context block. 
   It creates a single huge security context block.  When the block is created,
   we paste it to the end of the ARDP request.  */

/* ****************************************
 * Current picture (5/13/97):
 * Everything that's not an encryption block itself can be divided into two
 classes: needs encryption and doesn't need encryption.
 This leads to the structure and ordering:

 A) Stuff to be encrypted:
 A.1) labels (by default) & authenticators (by request)
 A. 2) integrity, (by request)

 B: Encryption itself:
   B.1: The cleartext (including cleartext security context) is
    destroyed. 
    Left is 1 security context block (more than 1 if multiple parallel
    encryption methods), containing the cyphertext

 C: Stuff not to be encrypted:
      C.1 authentication (by default) and labels (if requested)
      C.2 integrity (by default)

As of this writing (1/98), we do not encrypt any of the security context.  That
turned out to be just a little bit too much for us to hack right now, under
current deadline pressure. We expect to do this later.

 *************************************/

enum ardp_errcode
ardp_sec_commit(RREQ req)
{
    ardp_sectype *secp; 
    /* Flag. Set to nonzero if we destroy the cleartext when done -- this is
       set if we performed any encryption. */
    int destroy_the_cleartext_at_end = 0;
    assert( ! req->sec);

    /* Stuff needing encryption first. */
    /* A.1: The authentication should have be done by the time we call
       ardp_sec_commit(); don't need to DO any.  Just copy
       operations. .
       Labels have no special processing, other than being pasted out to
       provide cleartext for future encryption operation. */
    for (secp = req->secq; secp; secp = secp->next) {
	if (!secp->privacy)
	    continue;
	if (secp->service != ARDP_SEC_AUTHENTICATION &&
	    secp->service != ARDP_SEC_LABELS)
	    continue;
	if (secp->processing_state == ARDP__SEC_PREP_FAILURE)
	    goto cannot_commit_this_one_a1;
	/* Exclude the middle -- things that might be in other states of
	   processing. */ 
	if (secp->processing_state != ARDP__SEC_PREP_SUCCESS)
	    continue;
	if((secp->error_code = do_commit(req, secp)))
	    goto cannot_commit_this_one_a1;
	secp->processing_state = ARDP__SEC_COMMITTMENT_SUCCESS;
	continue;
	/* We never get here by falling through */
    cannot_commit_this_one_a1:
	if (!secp->error_code)
	    secp->error_code = ARDP_FAILURE;
	secp->processing_state = ARDP__SEC_COMMITTMENT_FAILURE;
	if (secp->criticality)
	    return secp->error_code;
	/* Just continue on if it's not a critical failure. */
    }
    /* A.2: Privacy-protected Integrity */
    for (secp = req->secq; secp; secp = secp->next) {
	if (!secp->privacy)
	    continue;
	if (secp->service != ARDP_SEC_INTEGRITY)
	    continue;
	if (secp->processing_state == ARDP__SEC_PREP_FAILURE)
	    goto cannot_commit_this_one_a2;
	/* Exclude the middle -- things that might be in other states of
	   processing. */ 
	if (secp->processing_state != ARDP__SEC_PREP_SUCCESS)
	    continue;
	if((secp->error_code = do_commit(req, secp)))
	    goto cannot_commit_this_one_a2;
	secp->processing_state = ARDP__SEC_COMMITTMENT_SUCCESS;
	continue;
	/* We never get here by falling through */
    cannot_commit_this_one_a2:
	if (!secp->error_code)
	    secp->error_code = ARDP_FAILURE;
	secp->processing_state = ARDP__SEC_COMMITTMENT_FAILURE;
	if (secp->criticality)
	    return secp->error_code;
	/* Just continue on if it's not a critical failure. */
    }
    /* B: ENCRYPTION (XXX IN PROGRESS)*/
    for (secp = req->secq; secp; secp = secp->next) {
	if (secp->service != ARDP_SEC_PRIVACY)
	    continue;
	if (secp->processing_state == ARDP__SEC_PREP_FAILURE)
	    goto cannot_commit_this_one_b;
	/* Exclude the middle -- things that might be in other states of
	   processing. */ 
	if (secp->processing_state != ARDP__SEC_PREP_SUCCESS)
	    continue;
	if((secp->error_code = do_commit(req, secp)))
	    goto cannot_commit_this_one_b;
	++destroy_the_cleartext_at_end;
	secp->processing_state = ARDP__SEC_COMMITTMENT_SUCCESS;
	continue;
	/* We never get here by falling through */
    cannot_commit_this_one_b:
	if (!secp->error_code)
	    secp->error_code = ARDP_FAILURE;
	secp->processing_state = ARDP__SEC_COMMITTMENT_FAILURE;
	if (secp->criticality)
	    return secp->error_code;
	/* Just continue on if it's not a critical failure. */
    }
    if (destroy_the_cleartext_at_end) {
	ardp_ptlfree(req->outpkt);
	req->outpkt = NULL;
    }
    /* ENCRYPTION DONE */
    
    /* C1: Cleartext auth. and labels */
    for (secp = req->secq; secp; secp = secp->next) {
	if (secp->service != ARDP_SEC_AUTHENTICATION &&
	    secp->service != ARDP_SEC_LABELS)
	    continue;
	if (secp->processing_state == ARDP__SEC_PREP_FAILURE)
	    goto cannot_commit_this_one_c1;
	/* Exclude the middle -- things that might be in other states of
	   processing (including things in the ARDP__SEC_COMMITTMENT_SUCCESS or
	   ARDP__SEC_COMMITTMENT_FAILURE states). */
	if (secp->processing_state != ARDP__SEC_PREP_SUCCESS)
	    continue;
	assert (!secp->privacy);
	if((secp->error_code = do_commit(req, secp)))
	    goto cannot_commit_this_one_c1;
	secp->processing_state = ARDP__SEC_COMMITTMENT_SUCCESS;
	continue;
	/* We never get here by falling through */
    cannot_commit_this_one_c1:
	if (!secp->error_code)
	    secp->error_code = ARDP_FAILURE;
	secp->processing_state = ARDP__SEC_COMMITTMENT_FAILURE;
	if (secp->criticality)
	    return secp->error_code;
	/* Just continue on if it's not a critical failure. */
    }
    
    /* C.2: Clear-Text (unencrypted) Integrity */
    for (secp = req->secq; secp; secp = secp->next) {
	if (secp->service != ARDP_SEC_INTEGRITY)
	    continue;
	if (secp->processing_state == ARDP__SEC_PREP_FAILURE)
	    goto cannot_commit_this_one_c2;
	/* Exclude the middle -- things that might be in other states of
	   processing. */ 
	if (secp->processing_state != ARDP__SEC_PREP_SUCCESS)
	    continue;
	assert (!secp->privacy);
	if((secp->error_code = do_commit(req, secp)))
	    goto cannot_commit_this_one_c2;
	secp->processing_state = ARDP__SEC_COMMITTMENT_SUCCESS;
	continue;
	/* We never get here by falling through */
    cannot_commit_this_one_c2:
	if (!secp->error_code)
	    secp->error_code = ARDP_FAILURE;
	secp->processing_state = ARDP__SEC_COMMITTMENT_FAILURE;
	if (secp->criticality)
	    return secp->error_code;
	/* Just continue on if it's not a critical failure. */
    }
    /* Done adding items to SEC. */

    /* Now add the completed 'sec' packets to the existing messages on the
       RREQ's OUTPKT member. */
    CONCATENATE_LISTS(req->outpkt, req->sec);
    req->sec = NULL;		/* make it clear that req->sec is departed. */
    
    return ARDP_SUCCESS;
}


/* do_commit() This writes out the actual bytes of the block. 
   The caller handles the criticality bit; this function just backtracks if
   problems arise and lets the higher level decide whether to abort or ignore
   the error.
   */
/* This routine should only be called on secp with ARDP__SEC_PREP_SUCCESS. */
/* At the end of this routine, secp's processing_state will probably be
   reset to one of:
     ARDP__SEC_COMMITTMENT_SUCCESS,
     ARDP__SEC_COMMITTMENT_FAILURE, */

static
enum ardp_errcode 
do_commit(RREQ req, ardp_sectype *secp)
{
    /* write out header */
    unsigned char ctmp;		/* tmp one byte quantity */
    u_int32_t dummy_4byte = 0;
    enum ardp_errcode retval = ARDP_SUCCESS; /* error code from underlying
						mechanism */ 
    void *context_length_pos;	/* This really points to a u_int32_t, but to
				   one that might not be on an appropriately
				   aligned boundary for the machine.
				   Therefore, it is not appropriate to make it
				   a u_int32_t *  */
    /* length count: length of the arguments or additional data attached to
       this particular security mechanism. */  
    u_int32_t arg_length;
    struct ardp__sec_mark_position whereIwas;
    int old_seclen = 0;
    unsigned char *lenpos;	/* where is this length count stored? We store 
			       it as:
			       <length-count> 
			       <length-count bytes of argument> */


    if (ardp_debug >= 4) {
	rfprintf(stderr, "ardp_sec_commit(): do_commit():"
		" context #%u, mechanism %s\n",
		secp->seq, ardp_sec_mechanism_name(secp));
    }
    if (secp->processing_state != ARDP__SEC_PREP_SUCCESS) {
	if (ardp_debug) {
	    rfprintf(stderr, "ARDP security: Inappropriate call to do_commit():"
		    " called on a block whose status is not"
		    " ARDP__SEC_PREP_SUCCESS.\n" );
	}
	return ARDP_CONTEXT_FAILURE;
    }
    /* If starting the security context, make room for the 4-byte length
       count of the whole security context for this request.  (The current
       implementation on the sending side doesn't use the protocol feature that
       allows one to start a new security context in later packets; we just
       create one security context, which we append to the text message
       (payload).)  This count (indicated by context_length_pos) applies to the
       entire security context started in this packet, including the 4-byte
       total length count and the length of each block in this packet's
       security context . 
       */ 
    if (!req->sec) {
	ardp__sec_add2secdata4req(req, ARDP_A2R_NOSPLITB, &dummy_4byte, 4);
	/* Assure that ardp__sec_add2secdata4req() is leaving ioptr & seclen set
	   appropriately. */
	assert(req->sec->ioptr == req->sec->start + 4);
	assert(req->seclen == 4);
    }
    whereIwas = ardp__sec_mark_position(req);
    /* context_length_pos also serves as a marker for us, so that we can revert
       if we need to. */
    /* The start of the security context is always the place where the
       four-byte count is stored.  Mark its location. */
    context_length_pos = req->sec->start;

    /* Now write out the information for the individual block. */
    
    ctmp = secp->service;
    if (secp->criticality)
	ctmp |= 0x80;
    if (secp->me)		/* we're sending information. */
	ctmp |= 0x40;
    if (secp->requesting_from_peer)
	ctmp |= 0x20;
    ardp__sec_add2secdata4req(req, ARDP_A2R_NOSPLITB, &ctmp, 1);

    ctmp = secp->mechanism;
    ardp__sec_add2secdata4req(req, ARDP_A2R_NOSPLITB, &ctmp, 1);

    /* Add in the sequence number // reference number of this particular 
       item.  */
    dummy_4byte = htonl(secp->seq);
    ardp__sec_add2secdata4req(req, ARDP_A2R_NOSPLITB, &dummy_4byte, 4);

    /* Add a 4-byte blank (receptacle for four byte quantity) into the security
       context packet being assembled.  
       The 4 bytes start at 'lenpos'.  They represent the length of the
       arguments to the current security context block being worked on. */ 
#ifndef NDEBUG
    dummy_4byte = 0;		/* set to zero, so we don't have spurious
				   values there that might confuse us while
				   debugging. */
#endif
    ardp__sec_add2secdata4req(req, ARDP_A2R_NOSPLITB, &dummy_4byte, 4);

    /* The last packet in the list (the packet currently being worked on) is
       req->sec->previous.  

       We check its value after the call to ardp__sec_add2secdata4req(), not
       before, since ardp__sec_add2secdata4req() may have a new packet to fit
       in the bytes just written. */
    lenpos = req->sec->previous->ioptr - sizeof dummy_4byte;

    old_seclen = req->seclen;	/* How many bytes had already been written?
				   Save this so we can calculate how many
				   new ones the arguments took up.  The
				   four-byte count itself is here. */

    /* write out the payload: this is a service-specific operation performed by
       secp->commit(). */
    /* Might not need to write out the payload if things already OK. */
    if(secp->commit && (retval = secp->commit(req, secp))) {
	/* If problems, clean up and go away. */
	ardp__sec_restore_position(req, whereIwas);
	secp->processing_state = ARDP__SEC_COMMITTMENT_FAILURE;
	if (ardp_debug) {
	    rfprintf(stderr, "ardp security: do_commit(): block #%d, mech %s"
		    " failed: COMMITTMENT_FAILURE\n",
		    secp->seq, ardp_sec_mechanism_name(secp));
	}
	return retval;
    }
    /* Write the length of the block's arguments. */
    arg_length = (req->seclen - old_seclen);
    dummy_4byte = htonl(arg_length);
    /* Copy the length of the arguments for this particular block to the
       (marked) position. */ 
    /* Lenpos might not point to a (sizeof arg_length)-byte boundary. */
    memcpy4(lenpos, &dummy_4byte);

    /*** Each time we write out a new security context block, we update the
      security context length that we store at the start of the first security
      context block. */
    /* We use memcpy4() here in order to avoid a
       potential Bus Error (word alignment error), since there is no guarantee
       that context_len_pos  will point to a byte on a 32-bit integer boundary.
       */ 
    dummy_4byte = htonl(req->seclen);
    memcpy4(context_length_pos, &dummy_4byte);
    secp->processing_state = ARDP__SEC_COMMITTMENT_SUCCESS;
    assert(retval == ARDP_SUCCESS);
    if (ardp_debug >=4) {
	rfprintf(stderr, "ardp security: do_commit(): block #%d, mech %s"
		" success: COMMITTMENT_SUCCESS\n",
		secp->seq, ardp_sec_mechanism_name(secp));

    }
    return retval;
}
#endif /* !defined(ARDP_NO_SECURITY_CONTEXT) */
