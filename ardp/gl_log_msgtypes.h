/* gl_log_msgtypes.h */

/*
 * Copyright (c) 1991-1995 by the University of Southern California
 *
 * For copying and distribution information, please see the file
 * <usc-license.h>.
 */

#include <usc-license.h>


/* XXX We should turn these into ENUMs.   --swa, 4/25/95 */

#define L_FIELDS	  0	/* Fields to include in messages    */
#define L_STATUS	  1	/* Startup, termination, etc        */
#define L_FAILURE	  2	/* Failure condition                */
#define L_STATS		  3	/* Statistics on server usage       */
#define L_NET_ERR	  4	/* Unexpected error in network code */
#define L_NET_RDPERR      5     /* Reliable datagram protocol error */
#define L_NET_INFO	  6	/* Info on network activity	    */
#define L_QUEUE_INFO      7     /* Info on queue managment          */
/* ARDP or Prospero server */
#define L_QUEUE_COMP	  8     /* Requested service completed      */
/* These are all Prospero  ones. */
#define L_DIR_PERR	  9	/* PFS Directory protocol errors    */
#define L_DIR_PWARN      10	/* PFS Directory protocol warning   */
#define L_DIR_PINFO	 11	/* PFS Directory protocol info	    */
#define L_DIR_ERR	 12	/* PFS Request error		    */
#define L_DIR_WARN	 13	/* PFS Request warning		    */
#define L_DIR_REQUEST	 14	/* PFS information request	    */
#define L_DIR_UPDATE     15     /* PFS information update           */
#define L_AUTH_ERR       16     /* Unauthorized operation attempted */
#define L_DATA_FRM_ERR	 17     /* PFS directory format error       */
#define L_DB_ERROR	 18     /* Error in database operation      */
#define L_DB_INFO	 19     /* Error in database operation      */
#define L_ACCOUNT	 20     /* Accounting info. record          */
/* This is GOSTLIB; usable by others. */
#define L_CONFIG_ERR	 21     /* Error detected in configuration setup. */
/* ARDP security */
#define L_CONTEXT_ERR    22     /* An ARDP critical context (probably the
                                   security context) was not understood by the
                                   ARDP code. */
#define L_NET_PACKET_HDR 23	/* Logging the packet headers as they go out! */
#define L_NET_PACKET_TXT 24	/* Enable this if you want the actual packets
				   to be logged as they fly about! */
