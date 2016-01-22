/*
 * Copyright (c) 1991-1998 by the University of Southern California
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

#ifdef ARDP_SEC_HAVE_PRIVACY_KERBEROS
#include "ardp_sec_members.h"
#include <krb5.h>
#include <com_err.h>		/* com_err() prototype */

#include <sys/socket.h>		/* defines AF_INET */
#include <malloc.h>		/* prototype for malloc() */
#include <ctype.h>		/* isupper(), tolower() */



/* this routine should be generally useful --swa, 1/98. */
static void splice_sublist_into_listp_before(
    PTEXT sublist, PTEXT *listp, PTEXT before);

static void
splice_in_cleartext(ardp_sectype *privref, 
		    krb5_data sec_context_message, 
		    PTEXT *req_rcvd);



static enum ardp_errcode pk_parse_args(RREQ, ardp_sectype *, va_list);
static enum ardp_errcode pk_commit(RREQ, ardp_sectype *);
static enum ardp_errcode pk_create(RREQ req, ardp_sectype *privref,
				   ardp_sectype *authref);
static enum ardp_errcode pk_decrypt(
    RREQ req, ardp_sectype *privref, ardp_sectype *authref,
    char *arg, int arglen);


ardp_sectype ardp_sec_privacy_kerberos = {
    UNINITIALIZED,
    ARDP_SEC_PRIVACY, 
    ARDP_SEC_PRIVACY_KERBEROS, 
    pk_parse_args,			/* parse args */
    NULL,			/* mech_spec_free() */
    pk_commit,			/* commit */
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
static
enum ardp_errcode
pk_parse_args(RREQ req, ardp_sectype *privref, va_list ap)
{
    ardp_sectype *authref = va_arg(ap, ardp_sectype *);
	
    return pk_create(req, privref, authref);
}


/* We need to mark authref as the one to be used when we commit.
 */
static
enum ardp_errcode
pk_create(RREQ GL_UNUSED_C_ARGUMENT req, 
	  ardp_sectype *privref, ardp_sectype *authref)
{
    if (authref->service != ARDP_SEC_AUTHENTICATION
	|| authref->mechanism != ARDP_SEC_AUTHENTICATION_KERBEROS) {
	if (ardp_debug) {
	    rfprintf(stderr, "ardp: privacy_kerberos (pk_create()): Need an"
		    " AUTHENTICATION_KERBEROS authenticator; didn't get"
		    " one.\n");
	}
	return ARDP_FAILURE; 	/* Bogus authentication system. */
    }
    if (authref->processing_state != ARDP__SEC_PREP_SUCCESS) {
	if (ardp_debug)
	    rfprintf(stderr, "ardp: privacy_kerberos (pk_create()):"
		    " Passed an authenticator which had not been successfully"
		    " prepared!  Can't proceed.\n");
	return ARDP_FAILURE;	/* Bogus authenticator. */
    }
    /* Setting privref->processing_state duplicates work done by
       ardp_req_security(). */ 
    privref->processing_state = ARDP__SEC_PREP_SUCCESS;
    privref->mech_spec[PK_AUTHREF].ptr = authref;
    return ARDP_SUCCESS;
}   


/* Prepare the krb_priv message and write out the formatted message. */
static 
enum ardp_errcode 
pk_commit(RREQ req, ardp_sectype *privref)
{
    u_int32_t tmp32;
    krb5_error_code	k5err;
    krb5_data kpriv_packet, inbuf;
    ardp_sectype *authref;

    authref = privref->mech_spec[PK_AUTHREF].ptr;
    /* Here, we need to convert the data part of the request into a single long
       string; that's the format Kerberos expects (wasteful of memory and
       processing time) */
    /* This is the data that will be the priv message.  It is just the
       payload. */
    ardp__sec_make_krb5_data_from_PTEXTs(req->outpkt, &inbuf);


    if (ardp_debug >= 11) {
	fputs("ardp: kerb priv:  pk_commit() before krb5_mk_priv(): ", stderr);
	ardp__sec_krb5_show_sequence_nums(authref->auth_context);
    }
    /* Actually make the priv message */
    if ((k5err = krb5_mk_priv(authref->k5context, authref->auth_context,
			      &inbuf, &kpriv_packet, NULL))){
	if (ardp_debug)
	    com_err("privacy_kerberos", k5err, "(while making KRB_PRIV message)");
	return ARDP_CONTEXT_FAILURE;
    }
    if (ardp_debug >= 11) {
	fputs("ardp: kerb priv:  pk_commit() after krb5_mk_priv(): ", stderr);
	ardp__sec_krb5_show_sequence_nums(authref->auth_context);
    }
    /* PRIVACY-specific: We do not need to destroy the cleartext here.
       Destroying the cleartext is done for us by ardp_sec_commit(). */

    krb5_xfree(inbuf.data); /* make_krb5_data_from_PTEXTs()
			       used malloc() to allocate memory here; this
			       makes krb5_xfree() safe and legal.  */ 
    
    tmp32 = htonl(kpriv_packet.length);
    ardp__sec_add2secdata4req(req, ARDP_A2R_NOSPLITB /* flags */, 
			 &tmp32, sizeof tmp32);
    /* Send KSAFE information to server! */
    /* Actually add it to sec-context, to be sent later. */
    ardp__sec_add2secdata4req(req, 0 /* flags */, 
			 kpriv_packet.data, kpriv_packet.length);
    krb5_xfree(kpriv_packet.data); /*  krb5_mk_priv used malloc() */
    return ARDP_SUCCESS;
}

/* Decrypt the krb_priv message */
/* To call this function, you need the authref. */
static
enum ardp_errcode
pk_decrypt(RREQ req, 
	  ardp_sectype *privref, ardp_sectype *authref, 
	  char *arg, int arglen)
{
    u_int32_t tmp32;
    krb5_data kpriv_packet;
    const char *argptr = arg;
    /* These two should compare identical. */
    krb5_data payload_message, sec_context_message;
    krb5_error_code	k5err;
    
    memcpy4(&tmp32, argptr);
    kpriv_packet.length = ntohl(tmp32);
    argptr += sizeof tmp32;
    kpriv_packet.data = (char *) argptr; /* must discard const */
    if (ardp_debug && (kpriv_packet.length + sizeof tmp32 < arglen)) {
	rfprintf(stderr, "ardp: privacy_kerberos: pk_decrypt() got a"
		" message with length %d, of which we only used %d"
		" bytes.\n", arglen, kpriv_packet.length);
    }
    ardp__sec_make_krb5_data_from_PTEXTs(req->rcvd, &payload_message);

    if (ardp_debug >= 11) {
	fputs("ardp: kerb priv: before krb5_rd_priv() in pk_decrypt(): ", stderr);
	ardp__sec_krb5_show_sequence_nums(authref->auth_context);
    }
    if ((k5err = krb5_rd_priv(authref->k5context, authref->auth_context, 
			      &kpriv_packet, &sec_context_message, NULL))) {
	if (ardp_debug)
	    com_err("verify_message_privacy_kerberos()", k5err, 
		    "while verifying PRIV message"); 
	return ARDP_CONTEXT_FAILURE;
    }
    if (ardp_debug >= 11) {
	fputs("ardp: kerb priv: after krb5_rd_priv() in pk_decrypt(): ", stderr);
	ardp__sec_krb5_show_sequence_nums(authref->auth_context);
    }
#if 0				/* Turning this off because it is a bogus test,
				   since req->rcvd also contains the security
				   context blocks. */
    /* PRIVACY-specific */
    /* In INTEGRITY, we verify the cleartext message (req->rcvd) against the
       payload carried in the security context and retrieved through Kerberos.
       Here in PRIVACY, we have to re-create req->rcvd from the decrypted
       security context so that the library's user (the application) can
       actually read the message where it expects to find it, on
       (req->rcvd). */ 
    if (req->rcvd) {
	if (ardp_debug) {
	    rfprintf(stderr, "decrypt_message_privacy_kerberos(): a cleartext"
		    " message present along with an encrypted one -- giving"
		    " up; hope that's OK -- you'll just see the cleartext"
		    " message.  This is probably an error on the"
		    " other end.\n");
	}
	return ARDP_CONTEXT_FAILURE;
    }
#endif
    /* When we're here, the first pass is being done over req->rcvd.  We can
       safely assume that, when time to read the cleartext comes, the
       application will start at req->rcvd. */
    /* Prepend this to whatever we read (req->rcvd).  Assume no cleartext.
       Don't free up the rest of the req->rcvd.  We don't free it up in case
       another security context shows up later. */ 
    splice_in_cleartext(privref, sec_context_message, &req->rcvd);
    return ARDP_SUCCESS;
}



static
void
splice_in_cleartext(ardp_sectype *privref, 
		    krb5_data sec_context_message, 
		    PTEXT *req_rcvd)
{
    PTEXT clearptexts;

    ardp__sec_make_PTEXTs_from_krb5_data(sec_context_message, &clearptexts);
    
    /* Put the (new?) cleartext into the right spot. */
    /* This will be helpful if we are ever in the multiple-context case, and
       where there are secret and non-secret messages. */
    splice_sublist_into_listp_before(clearptexts, req_rcvd,
				 privref->pkt_context_started_in);
}    

/* Should be generally useful.   Add to library later. XXX --swa 1/98*/
/* Two possible implementations.  One just chugs through sublist.  The other
   does more work, and won't detect garbage values. */
/* After this call, SUBLIST  no longer points to a valid doubly-linked
   list, although it does now point to the position of the old list,
   in *listp. */
static void 
splice_sublist_into_listp_before(
    PTEXT sublist, PTEXT *listp, PTEXT before)
{
    PTEXT tpkt;
    PTEXT begin = NULL, 
	  end = NULL;	/* Beginning and ending of *LISTP.  The
			   splice point is just before BEFORE */
    while (*listp != before) {
	assert(*listp);		/* Should only fail if 'before' is not a member
				 of *listp. */
	tpkt = *listp;
	EXTRACT_ITEM(tpkt, *listp);
	APPEND_ITEM(tpkt, begin);
    }
    assert(*listp == before);
    end = *listp;
    CONCATENATE_LISTS(begin, sublist);
    CONCATENATE_LISTS(begin, end);
    *listp = begin;
}


/* Returns ARDP_SUCCESS or ARDP_FAILURE */
/* Verifies privacy checksum across entire RREQ.
   Go through the ->rcvd member  of the RREQ. 
   The ->text member of the RREQ points to the start of the payload/text area;
   it is past the security context and all other contexts. */

/* client-specific */
enum ardp_errcode
ardp__sec_privacy_kerberos_decrypt_message(
    RREQ req,
    ardp_sectype *privref,
    const char *arg, /* Here, this is:
			(#1) a 4-byte count for (2)
			(#2) the output buffer generated by
			the client's call to krb5_mk_priv().
		     */ 
    int arglen)
{
    enum ardp_errcode retval;
    ardp_sectype *authref = NULL;

    if (privref->processing_state == ARDP__SEC_IN_PROCESS) {
	if (ardp_debug >= 3)
	    rfprintf(stderr, "ardp: privacy_kerberos: deferring decryption"
		    " for this pass\n");
	privref->processing_state = ARDP__SEC_IN_PROCESS_DEFERRED;
	return ARDP_SUCCESS;	/* We'll be back. */
    }
    authref = ardp__sec_find_krbauth_secref(req);
    if (!authref) {
	if (ardp_debug)
	    rfprintf(stderr, "ardp__sec_privacy_kerberos_decrypt_message():"
		    " Did not find a Kerberos authenticator that"
		    " we could use to verify the Kerberos priv message.\n");
	privref->processing_state = ARDP__SEC_PROCESSED_FAILURE;
	return ARDP_FAILURE;
    }
    privref->mech_spec[PK_AUTHREF].ptr = authref;
    if((retval = pk_decrypt(req, privref, authref, (char *) arg, arglen))) {
	privref->processing_state = ARDP__SEC_PROCESSED_FAILURE;
	return retval;
    }
    /* If we made it OK this far, then we have successfully prepared to commit
       a response! */
    privref->processing_state = ARDP__SEC_PROCESSED_SUCCESS;
    if (privref->requested_by_peer)
	privref->processing_state = ARDP__SEC_PREP_SUCCESS;
    return ARDP_SUCCESS;
}

#endif /* ARDP_SEC_HAVE_PRIVACY_KERBEROS */
#endif /* ndef ARDP_NO_SECURITY_CONTEXT */
