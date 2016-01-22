#include <ardp.h> 
#include "sample.h"
#include <stdio.h>

/*
 * get_data retrieves the data from the request and displays it on stdout.
 */


void
get_data(RREQ req, FILE *out)
{  
    struct ptext * pptr; 
    int data_length = 0;	/* length of the payload. */
    int total_data = 0;
  

    fputs("Retrieving data from received ARDP message:\n", out);
    for (pptr = req->rcvd; pptr; pptr = pptr->next) {
	data_length = pptr->length - (pptr->text - pptr->start);
	total_data += data_length;
	/* Arguments to fwrite(): pointer, size, nitems, stream */
	fwrite(pptr->text, sizeof (char), data_length, out);
    }
    rfprintf(out, "The message contained %d bytes of data (payload).\n", total_data);
}
