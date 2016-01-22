/*
 * Copyright (c) 1991-1996 by the University of Southern California
 *
 * For copying and distribution information, please see the file
 * <usc-license.h>.
 */

#include <usc-license.h>


#ifndef ARDP_NO_SECURITY_CONTEXT


#include <pmachine.h>		/* machdep. types */
#include "ardp_sec_config.h"	/* overrides ardp.h? */
#include <ardp.h>
#include "ardp_sec.h"

#ifdef ARDP_SEC_HAVE_INTEGRITY_KERBEROS
#include "ardp_sec_members.h"
#include <krb5.h>
#include <com_err.h>		/* com_err() prototype */

#include <sys/socket.h>		/* defines AF_INET */
#include <malloc.h>		/* prototype for malloc() */
#include <ctype.h>		/* isupper(), tolower() */

static enum ardp_errcode ik_parse_args(RREQ, ardp_sectype *, va_list);
static enum ardp_errcode ik_commit(RREQ, ardp_sectype *);
static enum ardp_errcode ik_create(RREQ req, ardp_sectype *integref,
				   ardp_sectype *authref);
static enum ardp_errcode ik_verify(
    RREQ req, ardp_sectype *integref, ardp_sectype *authref,
    char *arg, int arglen);

ardp_sectype ardp_sec_integrity_kerberos = {
    UNINITIALIZED,
    ARDP_SEC_INTEGRITY, 
    ARDP_SEC_INTEGRITY_KERBEROS, 
    ik_parse_args,			/* parse args */
    NULL,			/* mech_spec_free() */
    ik_commit,			/* commit */
    /* Dynamically updated. */
    ARDP__SEC_STATIC_CONSTANT,	/* .processing_state. */
    0,				/* .criticality. */
    NULL,			/* .args. */
    1,				/* .me.: I should provide this */
    1,				/* .requesting_from_peer.: */
    0,				/* .requested_by_peer. */
    0,				/* .privacy.: privacy requested */
    ARDP_SUCCESS,		/* .error_code. This is a cached error code.  */
    {},				/* .mech_spec. */
    NULL,			/* .ticket.: Kerberos ticket */
    NULL,			/* .auth_context.: Kerberos auth context */
    NULL,			/* k5context: Kerberos context */
    0,				/* .seq. */
    NULL,			/* .next. */
    NULL			/* .previous. */
};


/* The ardp_req_security() call has a reference to the authorization context
   we are basing this on.  That is its only variadic  argument. */
/* This supersedes ardp__sec_integrity_kerberos_v_create_client() */
static
enum ardp_errcode
ik_parse_args(RREQ req, ardp_sectype *integref, va_list ap)
{
    ardp_sectype *authref = va_arg(ap, ardp_sectype *);
	
    return ik_create(req, integref, authref);
}


/* We need to mark authref as the one to be used when we commit.
 */
static
enum ardp_errcode
ik_create(RREQ GL_UNUSED_C_ARGUMENT req, 
	  ardp_sectype *integref, ardp_sectype *authref)
{
    if (authref->service != ARDP_SEC_AUTHENTICATION
	|| authref->mechanism != ARDP_SEC_AUTHENTICATION_KERBEROS) {
	if (ardp_debug) {
	    rfprintf(stderr, "ardp: integrity_kerberos (ik_create()): Need an"
		    " AUTHENTICATION_KERBEROS authenticator; didn't get"
		    " one.\n");
	}
	return ARDP_FAILURE; 	/* Bogus authentication system. */
    }
    if (authref->processing_state != ARDP__SEC_PREP_SUCCESS) {
	if (ardp_debug)
	    rfprintf(stderr, "ardp: integrity_kerberos (ik_create()):"
		    " Passed an authenticator which had not been successfully"
		    " prepared!  Can't proceed.\n");
	return ARDP_FAILURE;	/* Bogus authenticator. */
    }
    /* Setting integref->processing_state duplicates work done by
       ardp_req_security(). */ 
    integref->processing_state = ARDP__SEC_PREP_SUCCESS;
    integref->mech_spec[IK_AUTHREF].ptr = authref;
    return ARDP_SUCCESS;
}   


