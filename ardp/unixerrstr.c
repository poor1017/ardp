/*
 * Copyright (c) 1991-1994 by the University of Southern California
 *
 * For copying and distribution information, please see the file
 * <usc-license.h>.
 */

#include <usc-license.h>

#include <pmachine.h>		/* for HAVESTRERROR */
#include <ardp.h>		/* for prototype for unixerrstring() */


#include <errno.h>


#if defined(HAVESTRERROR)
#include <string.h>			/* For stringerr*/
#else
/* SUNOS's <errno.h> and <stdio.h> do not contain this prototype; appears
   unavailable.  Probably most BSD-derived systems don't contain
   it? --swa, 1/13/98. */

extern int sys_nerr;
extern const char *const sys_errlist[];

#endif /* HAVESTRERROR */


const char *
unixerrstr(void)
{
#ifdef HAVESTRERROR
    /* sys_errlist is not in SOLARIS, replace with strerror() 
       which may not be thread safe in some applications
       there doesnt appear to be a posix compliant equivalent */
    return strerror(errno);
#else
#if 0				/* version changed 12/28/97 --swa */
    AUTOSTAT_CHARPP(mesgp);
    return qsprintf_GSP(
	mesgp, "%s (number %d)", 
	(errno < sys_nerr ? sys_errlist[errno] : "Unprintable Error"),
	errno);
#else  /* DEAD CODE */
    /* Would be nice to have the message include the error #. */
    return errno < sys_nerr ? sys_errlist[errno] : "Unprintable Error";
#endif /* DEAD CODE */
#endif /* HAVESTRERROR */
}

