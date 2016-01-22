/*
 * Copyright (c) 1996, 1997, 1998 by the University of Southern California
 *
 * For copying and distribution information, please see the file
 * <usc-license.h>.
 */
/*
 * Authors: Katia Obraczka <katia@ISI.EDU>, Steven Augart <swa@ISI.EDU>
 */

#include <usc-license.h>
#ifndef ARDP_NO_SECURITY_CONTEXT
#include <ardp.h>
#include "ardp_sec.h"

static enum ardp_errcode
dispatch_authentication_mechanism(RREQ req, 
				  ardp_sectype *newctxt,
				  PTEXT pkt_context_started_in, 
				  enum ardp__sec_authentication_mechanism 
				  sec_mechanism, 
				  const char *arg, 
				  int arglen);

static enum ardp_errcode
dispatch_integrity_mechanism(RREQ req, 
			     ardp_sectype *newctxt,
			     PTEXT pkt_context_started_in, 
			     enum ardp__sec_integrity_mechanism sec_mechanism,
			     const char *arg, int arglen);

static enum ardp_errcode
dispatch_privacy_mechanism(RREQ req, ardp_sectype *newctxt,
			   PTEXT pkt_context_started_in, 
			     enum ardp__sec_privacy_mechanism sec_mechanism,
			     const char *arg, int arglen);




/* This dispatches for the receiver.  
   There is no corresponding dispatch function for the sender.  This is because
   we already have the appropriate ardp_sectype when we call
   ardp_req_security() on the sender side. */
/* XXX Replace this with a call to the function that looks up sectype by
   service and mechanism; that will mean one less place to have special case
   code for each mechanism. */
enum ardp_errcode
ardp__sec_dispatch_service_receiver(
    RREQ req, ardp_sectype *newctxt, 
    /* XXX All of these remaining arguments are in fact derivable from newctxt.
       Indeed, they are passed directly from it.  
       I am not changing the interface now because it works and we have a
       deadline.  --swa 1/98 */
    PTEXT pkt_context_started_in, 
    enum ardp__sec_service sec_service, int sec_mechanism, 
    char *arg, int arglen)
{
    if (ardp_debug > 13) {
	rfprintf(stderr, "ardp_sec_dispatch_service_receiver():"
		" svc name: %s: Args length is %d bytes, data is: ",
		ardp_sec_mechanism_name(newctxt), arglen);
	ardp_showbuf(arg, arglen, stderr);
	fputc('\n', stderr);

    }
    /* Either the 'newctxt' was  put onto the secq or it was deferred. */
    /* We will later add an 'oldctxt' paremeter.  Right now, we do the matching
       up in the individual receipt-processing functions. */
    assert(req->secq && 
	   ((req->secq->previous == newctxt) 
	    || newctxt->processing_state == ARDP__SEC_IN_PROCESS_DEFERRED));

    switch(sec_service) {
    case ARDP_SEC_PAYMENT:
	break;
    case ARDP_SEC_INTEGRITY:
	return dispatch_integrity_mechanism(req, newctxt, 
					    pkt_context_started_in, 
					    sec_mechanism, arg, arglen);
	break;
    case ARDP_SEC_AUTHENTICATION:
	return dispatch_authentication_mechanism(
	    req, newctxt, pkt_context_started_in, sec_mechanism, arg, arglen);
	break;
    case ARDP_SEC_PRIVACY:
	return dispatch_privacy_mechanism(
	    req, newctxt, pkt_context_started_in, sec_mechanism, arg, arglen);
	break;
    case ARDP_SEC_LABELS:
#ifdef ARDP_SEC_HAVE_LABELS_CLASS_OF_SERVICE
	if (sec_mechanism == ARDP_SEC_LABELS_CLASS_OF_SERVICE)
	    return ardp__sec_extract_class_of_service_tags(
		req, newctxt, arg, arglen);
	else
#endif
	    return ARDP_FAILURE; /* unknown LABELS mechanism */
    default:
	if (ardp_debug) {
	    rfprintf(stderr, "ardp__sec_dispatch_service_receiver(): unsupported \
security service #%d\n", sec_service);
	}
    }
    return ARDP_FAILURE;	/* didn't process anything. */
}



