/*
 * Copyright (c) 1993 by the University of Southern California
 *
 * For copying and distribution information, please see the file
 * <usc-license.h>.
 *
 * Written  by bcn 1/93     For use in adrp_send
 */

#include <usc-license.h>

#include <stdio.h>

#include <ardp.h>

/* Horrible bugs with MEMCPY2().  These are experiments you can use if you read
   the comment in ardp.h --swa, 7/9/97 */
#undef memcpy1
#undef memcpy2
#if 0
#define memcpy2(a, b) memcpy((a), (b), 2)
#endif
#define memcpy1(b, a) do {                       \
    ((char *) (b))[0] = ((char *) (a))[0];      \
} while(0)                              

#define memcpy2(b, a) do {                        \
    memcpy1(b, a);                                 \
    /* Next line depends on unary cast having higher precedence than \
       addition.   (Guaranteed to be true.) */ \
    memcpy1((char *) (b) + 1, (char *) (a) + 1);   \
} while(0)


/*
 * This file contains functions that rewrite the ARDP header. 
 * We are encapsulating these so that we can support v0 and v1 in the same
 * code.  --katia & swa
 */

void
ardp__bwrite_cid(u_int16_t newcid, PTEXT ptmp)
{
    if (*(ptmp->start) == (char) 129)  /* v1 packet? */
       memcpy2(ptmp->start+5, &newcid);
    else                       /* v0 (old or new) */
       memcpy2(ptmp->start+1, &newcid);
}


/*
 * ardp_headers - Add headers to packets to be sent to server
 *
 *   ardp_headers takes pointer to a request structure and adds headers to 
 *   all the packets in the trns list.  The fields for the headers are
 *   taken from other fields in the request structure.  If the priority is
 *   zero, it is taken from the global vairable ardp_priority.
 *
 *   If ardp_headers is called more than once, subsequent calls will
 *   update the headers to conform to changes in the request fields.
 */
