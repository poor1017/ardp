/* ardp/authentication_kerberos.c */
#ifndef ARDP_NO_SECURITY_CONTEXT

/*
 * Copyright (c) 1997 by the University of Southern California
 *
 * For copying and distribution information, please see the file
 * <usc-license.h>.
 */

#include <usc-license.h>

#include "ardp_sec_config.h"	/* keytab locations.  May override stuff in
				   ardp.h  */
#include <ardp.h>		/* for RREQ structure, needed by ardp_sec.h */
#include "ardp_sec.h"		/* for prototypes, ardp_sectype, and the
				   definition of
				   ARDP_SEC_HAVE_AUTHENTICATION_KERBEROS*/ 

#ifdef ARDP_SEC_HAVE_AUTHENTICATION_KERBEROS
#include "ardp_sec_members.h"


#include <stddef.h>		/* for NULL */
#include <stdarg.h>
#include <ctype.h>		/* isupper(), tolower() */

#include <malloc.h>		/* prototype of free(); used internally by
				   <krb5.h>   */


static enum ardp_errcode ak_parse_args(RREQ, ardp_sectype *, va_list);
static enum ardp_errcode ak_commit(RREQ, ardp_sectype *);
static enum ardp_errcode ak_set_local_remote_addrs(
    struct _krb5_auth_context *auth_context, 
    struct sockaddr_in lsin, struct sockaddr_in rsin);

