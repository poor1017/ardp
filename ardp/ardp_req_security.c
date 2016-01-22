/*
 * Copyright (c) 1991-1996 by the University of Southern California
 *
 * For copying and distribution information, please see the file
 * <usc-license.h>.
 */

#include <usc-license.h>

#include <pmachine.h>
#include <ardp.h>

#ifndef ARDP_NO_SECURITY_CONTEXT
#include "ardp_sec.h"

#include <stddef.h>
#include <stdarg.h>


/* Design: ardp_req_security() is called by the client.  It is called once
   for each security context element/type/whatever.

   It adds security to an already otherwise complete Prospero request.
   E.g., if encryption or a checksum  is specified, the operation is performed
   on the whole request, whose payload packets are already present. 
   */
   
/* Should be called after ardp_add2req(); we'll just assume this is done
   properly. */
/* Can be called multiple times. */
/* Format Steve and Katia have been using (code implemented according to this
   one): */ 
/* This format is formally documented at 
   URL: http://GOST.ISI.EDU/info/ardp/ardp_protocol_sec_context.html */
/* 4 bytes: context length in bytes */
/* 1 byte: service (w/ crit. bit, me, peer) */
/* 1 byte: mechanism */
/* 2 bytes: block length */
/* block-length bytes: arguments/data */
/* Brian's proposed format for block: */
/* 2 byte count of # of blocks */
/* each block: six-byte hdr: 2 bytes: svc, 4 bytes length of block - 6 */
/* CURRENT ISSUES:
 * Coding issue: will need to update the context length count or # of packets
 * each time.  
 */
   
/* Design documentation:
 * If sec context fails, we could:
 * 1) erase half-written context; return error code from called s.algorithm().
 *    Client will probably send the request like that anyway.
 * 2) Some negotiation between ARDP and the application (Steve thinks that (1)
 *    would effectively permit this, if you wrote negotiation code in the
 *    client.)
 * 3) ardp_req_security() could destroy the request.  To do this, it
 *    would have to know which return values from s.algorithm() indicated
 *    failure and which success.  (We might assume 0 is ok and anything else
 *    is not.)
 *    
 * 4) ardp_req_security() could return with the request in a half-done
 *    state, but with no arguments to the security mechanism written.
 *    (Zero-length payload).  The caller chooses how to handle it.  The caller
 *    could clean up if it wanted to (remove the zero-argument part of the
 *    context), abort sending (call ardp_rqfree() on the request, or send the
 *    request as is.   
 *    [Steve: This is the easiest to implement]
 *
 * 5/8/97: The new deferred processing routines should handle the above problem
 *  more cleanly, since they never leave a request half-written before 
 *  discovering errors.
 */


enum ardp_errcode
ardp_req_security(RREQ req, 
		  const ardp_sectype s,
 		  ardp_sectype **refp,
		  ...)
{
    va_list ap;
    enum ardp_errcode retval = ARDP_SUCCESS; /* error code from underlying
						mechanism */ 
    ardp_sectype *Qelem;

    assert(s.processing_state == ARDP__SEC_STATIC_CONSTANT);
    if (ardp_debug >= 3) {
	rfprintf(stderr, "ardp_req_security(): Called with mechanism %s\n",
		ardp_sec_mechanism_name(&s));
    }
    if (req->peer_ardp_version < 1) {
	/* Security context not applicable before version 1 */
	return ARDP_BAD_VERSION;
    }
    Qelem = ardp_secopy(&s);
    /* If refp is null, we'll still allocate a new security context
       structure and put it on the queue; we just won't return a handle to
       it. */
    if (refp)
	*refp = Qelem;	
    if (req->secq)		/* if item is on the queue */
	Qelem->seq = req->secq->previous->seq + 1; /* new sequence # */
    else
	Qelem->seq = 1;		/* The first item on secq is number one. */
    APPEND_ITEM(Qelem, req->secq);
    /* On an algorithm-specific basis, read the arguments for later
       processing. */ 
    va_start(ap, refp);
    retval = s.parse_arguments(req, Qelem, ap);
    va_end(ap);
    if (retval) {
	Qelem->error_code = retval;
	Qelem->processing_state = ARDP__SEC_PREP_FAILURE;
    } else {
	Qelem->processing_state = ARDP__SEC_PREP_SUCCESS;
    }
    if (ardp_debug >= 4) {
	rfprintf(stderr, "Prepared request w/ sequence #%d.", Qelem->seq);
	rfprintf(stderr, "  processing_state is %s\n", 
		ardp_sec_processing_state_str(Qelem));
    }
    return retval;		/* return any error code from underlying
				   algorithm */
}
#endif /* ndef ARDP_NO_SECURITY_CONTEXT */

