/*
 * Copyright (c) 1997 by the University of Southern California
 *
 * For copying and distribution information, please see the file
 * <usc-license.h>.
 */

#include <usc-license.h>
#include <ardp.h>
#include <ardp_sec.h>

/* These contain the definitions for the ARDP ioctls, in the implementation
   where they are pointers to objects in real memory.  */

#ifdef ARDP__SEC_ARDP__IOCTL_T_CONST_POINTERS_TO_STRUCTS
/* We do this (using the _st and the ardp_ioctl_t pointing to the _st because,
   if we pass a structure, some compilers do so inefficiently.  (PCC
   compatible, for instance).  --swa, 4/97 */
const struct ardp__ioctl_t 
    ARDP_SEC_CRITICALITY_st = { 1 }, 
    ARDP_SEC_ME_st = { 2 },
    ARDP_SEC_YOU_st = { 3 }, 
    ARDP_SEC_PRIVACY_st = { 4 };

const ardp_ioctl_t 
	ARDP_SEC_CRITICALITY = &ARDP_SEC_CRITICALITY_st, 
	ARDP_SEC_ME = &ARDP_SEC_ME_st,
	ARDP_SEC_YOU = &ARDP_SEC_YOU_st,
	ARDP_SEC_PRIVACY = &ARDP_SEC_PRIVACY_st;

#endif


