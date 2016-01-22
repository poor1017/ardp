/*
 * Copyright (c) 1996 by the University of Southern California
 *
 * For copying and distribution information, please see the file
 * <usc-license.h>.
 */

#include <usc-license.h>
#include <ardp.h>
#include <ardp_time.h>

/* Authors:  Steve Augart, Sung Wook Ryu, 6/96, 9/96 */


/* This is returned as the amount of time until the next timeout occurs. */
/* This function is exported (it's part of the interface) */
/* We pack it with ardp_pr_actv() because any ARDP application uses
   ardp_pr_actv(), and ardp_pr_actv() uses this. */
struct timeval 
ardp__next_activeQ_timeout(const struct timeval now)
{
    RREQ req;
    struct timeval soonest;
    
    soonest = infinitetime;
    EXTERN_MUTEXED_LOCK(ardp_activeQ);
    for (req = ardp_activeQ; req; req = req->next) {
	soonest = min_timeval(soonest, req->wait_till);
    }
    EXTERN_MUTEXED_UNLOCK(ardp_activeQ);
    return subtract_timeval(soonest, now);
}