/* Prepare the krb_safe message and write out the formatted message. */
static 
enum ardp_errcode 
ik_commit(RREQ req, ardp_sectype *integref)
{
    u_int32_t tmp32;
    krb5_error_code	k5err;
    krb5_data ksafe_packet, inbuf;
    ardp_sectype *authref;

    authref = integref->mech_spec[IK_AUTHREF].ptr;
    /* Here, we need to convert the data part of the request into a single long
       string; that's the format Kerberos expects (wasteful of memory and
       processing time) */
    /* This is the data that will be the safe message.  It is just the
       payload. */
    ardp__sec_make_krb5_data_from_PTEXTs(req->outpkt, &inbuf);


    /* Actually make the safe message */
    if ((k5err = krb5_mk_safe(authref->k5context, authref->auth_context,
			      &inbuf, &ksafe_packet, NULL))){
	if (ardp_debug)
	    com_err("integrity_kerberos", k5err, "(while making KRB_SAFE message)");
	return ARDP_CONTEXT_FAILURE;
    }

    krb5_xfree(inbuf.data); /* make_krb5_data_from_PTEXTs()
			       used malloc() to allocate memory here; this
			       makes krb5_xfree() safe and legal.  */ 
    
    tmp32 = htonl(ksafe_packet.length);
    ardp__sec_add2secdata4req(req, ARDP_A2R_NOSPLITB /* flags */, 
			 &tmp32, sizeof tmp32);
    /* Send KSAFE information to server! */
    /* Actually add it to sec-context, to be sent later. */
    ardp__sec_add2secdata4req(req, 0 /* flags */, 
			 ksafe_packet.data, ksafe_packet.length);
    krb5_xfree(ksafe_packet.data); /*  krb5_mk_safe used malloc() */
    return ARDP_SUCCESS;
}

/* Decrypt and confirm the krb_safe message */
/* To call this function, you need the authref. */
static
enum ardp_errcode
ik_verify(RREQ req, 
	  ardp_sectype *integref, ardp_sectype *authref, 
	  char *arg, int arglen)
{
    u_int32_t tmp32;
    krb5_data ksafe_packet;
    const char *argptr = arg;
    /* These two should compare identical. */
    krb5_data payload_message, sec_context_message;
    krb5_error_code	k5err;
    
    memcpy4(&tmp32, argptr);
    ksafe_packet.length = ntohl(tmp32);
    argptr += sizeof tmp32;
    ksafe_packet.data = (char *) argptr; /* must discard const */
    if (ardp_debug && ksafe_packet.length + sizeof tmp32 < arglen) {
	rfprintf(stderr, "ardp: integrity_kerberos: ik_verify() got a"
		" message with length %d, of which we only used %d"
		" bytes.\n", arglen, ksafe_packet.length);
    }
    ardp__sec_make_krb5_data_from_PTEXTs(req->rcvd, &payload_message);

    if ((k5err = krb5_rd_safe(authref->k5context, authref->auth_context, 
			      &ksafe_packet, &sec_context_message, NULL))) {
	if (ardp_debug)
	    com_err("verify_message_integrity_kerberos()", k5err, 
		    "while verifying SAFE message"); 
	return ARDP_CONTEXT_FAILURE;
    }
    /* If a payload message was provided and it does not match the safe message
       in the security context, problem. */
    if (payload_message.length == 0) {
	/* Here, we can add code to recreate the payload.  this is just like
	   what we do for the priv case. */
    }
    if(memcmp(sec_context_message.data, payload_message.data,
	      payload_message.length) != 0) {
				/* if they do not match, we failed. */
	if (ardp_debug) {
	    rfprintf(stderr, 
		    "verify_message_integrity_kerberos(): Caught an attempted"
		    " spoof; the payload message didn't match Kerberos's"
		    " SAFE message's data content.\n");
	}
	return ARDP_CONTEXT_FAILURE;
    }
    return ARDP_SUCCESS;
}



/* Returns ARDP_SUCCESS or ARDP_FAILURE */
/* Verifies integrity checksum across entire RREQ.
   Go through the ->rcvd member  of the RREQ. 
   The ->text member of the RREQ points to the start of the payload/text area;
   it is past the security context and all other contexts. */

/* client-specific */
enum ardp_errcode
ardp__sec_integrity_kerberos_verify_message(
    RREQ req,
    ardp_sectype *integref,
    const char *arg, /* Here, this is:
			(#1) a 4-byte count for (2)
			(#2) the output buffer generated by
			the client's call to krb5_mk_safe().
		     */ 
    int arglen)
{
    enum ardp_errcode retval;
    ardp_sectype *authref = ardp__sec_find_krbauth_secref(req);

    if (!authref) {
	if (ardp_debug)
	    rfprintf(stderr, "ardp__sec_integrity_kerberos_verify_message():"
		    " Did not find a Kerberos authenticator that"
		    " we could use to verify the Kerberos safe message.\n");
	integref->processing_state = ARDP__SEC_PROCESSED_FAILURE;
	return ARDP_FAILURE;
    }
    integref->mech_spec[IK_AUTHREF].ptr = authref;
    if((retval = ik_verify(req, integref, authref, (char *) arg, arglen))) {
	integref->processing_state = ARDP__SEC_PROCESSED_FAILURE;
	return retval;
    }
    /* If we made it OK this far, then we have successfully prepared to commit
       a response! */
    integref->processing_state = ARDP__SEC_PROCESSED_SUCCESS;
    if (integref->requested_by_peer)
	integref->processing_state = ARDP__SEC_PREP_SUCCESS;
    return ARDP_SUCCESS;
}
#endif /* ARDP_SEC_HAVE_INTEGRITY_KERBEROS */
#endif /* ndef ARDP_NO_SECURITY_CONTEXT */
