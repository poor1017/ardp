/*
 * Copyright (c) 1997 by the University of Southern California
 *
 * For copying and distribution information, please see the file
 * <usc-license.h>.
 */

#include <usc-license.h>
#include <ardp.h>
#include <ardp_sec.h>
#include <stdarg.h>

#if !defined (ARDP_NO_SECURITY_CONTEXT)

enum ardp_errcode
ardp_sectype_ioctl(ardp_sectype *ref, ardp_ioctl_t ioctlname,
		   /* IOCTLs may take arguments. */
		   ...)
{
    va_list ap;
    enum ardp_errcode retval = ARDP_SUCCESS;

    va_start(ap, ioctlname);
    switch(ioctlname) {
    case ARDP_SEC_CRITICALITY:
	ref->criticality = va_arg(ap, int);
	if (ref->criticality 
	    && (ref->processing_state == ARDP__SEC_PREP_FAILURE
		|| ref->processing_state == ARDP__SEC_COMMITTMENT_FAILURE
		|| ref->processing_state == ARDP__SEC_PROCESSED_FAILURE))
	    return ARDP_CRITICAL_SERVICE_FAILED;
	break;
    case ARDP_SEC_ME:
	ref->me = va_arg(ap, int);
	break;
    case ARDP_SEC_YOU:
	ref->requesting_from_peer = va_arg(ap, int);
	break;
    case ARDP_SEC_PRIVACY_REQUIRED:
	ref->privacy = va_arg(ap, int);
	break;
    case ARDP_SEC_RECIPROCATION_REQUIRED:
	ref->reject_if_peer_does_not_reciprocate = va_arg(ap, int);
	if (ref->reject_if_peer_does_not_reciprocate) {
	    retval = ardp_sectype_ioctl(ref, ARDP_SEC_CRITICALITY, 1);
	}
	break;
    default:
	retval = ARDP_FAILURE;
	break;
    }
    va_end(ap);
    return retval;
}

#endif /* !defined (ARDP_NO_SECURITY_CONTEXT) */
