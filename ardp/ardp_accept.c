/*
 * Copyright (c) 1992, 1993, 1994, 1995 by the University of Southern California 
 *
 * For copying and distribution information, please see the file
 * <usc-license.h>
 *
 * Written  by bcn 1991     as check_for_messages in rdgram.c (Prospero)
 * Modified by bcn 1/93     modularized and incorporated into new ardp library
 * Modified by swa 12/93    made handle arbitrary length ardp_runQ
 * Modified by swa 6/94     made more portable to LINUX 
 * Modified by swa 3/95     for bunyip; trapping errno 9 in bad rcvfrom.
 * Modified by swa 9/95	    more changes to signal system.
 */

#include <usc-license.h>

#include <netdb.h>
#include <stdio.h>
#include <sys/param.h>
/* Need types.h for u_short */
#include <sys/types.h>
#include <sys/socket.h>
#ifdef AIX
#include <sys/select.h>
#endif

#include <string.h>		/* for strstr() proto. */

#include <gl_threads.h>	/* Mutex stuff */
#include <pmachine.h>

#include <errno.h>
#include <ardp.h>
#include <ardp_sec.h>
#include "ardp__int.h"
#include <ardp_time.h>
#include <gl_log_msgtypes.h>
#include "list_macros.h"

/* #include <pprot.h> */

/* This can be set by the library's caller. --swa, 4/25/95  */
/* Should probably be mutexed, but it's called outside of multi-threaded
   mode. */ 
void (*ardp_newly_received_additional)(RREQ nreq) = NULL; /* initially unset */

/* ardp_pendingQ,  ardp_runQ, and ardp_doneQ are declared in ardp.h and defined
   in ardp_srvport.c.  This is done to keep executable size down. */
static RREQ	ardp_partialQ = NOREQ;	 /* Incomplete requests             */
/* These fall under the ardp_partialQ mutex; don't need their own. */
static int ptQlen = 0;/* Length of incomplete queue      */
/* user configurable */
int		ptQmaxlen = 20;	/* Max length of incomplete queue  */


#define LOG_PACKET(REQ,QP) \
    if((REQ)->priority || (pQlen > 4)) \
      if(pQNlen && (REQ)->priority) \
         ardp__log(L_QUEUE_INFO, REQ, "Queued: %d of %d (%d) - Priority %d", \
              QP, pQlen, pQNlen, (REQ)->priority, 0); \
      else if(pQNlen) \
         ardp__log(L_QUEUE_INFO, REQ, "Queued: %d of %d (%d)", \
              QP, pQlen, pQNlen, 0); \
      else if((REQ)->priority) \
         ardp__log(L_QUEUE_INFO, REQ, "Queued: %d of %d - Priority %d", \
               QP, pQlen, (REQ)->priority, 0); \
      else if(QP != pQlen) \
         ardp__log(L_QUEUE_INFO, REQ, "Queued: %d of %d", QP, pQlen, 0); \
      else ardp__log(L_QUEUE_INFO, REQ, "Queued: %d", pQlen, 0); 


#ifndef max
#define max(x,y) ((x) > (y) ? (x) : (y))
#endif
#ifndef min
#define min(x,y) ((x) < (y) ? (x) : (y))
#endif

static void ardp_update_cfields(RREQ existing, RREQ newvalues);
static void ardp_header_ack_rwait(PTEXT tpkt, RREQ nreq, int is_ack_needed, 
                                  int is_rwait_needed); 

/* XXX DOES NOT DO ANY RETRANSMIT QUEUE TIMEOUTS --swa, katia, 2/97 */