ardp_sectype ardp_sec_authentication_kerberos = {
    UNINITIALIZED,
    ARDP_SEC_AUTHENTICATION,	/* .service */
    ARDP_SEC_AUTHENTICATION_KERBEROS, /* .mechanism */
    ak_parse_args,		/* .parse_arguments(). (client) */
    NULL,			/* .mech_spec_free.: optional:  */
    ak_commit,			/* .commit. */
    /* The following fields are all dynamically updated. */
    ARDP__SEC_STATIC_CONSTANT,	/* .processing_state.: this is an initializer
				 */ 
    0,				/* .criticality.: zero default in this case */
    NULL,			/* .args.: arguments given to us */
    1,				/* .me.: I should provide this */
    1,				/* .requesting_from_peer. */
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


#include <krb5.h>
#include <com_err.h>		/* com_err() prototype, for Kerberos error
				   messages.  */

static authentication_kerberos_initialized = 0;
static ak_server_initialized = 0;
#ifdef USE_RCACHE 
static krb5_rcache ak_open_rcache(void);
#endif


/* Actually just needs to be STATIC, since we pass it around... */
EXTERN_MUTEXED_DEF(krb5_context,ardp__sec_k5context);

/* Might we ever need multiple krb5_context structures?  Currently, we support
   one (and only one). --8/97 */

static
enum ardp_errcode
initialize_authentication_kerberos()
{

    if (!authentication_kerberos_initialized) {
	krb5_error_code k5err;
#if 0
	acc_init_errlib();
#endif
	if((k5err = krb5_init_context(&ardp__sec_k5context))) {
	    if (ardp_debug)
		com_err("ardp kerberos authentication: krb5_init_context()", 
			k5err, "Could not initialize k5context (and library)");
	    return ARDP_FAILURE;
	}
	authentication_kerberos_initialized = 1;
	if (ardp_debug >= 4) {
	    char *s;
	    char **realmlist;
	    if ((k5err = krb5_get_default_realm(ardp__sec_k5context, &s))) {
		com_err("ardp kerberos authentication:"
			" krb5_get_default_realm()", k5err,
			"While checking default realm");
	    } else {
		rfprintf(stderr, "ardp: Kerberos default realm is %s\n", s);
		krb5_xfree(s);
	    }
	    if ((k5err = krb5_get_host_realm(ardp__sec_k5context, 
					     myhostname(), &realmlist))) {
		com_err("ardp kerberos authentication:"
			" krb5_get_host_realm()", k5err,
			"");
	    } else {
		char **cpargv;
		rfprintf(stderr, "Kerberos realm names for %s are:", 
			myhostname());
		for (cpargv = realmlist; *cpargv; ++cpargv) {
		    rfprintf(stderr, " %s", *cpargv);
		}
		putc('\n', stderr);
		if ((k5err = 
		     krb5_free_host_realm(ardp__sec_k5context, realmlist))) {
		    com_err("ardp kerberos authentication:",
			    k5err, "While calling krb5_free_host_realm()");
		}
	    }
	} /* if (ardp_debug >= 4) */
    }
    return ARDP_SUCCESS;
}


/* The special arguments for the authentication_kerberos service are
   remote_hostname, remote_servicename. */
/* We are implementing assuming that ME will always be turned on.  This will
   not hurt if the user turns ME off; we simply ignore the fact that the user
   turned ME off.
   It is not clear to us if it is useful in Kerberos to call for authentication
   with ME off but YOU on.
   (Later: Because of the asymmetry between kerberos's mk_req() and mk_rep(),
   it is much easier for us to just turn on ME to get a reply from the server,
   although the server probably could be made into the requestor in the ME-off
   YOU-on case. --swa, 1/98 )
   Having YOU on means we can certify to the requestor that the response came
   from a legitimate provider of these sorts of responses.
*/
static 
enum ardp_errcode 
ak_parse_args(RREQ req, struct ardp__sectype *secref, va_list ap)
{
    krb5_auth_context 	  auth_context = NULL; /* this pointer is also a flag.
						  If set, we don't go through
						  setting the auth context.*/
#ifdef USE_RCACHE
    krb5_rcache rcache = NULL;		/*  Pointer to allocated memory. */
#endif
    krb5_data		inbuf, kreq_packet;
    krb5_ccache		ccdef;
    krb5_error_code	k5err;
    krb5_principal	self_kerb_principal;
    char *client_name = NULL;	/* Our own client name */
    char *remote_hostname = stcopy(va_arg(ap, const char *));
    const char *remote_servicename_arg = va_arg(ap, const char *);
    /* The remote_servicename argument to krb5_mk_req is specified to be 
       a char *, yet we here have a const char *.  I suspect the prototype is
       bogus, but just in case it isn't... */
    /* XXX Memory leak.  Big deal. */
    char *remote_servicename = stcopy(remote_servicename_arg);
    register char *cp;		/* index for loops */
    enum ardp_errcode tmp = -1;	/* unset? */
    char *full_hname = NULL;
    char **full_hnamep = &full_hname;
    struct sockaddr_in s_sock;		/* server address */
    char *s;			/* temp. string for assignments... */

    /* Now that we have the remote hostname and servicename, we need to use
       them to actually authenticate. */
    
    EXTERN_MUTEXED_LOCK(ardp__sec_k5context);
    if (!authentication_kerberos_initialized) {
	if ((tmp = initialize_authentication_kerberos()))
	    return tmp;
    }
    EXTERN_MUTEXED_UNLOCK(ardp__sec_k5context);
    if (!valid_cksumtype(CKSUMTYPE_CRC32)) {
	if (ardp_debug)
	    com_err("authentication_kerberos", KRB5_PROG_SUMTYPE_NOSUPP, "while using CRC-32");
	return ARDP_CONTEXT_FAILURE;
    } /* this is here because it was here in sim_client.c and we are desperate :) */
    if (!valid_cksumtype(CKSUMTYPE_RSA_MD5)) {
	if (ardp_debug)
	    com_err("authentication_kerberos", KRB5_PROG_SUMTYPE_NOSUPP, "while using RSA-md5");
	return ARDP_CONTEXT_FAILURE;
    }
    if(ardp_hostname2name_addr(remote_hostname, full_hnamep, &s_sock)) {
	if (ardp_debug)
	    rfprintf(stderr, "%s: ardp_hostname2name_addr(): unknown host\n", remote_hostname);
	GL_STFREE(full_hname);
	return ARDP_BAD_HOSTNAME;
    }
    /* lower-case to get name for the "instance" part of Kerberos's service
       name */ 
    for (cp = *full_hnamep; *cp; cp++)
        if (isupper(*cp))
            *cp = tolower(*cp);
    /* Here, sim_client gets the client socket's IP Address and IP port # here,
       so that we can pass it and the local address to krb5_gen_portaddr().  We
       do *THAT* so that we can get a krb5_address for the local port, so that
       we can pass it to the replay-cache thingie. */
    /* In ARDP, we get this from the CLIENT_ADDRESS_PORT global variable.  */
    /* Make sure ARDP_CLIENT_ADDRESS_PORT is initialized: */
    if (ardp_port == -1)
	ardp_init();
        
    /* make sure it was set.  If not, we may need to call connect() in
       ardp_init() */ 
    assert(ardp_client_address_port.sin_addr.s_addr); 
    /* This code from appl/client/sim_client.c */

    EXTERN_MUTEXED_LOCK(ardp__sec_k5context);
    /* Get our credentials cache */
    if ((k5err = krb5_cc_default(ardp__sec_k5context, &ccdef))) {
	if (ardp_debug)
	    com_err("ARDP library: authentication Kerberos:", k5err, "while getting default ccache");
	/* auth_context is not yet allocated here, so do not free it. */
	assert(!auth_context);

	/* krb5_auth_con_free(ardp__sec_k5context, auth_context); */
	/* The message won't get all the way up to the user if it's not
	   critical. */
	GL_STFREE(full_hname);
	return ARDP_CRITICAL_SERVICE_FAILED;
    }
    /* Mark who we ourselves are. */
    if ((k5err = krb5_cc_get_principal(ardp__sec_k5context, ccdef,
				       &self_kerb_principal))) {
	if (ardp_debug)
	    com_err("ARDP library: authentication Kerberos:", k5err, 
		    "while getting credentials cache's primary principal");
	/* auth_context is not yet allocated here, so do not free it. */
	assert(!auth_context);
	/* krb5_auth_con_free(ardp__sec_k5context, auth_context); */
	/* The message won't get all the way up to the user if it's not
	   critical. */
	GL_STFREE(full_hname);
	return ARDP_CRITICAL_SERVICE_FAILED;
    }
    if ((k5err = krb5_unparse_name(ardp__sec_k5context, self_kerb_principal,
				    &client_name))) {
	if (ardp_debug) {
	    com_err("ARDP Kerberos authentication generation", k5err, 
		    "while unparsing client name");
	}
	GL_STFREE(full_hname);
	return ARDP_CRITICAL_SERVICE_FAILED;
    }
    if (ardp_debug)
	rfprintf(stderr, "We are the Kerberos client %s\n", client_name);
    /* Ok, not remote here. */
    secref->mech_spec[KA_CLIENTNAME].ptr = stcopy(client_name);
    /* Copy the important data to a permanent data structure. */
    krb5_free_principal(ardp__sec_k5context, self_kerb_principal);

    /* Pre-allocate the authorization context. */
    if ((k5err = krb5_auth_con_init(ardp__sec_k5context, &auth_context))) {
	if (ardp_debug)
	    com_err("ardp kerberos authentication verification", 
		    k5err, "while initializing auth_context");
	secref->processing_state = ARDP__SEC_PROCESSED_FAILURE;
	EXTERN_MUTEXED_UNLOCK(ardp__sec_k5context);
	return ARDP_CONTEXT_FAILURE;
    }
    secref->auth_context = auth_context;
    secref->k5context = ardp__sec_k5context;

    if (ardp_debug >= 9)
	ardp__sec_krb5_show_sequence_nums(auth_context);

#ifdef USE_RCACHE
    if((rcache = ak_open_rcache()) == NULL) {
	if (ardp_debug) {
	    rfprintf(stderr, "ARDP server kerberos verification: \
unable to open Replay cache.\n");
	}
	secref->processing_state = ARDP__SEC_PROCESSED_FAILURE;
	EXTERN_MUTEXED_UNLOCK(ardp__sec_k5context);
	return ARDP_CONTEXT_FAILURE;
    }
    /* Now assign the rcache to the context. */
    krb5_auth_con_setrcache(ardp__sec_k5context, auth_context, rcache);
    rcache = NULL;
#endif /* USE_RCACHE */
    /* We could use timestamps or sequence numbers */
    krb5_auth_con_setflags(ardp__sec_k5context, auth_context, 
			   AK_AUTH_CONTEXT_FLAGS);

    /* Now set remote and local addresses in the auth context.*/
    if ((tmp = ak_set_local_remote_addrs(auth_context, 
					ardp_client_address_port, req->peer))) {
	if (ardp_debug)
	    fputs("ardp kerberos integrity: ik_verify: failed to set"
		  " local address\n", stderr);
	EXTERN_MUTEXED_UNLOCK(ardp__sec_k5context);
	return tmp;
    }
    
    /* PREPARE KRB_AP_REQ MESSAGE */

    /* This makes the authentication data; puts it into
       kreq_packet.length,kreq_packet.data */ 
    /* allocates and partly fills in auth_context, which we need. */
    inbuf.data = *full_hnamep;	/* this CAN'T be bad, can it? Taken blindly
				   from sim_client.c  */
    inbuf.length = strlen(*full_hnamep);
    /* XXX Kerberos bug: krb5_mk_req() should use a const char * for the
       remote_servicename, not a char * */ 
    
    if (ardp_debug >= 11)
	ardp__sec_krb5_show_sequence_nums(auth_context);
    if ((k5err = krb5_mk_req(ardp__sec_k5context, &auth_context, 0, 
			     remote_servicename, *full_hnamep, 
			     &inbuf, ccdef, &kreq_packet))) {
	if (ardp_debug)
	    com_err(NULL, k5err, "while preparing AP_REQ");
	/* The auth_context will be freed when we're done with the ardp_sectype
	   structure. */
	secref->processing_state = ARDP__SEC_PROCESSED_FAILURE;
	EXTERN_MUTEXED_UNLOCK(ardp__sec_k5context);
	GL_STFREE(full_hname);
	return ARDP_CRITICAL_SERVICE_FAILED;

    }
    if (ardp_debug >= 11)
	ardp__sec_krb5_show_sequence_nums(auth_context);

    /* At the end of krb5_mk_req(), the Kerberos authenticator is in
       kreq_packet.data, ready to be added to the security context.   This will
       not actually be done until do_commit(). */ 
    /* This can now be copied away for safekeeping */
    s = secref->mech_spec[KA_AUTHENTICATOR].ptr = stalloc(kreq_packet.length);
    memcpy(s, kreq_packet.data,
	   kreq_packet.length);
    p_bst_set_buffer_length_explicit(s, kreq_packet.length);
    secref->mech_spec[KA_REMOTE_SERVICENAME].ptr = stcopy(remote_servicename);
    secref->mech_spec[KA_REMOTE_SERVERNAME].ptr = stcopy(*full_hnamep);

    secref->auth_context = auth_context;
    secref->k5context = ardp__sec_k5context;
    EXTERN_MUTEXED_UNLOCK(ardp__sec_k5context);
    /* Save off pointers to what we used, too. */

    GL_STFREE(full_hname);
    return ARDP_SUCCESS;
}


/* This commit function should work for both the client and the server; they
   respectively store the REQ and the REP in the KA_AUTHENTICATOR element of
   the mech_spec array. */
/* Copy the data to req->sec */
static enum 
ardp_errcode ak_commit(RREQ req, ardp_sectype *secp)
{
    const char *authenticator = secp->mech_spec[KA_AUTHENTICATOR].ptr;
    assert(secp->processing_state == ARDP__SEC_PREP_SUCCESS);
    ardp__sec_add2secdata4req(req, 0, authenticator, p_bstlen(authenticator));
    secp->processing_state = ARDP__SEC_COMMITTMENT_SUCCESS;
    if (ardp_debug >= 11) {
	fputs("ardp: kerb auth: in ak_commit(): ", stderr);
	ardp__sec_krb5_show_sequence_nums(secp->auth_context);
    }
    return ARDP_SUCCESS;
}


/* Server Side  verification follows: */
/*    do_security_context() puts the context to be verified on
      req->secq, then calls us via
      ardp__sec_dispatch_service_receiver().  */

/* Returns ARDP_SUCCESS or ARDP_FAILURE */
/* Verifies Kerberos authentication for the server. */

/* Routine is server-specific */
enum ardp_errcode
ardp__sec_authentication_kerberos_server_verify_message(
    RREQ req,
    /* Pass a pointer to sec_ctxt so we can set the
       'ticket' and 'auth_context' members. */
    ardp_sectype *secref,
    const char *arg, /* This is the output buffer generated by
			the client's call to krb5_mk_req() */ 
    int arglen)
{
    krb5_data		 packet;
    krb5_ticket *ticket = NULL;
    krb5_error_code	k5err;
    krb5_auth_context auth_context = NULL;
    char *client_name = NULL;
    

    /* Generate necessary contexts. */
    static krb5_keytab keytab = NULL;	/* Server's keytab */
    static krb5_principal sprinc = NULL; /* server's principal */
#ifdef USE_RCACHE
#ifdef OPEN_RCACHE_ONLY_ONCE
    static 
#endif
	krb5_rcache rcache;
#endif /* USE_RCACHE */
    ardp_errcode tmp;

    EXTERN_MUTEXED_LOCK(ardp__sec_k5context);
    if (!authentication_kerberos_initialized) {
	if ((tmp = initialize_authentication_kerberos())) {
	    secref->processing_state = ARDP__SEC_PROCESSED_FAILURE;
	    return tmp;
	}
    }

    if (!ak_server_initialized) {
	if ((k5err = krb5_kt_resolve(ardp__sec_k5context, SERVER_KEYTAB_FILE, &keytab))) {
	    EXTERN_MUTEXED_UNLOCK(ardp__sec_k5context);
	    /* XXX We need a way of logging this problem to plog(); com_err just
	       writes to stderr.   There must be a way of doing this
	       properly. --8/97 */
	    if (ardp_debug) {
#if 1				/*  com_err() not displaying message? */
		com_err("Kerberos authentication (server verification)", k5err,
			"while resolving keytab file %s", SERVER_KEYTAB_FILE);
#else  /* This seems cuter but doesn't work. */
		rfprintf(stderr, "Kerberos authentication error (server"
			"verification) while resolving keytab file %s", 
			SERVER_KEYTAB_FILE);
		com_err("Kerberos authentication (server verification)", k5err);
#endif /* 1 */
	    }
	    secref->processing_state = ARDP__SEC_PROCESSED_FAILURE;
	    return ARDP_CONTEXT_FAILURE;
	}	    
	if ((k5err = krb5_sname_to_principal(ardp__sec_k5context, NULL, SERVER_SERVICE_NAME, 
					     KRB5_NT_SRV_HST, &sprinc))) {
	    if (ardp_debug)
		com_err("ARDP Kerberos authentication verification", k5err, "while generating service name %s", SERVER_SERVICE_NAME);
	    secref->processing_state = ARDP__SEC_PROCESSED_FAILURE;
	    EXTERN_MUTEXED_UNLOCK(ardp__sec_k5context);
	    return ARDP_CONTEXT_FAILURE;
	}
#ifdef OPEN_RCACHE_ONLY_ONCE
	if((rcache = ak_open_rcache()) == NULL) {
	    if (ardp_debug) {
		rfprintf(stderr, "ARDP server kerberos verification: \
unable to open Replay cache.\n");
	    }
	    secref->processing_state = ARDP__SEC_PROCESSED_FAILURE;
	    EXTERN_MUTEXED_UNLOCK(ardp__sec_k5context);
	    return ARDP_CONTEXT_FAILURE;
	}
#endif
	ak_server_initialized = 1;
    } /* if (!ak_server_initialized) */
    packet.data = (krb5_pointer) arg;
    packet.length = arglen;

    if ((k5err = krb5_auth_con_init(ardp__sec_k5context, &auth_context))) {
	if (ardp_debug)
	    com_err("ardp kerberos authentication verification", 
		    k5err, "while initializing auth_context");
	secref->processing_state = ARDP__SEC_PROCESSED_FAILURE;
	EXTERN_MUTEXED_UNLOCK(ardp__sec_k5context);
	return ARDP_CONTEXT_FAILURE;
    }
    secref->auth_context = auth_context;
    secref->k5context = ardp__sec_k5context;

    if (ardp_debug >= 11) {
	fputs("ardp: kerb auth: in ak_server_verify(): ", stderr);
	ardp__sec_krb5_show_sequence_nums(auth_context);
    }

#ifdef USE_RCACHE
#ifndef OPEN_RCACHE_ONLY_ONCE
    if((rcache = ak_open_rcache()) == NULL) {
	if (ardp_debug) {
	    rfprintf(stderr, "ARDP server kerberos verification: \
unable to open Replay cache.\n");
	}
	secref->processing_state = ARDP__SEC_PROCESSED_FAILURE;
	EXTERN_MUTEXED_UNLOCK(ardp__sec_k5context);
	return ARDP_CONTEXT_FAILURE;
    }
#endif
    /* Now assign the rcache to the context. */
    krb5_auth_con_setrcache(ardp__sec_k5context, auth_context, rcache);
    rcache = NULL;
#endif
    krb5_auth_con_setflags(ardp__sec_k5context, auth_context, 
			   AK_AUTH_CONTEXT_FLAGS);

    /* Now set remote and local addresses in the auth context.*/
    if ((tmp = ak_set_local_remote_addrs(auth_context, 
					ardp_client_address_port, req->peer))) {
	if (ardp_debug)
	    fputs("ardp kerberos integrity: ik_verify: failed to set"
		  " local address\n", stderr);
	EXTERN_MUTEXED_UNLOCK(ardp__sec_k5context);
	return tmp;
    }
    

    if (ardp_debug >= 11) {
	fputs("ardp: kerb auth: in ak_server_verify(): ", stderr);
	ardp__sec_krb5_show_sequence_nums(auth_context);
    }
    if ((k5err = krb5_rd_req(ardp__sec_k5context, &auth_context, &packet, 
			      sprinc, keytab, NULL, &ticket))) {
	if (ardp_debug)
	    com_err("ARDP Kerberos authentication verification", 
		    k5err, "while reading request");
	secref->processing_state = ARDP__SEC_PROCESSED_FAILURE;
	EXTERN_MUTEXED_UNLOCK(ardp__sec_k5context);
	return ARDP_CONTEXT_FAILURE;
    }

    if (ardp_debug >= 11) {
	fputs("ardp: kerb auth: in ak_server_verify(): ", stderr);
	ardp__sec_krb5_show_sequence_nums(auth_context);
    }
    if ((k5err = krb5_unparse_name(ardp__sec_k5context, ticket->enc_part2->client,
				    &client_name))) {
	if (ardp_debug) {
	    com_err("ARDP Kerberos authentication verification", k5err, 
		    "while unparsing client name");
	}
	secref->processing_state = ARDP__SEC_PROCESSED_FAILURE;
	EXTERN_MUTEXED_UNLOCK(ardp__sec_k5context);
	return ARDP_CONTEXT_FAILURE;
    }
    if (ardp_debug)
	rfprintf(stderr, "We have authenticated the Kerberos client %s\n", client_name);
    secref->mech_spec[KA_CLIENTNAME].ptr = stcopy(client_name);
    secref->processing_state = ARDP__SEC_PROCESSED_SUCCESS;
    /* XXX Here, we need to make this authentication information available to
       higher layers.  We do not provide this functionality right now, but must
       do so. --8/97 */ 
    /* Either use the sec member of the RREQ structure (use the sectypes we
       already have) or restore the PAUTH structure to the ARDP library, and
       hang a list of those off of the RREQ structure. */
    free(client_name);
    /* 'packet' is no longer relevant. */

    /* Now, if we need to, do the same thing we do in parse_args() on the
       client: Generate the authenticator. */
    /* We use 'packet' here as an OUT parameter. */
    if (secref->requested_by_peer) {
	krb5_data		 krep_packet;
	if (ardp_debug >= 11) {
	    fputs("ardp: kerb auth: in ak_server_verify() before krb5_mk_rep() ", stderr);
	    ardp__sec_krb5_show_sequence_nums(auth_context);
	}
	k5err = krb5_mk_rep(ardp__sec_k5context, auth_context,  &krep_packet);
	if (k5err) {
	    secref->processing_state = ARDP__SEC_PREP_FAILURE;
	    if (ardp_debug) {
		com_err("authentication_kerberos", k5err,
			"while server makes REP");
	    }
	    EXTERN_MUTEXED_UNLOCK(ardp__sec_k5context);
	    return ARDP_CONTEXT_FAILURE;
	} 
	if (ardp_debug >= 11) {
	    fputs("ardp: kerb auth: in ak_server_verify() after krb5_mk_rep() ", stderr);
	    ardp__sec_krb5_show_sequence_nums(auth_context);
	}
	/* We're done here. */
	EXTERN_MUTEXED_UNLOCK(ardp__sec_k5context);
	if (ardp_debug >= 12) {
	    rfprintf(stderr, "KRB5_REP length is %d bytes, data is: ", 
		    krep_packet.length);
	    ardp_showbuf(krep_packet.data, krep_packet.length, stderr);
	    fputc('\n', stderr);
	}
	secref->mech_spec[KA_AUTHENTICATOR].ptr = stalloc(krep_packet.length);
	memcpy(secref->mech_spec[KA_AUTHENTICATOR].ptr, krep_packet.data,
	       krep_packet.length);
	p_bst_set_buffer_length_explicit(
	    secref->mech_spec[KA_AUTHENTICATOR].ptr, krep_packet.length);
	krb5_xfree(krep_packet.data);
	secref->processing_state = ARDP__SEC_PREP_SUCCESS;
    }
    
    return ARDP_SUCCESS;
}

static
enum ardp_errcode 
ak_set_local_remote_addrs(
    struct _krb5_auth_context *auth_context, 
    struct sockaddr_in lsin, struct sockaddr_in rsin)
{
    krb5_address	laddr, raddr; /* local, remote */
    krb5_error_code	k5err;
    
    /* SET PORT */
    laddr.addrtype = ADDRTYPE_IPPORT;
    laddr.length = sizeof lsin.sin_port;
    laddr.contents = (krb5_octet *) &lsin.sin_port;

    raddr.addrtype = ADDRTYPE_IPPORT;
    raddr.length = sizeof rsin.sin_port;
    raddr.contents = (krb5_octet *) &rsin.sin_port;

    if ((k5err = krb5_auth_con_setports(ardp__sec_k5context, auth_context,
					 &laddr, &raddr))) {
	com_err("ARDP: authentication_kerberos()", k5err, "while setting local port\n");
	return ARDP_CONTEXT_FAILURE;
    }

  
    /* SET ADDRESS */
    laddr.addrtype = ADDRTYPE_INET;
    laddr.length = sizeof lsin.sin_addr;
    laddr.contents = (krb5_octet *) &lsin.sin_addr;
    
    raddr.addrtype = ADDRTYPE_INET;
    raddr.length = sizeof rsin.sin_addr;
    raddr.contents = (krb5_octet *) &rsin.sin_addr;
    
    if ((k5err = krb5_auth_con_setaddrs(ardp__sec_k5context, auth_context,
					 &laddr, &raddr))) {
	com_err("ARDP: integrity_kerberos()", k5err, "while setting local addr\n");
	return ARDP_CONTEXT_FAILURE;
    }
    return ARDP_SUCCESS;
}

#ifdef USE_RCACHE
#undef AK_MAKE_UNIQUE_RCACHE_NAME
/* The code to make unique rcache names is from sim_server.  I don't think it
   is necessary, or even desirable, here.  Why would we want a unique cache,
   given that we don't want replays?
   Consultation with Dr. Brian Tung, 1/21/98 --swa */ 

/* Give us the rcache.  This is called once on startup. */
krb5_rcache
ak_open_rcache(void)
{
    krb5_data	rcache_name;
    krb5_address	addr;	/* do not free */
#ifdef AK_MAKE_UNIQUE_RCACHE_NAME
    char *replay_cache_name_str; /* String representation of rcache_name. */
    krb5_address *portlocal_addr = NULL; /* This is allocated memory. */
#endif
    krb5_error_code	k5err;
    krb5_rcache rcache = NULL;		/*  Pointer to allocated memory. */

    /* SET ADDRESS */
    addr.addrtype = ADDRTYPE_INET;
    addr.length = sizeof(ardp_client_address_port.sin_addr);
    addr.contents = (krb5_octet *)&ardp_client_address_port.sin_addr;
    
#ifdef AK_MAKE_UNIQUE_RCACHE_NAME
    /* THIS IS UGLY (comment from sim_client.c) */
    if ((k5err = krb5_gen_portaddr(
	ardp__sec_k5context, &addr, 
	(krb5_pointer) &ardp_client_address_port.sin_port, &portlocal_addr))) {
	if (ardp_debug)
	    com_err("ARDP: authentication_kerberos", k5err, "while generating port address");
	return NULL;
    }
  
    /* Cannot find a prototype for krb5_gen_replay_name;
       did a grep through KERBEROS_ROOT/include/ *.h
       and KERBEROS_ROOT/include/ * / *.h --swa, katia, 7/97 */
    if ((k5err = krb5_gen_replay_name(ardp__sec_k5context,portlocal_addr,
				      "ARDP Version 1",
				      &replay_cache_name_str))) {
	if (ardp_debug)
	    com_err("ARDP: authentication_kerberos()", k5err, "while generating replay cache name");
	if (portlocal_addr) krb5_free_address(portlocal_addr);
	return NULL;
    }
    rcache_name.length = strlen(replay_cache_name_str);
    rcache_name.data = replay_cache_name_str;
    /* Must free replay_cache_name_str when done. */
    if (ardp_debug >= 8)
	rfprintf(stderr, "ardp: authentication_kerberos: Using a replay cache"
		" named: %s\n", replay_cache_name_str);

    /* Used the address now. */
    if (portlocal_addr) {
	krb5_free_address(portlocal_addr);
	portlocal_addr = NULL;
    }
#else
    rcache_name.length = strlen(SERVER_SERVICE_NAME);
    rcache_name.data = SERVER_SERVICE_NAME;
#endif


    if ((k5err = krb5_get_server_rcache(ardp__sec_k5context, &rcache_name, &rcache))) {
	if (ardp_debug)
	    com_err("ARDP: authentication_kerberos", k5err, 
		    "while getting server replay cache");
#ifdef AK_MAKE_UNIQUE_RCACHE_NAME
	krb5_xfree(replay_cache_name_str);
#endif
	return NULL;
    }
#ifdef AK_MAKE_UNIQUE_RCACHE_NAME
    /* Don't need the name any longer. */
    krb5_xfree(replay_cache_name_str);
    replay_cache_name_str = NULL; 
#endif
    return rcache;
}
#endif /* USE_RCACHE */

/* Client side verification follows. */
static ardp_sectype * GL_UNUSED_C_ARGUMENT find_original_client_secref(RREQ, ardp_sectype *);

/* This routine takes advantage of the fact that we already have the
   ardp__sec_k5context and auth_context in place. */

enum ardp_errcode
ardp__sec_authentication_kerberos_client_verify_message(
    RREQ req, ardp_sectype *secref, const char *arg_arg, int arglen)
{
    char *arg = gl_bstcopy(arg_arg);
    krb5_error_code	k5err;
    krb5_data		 input_authenticator;
    krb5_ap_rep_enc_part *krep;
    ardp_sectype *orig_secref;
    krb5_auth_context auth_context;


    if (ardp_debug > 13) {
	rfprintf(stderr, "ardp_sec_dispatch_service_receiver():"
		" svc name: %s: Args length is %d bytes, data is: ",
		ardp_sec_mechanism_name(secref), arglen);
	ardp_showbuf(arg, arglen, stderr);
	fputc('\n', stderr);

    }
    if(!(orig_secref = find_original_client_secref(req, secref))) {
	if (ardp_debug) {
	    rfprintf(stderr, "ardp__sec_authentication_kerberos_client_"
		    "verify_message(): Unable to find original security"
		    " ref.\n");
	}
	return ARDP_CONTEXT_FAILURE;
    }

    auth_context = orig_secref->auth_context;

    if (ardp_debug >= 11) {
	fputs("ardp: kerb auth: starting ak_client_verify(): ", stderr);
	ardp__sec_krb5_show_sequence_nums(auth_context);
    }
    input_authenticator.data = arg;
    input_authenticator.length = arglen;

    if (ardp_debug >= 12) {
	rfprintf(stderr, "ardp received: KRB5_REP length is %d bytes, data is: ", 
		input_authenticator.length);
	ardp_showbuf(input_authenticator.data, input_authenticator.length, stderr);
	fputc('\n', stderr);
    }
    if (ardp_debug >= 11) {
	fputs("ardp: kerb auth: ak_client_verify(): before rd_rep(): ", stderr);
	ardp__sec_krb5_show_sequence_nums(auth_context);
    }
    if ((k5err = krb5_rd_rep(ardp__sec_k5context, auth_context, 
			     &input_authenticator, 
			     &krep))) {
	if (ardp_debug) 
	    com_err("Client's ARDP Kerberos authentication verification", 
		    k5err, "while reading Kerberos REP");
	return ARDP_CONTEXT_FAILURE;
    }

    if (ardp_debug >= 11) {
	fputs("ardp: kerb auth: ak_client_verify(): after rd_rep(): ", stderr);
	ardp__sec_krb5_show_sequence_nums(auth_context);
    }
    krb5_free_ap_rep_enc_part(ardp__sec_k5context, krep);
    /* The only way I see to get the server's name is to remember what server
       we prepared this auth_context for, and then print it.  I assume only
       that server could have made krb5_rd_rep() return successfully. --swa,
       katia, 8/97 */
    if (ardp_debug)
	rfprintf(stderr, "ardp client: Kerberos successfully authenticated "
		"the remote service %s on the remote server %s\n",
		(char *) orig_secref->mech_spec[KA_REMOTE_SERVICENAME].ptr,
		(char *) orig_secref->mech_spec[KA_REMOTE_SERVERNAME].ptr);
		

    /* XXX We have no way of communicating a successful mutual authentication
       to the application layer.  This must be added.  (Steve will probably be
       stuck doing this when he finally does the ASRTHOST-style
       authentication). */
    return ARDP_SUCCESS;
}

/* This assumes that there is only one Kerberos authentication going on here.
   One could have a multiple-principal situation.  However, those are not
   currently (Kerberos Version 5 release 1) supported.
   */
static
ardp_sectype *
GL_UNUSED_C_ARGUMENT
find_original_client_secref(RREQ req,
			    ardp_sectype * GL_UNUSED_C_ARGUMENT repl_ctxt 
    )
{
    ardp_sectype *stl;
    for (stl = req->secq; stl; stl = stl->next) {
	if(stl->processing_state == ARDP__SEC_COMMITTMENT_SUCCESS
	   && stl->service == ardp_sec_authentication_kerberos.service
	   && (stl->mechanism == ardp_sec_authentication_kerberos.mechanism))
	    return stl;
    }
    return NULL;
}


/* Called by debugging code. */
void
ardp__sec_krb5_show_sequence_nums(krb5_auth_context auth_context)
{
    krb5_error_code k5err;
    krb5_int32 seqnum;
    if ((k5err = krb5_auth_con_getlocalseqnumber(
	ardp__sec_k5context, auth_context, &seqnum))) {
	com_err("ardp: kerberos authentication: ", k5err,
		"while calling krb5_auth_getlocalseqnumber()");
    } else {
	rfprintf(stderr, "ardp: kerberos auth: local sequence #: %d, ", seqnum);
    }
    if ((k5err = krb5_auth_con_getremoteseqnumber(
	ardp__sec_k5context, auth_context, &seqnum))) {
	com_err("ardp: kerberos authentication: ", k5err,
		"while calling krb5_auth_getremoteseqnumber()");
    } else {
	rfprintf(stderr, "remote sequence #: %d\n", seqnum);
    }
}

#endif /* def ARDP_SEC_HAVE_AUTHENTICATION_KERBEROS */

#endif /* ndef ARDP_NO_SECURITY_CONTEXT */
