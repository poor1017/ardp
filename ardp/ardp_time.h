/* ardp_time.h */
/*
 * Copyright (c) 1991-1998 by the University of Southern California
 *
 * For copying and distribution information, please see the file
 * <usc-license.h>.
 */

#include <usc-license.h>

/* Please maintain this list in alphabetical order. */
extern struct timeval add_times(struct timeval t1, struct timeval t2);
extern const struct timeval ardp_bogustime;
extern int eq_timeval(const struct timeval t1, const struct timeval t2);
extern struct timeval min_timeval(const struct timeval t1, const struct timeval t2);
extern struct timeval subtract_timeval(const struct timeval minuend, 
				       const struct timeval subtrahend);
extern int time_is_later(struct timeval t1, struct timeval t2);
extern const struct timeval infinitetime;
extern const struct timeval zerotime;
