/*

 * Copyright (c) 1993             by the University of Southern California
 *
 * For copying and distribution information, please see the file
 *  <usc-license.h>.
 *
 * Written  by bcn 1989-91  as part of the Prospero distribution
 * Modified by bcn 1/93     changed to conform to ardp.h
 * Modified by swa 12/93    threads added.
 */


#include <usc-license.h>

#include <ardp.h>
#include <stdlib.h>             /* For malloc or free */
#include <gl_threads.h>
#include <mitra_macros.h>

/* "free" conflicted with free() - Mitra */
static PTEXT	lfree = NOPKT;   /* locked with p_th_mutexPTEXT */
int 		ptext_count = 0;
int		ptext_max = 0;

/*
 * ardp_ptalloc - allocate and initialize ptext structure
 *
 *    ardp_ptalloc returns a pointer to an initialized structure of type
 *    PTEXT.  If it is unable to allocate such a structure, it
 *    signals out_of_memory();
 */
PTEXT
ardp_ptalloc()
{
    PTEXT	pt;
    TH_STRUC_ALLOC(ptext,PTEXT,pt);

    /* Initialize and fill in default values. */
    pt->seq = 0;
    pt->length = 0;
    pt->context_flags = 0;
    /* The offset is to leave room for additional headers */
    pt->start = pt->dat + ARDP_PTXT_HDR;
    pt->text = pt->start;
    pt->ioptr = pt->start;
    pt->mbz = 0;
    return(pt);
}

/*
 * ardp_ptfree - free a PTEXT structure
 *
 *    ardp_ptfree takes a pointer to a PTEXT structure and adds it to
 *    the free list for later reuse.
 */
void ardp_ptfree(pt)
    PTEXT	pt;
{
    TH_STRUC_FREE(ptext,PTEXT,pt);
}

/*
 * ardp_ptlfree - free a PTEXT structure
 *
 *    ardp_ptlfree takes a pointer to a PTEXT structure frees it and any linked
 *    PTEXT structures.  It is used to free an entrie list of PTEXT
 *    structures.
 */
void ardp_ptlfree(pt)
    PTEXT	pt;
{
  TH_STRUC_LFREE(PTEXT,pt,ardp_ptfree);
}

