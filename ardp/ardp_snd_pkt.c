/*
 * Copyright (c) 1993 by the University of Southern California
 *
 * For copying and distribution information, please see the file
 * <usc-license.h>.
 *
 * Written  by bcn 1/93     to send a single packet to a peer
 */

#include <usc-license.h>

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>		/* sendto() prototype */
#include <ardp.h>
#include <gl_log_msgtypes.h>
#include <errno.h>
#include <perrno.h>

/*
 * ardp_snd_pkt - transmits a single packet to address in req
 *
 *   ardp_snd_pkt takes a pointer to a packet of type PTEXT to be
 *   sent to a peer identified by req->peer.  It then send the packet to 
 *   the peer.  If the packet was sent successfully, ARDP_SUCCESS is
 *   returned.  Successful transmission of the packet does not provide
 *   any assurance of receipt by the peer.  If the attempt to send 
 *   the packet fails, ARDP_NOT_SENT is returned.
 */
enum ardp_errcode
ardp_snd_pkt(pkt,req)
    PTEXT		pkt; 
    RREQ		req;
{
    int	sent;


    ardp__log(L_NET_INFO,req,"Sending pkt %d.", pkt->seq);
    if (ardp_debug >= 10) {
	rfprintf(stderr, "Contents of pkt %d:", pkt->seq);
	ardp_showbuf(pkt->start, pkt->length, stderr);
	putc('\n', stderr);
    }
    sent = sendto(((ardp_prvport != -1) ? ardp_prvport : ardp_srvport), 
		  pkt->start, pkt->length, 0, 
		  (struct sockaddr *) &(req->peer), S_AD_SZ);
	    
    if(sent == pkt->length) return(ARDP_SUCCESS);
    
    ardp__log(L_NET_ERR, req, "Attempt to send message failed (errno %d %s);"
	      " sent %d bytes of %d", errno, unixerrstr(), sent, pkt->length);
	
    return(ARDP_NOT_SENT);
}
