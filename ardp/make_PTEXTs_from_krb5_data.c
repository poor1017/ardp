/*
 * Copyright (c) 1998 by the University of Southern California
 *
 * For copying and distribution information, please see the file
 * <usc-license.h>.
 */

#include <usc-license.h>

#include "ardp_sec_config.h"
#include <ardp.h>
#include <ardp_sec.h>

#if !defined(ARDP_NO_SECURITY_CONTEXT) && defined(ARDP_SEC_HAVE_KERBEROS)
/* Here, we just append onto *pptl */
void
ardp__sec_make_PTEXTs_from_krb5_data(krb5_data inbuf, PTEXT *pptl)
{
    /* The purpose of these manipulations is:
       -- allocate a dummy RREQ
       -- use ardp_add2req() to put the text we care about onto its 'outbuf'
           member 
       -- take it from the 'outbuf' member and put it where it belongs
       -- throw away the dummy
       */
    RREQ dummyreq = ardp_rqalloc();

    ardp_add2req(dummyreq, 0, inbuf.data, inbuf.length);
    *pptl = dummyreq->outpkt;
    dummyreq->outpkt = NULL;
    ardp_rqfree(dummyreq);
}
#endif /*  !defined(ARDP_NO_SECURITY_CONTEXT) &&
	   defined(ARDP_SEC_HAVE_KERBEROS) */