enum ardp_errcode
ardp_headers(RREQ req)
{
    PTEXT	ptmp;		/* Temporary packet pointer	      */

    for(ptmp = req->trns; ptmp; ptmp = ptmp->next) {
        int		old_hlength;    /* Old header length */
        int             new_hlength;    /* New header length, whatever it may
                                           be.  */
        u_int16_t ftmp;	/* Temporary for values of fields     */
        /* Boolean: do we stamp this packet with the window size? */
        /* We might add an explicit window-size stamp to the first packet
	   sent.   The first packet is the one stamped
           with sequence number 1.  Thus we know that the window size message
           won't be lost. */
        int  stamp_window_size = (req->flags & ARDP_FLAG_SEND_MY_WINDOW_SIZE) 
            && (ptmp->seq == 1);
        int stamp_priority = (req->priority || ardp_priority) 
            && (ptmp->seq == 1);
        int request_queue_status = 
            ardp_config.client_request_queue_status && (ptmp->seq == 1);
        /* Do we stamp this message with the # of packets in the outgoing
           message?  In v1, we do this on the first packet, from the client. */
        
        int stamp_message_npkts = (ptmp->seq == 1);

	/* if it is the last packet, set ACK bit.  */
	/* If it is the last packet of the next window to be sent, set ACK. */
	/* Otherwise unset ACK bit. */
	int set_ack_bit = !ptmp->next 
	    || (req->pwindow_sz 
		&& (ptmp->seq == req->pwindow_sz + req->prcvd_thru));

	old_hlength = ptmp->text - ptmp->start;

	/* XXX Should do further tests to make sure all packets present */
	if(ptmp->seq == 0) {
	    if (ardp_debug >= 1)
		rfprintf(stderr, "ardp: sequence number not set in packet\n");
	    fflush(stderr);
	    return(ARDP_BAD_REQ);
	}
	if (req->peer_ardp_version == 1) {
	    /* ARDP V1: 
	       We will always have at least 11 bytes of header, unless we 
	       are certain that we do not need to send the rcvd_thru.  There
	       are more elaborate tests we could perform; right now, we do the
	       simple test of whether anything has been received from the
	       peer. */
	    if (request_queue_status || stamp_message_npkts || 
		stamp_priority || stamp_window_size || req->rcvd_thru)
		new_hlength = 11;
	    else
		new_hlength = 9;
	    if (stamp_priority)
		new_hlength += 2;
	    if (stamp_window_size)
		new_hlength += 2;
	    if (stamp_message_npkts)
		new_hlength += 2;
	} else {
	    /* ARDP v0: If priority stamp or explicit client window size, need
	       octets 11 and 12 to be present. */
	   

	    /* If priority stamp or explicit client window size, need octets 11 and
	       12 to be present. */
	    /* If we need to turn on the ACK bit for this packet, need octet 11 to
	       be present.  Throw in octet12 for simplicity of implementation. */
	    if (stamp_priority || stamp_window_size || set_ack_bit) {
		new_hlength = 13;   /* room for octets through 12 */
		/* Note that this code (rewriting header length) will trash any
		   additional address information or higher-level protocol ID if
		   specified.  If we ever use those options, we'll need to make
		   this code recognize those. */
		if(stamp_priority)
		    new_hlength += 2; /* 2 octets for priority argument.  */
		if (stamp_window_size)
		    new_hlength += 2; /* 2 octets for window size */
	    } else {
		new_hlength = 9;    /* room for octets through 8 (received-through)
				     */ 
	    }
	}
	/* New header length now set. */
       
	/* Allocate space for the new header */
	ptmp->start = ptmp->text - new_hlength;
	ptmp->length += new_hlength - old_hlength;

	/* Fill out the header */
	if (req->peer_ardp_version == 1) {
	    /* ARDP V1*/

	    ptmp->start[0] = (char) 129;
	    ptmp->start[1] = ptmp->context_flags; /* fix this when doing security context */
	    ptmp->start[2] = 0; /* flags1 */
	    if (set_ack_bit) 
		ptmp->start[2] |= 0x01;
	    if (stamp_message_npkts) 
		ptmp->start[2] |= 0x04;
	    if (stamp_priority)
		ptmp->start[2] |= 0x08;
	    if (stamp_window_size)
		ptmp->start[2] |= 0x20;
	    /* Octet 3: one option */
	    if (request_queue_status)
		ptmp->start[3] = (unsigned char) 253; /* no arguments */
	    else
		ptmp->start[3] = 0;
	    ptmp->start[4] = (char) new_hlength;
	    /* Connection ID (octets 5 & 6) */
	    memcpy2(ptmp->start+5, &(req->cid));
	    /* Sequence number (octets 7 & 8) */
	    ftmp = htons(ptmp->seq);
	    memcpy2(ptmp->start+7, &ftmp);        
	    if (new_hlength > 9) {
		char *optiondata = ptmp->start + 11; /* where options go */
		/* Received through (octets 9 & 10) */
		ftmp = htons(req->rcvd_thru);
		memcpy2(ptmp->start + 9, &ftmp);  
		if (stamp_message_npkts) {
		    ftmp = htons(req->trns_tot);
		    memcpy2(optiondata, &ftmp);
		    optiondata += 2;
		}
		if (stamp_priority) {
		    if(req->priority) ftmp = htons(req->priority);
		    else ftmp = htons(ardp_priority);
		    memcpy2(optiondata, &ftmp);
		    optiondata += 2;
		} 
		if(stamp_window_size) {
		    ftmp = htons(req->window_sz);
		    memcpy2(optiondata, &ftmp);
		    optiondata += 2;
		}
		assert(optiondata == ptmp->start + new_hlength);
	    }
	} else { /* ARDP V0 */
	    /* Set the header length and version # (zeroth octet) */
	    /* Version # is zero in this version of the ARDP library; last 6 bits
	       of octet are header length. */
	    *(ptmp->start) = (char) new_hlength;
	    /* Set octets 1 through 8 of header */
	    /* Connection ID (octets 1 & 2) */
	    memcpy2(ptmp->start+1, &(req->cid));
	    /* Sequence number (octets 3 & 4) */
	    ftmp = htons(ptmp->seq);
	    memcpy2(ptmp->start+3, &ftmp);	
	    /* Total packet count (octets 5 & 6) */
	    ftmp = htons(req->trns_tot);
	    memcpy2(ptmp->start+5, &ftmp);	
	    /* Received through (octets 7 & 8) */
	    ftmp = htons(req->rcvd_thru);
	    memcpy2(ptmp->start+7, &ftmp);	
	    /* Now set any options. */
	    if (new_hlength > 9) {
		char     *       optiondata; /* where options go. */
		/* zero out octets 9 and 10 (expected time 'till response).  It is
		   not defined for the client to specify this to the server,so
		   it is always zero. */
		bzero2(ptmp->start + 9);
		/* set octet 11 (flags) initially clear */
		ptmp->start[11] = 0;
		/* Here Octet 12 (option) is zero (no special options). */
		ptmp->start[12] = 0;
		optiondata = ptmp->start + 13; /* additional octets start here */
		if (set_ack_bit)
		    ptmp->start[11] |= 0x80;
		/* Priority */
		if(stamp_priority) {
		    *(ptmp->start+11) |= 0x02; /* priority flag */
		    if(req->priority) ftmp = htons(req->priority);
		    else ftmp = htons(ardp_priority);
		    memcpy2(optiondata, &ftmp);
		    optiondata += 2;
		}
		if(stamp_window_size) {
		    *(ptmp->start+11) |= 0x08; /* Set window size flag */
		    ftmp = htons(req->window_sz);
		    memcpy2(optiondata, &ftmp);
		    optiondata += 2;
		}
		assert(optiondata == ptmp->start + new_hlength);
	    }
	}
    } /* for */

    return(ARDP_SUCCESS);
}
