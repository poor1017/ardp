/*
 * Copyright (c) 1998 by the University of Southern California
 *
 * Written  by Nader Salehi 1998
 *
 * For copying and distribution information, please see the file
 * <usc-license.h>.
 */

#include <usc-license.h>

#ifdef GL_THREADS
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/uio.h>
#include <pthread.h>

#include <ardp.h>
#include <ardp_sec.h>		/* ardp_sec_commit() prototype. */
#include <ardp_time.h>		/* internals routines; add_times() */
#include "ardp__int.h"		/* ardp__gettimeofday() */
#include <perrno.h>

static int timecmp(struct timeval *tv1, struct timeval *tv2);

static int timecmp(struct timeval *tv1, struct timeval *tv2)
{
    if (tv1->tv_sec > tv2->tv_sec) return 1;
    if (tv1->tv_sec < tv2->tv_sec) return -1;
    /* tv1->tv_sec == tv2->tv_sec */
    if (tv1->tv_usec > tv2->tv_usec) return 1;
    if (tv1->tv_usec < tv2->tv_usec) return -1;
    return 0;
} /* timecmp */

RREQ first_timeout(void) 
{
    RREQ index, 
	min;

    min = ardp_activeQ;
    for (index = ardp_activeQ; index; index = index->next)
	if (timecmp(&(min->wait_till), &(index->wait_till)) == 1)
	    min = index;
    return min;
} /* heap_first_timeout */

#endif /* GL_THREADS */
