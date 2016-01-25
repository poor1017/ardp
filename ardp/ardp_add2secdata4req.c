/*
 * Copyright (c) 1991-1996 by the University of Southern California
 *
 * For copying and distribution information, please see the file
 * <usc-license.h>.
 */

#include <usc-license.h>

#ifndef ARDP_NO_SECURITY_CONTEXT

#include <ardp.h>
#include "ardp_sec.h"

#ifndef min
#define min(a,b) ((a) < (b) ? (a) : (b))
#endif /* min() */

/* Add items to req->sec.  Add it to the TRNS member only when we've committed
   to sending the entire message. */
/* The only flag we support is ARDP_A2R_NOSPLITB */

void
ardp__sec_add2secdata4req(RREQ req, int flags, const void *buf, int buflen)
{
    int		remaining;      /* Space remaining in pkt  */
    int		written;	/* # Octets written        */
    PTEXT pkt = NOPKT;		/* current pkt. */

    if (req->sec) {
	pkt = req->sec->previous; /* last member of the list */
    } else {
	/* The first packet needs to be created. */
	pkt = ardp_ptalloc();
	pkt->context_flags = 0x04; /* set security context bit. */
	APPEND_ITEM(pkt, req->sec);
    }

    /* Trivial Implementation NOTE: if (!req->sec), pkt is set below.   The
       above initialization to NULL is not necessary.  However, GCC's -Wall
       feature is too dumb to realize this, so we go ahead and set it anyway.
       */ 
	
	       
 keep_writing:
    remaining = ARDP_PTXT_LEN - (pkt->ioptr - pkt->start);

    if ((flags | ARDP_A2R_NOSPLITB) && remaining < buflen) {
	remaining = 0;
    }
    if (remaining == 0) {
	/* Create a new packet. */
	pkt = ardp_ptalloc();
	assert(pkt->ioptr == pkt->start); /* code depends on this; should
					     always be true.  */
	APPEND_ITEM(pkt, req->sec);
	remaining = ARDP_PTXT_LEN;
    }
    written = min(remaining, buflen);
    memcpy(pkt->ioptr, (char *) buf, written);
    pkt->ioptr += written;
    /* Don't move 'pkt->text' forward, since:
       this packet will never contain text; we don't mix text and data in the
       security context right now.  Will later, though. */
    /*     pkt->text = pkt->ioptr; */
    pkt->length = pkt->ioptr - pkt->start;
    if(written != buflen) {
    char* temp = (char*) buf;
    temp += written;
    buf = temp;
	buflen -= written;
	goto keep_writing;
    }
    req->seclen += buflen;
}

/* Mark our current position.  The caller of ardp__sec_add2secdata4req() may call
   this before writing out a block of the security context.  Then, if a problem
   arises while writing out the block, we can revert to the marked position. */
struct ardp__sec_mark_position
ardp__sec_mark_position(RREQ req)
{
    struct ardp__sec_mark_position posit;
    PTEXT pkt = req->sec ? req->sec->previous : NOPKT;

    posit.req = req;
    posit.old_ioptr = pkt->ioptr;
    posit.pkt = pkt;

    return posit;
}

void
ardp__sec_restore_position(RREQ req, struct ardp__sec_mark_position posit)
{
    PTEXT pkt = posit.pkt ? posit.pkt : req->sec;
    PTEXT deleteme;

    assert(posit.req == req);	/* sanity as usual. */
    pkt->ioptr = posit.old_ioptr;
    pkt->length = pkt->ioptr - pkt->start;
    
    /* Chop off all packets in req->sec that follow pkt */
    while((deleteme = req->sec->previous) != pkt) {
	EXTRACT_ITEM(deleteme, req->sec);
	ardp_ptfree(deleteme);
    }
}


#endif
