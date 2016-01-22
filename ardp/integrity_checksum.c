/*
 * Copyright (c) 1991-1996 by the University of Southern California
 *
 * For copying and distribution information, please see the file
 * <usc-license.h>.
 */

#include <usc-license.h>


#ifndef ARDP_NO_SECURITY_CONTEXT
#include <ardp.h>
#include "ardp_sec.h"

#ifdef ARDP_SEC_HAVE_INTEGRITY_CRC
/* Obsolete prototypes: */
/* static enum ardp_errcode v_integrity_checksum_algorithm(RREQ, int me, 
					  int you, va_list ap); */
/* static void integrity_checksum_callback(RREQ, ...); */
static u_int32_t compute_integ_checksum(PTEXT data);

static enum ardp_errcode parse_crc_args(RREQ req, ardp_sectype *ref,
					va_list ap);
static enum ardp_errcode v_integrity_crc_commit(RREQ req, ardp_sectype *s);

ardp_sectype ardp_sec_integrity_crc = {
    UNINITIALIZED,
    ARDP_SEC_INTEGRITY,		/* .service. */
    ARDP_SEC_INTEGRITY_CRC,	/* .mechanism. */
    parse_crc_args,		/* .parse_arguments. */
    NULL,			/* .mech_spec_free. (optional) */
    v_integrity_crc_commit,	/* .commit. */
    ARDP__SEC_STATIC_CONSTANT,	/* .processing_state. */
    0,				/* .criticality. (default) */
    NULL,			/* .args. (default) */
    1,				/* .me. (I should provide this info.) */
    1,				/* .requesting_from_peer. */
    /* rest default to zero or null */
};


/* Returns ARDP_SUCCESS or ARDP_FAILURE */
/* Verifies checksum across entire RREQ.
   Go through the ->rcvd member  of the RREQ. 
a   The ->text member of the RREQ points to the start of the payload/text area;
   it is past the security context and all other contexts. */

enum ardp_errcode
verify_message_checksum(
    PTEXT rcvd, ardp_sectype *secref, const char *arg, int arglen)
{
    u_int32_t rcvd_checksum;		/* network byte order. */
    if (secref->peer_set_me_bit) {
	if (arglen < 4) {
	    secref->processing_state = ARDP__SEC_PROCESSED_FAILURE;
	    return ARDP_FAILURE;
	}
	memcpy4(&rcvd_checksum, arg);
	if (compute_integ_checksum(rcvd) == rcvd_checksum) {
	    secref->processing_state = ARDP__SEC_PROCESSED_SUCCESS;
	    if (ardp_debug)
		rfprintf(stderr, "ardp: received checksum verified correct.\n");
	} else {
	    secref->processing_state = ARDP__SEC_PROCESSED_FAILURE;
	    if (ardp_debug)
		rfprintf(stderr, "ardp: FAILURE: received checksum did not verify correct.\n");
	    if (secref->requested_by_peer)
		/* We could mark PREP_SUCCESS, if we only had a way to record
		   that the processing was still failed.  This will have to 
		   be changed -- turn processing_state into reception_state and
		   preparation_state */
		secref->processing_state = ARDP__SEC_PREP_FAILURE;
	    return ARDP_FAILURE;
	}
    } else {
	/* Automatic success. */
	secref->processing_state = ARDP__SEC_PROCESSED_SUCCESS;
    }	
    /* Successful processing, either automatic or otherwise. */
    assert(secref->processing_state == ARDP__SEC_PROCESSED_SUCCESS);
    if (secref->requested_by_peer)
	secref->processing_state = ARDP__SEC_PREP_SUCCESS;
    return ARDP_SUCCESS;
}

/* Return an integrity checksum, calculated in network byte order. */
/* Calculate it over the text/data area of the packet. */
static
u_int32_t
compute_integ_checksum(PTEXT data)
{
    PTEXT pkt = data;
    u_int32_t	checksum = 0;	/* if no packets(!) defaults to zero */

    /* compute checksum over all the data */
    for (pkt = data; pkt; pkt = pkt->next) {
	int datalen;		/* length of data (payload) area */
	int i;			/* index into data (payload) area */

	datalen = pkt->length - (pkt->text - pkt->start);

	for (i = 0; i < datalen; ++i) {
	    checksum += ((unsigned char *) pkt->text)[i] 
		<< 8 * (((4) - 1) - (i % 4));
	    
	}
    }
    return htonl(checksum);
}

static enum ardp_errcode 
parse_crc_args(RREQ GL_UNUSED_C_ARGUMENT req, 
	       ardp_sectype *ref, 
	       va_list GL_UNUSED_C_ARGUMENT ap)
{
    ref->processing_state = ARDP__SEC_PREP_SUCCESS;
    return ARDP_SUCCESS;
}


/* 1) must be called with all data present */
/* 2) all packets to be sent out are on the TRNS member; OUTPKT has been
   cleared. */
/* Calculate a checksum on the data area of each packet.  Take the data octets
   4 bytes at a time.  If any packet's data area is not a multiple of 4 octets,
   calculate the checksum as if the data area were padded out with zero-valued
   octets. */
/* The data area of each packet is considered to exclude the contexts for the
   purposes of this calculation. */

static
enum ardp_errcode
v_integrity_crc_commit(RREQ req, ardp_sectype * GL_UNUSED_C_ARGUMENT s)
{
    u_int32_t ltmp;
    u_int32_t	checksum = 0;	/* if no packets(!) defaults to zero.  Also
				   needs to be zero as a starting point. */

    assert(!req->trns);	/* condition for calling this function.  */
    checksum = compute_integ_checksum(req->outpkt);
    ltmp = htonl(checksum);
    ardp__sec_add2secdata4req(req, ARDP_A2R_NOSPLITB,
			      /* Take the last four bytes of the checksum, pass
				 those down to the item. */
			      (char *) &ltmp, sizeof ltmp);
    return ARDP_SUCCESS;
}



#endif /* HAVE_... */
#endif /* ndef ARDP_NO_SECURITY_CONTEXT */
