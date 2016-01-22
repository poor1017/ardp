/* -*-c-*- */
/* ardp_sec_config.h: */
/*
 * Copyright (c) 1998 by the University of Southern California
 *
 * For copying and distribution information, please see the file
 * <usc-license.h>.
 */

#include <usc-license.h>

/* ARDP security Configuration file. */
/* This is needed for Kerberos support in the ARDP Security context.

   Edit ardp_sec_config.h (this file) to set the default Kerberos service name
   and keytab file location.  These are also dynamically configurable
   as the resources "ardp.kerberos_srvtab" and "ardp.server_principal_name".
   */

/* This is the default Kerberos service */
#define ARDP_SERVER_PRINCIPAL_NAME_DEFAULT "sample"
#define ARDP_KERBEROS_SRVTAB_DEFAULT "/home/swa/kerberos-tabs/keytab-tonga.isi.edu"

/* You can configure these to specifically determine which defaults are going
   to be enabled.  You can leave this alone if you want to, though; if you do
   so, then support for all of the security features listed below will be
   compiled into ARDP. */
#ifndef ARDP_NO_SECURITY_CONTEXT

#define ARDP_SEC_HAVE_AUTHENTICATION_ASRTHOST
#define ARDP_SEC_HAVE_INTEGRITY_CRC
#define ARDP_SEC_HAVE_LABELS_CLASS_OF_SERVICE

#ifndef ARDP_NO_KERBEROS
#define ARDP_SEC_HAVE_KERBEROS
#endif /* ARDP_NO_KERBEROS */

#ifdef  ARDP_SEC_HAVE_KERBEROS
#define ARDP_SEC_HAVE_INTEGRITY_KERBEROS
#define ARDP_SEC_HAVE_AUTHENTICATION_KERBEROS
#define ARDP_SEC_HAVE_PRIVACY_KERBEROS
#endif /* ARDP_SEC_HAVE_KERBEROS */


/* These should not be touched; they are Kerberos options which we are always
   leaving on or off. */

#undef OPEN_RCACHE_ONLY_ONCE
#undef USE_RCACHE		/* Even open up the rcache. */
#undef AK_MAKE_UNIQUE_RCACHE_NAME
#undef OPEN_RCACHE_ONLY_ONCE
/* Setting the flags to zero causes failures during INTEGRITY_KERBEROS: Claims
   that certain fields are missing:  */
/* Simple, though. */
/* #define AK_AUTH_CONTEXT_FLAGS 0 */

#define AK_AUTH_CONTEXT_FLAGS KRB5_AUTH_CONTEXT_DO_SEQUENCE

#endif /* ARDP_NO_SECURITY_CONTEXT */
