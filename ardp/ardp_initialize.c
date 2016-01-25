/*
 * Copyright (c) 1993, 1994, 1995 by the University of Southern California
 *
 * For copying and distribution information, please see the file
 * <usc-license.h>.
 */

/* Authors: Sio-Man Cheang and Steven Augart */

#include <usc-license.h>
#include <stdlib.h>		/* for getenv() */
#include <string.h>
#ifdef GL_THREADS
#include <signal.h>
#endif /* GL_THREADS */

#include "ardp_sec_config.h"	/* Override configuration defaults in ardp.h */

#include <ardp.h>
#ifndef ARDP_STANDALONE
#include <pconfig.h>
#endif

#ifndef ARDP_STANDALONE
static p_config_query_type ardp_config_table[]; /* forward decl. */
#endif

void
ardp_initialize(void)
{
    if (ardp_debug == 0) {       /* if specified in a command line flag or
                                   otherwise by the program itself, this
                                   overrides the environment variable.  */
	/* This works because p_command_line_preparse(int *argcp, char **argv)
	   is always called before p_initialize() is called.
	   p_command_line_preparse() sets pfs_debug and ardp_debug based on the
	   -D flag.  */
        char *ardp_debug_string; /* temporary */
        if((ardp_debug_string = getenv("ARDP_DEBUG")))
            sscanf(ardp_debug_string, "%d", &ardp_debug);
#if 0
	if (!ardp_debug)
	    ardp_debug = 9;	/* ARDP's default if unspecified.  Why not.  */
#endif
	/* So, the order (as of right now, 8/27/96): --swa
	   0) setting ardp_debug after ardp_initialize() overrides
	   1) -D flag on command line (if non-zero) overrides
	   2) ardp_debug global variable defined at compilation time, if non-zero, overrides
	   3) ARDP_DEBUG environment variable (any value) overrides
	   4) value of pfs_debug, iff set by PFS_DEBUG envar (iff PFS_DEBUG
	    envar was examined, which it won't be if:
	    a) pfs_debug was set at comp. time
	    b) -D(anything-non-zero) was set
	    */
	/* XXX yes, the above is hokey and seems gross.  The whole
	   configuration thing is nearly beyond me. */
    }
    ardp__next_cid_initialize_if_not_already();
#ifndef ARDP_STANDALONE
    gl_initialize();
#else
#endif /* ARDP_STANDALONE */

#ifndef GL_THREADS
    ardp_init_mutexes();        /* need not be called if not running threaded.
                                   Won't hurt though. */ 
#endif

#ifndef ARDP_STANDALONE
    p_read_config("ardp", &(ardp_config.default_peer), ardp_config_table);    
#endif

#ifdef GL_THREADS
    ardp_init_daemon();
#endif /* GL_THREADS */
}

/* Ardp Configuration Structure, defined in <ardp.h> */
/* By default, all of the members of this structure are set to zero; this
   happens because of how C external data objects are initialized.   Therefore,
   any new members added later will be left to zero. */
/* We initialize this structure here so that (a) not calling ardp_initialize()
   will be less disastrous and (b) so that ARDP_STANDALONE will work properly. */
/* 9/4/97: However, there is a problem: any strings we provide here, if
   .ProsperoResources overrides them, will cause stcopyr() to blow up.  This is
   because stcopyr() expects the destination to be either NULL or a pointer to
   a GOSTlib string.  These are not GOSTlib strings; they are static character
   arrays.  Therefore, we'll use a kludge you can see here.  If ARDP_STANDALONE
   is defined, then we will continue to do static initialization; otherwise, we
   will leave any strings set to NULL. */
#ifdef ARDP_STANDALONE
#define Initstr(s) (s)
#else
#define Initstr(s) (NULL)
#endif
struct ardp_config_type ardp_config = {
    Initstr(ARDP_DEFAULT_PEER_DEFAULT),
    ARDP_DEFAULT_PORT_DEFAULT,
    ARDP_DEFAULT_TIMEOUT_DEFAULT,
    ARDP_DEFAULT_RETRY_DEFAULT,
    ARDP_DEFAULT_WINDOW_SZ_DEFAULT,
    ARDP_MAX_WINDOW_SZ_DEFAULT,
    ARDP_FIRST_PRIVP_DEFAULT,
    ARDP_NUM_PRIVP_DEFAULT,
    0,                         /* my receive window size */
    ARDP_VERSION_1,		/* preferred ARDP version */
    0,				/* wait time */
    0,				/* client request queue status */
    NULL,			/* server redirect to */
    Initstr(ARDP_SERVER_PRINCIPAL_NAME_DEFAULT), /* server principal name */
    Initstr(ARDP_KERBEROS_SRVTAB_DEFAULT), /* Kerberos SRVTAB name. */
};

#ifndef ARDP_STANDALONE
/* Table of Standard Ardp Configuration Titles, Types and Defaults */
/* This is used by the pconfig routines. */
static p_config_query_type ardp_config_table[] = {
    {"default_peer", P_CONFIG_STRING, ARDP_DEFAULT_PEER_DEFAULT},
    {"default_port", P_CONFIG_INT, (char *)ARDP_DEFAULT_PORT_DEFAULT},
    {"default_timeout", P_CONFIG_INT, (char *)ARDP_DEFAULT_TIMEOUT_DEFAULT},
    {"default_retry", P_CONFIG_INT, (char *)ARDP_DEFAULT_RETRY_DEFAULT},
    /* Peer window size the server will use for its default flow-control
       strategy.   Sets the window size we assume our peer will accept in
       the absence of an explicit request. */
    {"default_window_sz", P_CONFIG_INT, (char *)ARDP_DEFAULT_WINDOW_SZ_DEFAULT}, 
    {"max_window_sz", P_CONFIG_INT, (char *)ARDP_MAX_WINDOW_SZ_DEFAULT},
    {"first_privp", P_CONFIG_INT, (char *)ARDP_FIRST_PRIVP_DEFAULT},
    {"num_privp", P_CONFIG_INT, (char *)ARDP_NUM_PRIVP_DEFAULT},

    {"my_receive_window_size", P_CONFIG_INT, (char *) 0},
    {"preferred_ardp_version", P_CONFIG_INT, (char *) ARDP_VERSION_1},
    /* These flags are likely to be only ever used for debugging. */
    {"wait_time", P_CONFIG_INT, (char *) 0},
    {"client_request_queue_status", P_CONFIG_BOOLEAN, (char *) 0},
    /* Call ardp_redirect() as soon as we get any packet from a client. */
    {"server_redirect_to", P_CONFIG_STRING, (char *) NULL},
    {"server_principal_name", P_CONFIG_STRING, 
     (char *) ARDP_SERVER_PRINCIPAL_NAME_DEFAULT},
    {"kerberos_srvtab", P_CONFIG_STRING, 
     (char *) ARDP_KERBEROS_SRVTAB_DEFAULT},
    {NULL, P_CONFIG_STRING, NULL}
};
#endif
