/* lib/ardp/ardp_rreq_to_in.c */
/*
 * Copyright (c) 1992, 1993, 1995 by the University of Southern California
 *
 * For copying and distribution information, please see the file
 * <usc-license.h> 
 */

#include <usc-license.h>

#include <ctype.h>

#include <ardp.h>		/* for prototype of ardp_rreq_to_in(). */
#include <gl_parse.h>		/* def. of INPUT, etc. */

/* History: 
   9/95: swa@ISI.EDU: (1) Split off from lib/pfs/in_line.c.  Also changed
   interface & function name.   Also changed workings so that gl_parse routines
   (in gostlib) don't need to know anything about RREQ structures (part of
   lib/ardp).  Did this by adding third argument.    

   Incorporated fragments of old in_incc() and in_readc() */


static int ardp__rreq_readc(constINPUT in); 
static void ardp__rreq_incc(INPUT in);


void
ardp_rreq_to_in(RREQ rreq, INPUT in, ardp_rreq_to_in_aux_INPUT in_aux)
{
    in->sourcetype = GL__IO_CALLERDEF;
    
    in->flags = 0;
    in->u.c.readc = ardp__rreq_readc;
    in->u.c.incc = ardp__rreq_incc;
    in->u.c.dat = in_aux;
    in->u.c.datsiz = sizeof *in_aux;
    in->offset = 0;		/* on byte 0.  Needed by in_line. */
    if((in_aux->rreq = rreq)) {       /* might be NULL.  */
        in_aux->inpkt = rreq->rcvd;
        in_aux->ptext_ioptr = rreq->rcvd->text;
        /* Do a loop because there might be a crazy client that sends some
           packets in a sequence with empty length fields.  Skip any of them we
           encounter; go to the next packet with some content. */
        while (in_aux->ptext_ioptr >= in_aux->inpkt->text + in_aux->inpkt->length) {
            in_aux->inpkt = in_aux->inpkt->next;
            if (in_aux->inpkt == NULL)
                break;
            in_aux->ptext_ioptr = in_aux->inpkt->text;
        }
    } else {
        in_aux->inpkt = NULL;
        in_aux->ptext_ioptr = NULL;
    }
}


static int 
ardp__rreq_readc(constINPUT in)
{
    ardp_rreq_to_in_aux_INPUT in_aux = in->u.c.dat;

    assert(in->sourcetype == GL__IO_CALLERDEF);
    return  (in_aux->inpkt) 
            ? (unsigned char) *in_aux->ptext_ioptr : EOF;

}


static void 
ardp__rreq_incc(INPUT in)
{
    ardp_rreq_to_in_aux_INPUT in_aux = in->u.c.dat;

    assert(in->sourcetype == GL__IO_CALLERDEF);

    if (in_aux->inpkt) {
	++in_aux->ptext_ioptr;
	++in->offset;		/* needed for GL__IO_CALLERDEF */
	/* Do a loop because there might be a crazy client that
	   sends some packets in a sequence with empty length fields. 
	   Skip any of them we encounter; go to the next packet with some
	   content. */
	while (in_aux->ptext_ioptr 
	       >= in_aux->inpkt->start + in_aux->inpkt->length) {
	    in_aux->inpkt = in_aux->inpkt->next;
	    if (in_aux->inpkt == NULL)
		break;
	    in_aux->ptext_ioptr = in_aux->inpkt->text;
	}
    }
}
