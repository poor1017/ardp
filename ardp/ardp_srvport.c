/*
 * Copyright (c) 1991-1997 by the University of Southern California
 *
 * For copying and distribution information, please see the file
 * <usc-license.h>.
 */

#include <usc-license.h>
#include <ardp.h>

/* We keep these in a separate file because they are examined by
   ardp_process_active().  We don't want to be forced to link in the whole
   server just because we refer to these variables. */

int ardp_srvport = -1;
int ardp_prvport = -1;

/* ardp_partialQ is declared in ardp.h and defined in ardp_accept.c.  These
   definitions used to sit with it; they were moved here to keep executable
   size down, since some aspects of the code (e.g., the consistency checks in
   ardp_rqfree()) examine these server-specific queues but don't need to have
   the server-specific routines linked in. */  
EXTERN_MUTEXED_DEF_INITIALIZER(RREQ,ardp_pendingQ,NOREQ); /*Pending requests*/ 
/* These two fall under the ardp_pendingQ mutex; don't need their own. */
int		pQlen = 0;               /* Length of pending queue         */
int		pQNlen = 0;              /* Number of niced requests        */

EXTERN_MUTEXED_DEF_INITIALIZER(RREQ,ardp_runQ,NOREQ); /* Requests currently in
                                                          progress  */ 

EXTERN_MUTEXED_DEF_INITIALIZER(RREQ,ardp_doneQ,NOREQ); /* Processed requests 
                                                         */
/* These are mutexed with the ardp_doneQ. */
int		dQlen = 0;      	 /* Length of reply queue           */
int		dQmaxlen = 20;		 /* Max length of reply queue       */

