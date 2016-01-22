/*
 * Copyright (c) 1993 by the University of Southern California
 *
 * For copying and distribution information, please see the file
 * <usc-license.h>.
 *
 * Written  by bcn 1/93     to send a list of packets to a destination
 */

#include <usc-license.h>

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>		/* inet_addr() prototype */
#include <ardp.h>

extern int	       ardp_debug;            /* Debug level                */
extern int	       ardp_port;	     /* Opened UDP port		   */

/*
 * ardp_xmit - transmits packets in req->trns
 *
 *   ardp_xmit takes a pointer to a request structure of type RREQ and a
 *   window size.  It then sends the packet on the request structure
 *   transmit queue to the peer address in the request structure starting after
 *   the indicated peer rcvd_thru value, and up to the number of packets 
 *   specified by the window size.  It returns ARDP_SUCCESS on success and
 *   returns an error code on failure.
 *
 *   This is called on both the client and server side, called respectively by
 *   ardp_send() and ardp_pr_actv(). 
 */

enum ardp_errcode
ardp_xmit(RREQ	req,            /* Request structure with packets to send   */
	  int	window)         /* Max packets to send at once.  Note that
                                   right now this is always identical to
                                   req->pwindow_sz.  
                                   0 means infinite window size
                                   -1 means just send an ACK; don't send any
                                      data packets. */    
{
    PTEXT 	 	ptmp;	/* Packet pointer for stepping through trns */
    u_int16_t      	stmp;	/* Temp short for conversions    */
    int		 	ns;	/* Number of bytes actually sent            */
    PTEXT ack = NOPKT;	/* Only an ack to be sent                   */

    if(window < 0 || req->prcvd_thru >= req->trns_tot) { 
	/* All our packets got through, send acks only */
        if (ardp_debug && req->prcvd_thru > req->trns_thru)
            rfprintf(stderr, "Our peer appears to be confused: it thinks we \
sent it %d packets; we only have sent through %d.  Proceeding as well as we \
can.\n", req->prcvd_thru, req->trns_tot);
	ack = ardp_ptalloc();
       if (req->peer_ardp_version == 1) {
           /* add V1 header */
           ack->start -= 11;
           ack->length += 11;
           ack->start[0] = 129; /* version # */
           ack->start[1] = '\0'; /* context bits; protected bits --
                                    unused  */
           ack->start[2] = ack->start[3] = '\0'; /* flags & options; all
                                                    unset */ 
           ack->start[4] = 11; /* header length */
           /* Connection ID */
           memcpy2(ack->start + 5, &(req->cid));
           ack->start[7] = ack->start[8] = 0; /* Sequence # is 0 (unsequenced
						 control pkt) */ 
           /* Received Through */
           stmp = htons(req->rcvd_thru);
           memcpy2(ack->start+9 , &stmp);
       } else {
           /* V0 */

	   /* Add header */

	   ack->start -= 9;
	   ack->length += 9;
	    *(ack->start) = (char) 9;
           /* Connection ID */
           memcpy2(ack->start + 1, &(req->cid));
	   /* An unsequenced control packet */
	   bzero4(ack->start+3);
	   /* Received Through */
	   stmp = htons(req->rcvd_thru);
	   memcpy2(ack->start+7, &stmp);
       }
       ptmp = ack;
    } else {		/* don't send acks yet; more to deliver */
	ptmp = req->trns;
    }

    /* Note that we don't want to get rid of packts before the */
    /* peer received through since the peer might later tell   */
    /* us it forgot them and ask us to send them again         */
    /* XXX whether this is allowable should be an application  */
    /* specific configration option.                           */
    while(ptmp) {
	if((window > 0) && (ptmp->seq > req->prcvd_thru + window)) break;
	if((ptmp->seq == 0) || (ptmp->seq > req->prcvd_thru)) {
	    if (ardp_debug >= 6) {
		rfprintf(stderr,
			"Sending message%s (cid=%d) (seq=%d) (v%d)",
			(ptmp == ack) ? " (ACK only)" : "", 
			ntohs(req->cid), ntohs(ptmp->seq),
			req->peer_ardp_version);
		if (req->peer.sin_family == AF_INET) {
		    fputs(" to", stderr);
		    if (req->peer_hostname && *req->peer_hostname)
			rfprintf(stderr, " %s", req->peer_hostname);
		    rfprintf(stderr, " [%s(%d)]",
			    inet_ntoa(req->peer_addr), 
			    /* Note that the PEER_PORT() macro does ntohs() 
			       for us. */
			    PEER_PORT(req));
		} 
		fputs("...", stderr);
		(void) fflush(stderr);
	    }
	    ns = sendto(ardp_port,(char *)(ptmp->start), ptmp->length, 0, 
			(struct sockaddr *) &(req->peer), S_AD_SZ);
	    if (ardp_debug >= 9) {
		/* Display header octets */
		int i;
		int hdr_len;
		if (req->peer_ardp_version == 1)
		    hdr_len = ((unsigned char *) ptmp->start)[4];
		else if (req->peer_ardp_version == 0)
		    hdr_len = ((unsigned char *) ptmp->start)[0];
		else {
		    hdr_len = 0;
		    rfprintf(stderr, "ardp_xmit(): \
Unknown peer ARDP version: %d; can't display header\n", req->peer_ardp_version);
		}
		if (hdr_len) {
                   rfprintf(stderr, "Pkt %d header:", ptmp->seq);
		   for (i = 0; i < hdr_len; ++i)
		       rfprintf(stderr, " b%d:0x%x,0%o,%d", i,
			       ((unsigned char *) ptmp->start)[i],
			       ((unsigned char *) ptmp->start)[i],
			       ((unsigned char *) ptmp->start)[i]);
		   fputc('\n', stderr);
		}
	    }
	    if(ns != ptmp->length) {
		if (ardp_debug >= 6)
		    fputs("Failed.\n", stderr);
		if (ardp_debug >= 1) {
		    rfprintf(stderr,"ardp: error in sendto(): sent sent only"
			    " %d bytes of the %d byte message: %s\n",
			    ns, ptmp->length, unixerrstr());
		}
		if (ack)
		    ardp_ptfree(ack);
		return(ARDP_NOT_SENT);
	    }
	    if (ardp_debug >= 9)
		fputs("...", stderr);
	    if (ardp_debug >= 6) rfprintf(stderr,"Sent.\n");
            if (req->trns_thru < ptmp->seq)
                req->trns_thru = ptmp->seq;
	}
	ptmp = ptmp->next;
    }
    if (ack)
       ardp_ptfree(ack);
    return(ARDP_SUCCESS);
}


