/*
 * Copyright (c) 1993, 1996 by the University of Southern California
 *
 * For copying and distribution information, please see the file 
 * <usc-license.h>
 *
 * Written  by bcn 1991     as part of the Prospero distribution
 * Modified by bcn 1/93     extended rreq structure
 * Modified by swa 11/93    multithreaded
 */

#include <usc-license.h>
#include <stdio.h>
#include <stdlib.h>             /* malloc(), free(), memset() */

#include <pmachine.h>		/* memset() prototype, if needed */
#include <ardp.h>
#include <string.h>		/* The ANSI C standard (X3.159-1989) says that
				   <string.h> is the standard header file for
				   the memset() prototype. */
#ifndef ARDP_STANDALONE		/* standalone ARDP doesn't know about
				   strings. */ 
#include <gl_strings.h>
#endif

/* Defaults are set here. */
#if 0
#define ARDP_MY_WINDOW_SZ   2 /* Our window size;currently only implemented on
                                 the client.  Here, the client asks for a
                                 window size of 2, for testing. */
#endif
#ifndef ARDP_NO_SECURITY_CONTEXT
#include "ardp_sec.h"
#endif

#ifdef PURIFY
#include <purify.h>		/* PURIFY macros. */
#endif

int    ardp_priority = 0; /* Default priority */

static RREQ	lfree = NOREQ;
int 		rreq_count = 0;
int		rreq_max = 0;

#ifndef NDEBUG
static int rreq_member_of_list(RREQ rr, RREQ list);
#endif

static void *(*rqappallocfunc)(void) = NULL; /* yield a generic pointer,
                                                If no memory left, you can
                                                either signal your own
                                                out_of_memory() error, or just
                                                return NULL and ardp_rqalloc()
                                                will perform its own
                                                out-of-memory handling. */
static void (*rqappfreefunc)(void *) = NULL; /* destroy memory referred to by
                                                allocated generic pointer.  */

const int  ardp__mbz[ARDP__DBG_MBZ_SIZE]; /* Initialized to zero by compiler. */
/*
 * Specify which special allocation function (if any) to be used to set the
 * app.ptr member of the RREQ structure when an RREQ is allocated.
 */
void            
ardp_rqappalloc(void *(* appallocfunc)(void))
{
    rqappallocfunc = appallocfunc;
}


/*
 * Specify which special freeing function (if any) to be used to free the
 * app.ptr member of the RREQ structure, if set. 
 */
void            
ardp_rqappfree(void (* appfreefunc)(void *))
{
    rqappfreefunc = appfreefunc;
}



/*
 * rralloc - allocate and initialize an rreq structure
 *
 *    ardp_rqalloc returns a pointer to an initialized structure of type
 *    RREQ.  If it is unable to allocate such a structure, it
 *    calls the out_of_memory() handler in gostlib/gl_internal_err.c.
 */


