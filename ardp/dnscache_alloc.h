#ifndef dnscache_alloc_h
#define dnscache_alloc_h
/*
 * If DNSCACHE_MAX is defined, then the ARDP library will cache up to that many
 
 * DNS addresses. "#undef" this for no caching at all.
 * XXX This configuration variable is present currently in
 * lib/ardp/dnscache_alloc.h.   We may later move to a unified system for
 * compilation options for the library, (some sort of include/ardp.config.h
 * file, much as we have include/psite.h and include/pserver.h).  However, for
 * now, the ARDP library does not have such a compilation-time configuration
 * file.  --swa
 */

/* This defines an interface to the functions in lib/ardp/dnscache_alloc.c */
/* This is mostly mitracode. --swa */

#define DNSCACHE_MAX 300


#ifdef DNSCACHE_MAX

#include <ardp.h>               /* for P_ALLOCATOR_CONSISTENCY_CHECK */
#include <gl_threads.h>
#include <netinet/in.h>

struct dnscache {
#ifdef P_ALLOCATOR_CONSISTENCY_CHECK
	enum p__consistency consistency;
#endif
	char	*name;
	char	*official_hname;
	struct	sockaddr_in sockad;
	int     usecount;       /* For determining size of cache */
	struct	dnscache	*next;
	struct	dnscache	*previous;
};
typedef struct dnscache *DNSCACHE;
typedef struct dnscache DNSCACHE_ST;

extern DNSCACHE dnscache_alloc(void);
extern void dnscache_free(DNSCACHE acache);
extern void dnscache_lfree(DNSCACHE acache);
extern void dnscache_freespares(void); /* XXX This function definition is
                                      commented out. If you find a need for it
                                      then the integration should be done
                                      properly; see notes by function def.
                                      --swa, 4/25/95 */
extern int dnscache_count, dnscache_max;

/* Not sure where these are defined..--- swa */

/* extern DNSCACHE dnscache_copy(DNSCACHE f, int r); */

#ifdef GL_THREADS
extern pthread_mutex_t p_th_mutexDNSCACHE; /* for memory allocator. */
extern pthread_mutex_t p_th_mutexALLDNSCACHE; /* used to mutex gethostbyname()
						 in 
                                            the absence of a multi-threaded
                                            version(?).  I ASSUME. --swa,
                                            4/25/95 */ 
#endif

#endif /* DNSCACHE_MAX */
#endif /*dnscache_alloc_h*/