enum ardp_errcode
ardp_accept_and_wait(int timeout_sec, int timeout_usec)
{
    struct sockaddr_in 	from;
    int			fromlen;
    int			n = 0;
    PTEXT		pkt;
    unsigned char       flags1,flags2;
    u_int16_t     pid;    /* protocol ID for higher-level protocol.  This
                               is currently ignored after being set. */
    int			qpos; /* Position of new req in queue       */
    int			dpos; /* Position of dupe in queue          */
    RREQ		creq; /* Current request                    */
    RREQ		treq; /* Temporary request pointer          */
    RREQ		nreq; /* New request pointer                */
    RREQ		areq = NOREQ; /* Request needing ack        */
    RREQ                match_in_runQ = NOREQ; /* if match found in runq for
                                                  completed request. */
    int			ack_bit_set; /* ack bit set on packet we're processing?
				      */
    char		*ctlptr;
    u_int16_t		stmp;
    int			tmp;
    int			check_for_ack = 1; 
    fd_set		readfds; /* used for select */
    struct timeval	now = ardp_bogustime; /* Time - used for retries  */
    struct timeval	rr_time = zerotime; /* Time last retrans from done
					       queue */ 
    struct timeval	time_out = ardp_bogustime; 


#ifdef GL_THREADS
    /* Non-zero value (C true) means we didn't get the lock */
    if (pthread_mutex_trylock(&(p_th_mutexARDP_ACCEPT)))
        return ARDP_SUCCESS;    /* ARDP_ACCEPT is already locked, which means
				   that packets are already being retrieved. */
#endif /* GL_THREADS */
 check_for_more:
    /* This is initialized afresh before each select().  Some operating
       systems, such as LINUX, modify the time_out parameter to SELECT().  
       --swa, 6/19/94 */

    time_out.tv_sec = timeout_sec;
    time_out.tv_usec = timeout_usec;
    now = ardp__gettimeofday();

    /* Check both the prived and unprived ports if necessary */
    FD_ZERO(&readfds);
    FD_SET(ardp_srvport, &readfds);
    if(ardp_prvport != -1) FD_SET(ardp_prvport, &readfds); 

#if 0
#ifdef GL_THREADS_FLORIDA
    /* We hoped this would fix a bug under FSU Pthreads 2.8: We believe their
       implementation of select() hangs when given a wait time of zero.
       So we set it to a tiny value. --swa & egim, 5/15/96 */
    /* This didn't work; select() still hung. */
    if (time_out.tv_sec == 0 && time_out.tv_usec == 0)
	time_out.tv_usec = 1;
#endif
#endif /* 0 */
    tmp = select(max(ardp_srvport,ardp_prvport) + 1,
		 &readfds,(fd_set *)0,(fd_set *)0,&time_out);
    
    if(tmp == 0) {
	if(areq) ardp_acknowledge(areq); areq = NOREQ;
#ifdef GL_THREADS
        pthread_mutex_unlock(&(p_th_mutexARDP_ACCEPT));
#endif /* GL_THREADS */
	return ARDP_SUCCESS;
    }
    if(tmp < 0) {
#ifdef GL_THREADS
        pthread_mutex_unlock(&(p_th_mutexARDP_ACCEPT));
#endif /* GL_THREADS */
        return ARDP_SELECT_FAILED;
    }
    creq = ardp_rqalloc();
    pkt = ardp_ptalloc();
    
    /* There is a message waiting, add it to the queue */
    
    fromlen = sizeof(from);
    if((ardp_prvport >= 0) && FD_ISSET(ardp_prvport,&readfds))
	n = recvfrom(ardp_prvport, pkt->dat, ARDP_PTXT_LEN_R, 0, 
		     (struct sockaddr *) &from, &fromlen);
    else {
        assert(FD_ISSET(ardp_srvport, &readfds));
        n = recvfrom(ardp_srvport, pkt->dat, ARDP_PTXT_LEN_R, 0, 
                     (struct sockaddr *) &from, &fromlen);
    }
    if (n <= 0) {
	ardp__log(L_NET_ERR,NOREQ,"Bad recvfrom n = %d errno = %d %s",
	     n, errno, unixerrstr(), 0);

        /* I added this in response to a problem experienced by Bunyip. 
           --swa, 3/21/95 */
	/* Modified further to work with egim too  -- swa, 7/11/96 */
        if (errno == EBADF || errno == ENOTSOCK) {/* #s 9 and 38 */
            if ((ardp_prvport >= 0) && FD_ISSET(ardp_prvport, &readfds))
                ardp__log(L_NET_ERR, NOREQ, "The bad descriptor was descriptor #%d,
represented internally by the variable ardp_prvport", ardp_prvport);
            else if (FD_ISSET(ardp_srvport, &readfds))
                ardp__log(L_NET_ERR, NOREQ, "The bad descriptor was descriptor #%d,
represented internally by the variable ardp_srvport", ardp_srvport);
            else
                internal_error("Something is wrong with your system's version
of select().  No descriptors are readable; unclear how we got here.");

            ardp__log(L_NET_ERR, NOREQ, 
                 "This should never happen.  Giving up.");
	    internal_error("It's time to restart whatever server we're \
running; select() is misbehaving in ardp_accept().");
	    /* Under Prospero, at this point the internal_error() handling code
	       (the (*internal_error_handler)()) will restart the Prospero
	       server with SIGUSR1.  Other servers will do whatever they choose
	       to.  This avoids doing something Prospero-specific. */
	    /* NOTREACHED */
        }
	ardp_rqfree(creq); ardp_ptfree(pkt);
#ifdef GL_THREADS
        pthread_mutex_unlock(&(p_th_mutexARDP_ACCEPT));
#endif /* GL_THREADS */
	return ARDP_BAD_RECV;
    }
    
    memcpy(&(creq->peer),&from,sizeof(creq->peer));
    creq->cid = 0; 
    creq->rcvd_time = now;
    creq->prcvd_thru = 0;
    
    pkt->start = pkt->dat;
    pkt->length = n;
    *(pkt->start + pkt->length) = '\0';
    pkt->mbz = 0; /* force zeros to catch runaway strings     */
    
    pkt->seq = 1;
    /* If < 32, then we have a full-v0 packet. */
    /* It is actually the case that a first octet of zero is the
       version-independent version-unknown message.  The server should never
       receive such a message when acting as a server. */
    /* If 129, it's a v1 (new) packet.   See comment '*ARDP-V1' below. */
    /* If a printable ASCII character, it's almost certainly a pre-v0 request.
       There are still old Prospero (archie) clients around which send those
       out.  We will yank support for pre-v0 after updated versions of xarchie
       are widely available. See Comment '*ARDP-PRE-V0' below. */
    if((unsigned char) *(pkt->start) < 32) {
	int     hdr_len = (unsigned char) pkt->start[0];
	creq->peer_ardp_version = 0; /* might reset to -1 or -2 later? */

	ctlptr = pkt->start + 1;
	pkt->seq = 0;
	if(hdr_len >= 3) { 	/* Connection ID */
	    memcpy2(&stmp, ctlptr);
	    if(stmp) creq->cid = ntohs(stmp);
	    ctlptr += 2;
	}
#ifdef ARDP_PURE_V1_SERVER /* For testing now; can be used later if we
                                 decide to not support the code below
                                 again. */ 
       
       ardp_version_not_supported_by_server(creq);
       ardp_rqfree(creq);
       ardp_ptfree(pkt);
       goto check_for_more;
#endif

	if(hdr_len >= 5) {	/* Packet number */
	    memcpy2(&stmp, ctlptr);
	    pkt->seq = ntohs(stmp);
	}
	else { /* No packet number specified, so this is the only one */
	    pkt->seq = 1;
	    creq->rcvd_tot = 1;
	}
	ctlptr += 2;
	if(hdr_len >= 7) {	    /* Total number of packets */
	    memcpy2(&stmp, ctlptr);  /* 0 means don't know      */
	    if(stmp) creq->rcvd_tot = ntohs(stmp);
	}
	ctlptr += 2;
	if(hdr_len >= 9) {	/* Receievd through */
	    memcpy2(&stmp, ctlptr);  /* 0 means don't know      */
	    if(stmp) {
		creq->prcvd_thru = ntohs(stmp);
	    }
	}
	ctlptr += 2;
	if(hdr_len >= 11) {	/* Backoff */
	    /* Not supported by server */
	}
	ctlptr += 2;
	
	flags1 = flags2 = 0;
	if(hdr_len >= 12) flags1 = *ctlptr++; /* Flags */
	if(hdr_len >= 13) flags2 = *ctlptr++; /* Flags */
	
	/* Was the ack bit set on the packet we just received? */
	ack_bit_set = flags1 & 0x80;

	/* This next statement will lead to a whole lot of info. being logged.  */
	/* if (ardp_debug >= 8) { */

	ardp__log(L_NET_INFO, creq, 
                 "Received v%d packet (cid = %d; seq = %d of %d%s)", 
                 creq->peer_ardp_version,
                 creq->cid,
		  pkt->seq, creq->rcvd_tot,
		  ack_bit_set ? "; ACK bit set" : "");

	if(flags2 == 1) { /* Cancel request */
       cancel_request:

            EXTERN_MUTEXED_LOCK(ardp_pendingQ);
	    treq = ardp_pendingQ;
	    while(treq) {
		if((treq->cid == creq->cid) && 
		   (treq->peer_port == creq->peer_port) &&
		   (treq->peer_addr.s_addr == creq->peer_addr.s_addr)){
		    ardp__log(L_QUEUE_INFO,treq,
			 "Request canceled by client - dequeued",0);
		    pQlen--;if(treq->priority > 0) pQNlen--;
		    EXTRACT_ITEM(treq,ardp_pendingQ);
		    ardp_rqfree(treq);
		    ardp_rqfree(creq); ardp_ptfree(pkt);
                    EXTERN_MUTEXED_UNLOCK(ardp_pendingQ);
		    goto check_for_more;
		}
		treq = treq->next;
	    }
	    ardp__log(L_QUEUE_INFO,creq,
		 "Request canceled by client - not on queue",0);
	    ardp_rqfree(creq); ardp_ptfree(pkt);
            EXTERN_MUTEXED_UNLOCK(ardp_pendingQ);
	    goto check_for_more;
	}
	
        /* If the client specifies option flags and doesn't specify control
           info to match them, we probably should log this; right now we just
           silently ignore it. */
        if (ctlptr < pkt->start + hdr_len) /* if still control info */ {
            /* Priority specified. */
	    if(flags1 & 0x2) {
                memcpy2(&stmp, ctlptr);
                creq->priority = ntohs(stmp);
                ctlptr += 2;
            }
        }
        if (ctlptr < pkt->start + hdr_len) /* if still control info */ {
            /* Higher-level protocol ID follows. */
            /* N.B.: used to be a bug here where this was 0x1; this is in
               variance with the ARDP spec and I've corrected the
               implementation to conform to the spec. */
	    if(flags1 & 0x4) {
                memcpy2(&stmp, ctlptr);
                pid = ntohs(stmp);
                ctlptr += 2;
            }
        }
        if (ctlptr < pkt->start + hdr_len) /* if still control info */ {
            /* Window size follows (2 octets of information). */
	    if(flags1 & 0x8) {
                memcpy2(&stmp, ctlptr);
                creq->pwindow_sz = ntohs(stmp);
                ctlptr += 2;
                if (creq->pwindow_sz == 0) {
                    creq->pwindow_sz = ardp_config.default_window_sz;
                    ardp__log(L_NET_INFO, creq, 
                         "Client explicity reset window size to server \
default (%d)",
                         ardp_config.default_window_sz);
                } else {
                    if (creq->pwindow_sz > ardp_config.max_window_sz) {
                        ardp__log(L_NET_INFO, creq, "Client set window size \
to %d; server will limit it to %d", creq->pwindow_sz, ardp_config.max_window_sz);
                        creq->pwindow_sz = ardp_config.max_window_sz;
                    } else {
                        ardp__log(L_NET_INFO, creq, 
                             "Client set window size to %d", creq->pwindow_sz);
                    }
                }
            }
        }
	
	if((creq->priority < 0) && (creq->priority != -42)) {
	    ardp__log(L_NET_RDPERR, creq, "Priority %d requested - ignored", creq->priority, 0);
	    creq->priority = 0;
	}
	
	if(pkt->seq == 1) creq->rcvd_thru = max(creq->rcvd_thru, 1);
	
	pkt->length -= hdr_len;
	pkt->start += hdr_len;
        pkt->text = pkt->start;
	/* End of block of code handling v0 ARDP requests. */
    } else if ((unsigned char) pkt->start[0] >= 32 
              && (unsigned char) pkt->start[0] <= 128) {
	/* *ARDP-PRE-V0 */
	/* This is support for the pre-v0 request format.  In this format, 
	   there was no ARDP header, and multi-packet responses were handled by
	   the literal 
	   text 'MULTI-PACKET %d (of %d)' appearing  directly in the text of
	   the message. */
	/* We test here to see whether an explicit ASCII character is the first
	   byte of the packet. */
	/* This test would blow up if there were ever v0 requests that have
	   headers of 32 octets or more; this case does not come up in
	   practice.  (We had many discussions about this; this was not a
	   casual design decision. -- katia & swa, 3/28/96) */
	/* More comments up above on ARDP versions. */

	/* When we are no longer getting any of these old format  */
	/* requests, we know that everyone is using the new       */
	/* reliable datagram protocol, and that they also         */
	/* have the singleton response bug fixed.  We can then    */
	/* get rid of the special casing for singleton responses  */

	/* Neither Katia nor I know what the 'Singleton response bug' was or
	   is.  Neither of us have found any special case handling singleton
	   responses.  --swa & katia, 3/28/96 */
	
	/* Lower the priority to encourage people to upgrade  */
	creq->priority = 1;
	creq->peer_ardp_version = -1;
#ifdef LOGOLDFORMAT
	ardp__log(L_DIR_PWARN,creq,"Old RDP format",0);
#endif LOGOLDFORMAT
	pkt->seq = 1;
	creq->rcvd_tot = 1;
	creq->rcvd_thru = 1;
	flags2 = 0;		/* clear flags2 for this request; will be
				   tested later. */
	ack_bit_set = 0;	/* can't be set in old format; tested later */
	/* End of code parsing PRE-V0 requests */
    } else if (pkt->start[0] == (char) 129) {	/* *ARDP-V1 */


	int	hdr_len;

	creq->peer_ardp_version = 1;
	/* XXX Octet 1 has the contexts; to be implemented */
	pkt->context_flags = pkt->start[1];
	
	/* Octet 2: flags */
	flags1 = pkt->start[2];

	/* Was the ack bit set on the packet we just received? */
	ack_bit_set = flags1 & 0x01;

	flags2 = pkt->start[3];
	hdr_len = (unsigned) pkt->start[4]; /* header length */
	if (hdr_len >= 7) {	/* connection ID */
	    memcpy2(&stmp, pkt->start + 5);
	    if(stmp) creq->cid = ntohs(stmp);
	} 
#ifdef ARDP_PURE_V0_SERVER /* For testing now; can be used later if we
				  decide to not support the code below
				  again. */ 
	
	ardp_version_not_supported_by_server(creq);
	ardp_rqfree(creq);
	ardp_ptfree(pkt);
	goto check_for_more;
#endif
	if(hdr_len >= 9) {	/* Packet sequence number */
	    memcpy2(&stmp, pkt->start + 7 );
	    pkt->seq = ntohs(stmp);
	}
	else { /* No packet number specified, so this is the only one */
	    pkt->seq = 1;
	    creq->rcvd_tot = 1;
	}
	if(hdr_len >= 11) {	/* Received through */
	    memcpy2(&stmp, pkt->start + 9);  /* 0 means don't know      */
	    if(stmp) {
		creq->prcvd_thru = ntohs(stmp);
	    }
	}

	/* Say that we got it. */
	ardp__log(L_NET_INFO, creq, 
		  "Received packet (cid = %d; seq = %d of %d%s; ardp v%d)", creq->cid,
		  pkt->seq, creq->rcvd_tot,
		  ack_bit_set ? "; ACK bit set" : "", creq->peer_ardp_version);
	/* Here, we parse arguments to the various flags.  If a packet is
	   malformed (has options needing arguments, but doesn't have room in
	   the header to specify those arguments), then we will ignore those
	   arguments.  (Could also add logging to look for bad implementations;
	   this isn't necessary though.)
	   */
	ctlptr = pkt->start + 11;
	
	/* **** Handle the octet 2 flags (flags1) : *** */
	/* bit 0: ack bit: handled above */
	/* bit 2: Total packet count specified in a 2-octet argument that
	   follows. */ 
	if ((flags1 & 0x4) && (ctlptr < pkt->start + hdr_len)) {
	    memcpy2(&stmp, ctlptr);
	    creq->rcvd_tot = ntohs(stmp);
	    ctlptr += 2;
	}
	/* Bit 3: Priority specified in a 2-octet argument  */
	if ((flags1 & 0x8) && ctlptr < pkt->start + hdr_len) {
	    memcpy2(&stmp, ctlptr);
	    creq->priority = ntohs(stmp);
	    ctlptr += 2;
	}
	/* Bit 4: protocol id specified in a 2-octet argument */
	if ((flags1 & 0x10) && ctlptr < pkt->start + hdr_len) {
	    memcpy2(&stmp, ctlptr);
	    pid = ntohs(stmp);
	    ctlptr += 2;
	}
        if (ctlptr < pkt->start + hdr_len) /* if still control info */ {
            /* Window size follows (2 octets of information). */
	    if(flags1 & 0x20) {
                memcpy2(&stmp, ctlptr);
                creq->pwindow_sz = ntohs(stmp);
                ctlptr += 2;
                if (creq->pwindow_sz == 0) {
                    creq->pwindow_sz = ardp_config.default_window_sz;
                    ardp__log(L_NET_INFO, creq, 
                         "Client explicity reset window size to server \
default (%d)",
                         ardp_config.default_window_sz);
                } else {
                    if (creq->pwindow_sz > ardp_config.max_window_sz) {
                        ardp__log(L_NET_INFO, creq, "Client set window size \
to %d; server will limit it to %d", creq->pwindow_sz, ardp_config.max_window_sz);
                        creq->pwindow_sz = ardp_config.max_window_sz;
                    } else {
                        ardp__log(L_NET_INFO, creq, 
                             "Client set window size to %d", creq->pwindow_sz);
                    }
                }
            }
	}
	/* Bit 6: Wait time specified in a 2-octet argument.  The server
	   ignores this flag; it is only sent by the server to the client. */
	/* Bit 7: OFLAGS, which means that octet 2 should be interpreted as an
	   additional set of flags.  No flags are currently defined; this is
	   ignored. */
	
	/* **** Handle the octet 3 flags (flags2) : *** */
	if (flags2 == 1) {	/* cancel request */
	    goto cancel_request; /* abuse v0 code. */
	} 
	/* 2: reset peer's received-through count -- we test for this below, in
	   the ardp_doneQ handling. */
	/* 3: packets beyond rcvd-thru -- ignored for now; implement when we do
	   selective acknowledgement. */
	/* 4 & 5 (redirect opts) used only by servers -- client never
	   sends. */
	/* 6, 7: (forwarded opts): could be received by a server; servers don't
	   currently send though. */
	/* 8 -- 252: undefined */
	/* 253: request queue status.  This is NOT
	   recognized by the v0 or v1 server implementations, as of 4/29/96 */
	/* 254: reply to 253.  Server sends it but should never receive it. */
	/* 255: RFU */


	if((creq->priority < 0) && (creq->priority != -42)) {
	    ardp__log(L_NET_RDPERR, creq, "Priority %d requested - ignored", creq->priority, 0);
	    creq->priority = 0;
	}
	
	if(pkt->seq == 1) creq->rcvd_thru = max(creq->rcvd_thru, 1);
	
	pkt->length -= hdr_len;
	pkt->start += hdr_len;
        pkt->text = pkt->start;
	/* End of block of code handling v1 ARDP headers. */
    } else {
        /* POST-v1 (v2+) or MALFORMED */
        /* The high bit is set on the first octet of the header. */
        /* This means that it is a v2 or later request; this is a V0/V1 server,
           so we are trying to deal with the future. */
        assert((unsigned) pkt->start[0] > 129);
        creq->peer_ardp_version = ((unsigned) pkt->start[0]) - 128;
        /* We need to return a bad version indicator to the client. */
        ardp__log(L_DIR_PWARN,creq,"Unknown: V%d ARDP format", 
                  creq->peer_ardp_version, 0);
        /* Generate the version-independent bad-version response. */
        ardp_version_not_supported_by_server(creq);
        ardp_rqfree(creq); 
        ardp_ptfree(pkt);
        goto check_for_more;    /* done handling this. */
    }
    
    /* We are now done parsing the packet.  All of the code after here works in
       terms of the abstractions (the explicitly named members of the RREQ and
       PTEXT structures, and is identical for v0 or v1 (well, almost) */
    
    /* Should we redirect this request?  the req->peer_ardp_version flag is now
       set appropriately. */
    if (ardp_config.server_redirect_to) {
        struct sockaddr_in target_st;
	target_st.sin_port = 0;	/* will use default. */
        if(ardp_hostname2addr(ardp_config.server_redirect_to, &target_st)) {
            /* If we fail to look up the hostname, then whine. */
            ardp__log(L_DIR_PWARN, creq, "DNS server down or \
ardp_config.server_redirect_to misconfigured; couldn't resolve hostname %s; \
ignoring redirect request", 
                      ardp_config.server_redirect_to);
        } else {
            ardp_redirect(creq, &target_st);

            ardp_rqfree(creq);
            ardp_ptfree(pkt);
            goto check_for_more;        /* done handling */
        }
    }
    /* XXX The comment below appears incorrect.  We do not check the pendingQ
       unless we have a complete new request.  This could mean we go to the
       trouble of reassembling the request on the partialQ (and even requesting
       retransmissions!) after we already have a complete version
       pending. --swa, katia, 8/97 */
    /* Check to see if it is already on done, partial, or pending */
    
    /* Done queue */
    EXTERN_MUTEXED_LOCK(ardp_doneQ);
    for(treq = ardp_doneQ; treq; treq = treq->next) {
	if((treq->cid != 0) && (treq->cid == creq->cid) &&
	   (memcmp((char *) &(treq->peer),
		 (char *) &from,sizeof(from))==0)){
	    /* Request is already on doneQ */
           /* flags2 lets us reset the received-through count. */
           if((flags2 == 2) || creq->prcvd_thru > treq->prcvd_thru) {
		treq->prcvd_thru = creq->prcvd_thru;
		rr_time = zerotime; /* made progress, don't supress
				       retransmission */ 
	    } else {
		/* We did not make progress; let's cut back on the */
		/* number of packets so we don't flood the */
		/* client.  This is identical to the client's */
		/* behavior; rationale documented in ardp_pr_actv.c */
		if (treq->pwindow_sz > 4)
		    treq->pwindow_sz /= 2;
	    }
	    nreq = treq; 
	    
	    /* Retransmit reply if not completely received */
	    /* and if we didn't retransmit it this second  */
	    if((nreq->prcvd_thru != nreq->trns_tot) &&
	       (!eq_timeval(rr_time,now) || (nreq != ardp_doneQ))) {
                PTEXT		tpkt; /* Temporary pkt pointer */

		ardp__log(L_QUEUE_INFO,nreq,"Transmitting reply window from %d to %d (prcvd_thru = %d of %d total response size (trns_tot))",
                    nreq->prcvd_thru + 1, min(nreq->prcvd_thru + nreq->pwindow_sz,nreq->trns_tot), nreq->prcvd_thru, nreq->trns_tot, 0);
		/* Transmit a window's worth of outstanding packets */
		for (tpkt = nreq->trns; tpkt; tpkt = tpkt->next) {
		    if((tpkt->seq <= (nreq->prcvd_thru + nreq->pwindow_sz)) &&
		       ((tpkt->seq == 0) || (tpkt->seq > nreq->prcvd_thru))) {
			int ack_bit; /* for debugging output */
                        /* (A) Request an ack at the end of a window
                           (B) Request an ack in the last packet if the 
                               service has rescinded a wait request, but we
                               aren't sure that the client knows this.
                         */

                       /* We re-emphasize the RWAIT (transmit it in all
                          retransmitted packets) if the rescinded rwait
                          hasn't already been acknowledged by client.  This is
                          not actually necessary; the retransmission code
                          above will handle this for us. Therefore, the real
                          purpose of this function is because of the ack
                          bit. */ 
                        ardp_header_ack_rwait(tpkt, nreq, ack_bit = 
                                              /* do set ACK BIT: */
                                              ((/*A*/ tpkt->seq == 
                                               (nreq->prcvd_thru 
                                               + nreq->pwindow_sz))
                                              || /*B*/ 
                                              ((tpkt->seq == nreq->trns_tot)
                                               /* last pkt */
                                               && (nreq->svc_rwait_seq >
                                                   nreq->prcvd_thru))),
                                              /* Might an  RWAIT be needed? */
                                              (nreq->svc_rwait_seq >
                                              nreq->prcvd_thru));
			if (ack_bit)
			    ardp__log(L_NET_INFO, nreq, "Pkt %d will request an ACK", tpkt->seq);
			ardp_snd_pkt(tpkt,nreq);
                    }
		}
		rr_time = now; /* Remember time of retransmission */
	    }
	    /* Move matched request to front of queue */
            /* nreq is definitely in ardp_doneQ right now. */
            /* This replaces much icky special-case code that used to be here
               -- swa, Feb 9, 1994 */
            EXTRACT_ITEM(nreq, ardp_doneQ);
            PREPEND_ITEM(nreq, ardp_doneQ);
	    assert(!creq->rcvd);
	    ardp_rqfree(creq); ardp_ptfree(pkt);
            EXTERN_MUTEXED_UNLOCK(ardp_doneQ);
	    goto check_for_more;
	}

	/* While we're checking the done queue also see if any    */
	/* replies require follow up requests for acknowledgments */
        /* This is currently used when the server has requested that the client
           wait for a long time, and then has rescinded that wait request
           (because the message is done).  Such a rescinding of the wait
           request should be received by the client, and the server is
           expecting an ACK. */
        /* You can add additional tests here if there are other circumstances
           under which you want the server to expect the client to send an
           acknowledgement. */
	if(check_for_ack && (treq->svc_rwait_seq > treq->prcvd_thru) && 
	   (treq->retries_rem > 0) && time_is_later(now, treq->wait_till)) {
#define ARDP_LOG_SPONTANEOUS_RETRANSMISSIONS
#ifdef ARDP_LOG_SPONTANEOUS_RETRANSMISSIONS
	    ardp__log(L_QUEUE_INFO,treq,"Requested ack not received - pinging client (%d of %d/%d ack)",
		 treq->prcvd_thru, treq->svc_rwait_seq, treq->trns_tot, 0);
#endif /* ARDP_LOG_SPONTANEOUS_RETRANSMISSIONS */
	    /* Resend the final packet only - to wake the client  */
	    if(treq->trns) 
		ardp_snd_pkt(treq->trns->previous,treq);
	    ardp__adjust_backoff(&(treq->timeout_adj));
	    treq->wait_till = add_times(treq->timeout_adj, now);
	    treq->retries_rem--;
	}
    } /*for */
    EXTERN_MUTEXED_UNLOCK(ardp_doneQ);
    check_for_ack = 0; /* Only check once per call to ardp_accept */
    
    /* If unsequenced control packet free it and check for more */
    if(pkt->seq == 0) {
	ardp_rqfree(creq); ardp_ptfree(pkt);
	goto check_for_more;
    }

    assert(!creq->rcvd);
    /* Check if incomplete and match up with other incomplete requests */
    if(creq->rcvd_tot != 1) { /* Look for rest of request */
	for(treq = ardp_partialQ; treq; treq = treq->next) {
	    if((treq->cid != 0) && (treq->cid == creq->cid) &&
	       (memcmp((char *) &(treq->peer),
		     (char *) &from,sizeof(from))==0)) {
		/* We found the rest of the request     */
		/* coallesce and log and check_for more */
                PTEXT		tpkt; /* Temporary pkt pointer */

		ardp_update_cfields(treq,creq);
		if(creq->rcvd_tot) treq->rcvd_tot = creq->rcvd_tot;
		/* We now have no further use for CREQ, since we need no more
		   information from it. */
		ardp_rqfree(creq);
		creq = NULL;
		tpkt = treq->rcvd;
		while(tpkt) {
		    if(tpkt->seq == treq->rcvd_thru+1) treq->rcvd_thru++;
		    if(tpkt->seq == pkt->seq) {
			/* Duplicate - discard */
			ardp__log(L_NET_INFO,treq,"Multi-packet duplicate received (pkt %d of %d)",
			     pkt->seq, treq->rcvd_tot, 0);
			if(ack_bit_set && areq != treq) {
			    if(areq) {
				ardp_acknowledge(areq);
			    }
			    areq = treq;
			}
			ardp_ptfree(pkt);
			goto check_for_more;
		    }
		    if(tpkt->seq > pkt->seq) {
			/* Insert new packet in rcvd */
			INSERT_NEW_ITEM1_AFTER_ITEM2_IN_LIST(pkt, tpkt, treq->rcvd);
			if(pkt->seq == treq->rcvd_thru+1) 
			    treq->rcvd_thru++;

			while(tpkt && (tpkt->seq == treq->rcvd_thru+1)) {
			    treq->rcvd_thru++;
			    tpkt = tpkt->next;
			}
			pkt = NOPKT;
			break;
		    }
		    tpkt = tpkt->next;
		}
		if(pkt) { /* Append at end of rcvd */
		    APPEND_ITEM(pkt,treq->rcvd); 
		    if(pkt->seq == treq->rcvd_thru+1) 
			treq->rcvd_thru++;
		    pkt = NOPKT;
		}
		if(treq->rcvd_tot && (treq->rcvd_thru == treq->rcvd_tot)) {
		    if(areq == treq) areq = NOREQ;
		    creq = treq; EXTRACT_ITEM(creq, ardp_partialQ); --ptQlen;
		    ardp__log(L_NET_INFO,creq,"Multi-packet request complete",0);
		    goto req_complete;
		}
		else {
		    if(ack_bit_set && areq != treq) {
			if(areq) {
			    ardp_acknowledge(areq);
			}
			areq = treq;
		    }
		    ardp__log(L_NET_INFO, treq,
			 "Multi-packet request continued (rcvd %d of %d)",
			 treq->rcvd_thru, treq->rcvd_tot, 0);
		    goto check_for_more;
		}
		internal_error("Should never get here.");
	    }
	}
 	/* This is the first packet we received in the request */
	/* log it and queue it and check for more              */
	/* Acknowledge it if an ack requested (i.e., tiny windows). */
	if(ack_bit_set && areq != creq) {
	    if(areq) {
		ardp_acknowledge(areq);
	    }
	    areq = creq;
	    /* XXX note that this code will blow up if the incomplete request
	       pointed to by AREQ is dropped before the ACK is sent.  This
	       should be fixed, but won't be right now. */
	}
	ardp__log(L_NET_INFO,creq,"Multi-packet request received (pkt %d of %d)",
	     pkt->seq, creq->rcvd_tot, 0);
	APPEND_ITEM(pkt,creq->rcvd);
	APPEND_ITEM(creq,ardp_partialQ); /* Add at end of incomp queue   */
	if(++ptQlen > ptQmaxlen) {       
	    treq = ardp_partialQ;        /* Remove from head of queue    */
	    EXTRACT_ITEM(treq,ardp_partialQ); ptQlen--;
	    ardp__log(L_NET_ERR, treq,
		 "Too many incomplete requests - dropped (rthru %d of %d)",
		 treq->rcvd_thru, treq->rcvd_tot, 0);
	    ardp_rqfree(treq);    
	}
	goto check_for_more;
    }
    
    ardp__log(L_NET_INFO, creq, "Received v%d packet", 
             creq->peer_ardp_version);
    
 req_complete:

    /* A complete multi-packet request has been received or a single-packet
       request (always complete) has been received. */
    /* At this point, creq refers to an RREQ structure that has either just
       been removed from the ardp_partialQ or was never put on a queue. */


    qpos = 0; dpos = 1;

    /* Insert this message at end of pending but make sure the  */
    /* request is not already in pending                        */
    nreq = creq; 
    creq = NOREQ;		/* turn off the request. */
	
    if(pkt) {
	/* This code is exercised, in at least some cases, when we receive a
	   single-packet request.  This comment supersedes the comment below.
	   Ah, the joys of legacy code.  --swa, 8/97. */
	/* OLD COMMENT: swa wonders if this is ever exercised and in what
	   circumstances --3/97 */ 
	nreq->rcvd_tot = 1;
	APPEND_ITEM(pkt,nreq->rcvd);
	pkt = NOPKT;		/* Safeguard to double-check that we don't do
				   double-freeing. --swa, 3/20/97*/
    }
    
    /* nreq now refers to a freestanding RREQ structure (a message) that is
       complete but on no queue.  We search for a match. */
    /* First search for a match in the runQ (a match that is being run) */
    EXTERN_MUTEXED_LOCK(ardp_runQ);
    for (match_in_runQ = ardp_runQ; match_in_runQ; 
         match_in_runQ = match_in_runQ->next) {
        if((match_in_runQ->cid == nreq->cid) && (match_in_runQ->cid != 0) &&
           (memcmp((char *) &(match_in_runQ->peer),
                 (char *) &(nreq->peer),sizeof(nreq->peer))==0)) {
            /* Request is running right now */
            ardp_update_cfields(match_in_runQ,nreq);
            ardp__log(L_QUEUE_INFO,match_in_runQ,"Duplicate discarded (presently executing)",0);
            ardp_rqfree(nreq);
            nreq = match_in_runQ;
            break;              /* leave match_in_runQ set to nreq  */
        }
    }
    EXTERN_MUTEXED_UNLOCK(ardp_runQ);
    /* XXX this code could be simplified and improved -- STeve & Katia,
       9/26/96 */
    if (match_in_runQ) {
                                /* do nothing; handled above. */
    } else {
	EXTERN_MUTEXED_LOCK(ardp_pendingQ);
	if(ardp_pendingQ) {
	    int     keep_looking = 1; /* Keep looking for dup.  Initially say to
					 keep on looking. */


	    for(treq = ardp_pendingQ; treq; ) {
		/* The test of treq->cid against 0 can be removed when there are no
		   more old v0 clients out there. --swa, 9/96 */
		if((treq->cid != 0) && (treq->cid == nreq->cid) &&
		   (memcmp((char *) &(treq->peer),
			 (char *) &(nreq->peer),sizeof(treq->peer))==0)){
		    /* Request is already on queue */
		    ardp_update_cfields(treq,nreq);
		    ardp__log(L_QUEUE_INFO,treq,"Duplicate discarded (%d of %d)",dpos,pQlen,0);
		    ardp_rqfree(nreq);
		    nreq = treq;
#if 0
		    qpos = dpos; /* we superseded the duplicate.  Set qpos
				    too. */ 
		    /* XXX This code is disabled BECAUSE we don't unlock the
		       queue in this path.  
		       we could add an UNLOCK, but it is probably cleaner to
		       avoid the goto and save ourselves yet another path
		       through the code,  --swa, salehi, 5/98 */
		    goto update_queue_position_and_check_for_more;
#else
		    keep_looking = 0;  /* We found the duplicate */
		    break;
#endif
		}
		if((ardp_pri_override && ardp_pri_func && (ardp_pri_func(nreq,treq) < 0)) ||
		   (!ardp_pri_override && ((nreq->priority < treq->priority) ||
					   ((treq->priority == nreq->priority) && ardp_pri_func &&
					    (ardp_pri_func(nreq,treq) < 0))))) {
#if 1				/* NEW WAY */
		    INSERT_NEW_ITEM1_BEFORE_ITEM2_IN_LIST(nreq, treq, ardp_pendingQ);
#else  /* OLD WAY */
		    /* Steve suggests writing
		       INSERT_ITEM1_BEFORE_ITEM2_IN_LIST() and using it
		       here. */ 
		    if(ardp_pendingQ == treq) {
			PREPEND_ITEM(nreq, ardp_pendingQ);
		    }
		    else {
			INSERT_ITEM1_BEFORE_ITEM2(nreq, treq);
		    }
#endif
		    pQlen++;
		    if(nreq->priority > 0) pQNlen++;
		    LOG_PACKET(nreq, dpos);
		    qpos = dpos;
		    keep_looking = 1;  /* There may still be a duplicate */
		    break;
		}
		if(!treq->next) {
		    APPEND_ITEM(nreq, ardp_pendingQ);
		    pQlen++;
		    if(nreq->priority > 0) pQNlen++;
		    LOG_PACKET(nreq,dpos+1);
		    qpos = dpos+1;
		    keep_looking = 0; /* Nothing more to check */
		    break;
		}
		treq = treq->next;
		dpos++;
	    } /* for (; treq; ) */
	    /* Save queue position to send to client if appropriate */
	    qpos = dpos;
	    /* If not added at end of queue, check behind packet for dup */
	    if(keep_looking) {
		while(treq) {
		    if((treq->cid == nreq->cid) && (treq->cid != 0) && 
		       (memcmp((char *) &(treq->peer),
			     (char *) &(nreq->peer),
			     sizeof(treq->peer))==0)){
			/* Found a dup */
			ardp__log(L_QUEUE_INFO,treq,"Duplicate replaced (removed at %d)", dpos, 0);
			pQlen--;if(treq->priority > 0) pQNlen--;
			EXTRACT_ITEM(treq,ardp_pendingQ);
			ardp_rqfree(treq);
			break;
		    }
		    treq = treq->next;
		    dpos++;
		}
	    } /* if keep_looking */
	} else {
	    /* !pendingQ */
	    /* Make ardp_pendingQ a single-element list containing
	       nreq. */
	    nreq->next = NULL;
	    ardp_pendingQ = nreq;
	    ardp_pendingQ->previous = nreq;
	    pQlen++;if(nreq->priority > 0) pQNlen++;
	    LOG_PACKET(nreq, dpos);
	    qpos = dpos;
	}
	/* XXX NREQ is now on the ardp_pendingQ.  We still have the
	   ardp_pendingQ locked, however.  We need this locking because we
	   don't want NREQ to be taken off of the pendingQ as long we still
	   need to verify authentication, or other operation.

	   Keeping the pendingQ locked is not a very good idea, and should be
	   changed.  Will probably involve either adding a new intermediate
	   queue, or adding checks for processing state to the routines that
	   remove from the pendingQ.   (Thereby effectively multiplexing two
	   queues onto one :)) */
#ifndef ARDP_NO_SECURITY_CONTEXT
	/* We process the received security contexts (usually by verifying
	   them) as soon as we have a complete request on the pendingQ.  The
	   alternative (B) would be to do this before returning the request
	   from ardp_get_nxt(), or similar function.  The problem with (B) is
	   that Kerberos authenticators on the wire time out after 5 minutes by
	   default; in case of a queue, we would end up failing to
	   authenticate. */
	if (ardp__sec_server_process_received_security_contexts(nreq)) {
	    /* Throw away the message once we've said we failed. */
	    EXTRACT_ITEM(nreq, ardp_pendingQ);
	    EXTERN_MUTEXED_UNLOCK(ardp_pendingQ);
	    ardp_rqfree(nreq);
	    goto check_for_more;
	}
#endif /* ARDP_NO_SECURITY_CONTEXT */
	EXTERN_MUTEXED_UNLOCK(ardp_pendingQ);
    }    
    /* Here, NREQ may refer to:
       1) A new request; the RREQ just moved from the partialQ or nonexistence
       (in the case of a one-packet request) to the pendingQ
       2) A newly allocated RREQ which is a request that has already been
       received and is pending -- in this case, the former RREQ structure
       referring to the request was removed from the pendingQ and supplanted by
       this one.
       3) A request that was already in process.  NREQ is a pointer to an old
       member of the runQ. */ 
       
#if 0				/* IGNORE THIS COMMENT UNTIL WE REVIEW IT
				   3/20/97 XXX  */
    /* At this point, the security context for the newly received RREQ has
       not yet been processed, *unless* we superseded a previous older RREQ
       in the ardp_pendingQ.  In *that* case, the output from processing
       the older RREQ's security context will be discarded along with the older
       RREQ.  
       Therefore, we will see 'replay detected; message refused' problems under
       conditions of high load, when the pendingQ may be several or more items
       long. --swa, 3/20/97 XXX */

#endif

    /* This is an old version of the Prospero clients that only archie servers
       will need to be able to talk to; we include this test because the archie
       servers use the ARDP library.  No need to remove it; shouldn't hurt
       anything else.  --swa, 4/25/95 */
    /* Updated.  This is the version that we call 'old-v0'.   IT has a version
       # of -2.  It predates the version we call 'pre-ARDP', which has a
       version number of -1.  Fun, eh? --swa & katia, 4/29/96 */
    if((nreq->peer_ardp_version == 0) && (strstr(nreq->rcvd->start,"VERSION 1")))
      nreq->peer_ardp_version = -2;

    /* Fill in queue position and system time             */
    nreq->inf_queue_pos = qpos;

    /* Here we can perform additional processing on a newly-received ARDP
       request.  (This includes additional processing on a newly-received
       request that was a duplicate of a previous request.)
       At this point, 'nreq' is a variable referring to the request that we've
       just received.  The Prospero directory service takes advantage of this
       to request an additional wait for heavily-loaded databases. */
    if (ardp_newly_received_additional)
        (*ardp_newly_received_additional)(nreq);
    /* Each time we receive a  completed request, 
       including a duplicate of a request already on one of the queues, we tell
       the client about our backoff.  This is how the archie servers have
       historically done it. */ 
    if (ardp_config.wait_time)
       ardp_rwait(nreq,ardp_config.wait_time,
                  nreq->inf_queue_pos,nreq->inf_sys_time); 

    goto check_for_more;
}



static 
void
ardp_update_cfields(RREQ existing,RREQ newvalues)
{
    if(newvalues->prcvd_thru > existing->prcvd_thru)
	existing->prcvd_thru = newvalues->prcvd_thru;
}

/* Rewrite the header of TPKT to conform to the status in nreq.  The only
   fields 
   we need to set are possibly the ACK bit in octet 11 and possibly the rwait
   field in octets 9 & 10.  We always send the received-through field for the
   sake of simplicity of implementation.  */
/* Katia and I wonder if this is needed.
   headers will be set by ardp_headers()
   if client waiting for an rwait  */

static void ardp_header_ack_rwait(PTEXT tpkt, RREQ nreq, int is_ack_needed, 
                                  int is_rwait_needed)
{
    int new_hlength;            /* new header length. */
    int old_hlength;            /* old header length */
    u_int16_t stmp;      /* tmp for values. */
    old_hlength = tpkt->text - tpkt->start;

    if (nreq->peer_ardp_version <= 0) {
       /* If rwait needed, leave room for it. */
       new_hlength = 9;            /* Include room for received-through count
				    */  
       if (is_rwait_needed) 
           new_hlength = 11;
       if (is_ack_needed) new_hlength = 12;

       /* Allocate space for new header. */
       tpkt->start = tpkt->text - new_hlength;
       tpkt->length += new_hlength - old_hlength;
       /* Set the header length and version # (zeroth octet) */
       /* Version # is zero in this version of the ARDP library; last 6 bytes
          of octet are header length. */
       *(tpkt->start) = (char) new_hlength;
       /* Set octets 1 through 8 of header */
       /* Connection ID (octets 1 & 2) */
       memcpy2(tpkt->start+1, &(nreq->cid));
       /* Sequence number (octets 3 & 4) */
       stmp = htons(tpkt->seq);
       memcpy2(tpkt->start+3, &stmp);    
       /* Total packet count (octets 5 & 6) */
       stmp = htons(nreq->trns_tot);
       memcpy2(tpkt->start+5, &stmp);    
       /* Received through (octets 7 & 8) */
       stmp = htons(nreq->rcvd_thru);
       memcpy2(tpkt->start+7, &stmp);    
       /* Now set any options. */
       /* expected time 'till response */
       if (new_hlength > 9) {
           stmp = htons(nreq->svc_rwait);
           memcpy2(tpkt->start+9, &stmp);
       }
       if (new_hlength > 11) {
           tpkt->start[11] = 
               is_ack_needed ? 0x80 /* ACK BIT */ : 0x00 /* Don't ack */;
       }
    } else {
       /* add total # of packets to last packet (save space).  */      
       int stamp_message_npkts = (tpkt->seq == nreq->trns_tot);
       char *ctlptr = tpkt->start + 11;

       assert(nreq->peer_ardp_version == 1);
       if (is_rwait_needed)
           new_hlength = 13;   /* RWAIT flag takes a two-byte argument. */
       else
           new_hlength = 11;   /* include header fields through
                                  received-through */ 
       if (stamp_message_npkts)
           new_hlength += 2;
       /* Allocate space for new header. */
       tpkt->start = tpkt->text - new_hlength;
       tpkt->length += new_hlength - old_hlength;
       
       tpkt->start[0] = (char) 129; /* version # */
       tpkt->start[1] = 0;     /* no contexts here (yet) */
       tpkt->start[2] = 0;     /* flags */
       if (is_ack_needed)
           tpkt->start[2] |= 0x01;     /* flags: please-ack bit on. */
       if (stamp_message_npkts)
           tpkt->start[2] |= 0x04; /* 2 octet arg too. */
       if (is_rwait_needed) 
           tpkt->start[2] |= 0100; /* bit 6: clear wait (2 octet arg. too) */
               tpkt->start[3] = 0;     /* no options */
       memcpy2(tpkt->start + 5, &nreq->cid);             /* Connection ID     */
       stmp = htons(tpkt->seq);
       memcpy2(tpkt->start + 7, &stmp);         /* Packet sequence number */
       stmp = htons(nreq->rcvd_thru);
       memcpy2(tpkt->start + 9, &stmp); /* Received Through   */
       /* Possible Optimization: We might be able to leave out the
          received-through under some circumstances, and thereby save
          ourselves 2 bytes.  */
       /* Write options */
       ctlptr = tpkt->start + 11;
       if (stamp_message_npkts) {
           stmp = nreq->trns_tot;
           memcpy2(ctlptr, &stmp);
           ctlptr += 2;
       }
       if (is_rwait_needed) {
           stmp = htons(nreq->svc_rwait);
           memcpy2(ctlptr, &stmp);
           ctlptr += 2;
       }

    }
}

/* XXX DOES NOT DO ANY RETRANSMIT QUEUE TIMEOUTS --swa, katia, 2/97 */
enum ardp_errcode
ardp_accept() 
{
    return ardp_accept_and_wait(0,0);
}
