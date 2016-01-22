/*
 * Copyright (c) 1991-1994 by the University of Southern California
 *
 * For copying and distribution information, please see the file
 * <usc-license.h>.
 */

#include <usc-license.h>

#include <errno.h>		/* for ERANGE */
#include <sys/types.h>          /* for gethostbyname() */
#include <sys/socket.h>         /* for gethostbyname */
#include <netdb.h>              /* for gethostbyname. */
#include <netinet/in.h>         /* for struct sockaddr_in */
#include <arpa/inet.h>		/* inet_addr() prototype */
#include <string.h>

#include <ardp.h>
#include <perrno.h>


/* Thread-safe gethostbyname() by swa */
/* DNS caching code written by mitra; code cleaned up by SWA, 4/25/95.
 * ardp_hostname2name_addr() added, swa & katia, 8/96
 */ 

#include "dnscache_alloc.h"

/*
 * This function serves as a caching and thread-safe version of
 * gethostbyname(); 
 * gethostbyname() is not a re-entrant function since it uses static data.
 * (However, gethostbyname_r() is.)
 * 
 * It normally accepts a hostname and initializes the socket address
 * appropriately, then returns PSUCCESS.  Also accepts numeric addresses.
 * It handles a following port number in parentheses or following a colon.
 *
 * It returns ARDP_BAD_HOSTNAME if the hostname could not be resolved.
 * gethostbyname() should not be called in multi-threaded versions
 * of the Prospero code, although gethostbyname_r() is OK.  Nevertheless, this
 * interface is preferred because it caches.
 *
 * We used to mutex gethostbyname(), but with the availability of threads
 * packages containing re-entrant versions of gethostbyname_r(), this is
 * superior; we don't want to block on name resolution as a bottleneck.  
 * Probably want a multithreaded name resolver library.  Release such a thing
 * as freeware? 
 *
 * It also converts numeric addresses appropriately.
 */
/* If you use this - uncomment/comment initialization in server/dirsrv.c */

#if 0				/* When you're doing development, turn off
				   caching sometimes and on other times, just
				   to make sure it still works both
				   ways. --swa.  */
#undef DNSCACHE_MAX		/* XXX experiment -- turning off caching  */
#endif

#ifdef DNSCACHE_MAX
#include <mitra_macros.h>		/* FIND_FNCTN_LIST */
#include <string.h>			/* For strcmp */

DNSCACHE	alldnscaches = NULL;
int           alldnscache_count = 0;

/* #define USE_SOCKADDR_COPY */
#ifdef USE_SOCKADDR_COPY
/* XXX this code should actually die; we don't need it.   Can just do
   assignment. */
/* Copy all but the port number.  (i.e. the host number and address family.  Do
   not copy the port number.).  Leaving sin_zero unset should not be bad.
   We'll copy it for superstitious reasons. */
static
void
sockaddr_copy(struct sockaddr_in *src, struct sockaddr_in *destn)
{
    u_int16_t       portnum = destn->sin_port;

    /* Nothing in a sockaddr_in is a pointer */
    memcpy(destn, src, sizeof(struct sockaddr_in));
    destn->sin_port = portnum;	/* keeps the code short to set this twice. */
}
#endif
#endif /*DNSCACHE_MAX*/

/* Mitra comments follow.  --swa */
/* Caching has been added, to this - take care that it does what you
   want, I'm certainly open to changes if this isnt what we need.
   Currently it is called by something at a higher layer, with a 
   hostname of the name to cache, and a hostaddr of NULL.
   Currently this is used to cache all the hostnames hard coded
   into the configuration files on the assumption that these are unlikely to
   move around while a server is running.   Later, we may want to delete a
   cached entry periodically, or if it fails. 

   The first thing in alldnscaches is always a copy of the last thing
   found, to allow really quick returns on repeat requests.

   Note - great care is taken here to 
   a) avoid deadlock between GETHOSTBYNAME and DNSCACHE mutexes
   b) avoid requirement for GETHOSTBYNAME mutex, if found in cache 
   This allows multiple cached results to be returned while a 
   single thread calls gethostbyname
*/
  
void
ardp_hostname2addr_initcache()
{
}

#ifdef DNSCACHE_MAX
/* Clean out some old cached entries, if we have too many. */
static void
dnscache_clean()
{
    DNSCACHE    dc, nextdc;
    if (alldnscache_count > DNSCACHE_MAX) {
	/* pthread_mutex_trylock(&()) returns non-zero if we did not get the lock. */
#ifdef GL_THREADS
        if (! pthread_mutex_trylock(&(p_th_mutexALLDNSCACHE))) {
#endif /* GL_THREADS */
            /* Since this is only an optimisation, skip the optimization if
	       it's locked already.  Here, we have the lock.  */ 
	    /* Some of the above comment was written by a Briton, other bits of
	       it were added by an American.  Yes, spelling is inconsistent. */
            for (dc = alldnscaches; dc ; dc = nextdc) {
                nextdc = dc->next;
                if (!(--dc->usecount)) {
                    EXTRACT_ITEM(dc,alldnscaches);
                    dnscache_free(dc);
                    alldnscache_count--;
                }
            }
#ifdef GL_THREADS
            pthread_mutex_unlock(&(p_th_mutexALLDNSCACHE));
        }
#endif /* GL_THREADS */
    }
}
#endif /* DNSCACHE_MAX */


