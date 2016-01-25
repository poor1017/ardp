/*
 * Copyright (c) 1993,1994 by the University of Southern California
 *
 * For copying and distribution information, please see the file
 * <usc-license.h>.
 *
 * Original author: BCN (washington)  (1989, 1991).
 * Hacked by SWA.
 */

/* XXX this code is pretty bad; it does not respect the
   overrides of PSRV_HOSTNAME, etc., when setting the host address */
#include <usc-license.h>

/* The gethostname() prototype is missing under Solaris V.  We live with this
   rather than defining another missing prototype warning, at least for now. */

#include <sys/param.h>		/* for MAXHOSTNAMELEN */
#include <netdb.h>
#include <memory.h>		/* memset() prototyped. */
#include <stdlib.h>		/* memset() prototyped on Posix-compilant
				   systems.  */
#include <errno.h>

/* MAXHOSTNAMELEN is not defined in <sys/param.h> on Solaris 2.5.1, yet the
   official manual page for gethostname(3C) claims it is there. */
#ifndef MAXHOSTNAMELEN		/* some unixes don't do this; not posix */
#define MAXHOSTNAMELEN 257      /* 255 is the limit in the 4.2BSD manual pages.
				   The sysinfo(2) manual page for Solaris 2.5.1
				   suggests 257. */
#endif

#include <perrno.h>
#include <ardp.h>         /* for prototypes of myhostname() and myaddress(),
			     the exported interfaces in this file.  */


/* These are protected since it is only set via a pthread_once. */
static char	*myhname = NULL;
#ifdef GL_THREADS
static pthread_once_t set_haddr_once = PTHREAD_ONCE_INIT;
#endif
static long	myhaddr = 0L;

/* set_haddr() is not inherently thread-safe, but it is thread-safe because it
   is called from pthread_once(&set_haddr_once,set_haddr). */

/*
 * Implementation and Specification Comments:
 * From the SunOS 4.1.3 manual page for "gethostbyname(3)":
 * "The members of this structure are: 
    h_name              Official name of the host. [ ... ]"
 * 
 * Under Solaris 2.3, Mitra reports that a short name that is not the fully
 * qualified domain name is being returned in the h_name member.  This makes
 * myhostname() return a name that is not the official name of the host.
 *
 *
 * Problems if we don't get the fully qualified domain name (swa went through
 * all the existing code calling this function to check and make sure no 
 * problems would arise):
 * dirsrv.c: no problem; envar overrides #define overrides myhostname()
 * ftp.c (in vcache): used to construct password for anonymous FTP.  
 * So: no problem.
 * Not used elsewhere in the existing code; future code will be aware of the
 * potential problem.
 */
 

/* Further work, 9/9/97: 
   Problem: Under Solaris 2.5.1, on TONGA.ISI.EDU, we are getting a non-zero
   value returned from gethostname().  The manpage for gethostname(3c) assures
   us that errno will be set if a non-zero value is returned.  This is not the
   case.  (However, the value returned is in fact correct.)

   *** variable: weird_Solaris_2_5_1_gethostname_bug_demonstrated. ***
   */

/* If we get an error return without errno being set, that
   demonstrates the sporadic Solaris 2.5.1 bug observed
   9/9/97. */ 
/* Ok, this has been determined.  Sometimes (happened in a
   version of vache), the # of bytes needed for the buffer to
   store the result string (the hostname) is returned.  In
   other cases, running the same version of the library linked
   slightly differently, we get a 0, which is the value
   reported by the manpage.  WE'll test against -1 instead. */

static void 
set_haddr(void)
{
    /* See above for explanation.  EWWWWWWW */
    static int weird_Solaris_2_5_1_gethostname_bug_demonstrated = 0;

    /* This means bugs that haven't appeared yet, but might. */
    static int even_weirder_gethostname_bug_demonstrated = 0;

    /* We use a temporary here; bug-catching. */
    int gethostname_retval;

    char *s = NULL;
    struct sockaddr_in sin;
    
    assert(!myhname);
    myhname = stalloc(MAXHOSTNAMELEN);
trybigger:
    gethostname_retval = gethostname(myhname, p_bstsize(myhname));
    if(gethostname_retval == -1) {
    badreturn:
	perrno = ARDP_FAILURE;
	if (ardp_debug) {
	    perror("ARDP: Error retrieving host name");
	}
	myhname = stcopyr(
	    "THE UNIX GETHOSTNAME() SYSTEM CALL FAILED UNEXPECTEDLY", 
	    myhname); /* failure */
    }
    /* Confirm that this was enough space. */
    if (gl_strnlen(myhname, p_bstsize(myhname)) == p_bstsize(myhname)) {
	size_t newsiz = p_bstsize(myhname) + MAXHOSTNAMELEN;
	stfree(myhname);
	myhname = stalloc(newsiz);
	goto trybigger;
    }
    if (gethostname_retval) {
	/* Test for bogus return error on Solaris 2.5.1 */
	/* We know that this is a bogus error now. */
	/* See comments above. */
	/* cast strlen() to int to turn off warning about signed/unsigned
	   comparison.  We know this string won't be huge. */
	if (!errno &&  ((int) strlen(myhname) + 1 == gethostname_retval)) {
	    ++weird_Solaris_2_5_1_gethostname_bug_demonstrated;
	    if (ardp_debug >= 10) {
		rfprintf(stderr, "gethostname() here operated"
			" incorrectly, but in a way that we've seen"
			" on Solaris 2.5.1.  Compensating.\n");
	    }
	} else {
	    ++even_weirder_gethostname_bug_demonstrated;
	    if (ardp_debug) {
		rfprintf(stderr, "Really weird (not yet seen) \
gethostname bug appeared.  errno = %d, gethostname() returned %d...",
			errno, gethostname_retval);
	    }
	    if (myhname[0] != '\0') {
		if (ardp_debug)
		    fputs("Recovering.\n", stderr);
	    } else {
		if (ardp_debug)
		    fputs("Unable to compensate.\n", stderr);
		goto badreturn;
	    }
	}
    }
    /* Well, we read something successfully into myhname.  Onward. */
	    
    ardp_hostname2name_addr(myhname, &s, &sin);
    stfree(myhname);
    myhname = s;
    myhaddr = sin.sin_addr.s_addr;
#if 0				/* no need for this.  Plus, ucase() is still in
				 lib/pfs. */
    ucase(myhname);
#endif
} /* set_haddr */

/* We will call myaddress() from ardp_send() to set the
   ardp_client_address_port global variable.  That variable will not be set if
   there is no client port. */  
u_int32_t
myaddress(void)
{
    /* Make sure set_haddr() has been run before we return */
#ifdef GL_THREADS
    int status;

    status = pthread_once(&set_haddr_once, set_haddr);
    if (status) {
	internal_error("Failure in pthread_once().");
    } /* if */
#else
    if (!myhaddr)
	set_haddr();
#endif
    return(myhaddr);
}

/* myhostname() always returns the official name of the host (see comment below
   for an exception to this claim; do not rely on it. 
   This might be different from the version returned by gethostname(), and
   might well be different from the version stored in the 'hstname' environment
   variable defined in dirsrv.c */
   
const char *
myhostname(void)
{
    /* Make sure set_haddr() has been run before we return */
#ifdef GL_THREADS
    int status;

    status = pthread_once(&set_haddr_once, set_haddr);
    if (status) {
	internal_error("Failure in pthread_once().");
    } /* if */
#else
    if (!myhaddr)
	set_haddr();
#endif
    return(myhname);
}
