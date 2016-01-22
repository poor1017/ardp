/* Copyright (c) 1993, by Pandora Systems */
/* Author:	Mitra <mitra@path.net> */
/* Allocation code copied and adapted from:
	 prospero/alpha.5.2a+/lib/pfs/flalloc */

#include "dnscache_alloc.h"
#include <gl_threads.h>
#include <mitra_macros.h>
#include <memory.h>

static DNSCACHE	lfree = NULL;		/* Free dnscaches */
/* These are global variables which will be read by dirsrv.c and used for
   status reports.
   Too bad C doesn't have better methods for structuring such global data.  
   */

int		dnscache_count = 0;
int		dnscache_max = 0;


/************* Standard routines to alloc, free and copy *************/
/*
 * dnscache_alloc - allocate and initialize DNSCACHE structure
 *
 *    returns a pointer to an initialized structure of type
 *    DNSCACHE.  If it is unable to allocate such a structure, it
 *    signals out_of_memory();
 */

DNSCACHE
dnscache_alloc(void)			
{
    DNSCACHE	acache;
    
    TH_STRUC_ALLOC(dnscache,DNSCACHE,acache);
    acache->name = NULL;
    acache->official_hname = NULL;
    acache->usecount = 0;
    memset(&acache->sockad,0, sizeof(acache->sockad));
    return(acache);
}

/*
 * dnscache_free - free a DNSCACHE structure
 *
 *    dnscache_free takes a pointer to a DNSCACHE structure and adds it to
 *    the free list for later reuse.
 */
void
dnscache_free(DNSCACHE acache)
{
    GL_STFREE(acache->name);
    GL_STFREE(acache->official_hname);
    TH_STRUC_FREE(dnscache,DNSCACHE,acache);
}

/*
 * dnscache_lfree - free a linked list of DNSCACHE structures.
 *
 *    dnscache_lfree takes a pointer to a dnscache structure and frees it and 
 *    any linked
 *    DNSCACHE structures.  It is used to free an entire list of DNSCACHE
 *    structures.
 */
void
dnscache_lfree(DNSCACHE acache)
{
	TH_STRUC_LFREE(DNSCACHE,acache,dnscache_free);
}

#if 0                           /* unused; could be useful for later debugging
                                   or as an emergency out-of-memory
                                   handler.   However, we currently don't do
                                   this when we're out of memory.  --swa
                                   4/25/95 */ 

void
dnscache_freespares(void)
{
	TH_FREESPARES(dnscache,DNSCACHE);
}
#endif /* 0 */
