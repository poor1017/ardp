/*

 * Copyright (c) 1993, 1998 by the University of Southern California
 *
 * For copying and distribution information, please see the file
 *  <usc-license.h>.
 *
 * Written  by bcn 1989-91  as ardp_ptalloc(), part of the Prospero
 *				distribution 
 * Modified by bcn 1/93     changed to conform to ardp.h
 * Modified by swa 12/93    threads added.
 * Copied by swa   1/98     Converted to allocate ardp_sectypes, not PTEXTs.
 */

#include <usc-license.h>


#ifndef ARDP_NO_SECURITY_CONTEXT

#include <ardp_sec_config.h>
#include <ardp.h>
#include <ardp_sec.h>		/* for ardp_sectype * */
#include <gl_threads.h>	/* For mutex */
#include <mitra_macros.h>	/* For structure allocation and freeing */

#include <stdlib.h>             /* For malloc or free */

#ifdef ARDP_SEC_HAVE_KERBEROS
#include <krb5.h>
#include <com_err.h>
#endif /* ARDP_SEC_HAVE_KERBEROS */

/* local use; macros need it. */
typedef ardp_sectype ARDP_SECTYPE_ST;
typedef ardp_sectype *ARDP_SECTYPE;

static ardp_sectype *	lfree = NULL;   /* locked with p_th_mutexardp_sectype *
					 */ 
int 		ardp_sectype_count = 0;
int		ardp_sectype_max = 0;

/* Copy from one of the master sectypes, returning a properly allocated
   variant. */
ardp_sectype *
ardp_secopy(const ardp_sectype * orig)
{
    ardp_sectype *se;

    if (!orig)
	return NULL;
    se = ardp_sealloc();
    /* Initialize and fill in default values. */
    se->service = orig->service;
    se->mechanism = orig->mechanism;
    se->parse_arguments = orig->parse_arguments;
    se->mech_spec_free = orig->mech_spec_free;
    se->commit = orig->commit;
    se->criticality = orig->criticality;
    se->me = orig->me;
    se->requesting_from_peer = orig->requesting_from_peer;
    se->privacy = orig->privacy;
    se->reject_if_peer_does_not_reciprocate 
	= orig->reject_if_peer_does_not_reciprocate;

    return se;
}

/*
 * ardp_sealloc - allocate and initialize ardp_sectype structure
 *
 *    ardp_sealloc returns a pointer to an initialized structure of type
 *    ARDP_SECTYPE.  If it is unable to allocate such a structure, it
 *    signals out_of_memory();
 */
ardp_sectype *
ardp_sealloc(void)
{
    ardp_sectype *se;

    TH_STRUC_ALLOC(ardp_sectype, ARDP_SECTYPE, se);

    /* Initialize and fill in default values. */
    se->service = ARDP_SEC_SERVICE_UNDEFINED;
    se->mechanism = 0;
    se->parse_arguments = NULL;
    se->mech_spec_free = NULL;
    se->commit = NULL;
    se->processing_state = ARDP__SEC_JUST_ALLOCATED;
    se->criticality = 0;
    se->args = NULL;
    se->me = 0;
    se->requesting_from_peer = 0;
    se->requested_by_peer = 0;
    se->privacy = 0;
    se->error_code = ARDP_SUCCESS;
    memset(se->mech_spec, 0, sizeof se->mech_spec);
#ifdef ARDP_SEC_HAVE_KERBEROS
    se->ticket = NULL;
    se->auth_context = NULL;
    se->k5context = ardp__sec_k5context; /* don't rely on initialization */
#endif /* ARDP_SEC_HAVE_KERBEROS */
    se->seq = 0;
    se->previous = NULL;
    se->next = NULL;
    se->peer_set_me_bit = 0;
    se->pkt_context_started_in = NOPKT;
    se->reject_if_peer_does_not_reciprocate = 0;
    se->mate = NULL;
    memset(se->mbz, 0, sizeof se->mbz);

    return se;
}

/*
 * ardp_sefree - free a ARDP_SECTYPE structure
 *
 *    ardp_sefree takes a pointer to a ARDP_SECTYPE structure and adds it to
 *    the free list for later reuse.
 */
void 
ardp_sefree(ardp_sectype * se)
{
#ifdef ARDP_SEC_HAVE_KERBEROS
    krb5_error_code err = 0L;
#endif /* ARDP_SEC_HAVE_KERBEROS */
    if (se->mech_spec_free)
	se->mech_spec_free(se);
    GL_STFREE(se->args);
#ifdef ARDP_SEC_HAVE_KERBEROS
    /* I think se->ticket is part of the auth. context. */
    if (se->auth_context) {
#ifdef OPEN_RCACHE_ONLY_ONCE
	/* The rcache is important to keep open, since the server is a
	   persistent process.  Therefore, we remove it from the auth_con
	   structure, so that we don't free it inappropriately. */
	/* krb5_auth_con_setrcache() actually never returns errors. */
	err = krb5_auth_con_setrcache(se->k5context, se->auth_context, NULL);
#endif
	if (!err) 
	    err = krb5_auth_con_free(se->k5context, se->auth_context);
	if (err) {
	    if (ardp_debug)
		com_err("ardp: ardp_secfree() while freeing auth context", 
			err, "");
	}
    }
    assert(!se->k5context || se->k5context == ardp__sec_k5context);
#endif /* ARDP_SEC_HAVE_KERBEROS */
    TH_STRUC_FREE(ardp_sectype,ARDP_SECTYPE,se);
}

/*
 * ardp_selfree - free a ARDP_SECTYPE structure
 *
 *    ardp_selfree takes a pointer to a ARDP_SECTYPE structure frees it and any linked
 *    ARDP_SECTYPE structures.  It is used to free an entrie list of ARDP_SECTYPE
 *    structures.
 */
void ardp_selfree(ardp_sectype * se)
{
    TH_STRUC_LFREE(ardp_sectype *,se,ardp_sefree);
}
#endif /*  ARDP_NO_SECURITY_CONTEXT */
