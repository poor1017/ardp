#ifndef ARDP_NO_SECURITY_CONTEXT
#include <ardp.h>
#include "ardp_sec.h"

#define Entry(st) {&(st), #st}

/* Update this list each time a new ardp_sectype is added. */
static
const
struct stype_name_array {
    const ardp_sectype *st;
    const char *name;
}
ardp__sectypes[] = {
#ifdef ARDP_SEC_HAVE_INTEGRITY_CRC	
    Entry(ardp_sec_integrity_crc),
#endif
#ifdef ARDP_SEC_HAVE_AUTHENTICATION_ASRTHOST
    Entry(ardp_sec_authentication_asrthost),
#endif
#ifdef ARDP_SEC_HAVE_INTEGRITY_KERBEROS
    Entry(ardp_sec_integrity_kerberos),
#endif
#ifdef ARDP_SEC_HAVE_AUTHENTICATION_KERBEROS
    Entry(ardp_sec_authentication_kerberos),
#endif
#ifdef ARDP_SEC_HAVE_PRIVACY_KERBEROS
    Entry(ardp_sec_privacy_kerberos),
#endif
#ifdef ARDP_SEC_HAVE_LABELS_CLASS_OF_SERVICE
    Entry(ardp_sec_labels_class_of_service),
#endif
/* more to follow */
};			/*  */


const char *
ardp_sec_mechanism_name(const ardp_sectype *sp)
{
    const struct stype_name_array * idx;

    for(idx = ardp__sectypes; 
	idx < ardp__sectypes 
	    + sizeof ardp__sectypes / sizeof ardp__sectypes[0];
	++idx) {
	if (idx->st->service == sp->service 
	    && idx->st->mechanism == sp->mechanism)
	    return idx->name;
    }
    return "Unknown Mechanism";
}

/* Return a pointer to the static/global/external ardp_sectype object
   associated with this particular service and mechanism. */
const
ardp_sectype *
ardp__sec_look_up_service(enum ardp__sec_service sec_service, 
		unsigned char sec_mechanism)
{
    const struct stype_name_array * idx;

    for(idx = ardp__sectypes; 
	idx < ardp__sectypes 
	    + sizeof ardp__sectypes / sizeof ardp__sectypes[0];
	++idx) {
	if (idx->st->service == sec_service
	    && (idx->st->mechanism == sec_mechanism))
	    return idx->st;
    }
    return NULL;
}
#endif /* ARDP_NO_SECURITY_CONTEXT */