static 
enum ardp_errcode
dispatch_authentication_mechanism(
    RREQ req, 
    ardp_sectype *newctxt,
    PTEXT GL_UNUSED_C_ARGUMENT pkt_context_started_in, 
    enum ardp__sec_authentication_mechanism sec_mechanism, 
    const char *arg, 
    int arglen)
{
    if (ardp_debug > 13) {
	rfprintf(stderr, "ardp: dispatch_authentication_mechanism():"
		" svc name: %s: Args length is %d bytes, data is: ",
		ardp_sec_mechanism_name(newctxt), arglen);
	ardp_showbuf(arg, arglen, stderr);
	fputc('\n', stderr);

    }
    switch(sec_mechanism) {
	/* Kerberos authentication currently uses the mechanisms that 
	   Kerberos integrity uses. */
#ifdef ARDP_SEC_HAVE_AUTHENTICATION_ASRTHOST
    case ARDP_SEC_AUTHENTICATION_ASRTHOST:
	return ardp__sec_authentication_asrthost_accept_message(
	    req, newctxt, arg, arglen);
	break;
#endif	
#ifdef ARDP_SEC_HAVE_AUTHENTICATION_KERBEROS
    case ARDP_SEC_AUTHENTICATION_KERBEROS:
	if (req->trns)		/* we've transmitted a message already, so must
				 be the client.  (Server receives before
				 transmitting) */
	    return ardp__sec_authentication_kerberos_client_verify_message(
		req, newctxt, arg, arglen);
	else return ardp__sec_authentication_kerberos_server_verify_message\
	    (req, newctxt, arg, arglen);
	break;
#endif
    default:
	/* The unsupported context is not necessarily criticial, but if it's
	   not critical then this error message won't be propagated very far
	   up. */  
	return ARDP_CRITICAL_CONTEXT_NOT_SUPPORTED;
    }
    internal_error("Can't get here.");
}


static
enum ardp_errcode
dispatch_integrity_mechanism(RREQ req, 
			     ardp_sectype *newctxt,
			     PTEXT GL_UNUSED_C_ARGUMENT pkt_context_started_in,
			     enum ardp__sec_integrity_mechanism sec_mechanism,
			     const char *arg, int arglen)
{
    register enum ardp_errcode retval;
    switch(sec_mechanism) {
#ifdef ARDP_SEC_HAVE_INTEGRITY_CRC
    case ARDP_SEC_INTEGRITY_CRC:
	retval = verify_message_checksum(req->rcvd, newctxt, arg, arglen);
	if (ardp_debug) {
	    rfprintf(stderr, "We got a message with a CRC checksum."
		    " Verification returned %d \n", retval);
	}
	return retval;
	break;
#endif
#ifdef ARDP_SEC_HAVE_INTEGRITY_KERBEROS
    case ARDP_SEC_INTEGRITY_KERBEROS:
	return ardp__sec_integrity_kerberos_verify_message(
	    req, newctxt, arg, arglen);
	break;
#endif
    default:
	/* The unsupported context is not necessarily criticial, but if it's
	   not critical then this error message won't be propagated very far
	   up. */  
	return ARDP_CRITICAL_CONTEXT_NOT_SUPPORTED;
    }
    internal_error("Can't get here.");
}


static
enum ardp_errcode
dispatch_privacy_mechanism(RREQ req, 
			   ardp_sectype *newctxt,
			   PTEXT GL_UNUSED_C_ARGUMENT pkt_context_started_in, 
			   enum ardp__sec_privacy_mechanism sec_mechanism,
			   const char *arg, int arglen)
{
    register enum ardp_errcode retval;
    switch(sec_mechanism) {
#ifdef ARDP_SEC_HAVE_PRIVACY_ROT13
    case ARDP_SEC_PRIVACY_ROT13:
	retval = decrypt_message_rot13(req->rcvd, newctxt, arg, arglen);
	if (ardp_debug) {
	    rfprintf(stderr, "We got a message with Rot13 pseudo-encryption"
		    " Verification returned %d \n", retval);
	}
	return retval;
	break;
#endif
#ifdef ARDP_SEC_HAVE_PRIVACY_KERBEROS
    case ARDP_SEC_PRIVACY_KERBEROS:
	return ardp__sec_privacy_kerberos_decrypt_message(
	    req, newctxt, arg, arglen);
	break;
#endif
    default:
	/* The unsupported context is not necessarily criticial, but if it's
	   not critical then this error message won't be propagated very far
	   up. */  
	return ARDP_CRITICAL_CONTEXT_NOT_SUPPORTED;
    }
    internal_error("Can't get here.");
}


#endif /*  ARDP_NO_SECURITY_CONTEXT */