RREQ
ardp_rqalloc(void)
{
    RREQ	rq;

#ifdef GL_THREADS
    pthread_mutex_lock(&(p_th_mutexARDP_RQALLOC));
#endif /* GL_THREADS */
    if(lfree) {
	rq = lfree;
	EXTRACT_ITEM(rq, lfree);
    }
    else {
	rq = (RREQ) malloc(sizeof(RREQ_ST));
	if (!rq) {
            out_of_memory();    /* bail */
            internal_error("should never get here.");
        }
	rreq_max++;
    }
    rreq_count++;
#ifdef GL_THREADS
    pthread_mutex_unlock(&(p_th_mutexARDP_RQALLOC));
#endif /* GL_THREADS */

#if 0
    memset(rq, '\0', sizeof *rq);	/* XXX inefficient; duplicated below.
					   When we are changing the
					   structure and adding members, this
					   saves any surprises. --3/97 */ 
#endif
#ifdef GL_THREADS
    rq->mutex = NULL;
    rq->cond = NULL;
    memset(&rq->thread_id, '\0', sizeof rq->thread_id);
#endif

    rq->status = ARDP_STATUS_NOSTART;
#ifdef ARDP_MY_WINDOW_SZ
    rq->flags = ARDP_FLAG_SEND_MY_WINDOW_SIZE; /*  used by clients. */
#else
    rq->flags = 0;
#endif
    rq->cid = 0;
    rq->priority = ardp_priority;
    rq->pf_priority = 0;
    rq->peer_ardp_version = ardp_config.preferred_ardp_version;
    rq->inpkt = NOPKT;
    rq->rcvd_tot = 0;
    rq->rcvd = NOPKT;
    rq->rcvd_thru = 0;
    rq->comp_thru = NOPKT;
    rq->outpkt = NOPKT;
    rq->prcvd_thru = 0;
    rq->trns_thru = 0;
    rq->trns_tot = 0;
    rq->trns = NOPKT;

#ifndef NDEBUG
    memset(rq->mbz1, 0, sizeof rq->mbz1);
#endif

    rq->prcvd_thru = 0;
    /* This sets the window size we'll ask for */
    /* This is normally used by clients; however, the server will notify
       clients (if it needs to) on receiving that first ACK. */
    if (ardp_config.my_receive_window_size) /* default is zero */ {
       rq->flags |= ARDP_FLAG_SEND_MY_WINDOW_SIZE; 
    } 
    rq->window_sz = ardp_config.my_receive_window_size;	/* Initialize to default. */
    /* pwindow_sz is the window size we assume our peer will accept in the
       absence of an explicit request.  */ 
    rq->pwindow_sz = ardp_config.default_window_sz; 

    memset(&(rq->peer), '\000', sizeof(rq->peer));
    rq->peer_hostname = NULL;
    rq->rcvd_time.tv_sec = 0;
    rq->rcvd_time.tv_usec = 0;
    rq->svc_start_time.tv_sec = 0;
    rq->svc_start_time.tv_usec = 0;
    rq->svc_comp_time.tv_sec = 0;
    rq->svc_comp_time.tv_usec = 0;
    rq->timeout.tv_sec = ardp_config.default_timeout;
    rq->timeout.tv_usec = 0;
    rq->timeout_adj.tv_sec = ardp_config.default_timeout;
    rq->timeout_adj.tv_usec = 0;
    rq->wait_till.tv_sec = 0;
    rq->wait_till.tv_usec = 0;
    rq->retries = ardp_config.default_retry;
    rq->retries_rem = ardp_config.default_retry;
    rq->svc_rwait = 0;
    rq->svc_rwait_seq = 0;
    rq->inf_queue_pos = 0;
    rq->inf_sys_time = 0;
    rq->client_name = NULL;
    rq->peer_sw_id = NULL;
    rq->cfunction = NULL;
    rq->cfunction_args = NULL;
    /*  Security Context */
    rq->seclen = 0;
    rq->sec = NOPKT;
    rq->secq = NULL;

    /* See comment at the head of this function on how we handle
       rqappalloc() being unable to do enough with its memory. --swa
       4/27/95 */
    if (rqappallocfunc) {
        rq->app.ptr = (*rqappallocfunc)();
        if (!rq->app.ptr) {
            ardp_rqfree(rq);
            out_of_memory();      /* bail in case sub-allocator fails. */
            internal_error("should never get here.");
        }
    } else {
        rq->app.ptr = NULL;       
    }

    rq->previous = NOREQ;
    rq->next = NOREQ;
#ifndef NDEBUG
    memset(rq->mbz2, 0, sizeof rq->mbz2);
#endif
    rq->class_of_service_tags = NULL;
    return(rq);
}


/*
 * ardp_rqfree - free a RREQ structure
 *
 *    ardp_rqfree takes a pointer to a RREQ structure and adds it to
 *    the free list for later reuse.
 */
