/*
 * Copyright (c) 1991-1998 by the University of Southern California
 *
 * For copying and distribution information, please see the file
 * <usc-license.h>.
 */

#include <usc-license.h>
/* ARDP internals functions. */
/* We might want to put these into ardp.h later. */
/* Right now we don't, as part of our strategy of hiding the internals from the
   rest of the world. */ 


#define UFACTOR 1000000		/* convert seconds to microseconds */

/* Please maintain this list in alphabetical order. */
extern enum ardp_error (*ardp__accept)(void);
extern void ardp__adjust_backoff(struct timeval *tv);
extern struct timeval ardp__gettimeofday(void);
extern struct timeval ardp__next_activeQ_timeout(const struct timeval now);
extern void ardp__next_cid_initialize_if_not_already();
