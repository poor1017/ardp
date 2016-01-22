/*
 * Copyright (c) 1996 by the University of Southern California
 *
 * For copying and distribution information, please see the file
 * <usc-license.h>.
 */

/* Author: Steve Augart <swa@ISI.EDU>, 9/96 */
/* 2/97: Added handling so that 'ardp_bogustime' is always treated specially,
 * when possible.  This was informative; it shows how time consuming error
 * handling can be, and how unclear the right thing to do is. 
 *
 * The functions in this file are my test case for
 *  the GL_FUNCTION_ARGUMENTS_ERROR_PREFERRED compilation directive. 
 */
/*
 * Math functions on 'struct timeval's.  These could all be broken into
 * individual files; this might decrease the code size of the clients.
 * However, if we do that, we'd probably better do it for everything.  That
 * would be a lot of files.  Any thoughts on this?  --swa, katia, 2/97
 */

#include <usc-license.h>
#include <sys/time.h>
#include <ardp.h>
#include <ardp_time.h>
#include "ardp__int.h"



/* Constant: ZERO */
const struct timeval zerotime = {0,0};


/* We pick the most weird value we can to indicate infinity.  This is not a
   valid struct timeval; the microseconds field is (much) too big. */
const struct timeval infinitetime = {-1,-1};


/* We pick a pretty weird value to indicate bogus timestamps. */
const struct timeval ardp_bogustime = {-2,-2};



/* Is time T1 equal to or later than T2?  Boolean returns */
int 
time_is_later(struct timeval t1, struct timeval t2)
{
    if (eq_timeval(t1,ardp_bogustime) || 
	eq_timeval(t2, ardp_bogustime)) {
	gl_function_arguments_error("time_is_later(): bad arguments.");
	/* NOTREACHED */
    }
    /* If one or the other is infinite, special handling --swa, 2/97 */
    if (eq_timeval(t2,infinitetime))
	return eq_timeval(t1,infinitetime);
    if (eq_timeval(t1,infinitetime))
	return TRUE;
    return ((t1.tv_sec > t2.tv_sec)
	    || ((t1.tv_sec == t2.tv_sec) && (t1.tv_usec >= t2.tv_usec)));
}

struct timeval
add_times(struct timeval t1, struct timeval t2)
{
    struct timeval retval;

    if (eq_timeval(t1,ardp_bogustime) || 
	eq_timeval(t2, ardp_bogustime)) {
#if GL_FUNCTION_ARGUMENTS_ERROR_PREFERRED
	gl_function_arguments_error("add_times(): bad arguments.");
	/* NOTREACHED */
#else
	return ardp_bogustime;
#endif
    }
    retval.tv_sec = t1.tv_sec + t2.tv_sec;
    retval.tv_usec = t1.tv_usec + t2.tv_usec;
    if (retval.tv_usec >= UFACTOR) {
	retval.tv_usec -= UFACTOR;
	++(retval.tv_sec);
    }
    return retval;
}

struct timeval
ardp__gettimeofday(void)
{
    struct timeval now;
    int tmp = gettimeofday(&now, NULL);
    if (tmp)
	internal_error("gettimeofday() returned error code; can't \
happen!"); 
    return now;
}

/* We do not treat ardp_bogustime specially here; it's ok to compare
   against.   This is like C's math library (with special NaN returns), but
   unlike Java, where two NaNs do not compare equal.  --swa, 2/97 */  
int
eq_timeval(const struct timeval t1, const struct timeval t2)
{
    return t1.tv_sec == t2.tv_sec && t1.tv_usec == t2.tv_usec;
}


/* Return the difference of two timevals: minuend - subtrahend */
/* subtract_timeval() will never return negative values; zero only. */
/* infinity minus anything is infinity. */

struct timeval
subtract_timeval(const struct timeval minuend, const struct timeval subtrahend)
{
    register int32_t tmp;
    struct timeval difference;
    int borrow = 0;

    if (eq_timeval(subtrahend,infinitetime)) /* should never happen */ {
	/* This is a nasty decision: which of these is better?  My response to
	   BAA 96-40  could have used this as an example. */
#if GL_FUNCTION_ARGUMENTS_ERROR_PREFERRED
	gl_function_arguments_error("subtract_timeval(): subtrahend can't be
infinite.");
#else
	return ardp_bogustime;
#endif
    }
    if (eq_timeval(subtrahend,ardp_bogustime) || 
	eq_timeval(minuend, ardp_bogustime)) {
#if GL_FUNCTION_ARGUMENTS_ERROR_PREFERRED
	gl_function_arguments_error("subtract_timeval(): bad arguments.");
	/* NOTREACHED */
#else
	return ardp_bogustime;
#endif
    }
    if (eq_timeval(minuend,infinitetime))
	return infinitetime;

    tmp = minuend.tv_usec - subtrahend.tv_usec;
    if (tmp < 0) {
	++borrow;	/* borrow a 1 */
	tmp = tmp + UFACTOR;
    }
    difference.tv_usec = tmp;
    tmp = minuend.tv_sec - borrow - subtrahend.tv_sec;
    if (tmp < 0)
	return zerotime;
    difference.tv_sec = tmp;
    return difference;
}


/* This returns ardp_bogustime appropriately, or signals an error. */

struct timeval
min_timeval(const struct timeval t1, const struct timeval t2)
{
    struct timeval difference;

    /* Error handling. */
    if (eq_timeval(t1,ardp_bogustime) || 
	eq_timeval(t2, ardp_bogustime)) {
#if GL_FUNCTION_ARGUMENTS_ERROR_PREFERRED
	gl_function_arguments_error("min_timeval(): bad arguments.");
	/* NOTREACHED */
#else
	return ardp_bogustime;
#endif
    }

    if (eq_timeval(t2, infinitetime))
	return t1;
    difference = subtract_timeval(t1, t2);
    if (eq_timeval(difference,zerotime))
	return t1;
    else
	return t2;
}
