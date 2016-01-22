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

#ifdef ARDP_SEC_HAVE_KERBEROS
#include "ardp_sec_members.h"
#include <krb5.h>
#include <com_err.h>		/* com_err() prototype */




/* This assumes that there is only one Kerberos authentication going on here.
   One could have a multiple-principal situation.  However, those are not
   currently (Kerberos Version 5 release 1) supported.
   */
ardp_sectype *
ardp__sec_find_krbauth_secref(RREQ req)
{
    ardp_sectype *stl;
    for (stl = req->secq; stl; stl = stl->next) {
	if(stl->processing_state == ARDP__SEC_PREP_SUCCESS
	   || stl->processing_state == ARDP__SEC_PROCESSED_SUCCESS
	   || stl->processing_state == ARDP__SEC_COMMITTMENT_SUCCESS)
	    if (stl->service == ardp_sec_authentication_kerberos.service
		&& (stl->mechanism 
		    == ardp_sec_authentication_kerberos.mechanism))
		return stl;
    }
    return NULL;
}
#endif /* defined(ARDP_SEC_HAVE_KERBEROS) */

#endif /* !defined(ARDP_NO_SECURITY_CONTEXT) */
