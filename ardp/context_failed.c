/*

 * Copyright (c) 1996, 1997, 1998 by the University of Southern California
 *
 * For copying and distribution information, please see the file
 *  <usc-license.h>.
 *
 * Written  by bcn 1991     as transmit in rdgram.c (Prospero)
 * Modified by bcn 1/93     modularized and incorporated into new ardp library
 */

#include <ardp.h>
#include <ardp_sec.h>

#ifndef ARDP_NO_SECURITY_CONTEXT

/* This function is only called on the server. */
/*
 * reject_failed_ardp_context  (formerly context_failed)
 * - sends a message to the recipient indicating
 *   that its connection attempt has been refused because some critical context
 * (probably the security context) was not understood.
 * Pass zero for service and mechanism if unavailable.
 * errcode is currently a standard ARDP library error return code; we might
 * change this to another definition once we start using it.  
 * Right now, the peer will pass this error code back up to the caller.
 */
enum ardp_errcode
ardp__sec_reject_failed_ardp_context(RREQ req, char service, char mechanism, 
		    enum ardp_errcode errcode)
{
    PTEXT	r;
    short	cid = htons(req->cid);
    short	zero = 0;
    int		tmp;
    
    /* This code should never be called if the ARDP version is < 1) */
    if(req->peer_ardp_version < 1) 
	return(ARDP_FAILURE);

    r = ardp_ptalloc();

    {
	assert(req->peer_ardp_version == 1); /* ARDP v1 */
	r->length = 9;
	if (service && mechanism)
	    r->length += 3;
	r->start[0] = (char) 129;
	r->start[1] = 0;	/* XXX context later */
	r->start[2] = 0;	/* flags */
	r->start[3] = (unsigned char) 9;	/* Context not understood
						   option */ 
	r->start[4] = (char) r->length;	/* header length */
	memcpy2(r->start+5, &cid);  	/* Connection ID */
	memcpy2(r->start+7, &zero);  	/* Packet Sequence Number
					   (unseq. control pkt.) */ 
	if (service && mechanism) {
	    unsigned char uchar_tmp = errcode;
	    memcpy1(r->start + 9, &uchar_tmp);
	    memcpy1(r->start + 10, &service);
	    memcpy1(r->start + 11, &mechanism);
	}
    }    
    tmp = ardp_snd_pkt(r,req);
    ardp_ptfree(r);
    return(tmp);
}


#endif /* ndef ARDP_NO_SECURITY_CONTEXT */
 