void 
ardp_rqfree(RREQ rq)
{
    if (!rq)
	return;
#ifndef NDEBUG
    /* This is different from the way we do consistency checking in the PFS
       library, because here the status field is just sitting here waiting to
       be used. */ 
    
    assert(rq && rq->status != ARDP_STATUS_FREE);
    rq->status = ARDP_STATUS_FREE;
    assert(!rreq_member_of_list(rq, ardp_activeQ)); 
    assert(!rreq_member_of_list(rq, ardp_completeQ)); 
    /* Don't test for membership in ardp_partialQ, because that is local to
       ardp_accept.c */
    assert(!rreq_member_of_list(rq, ardp_pendingQ)); 
    assert(!rreq_member_of_list(rq, ardp_runQ));
    assert(!rreq_member_of_list(rq, ardp_doneQ));
    assert(memcmp(ardp__mbz, rq->mbz1, ARDP__DBG_MBZ_SIZE) == 0);
    assert(memcmp(ardp__mbz, rq->mbz2, ARDP__DBG_MBZ_SIZE) == 0);
#endif
#ifdef GL_THREADS
    if (rq->cond) {
	pthread_cond_destroy(rq->cond);
	free(rq->cond);
    }
    if (rq->mutex) {
	pthread_mutex_destroy(rq->mutex);
	free(rq->mutex);
    }
#endif /* GL_THREADS */
#ifndef ARDP_NO_SECURITY_CONTEXT
    ardp_selfree(rq->secq);
    rq->secq = NULL;
#endif
    /* Don't free inpkt or comp_thru, already on rcvd     */
    if(rq->rcvd) ardp_ptlfree(rq->rcvd);
    /* But outpkt has not been added to trns */
    if(rq->outpkt) ardp_ptlfree(rq->outpkt);
    if(rq->trns) ardp_ptlfree(rq->trns);

#ifndef ARDP_STANDALONE
    /* We do this because the ARDP_STANDALONE package does not
       contain support for GOSTLIB BSTRINGs. */
    if (rq->peer_hostname)
	GL_STFREE(rq->peer_hostname);
    if (rq->client_name) 
        GL_STFREE(rq->client_name);
    if (rq->peer_sw_id) 
        GL_STFREE(rq->peer_sw_id);
#endif
#ifndef ARDP_NO_SECURITY_CONTEXT
    ardp_cos_lfree(rq->class_of_service_tags);
#endif
    if (rq->app.ptr && rqappfreefunc) {
        (*rqappfreefunc)(rq->app.ptr);
        rq->app.ptr = NULL;
    }
#ifdef GL_THREADS
    pthread_mutex_lock(&(p_th_mutexARDP_RQALLOC));
#endif /* GL_THREADS */
#ifdef PURIFY
    if (purify_is_running()) {
	free(rq);
    } else {
#endif
	PREPEND_ITEM(rq, lfree);
#ifdef PURIFY
    }
#endif
    rreq_count--;
#ifdef GL_THREADS
    pthread_mutex_unlock(&(p_th_mutexARDP_RQALLOC));
#endif /* GL_THREADS */
}


#ifndef NDEBUG
/* Free just the fields that are needed only while processing the request
   but not the fields that will be used for a request that is on the ardp_doneQ
   so that the results can be retransmitted.  This is only needed when
   debugging the server.  It is used in ardp_respond() and is #ifdef'd NDEBUG.
*/
/* Note that this function assumes it can safely free the APP structure too. */

void
ardp_rq_partialfree(RREQ rq)
{
#ifndef ARDP_STANDALONE
    if (rq->client_name)
        GL_STFREE(rq->client_name);
    if (rq->peer_sw_id) 
        GL_STFREE(rq->peer_sw_id);
#endif
    if (rq->app.ptr && rqappfreefunc) {
        (*rqappfreefunc)(rq->app.ptr);
        rq->app.ptr = NULL;
    }
}
#endif


/*
 * ardp_rqlfree - free many RREQ structures
 *
 *    ardp_rqlfree takes a pointer to a RREQ structure frees it and any linked
 *    RREQ structures.  It is used to free an entrie list of RREQ
 *    structures.
 */

void 
ardp_rqlfree(RREQ rq)
{
    RREQ	nxt;

    while(rq != NOREQ) {
	nxt = rq->next;
	ardp_rqfree(rq);
	rq = nxt;
    }
}


/*
 * ardp_set_retry - change default values for timeout
 *
 *    ardp_set_retry takes a value for timout in seconds and a count
 *    of the number of retries to allow.  It sets static variables in this
 *    file used by ardp_rqalloc() to set the default values in request
 *    structures it allocates.
 *
 *  4/28/95:  This function is obsolete now.  Using it has been replaced by
 *          setting fields in the ardp_config structure.
 */
enum ardp_errcode
ardp_set_retry(int to, int rt)
{
    /* XXX This is a critical section, but it is safe as long as we are not on
       a multiprocessor and don't block.  So no mutexes. */
    /* XXX In any case, the worst that can happen is that we'll possibly 
       use a new ardp timeout with an old # of retries. --swa, 4/28/95 */
    ardp_config.default_timeout = to;
    ardp_config.default_retry = rt;
    /*** XXX End Critical section */
    return(ARDP_SUCCESS);
}


#ifndef NDEBUG
/* Return 1 if the RREQ rr is a member of the list LIST. */
static int 
rreq_member_of_list(RREQ rr, RREQ list)
{
    for ( ; list; list = list->next) {
        if (rr == list) return 1;
    }
    return 0;                   /* false */
}
#endif