enum ardp_errcode
ardp_hostname2addr(const char *hostname_arg, struct sockaddr_in *hostaddr_arg)
{
    return ardp_hostname2name_addr(hostname_arg, NULL, hostaddr_arg);
}


/* This function is called with a hostname to resolve and a hostaddr structure
   to put the results into.  If called with a NULL hostaddr
   structure, it'll just look up the name and pop it into the cache (Mitra
   wanted this extension).
   
   The hostname may be of the three forms:
	1) "hostname"
	2) "hostname:port-no"
	3) "hostname(port-no)"
   
   If port-no is not specified (in the first form above), then
   ardp_hostname2name_addr() will return with hostaddr_arg->sin_port set to
   zero.  If port-no is specified, then hostaddr_arg->sin_port will be set to
   the (network byte order) decimal integer value of port-no.

   This function ignores ardp_config.default_port; that is examined at higher
   levels. 
   */

enum ardp_errcode
ardp_hostname2name_addr(const char *hostname_arg, char **official_hnameGSP, struct sockaddr_in *hostaddr_arg)
{    
    struct hostent *hp;		/* Remote host we're connecting to. */
#ifdef GL_THREADS
    struct hostent hp_st;
    GL_KEYED_STRING_DECL(gethostbuf);
    int hostbuflen;
#endif
    char *hostname = stcopy(hostname_arg); /* working copy */
    char *openparen;		/* for parsing hostname string. */
    int req_udp_port = 0;		/* port requested, HOST byte order */
    struct sockaddr_in *hostaddr = hostaddr_arg;
#ifdef DNSCACHE_MAX
    DNSCACHE	acache = NULL;
    struct sockaddr_in	hostaddr_st; /* physical structure in case hostaddr is
					null  */
#endif

    if (!hostname)		/* nothing to do, eh? */
	return perrno = ARDP_BAD_HOSTNAME;

    if (!hostaddr_arg) {
	/* If passed no host address structure, set up a temporary one so that
	   the DNScache code can copy from it, and so that we can just go on.
	   Moreover, we might just be going for the official hostname . */
	hostaddr = &hostaddr_st;
    }

    acache = alldnscaches;
	
    /* If a port is included, save it away */
    /* 4/27/95: I added ':' as an alternative port specifier. */
    if((openparen = strchr(hostname,'(')) || (openparen = strchr(hostname, ':'))){
	sscanf(openparen+1,"%d",&req_udp_port);
	*openparen = '\0';
    }
    /* hostname is now the host name without the port number. */

#ifdef DNSCACHE_MAX
#ifdef GL_THREADS
    pthread_mutex_lock(&(p_th_mutexALLDNSCACHE));
#endif /* GL_THREADS */
    /* We can't use TH_FIND_FNCTN_LIST because must retain lock. */
    FIND_FNCTN_LIST(acache, name, hostname, stcaseequal);
    if(acache)  {
        acache->usecount++;
#ifdef USE_SOCKADDR_COPY
	sockaddr_copy(&(acache->sockad),hostaddr);
#else
	*hostaddr = acache->sockad;
#endif
	if (official_hnameGSP)
	    *official_hnameGSP = stcopyr(acache->official_hname, *official_hnameGSP);
	/* Done with acache. */

	/* We do not always cache the official hostname; if not, do the lookup
	   again (and maybe put it into the cache this time :)) XXX */
	/* If we're not looking for the official hostname, or if we already
	   have it, then we're done. */
	if (!official_hnameGSP || *official_hnameGSP) {
#ifdef GL_THREADS
	    pthread_mutex_unlock(&(p_th_mutexALLDNSCACHE)); /* Also released below*/
#endif /* GL_THREADS */
	    GL_STFREE(hostname);
	    /* if req_udp_port is zero (the common case), then the caller will
	       use whatever default port it wishes.  */
	    hostaddr->sin_port = htons(req_udp_port);
	    return(ARDP_SUCCESS); /* Don't free acache */
	}
    }
#ifdef GL_THREADS
    pthread_mutex_unlock(&(p_th_mutexALLDNSCACHE)); /* Note also released above*/
#endif /* GL_THREADS */

    /* We get here if:
       (1) We didn't find the hostname in the cache 
       or (2) There was a cache hit, but the cache entry did not contain the
       official hostname, and we wanted the official hostname. */

    /* Didn't find it in the list of cached entries.  Now look it up, just as
       we always do if compiled without caching. */
#endif /*DNSCACHE_MAX*/

    /* Here, we do a regular gethostbyname() lookup, ignoring the cache. */
#ifndef GL_THREADS

    hp = gethostbyname((char *) hostname); /* cast to char * since
                                              GETHOSTBYNAME has a bad
                                              prototype on some systems -- we
                                              know it's const char *,
                                              though. */  
#else
    /* gethostbyname_r() needs a buffer that is "large enough".  It indicates
       ERANGE if the buffer is too small. */
    
    /* This will initially be empty.  */
    hostbuflen = p_bstsize(gethostbuf);
    for (;;) {
	hp = gethostbyname_r(hostname, &hp_st, gethostbuf, hostbuflen, NULL);
	if (hp)
	    break;
	if (errno != ERANGE)
	    break;
	/* Need more core. */
	hostbuflen += 256;	/* 256 is a random amount, really. */
	stfree(gethostbuf);
	gethostbuf = stalloc(hostbuflen);
    }
#endif /* GL_THREADS */

    /* We may have the hostent structure we need. */
    if (hp == NULL) {
	/* Here, there is NO hostname match. */
	/* We currently don't cache failed attempts to look up hostnames.  It
	   might be wise to do so. --swa, 5/8/96) */
	/* It also would be wise to cache numeric addresses. -- 8/96 */
	/* Try to see if it might be a numeric address.  */
	hostaddr->sin_family = AF_INET;
	hostaddr->sin_addr.s_addr = inet_addr(hostname);
	if(hostaddr->sin_addr.s_addr == (unsigned long) -1) {
	    p_clear_errors();       /* clear p_err_string if set. */
	    GL_STFREE(hostname);
#ifdef GL_THREADS
	    GL_KEYED_STRING_DONE(gethostbuf);
#endif
	    return perrno = ARDP_BAD_HOSTNAME;
	}
	/* We were given a Numeric Address; we do not have an official hostname
	   yet. */
	if (official_hnameGSP) {
	    /* We have already figured out the host's address.  Now, if
	       (and only if) the caller requested the official hostname,
	       we'll call gethostbyaddr() so we can provide one. */
#ifndef GL_THREADS
	    hp = gethostbyaddr((char*)hostaddr, sizeof(struct sockaddr_in), AF_INET);
#else
	    /* gethostbyname_r() needs a buffer that is "large enough".  It indicates
	       ERANGE if the buffer is too small. */
	    
	    /* This will often initially be zero.   I wrote it that way to test
	     the code through one of these cycles.  --swa 6/98 */
	    hostbuflen = p_bstsize(gethostbuf);
	    for (;;) {
		hp = gethostbyaddr_r((char*) hostaddr, sizeof(struct sockaddr_in), AF_INET, 
				     &hp_st, gethostbuf, hostbuflen, NULL);
		if (hp)
		    break;
		if (errno != ERANGE)
		    break;
		/* Need more core. */
		hostbuflen += 256;	/* 256 is a random amount, really. */
		stfree(gethostbuf);
		gethostbuf = stalloc(hostbuflen);
	    }
#endif
	    if (hp == NULL) {
		p_clear_errors();       /* clear p_err_string if set. */
		GL_STFREE(hostname);
#ifdef GL_THREADS
		GL_KEYED_STRING_DONE(gethostbuf);
#endif
		return perrno = ARDP_BAD_HOSTNAME;
	    }
	}
    }
    /* Here we have a HOSTENT. */
    /* Put the new data into the cache. */
    /* If we have a numeric address looked up, with or without an offical hostname, 
       we put it it in the cache. This saves us a future call to the name resolver.   
       (Of course, we could cut down on calls to the name resolver by writing a
       better test for numeric IP addresses, and by executing that test first;
       of course, that will fail when we go to IP v6. */

    memset((char *) hostaddr, 0, sizeof *hostaddr);
    memcpy((char *) &hostaddr->sin_addr, hp->h_addr, hp->h_length);
    hostaddr->sin_family = hp->h_addrtype;
    if (official_hnameGSP)
	*official_hnameGSP = stcopyr(hp->h_name, *official_hnameGSP);
    
#ifdef DNSCACHE_MAX
    /* Copy last result into cache */
    if (alldnscaches == NULL)   
	ardp_hostname2addr_initcache(); /* unneeded actually */
    acache = dnscache_alloc(); /* This call locks DNSCACHE, then unlocks it;
				  this doesn't conflict with mutexes in this
				  function. */
    acache->name = stcopy(hostname);
    acache->official_hname = stcopy(hp->h_name);
#ifndef USE_SOCKADDR_COPY
    acache->sockad = *hostaddr;
#else
    sockaddr_copy(hostaddr,&(acache->sockad));
#endif
    /* Boost the use count initially, to bias towards keeping recently used
       entries. */ 
    acache->usecount = 5; 
#ifdef GL_THREADS
    pthread_mutex_lock(&(p_th_mutexALLDNSCACHE));
#endif /* GL_THREADS */
    PREPEND_ITEM(acache,alldnscaches); /* put it at the front; I made this
                                          change.  --swa 4/25/95 */
    alldnscache_count++;
#ifdef GL_THREADS
    pthread_mutex_unlock(&(p_th_mutexALLDNSCACHE));
#endif /* GL_THREADS */
    dnscache_clean();   /* Only does anything if cache too big */
#endif /*DNSCACHE_MAX*/

    GL_STFREE(hostname);
#ifdef GL_THREADS
    GL_KEYED_STRING_DONE(gethostbuf);
#endif
    hostaddr->sin_port = htons(req_udp_port);
    return(ARDP_SUCCESS);
}


