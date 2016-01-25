/* ardp/authentication_asrthost.c */
#ifndef ARDP_NO_SECURITY_CONTEXT

/*
 * Copyright (c) 1997 by the University of Southern California
 *
 * For copying and distribution information, please see the file
 * <usc-license.h>.
 */

#include <usc-license.h>

#include <gl_function_attributes.h>
#include "ardp_sec_config.h"	/* keytab locations.  May override stuff in
				   ardp.h  */
#include <ardp.h>		/* for RREQ structure, needed by ardp_sec.h */
#include "ardp_sec.h"		/* for prototypes, ardp_sectype, and the
				   definition of
				   ARDP_SEC_HAVE_AUTHENTICATION_ASRTHOST*/ 

#ifdef ARDP_SEC_HAVE_AUTHENTICATION_ASRTHOST
#include "ardp_sec_members.h"

#include <stddef.h>		/* for NULL */
#include <stdarg.h>		/* for va_arg */
#include <unistd.h>		/* getuid(), getreuid() */
#include <sys/malloc.h>		/* prototype of free(); used internally by
				   <krb5.h>   */


static enum ardp_errcode aa_commit(RREQ, ardp_sectype *);
static enum ardp_errcode aa_parse_args(RREQ req, struct ardp__sectype *secref,
				       va_list ap); 
static enum ardp_errcode aa_prep(struct ardp__sectype *secref);


const ardp_sectype ardp_sec_authentication_asrthost = {
    UNINITIALIZED,
    ARDP_SEC_AUTHENTICATION,	/* .service */
    ARDP_SEC_AUTHENTICATION_ASRTHOST, /* .mechanism */
    aa_parse_args,		/* .parse_arguments(). (client) */
    NULL,			/* .mech_spec_free.: optional:  */
    aa_commit,			/* .commit. */
    /* The following fields are all dynamically updated. */
    ARDP__SEC_STATIC_CONSTANT,	/* .processing_state.: this is an initializer
				 */ 
    0,				/* .criticality.: zero default in this case */
    NULL,			/* .args.: arguments given to us */
    1,				/* .me.: I should provide this */
    0,				/* .requesting_from_peer. */
    0,				/* .requested_by_peer. */
    0,				/* .privacy.: privacy requested */
    ARDP_SUCCESS,		/* .error_code. This is a cached error code.
				 */ 
    {},				/* .mech_spec.: empty initializer */
    NULL,			/* .ticket.: Kerberos ticket */
    NULL,			/* .auth_context.: Kerberos auth context */
    NULL,			/* k5context: Kerberos context */
    0,				/* seq */
    NULL,			/* .next. */
    NULL			/* .previous. */
};


/* arguments: none. */
/* Most of this is taken from the old Prospero routines. */
static 
enum ardp_errcode 
aa_parse_args(RREQ GL_UNUSED_C_ARGUMENT req, 
	      struct ardp__sectype *secref,
	      va_list GL_UNUSED_C_ARGUMENT ap)
{
    return aa_prep(secref);
}


static
enum ardp_errcode
aa_prep(struct ardp__sectype *secref)
{
    uid_t ruid, euid;
    
    /* Get the real and effective user ids */
    ruid = getuid();
    euid = geteuid();
	
    /* Ignore the old special authenticator stuff. */
    secref->mech_spec[AA_MY_USERNAME].ptr = gl_uid_to_name_GSP(ruid, NULL);
    secref->processing_state = ARDP__SEC_PREP_SUCCESS;
    return ARDP_SUCCESS;
}



enum ardp_errcode
ardp__sec_authentication_asrthost_accept_message(
    RREQ GL_UNUSED_C_ARGUMENT req,
    /* Pass a pointer to sec_ctxt, so we can set members. */
    ardp_sectype *secref,
    const char *arg, /* This is a simple string -- the username. */
    int arglen)
{
    assert(secref->processing_state == ARDP__SEC_IN_PROCESS);
    if (arglen < 1 || arg[arglen - 1] != '\0') {
	if (ardp_debug)
	    rfprintf(stderr, "ardp authentication_asrthost_accept: network"
		    " message had a bogus authenticator.\n");
	secref->processing_state = ARDP__SEC_PROCESSED_FAILURE;
	return ARDP_FAILURE;
    }
    /* arg is null terminated. */
    secref->mech_spec[AA_PEER_USERNAME].ptr = stcopy(arg);
    if (ardp_debug)
	rfprintf(stderr, "ardp authentication_asrthost: success: peer asserts principal: %s\n",
		(char *) secref->mech_spec[AA_PEER_USERNAME].ptr);
    
    secref->processing_state = ARDP__SEC_PROCESSED_SUCCESS;
    if (secref->requested_by_peer) {
	return aa_prep(secref);
    }
    return ARDP_SUCCESS;
}


/* This commit function should work for both the client and the server; they
   respectively store the REQ and the REP in the AA_AUTHENTICATOR element of
   the mech_spec array. */
/* Copy the data to req->sec */
static enum 
ardp_errcode aa_commit(RREQ req, ardp_sectype *secp)
{
    const char *authenticator = secp->mech_spec[AA_MY_USERNAME].ptr;
    assert(secp->processing_state == ARDP__SEC_PREP_SUCCESS);
    /* Passing a null-terminated string. */
    ardp__sec_add2secdata4req(
	req, 0, authenticator, 1+ p_bstlen(authenticator));
    secp->processing_state = ARDP__SEC_COMMITTMENT_SUCCESS;
    return ARDP_SUCCESS;
}

#endif /*  ARDP_SEC_HAVE_AUTHENTICATION_ASRTHOST */
#endif /* #ifndef ARDP_NO_SECURITY_CONTEXT */
