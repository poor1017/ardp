/* -*-c-*- */
/*
 * Copyright (c) 1991-1998 by the University of Southern California
 *
 * For copying and distribution information, please see the file
 * <usc-license.h>.
 */

#include <usc-license.h>

#ifndef ARDP_NO_SECURITY_CONTEXT
#include <ardp.h>

#include "ardp_sec.h"

static const
char *names[] = {
    "NONE",
    "JUST_ALLOCATED",
    "STATIC_CONSTANT",
    "PREP_SUCCESS",
    "PREP_FAILURE",
    "COMMITTMENT_SUCCESS",
    "COMMITTMENT_FAILURE",
    "RECEIVED",
    "IN_PROCESS",
    "IN_PROCESS_DEFERRED",
    "PROCESSED_SUCCESS", 
    "PROCESSED_FAILURE",
    "REPLIED"
};

const char *
ardp_sec_processing_state_str(const ardp_sectype *const sec)
{
    return names[sec->processing_state];
}
#endif /* ndef ARDP_NO_SECURITY_CONTEXT */
