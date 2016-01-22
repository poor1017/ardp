/*
 * Copyright (c) 1996 -- 1998 by the University of Southern California
 *
 * For copying and distribution information, please see the file
 * <usc-license.h>.
 */

#include <usc-license.h>

#ifndef ARDP_NO_SECURITY_CONTEXT

#include "ardp_sec_config.h"	/* overrides ardp.h? */
#include <ardp.h>
#include "ardp_sec.h"

#if defined(ARDP_SEC_HAVE_INTEGRITY_KERBEROS) || defined(ARDP_SEC_HAVE_PRIVACY_KERBEROS)
#include <malloc.h>		/* prototype for malloc() on some systems. */
#include <krb5.h>

static size_t calc_buflen(PTEXT);



/* Here, we use MALLOC(); this follows what we know of Kerberos conventions.
   Free outbufp->data with krb5_xfree() when done.  */
void 
ardp__sec_make_krb5_data_from_PTEXTs(PTEXT ptl, krb5_data *outbufp)
{
    char *cp;			/* pointer to target data byte for copy */

    outbufp->length = calc_buflen(ptl);
    outbufp->data = malloc(outbufp->length);
    if (!outbufp->data) {
	out_of_memory();
	internal_error("should never get here.");
    }
    /* copy data into outbufp->data  (2nd pass) */
    for (cp = outbufp->data; ptl; ptl = ptl->next) {
	/* Get the size of the payload area of the packet we are currently
	   working on (copying) */
	register size_t payloadsize = ptl->length - (ptl->text - ptl->start);
	memcpy(cp, ptl->text, payloadsize);
	cp += payloadsize;
    }
    /* Confirm that the amount we copied was the same as the amount of space we
       thought we had to allocate. */
    assert(cp - outbufp->data == outbufp->length);
}



static 
size_t 
calc_buflen(PTEXT pt)
{
    register size_t len = 0;

    for (; pt; pt = pt->next) {
	/* Add  the size of the payload area for this packet. 
	 (pt->text - pt->start) is the amount of data space allocated for
	 packet headers. pt->length is the total amount of data in the packet. 
	 */ 
	len += pt->length - (pt->text - pt->start);
    }
    return len;
}

#endif
#endif
