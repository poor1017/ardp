/*
 * This is a sample server program using the ARDP library.
 * 
 * This server receives and replies to requests.
 *
 * This server runs forever; have to kill it.
 */

/*  Last Change: g.chugh (9/30/97) handle security contexts */


#include <stdio.h>
#include <pconfig.h>		/* Prospero command line parsing routines */
#include <ardp.h> 
#include <pmachine.h>
#include "sample.h"

#ifdef DEMONSTRATE_SECURITY_CONTEXT
#include <ardp_sec.h>
#include <ardp_sec_members.h>

static void display_security_info(RREQ req, FILE *output);

#endif

int
main(int argc, char **argv)
{
    int p; 
    RREQ req; 
    char port[80];      
    int retval;
    char BUFFER[256];
    int i = 0;	
    int length = 0; 	

    puts("ARDP sample server starting up.  Don't forget to manually kill\n\
this when you're done, or it will go on running forever.");
    /* This parses the standard Prospero and GOST and ARDP arguments.
       This handles the -pc configuration arguments and the -D debugging
       flag.   Try -D9 for lots of detail about ARDP. */
    p_command_line_preparse(&argc, argv);
    ardp_initialize();

    sprintf(port,"#%d", SAMPLE_DEFAULT_SERVER_PORT);

    /* Server listens on "port". */
    p = ardp_bind_port(port);
    printf("Sample ARDP Server: listening on port %d\n", 
	   p);   

    for(i = 1; ; ++i) 
    {
	/* Get the next request for the server.   ardp_get_nxt() performs a
	   BLOCKING wait. */
        req = ardp_get_nxt();

        printf("ARDP sample server: Got a request:\n"); 
        get_data(req,stdout);  

#ifdef DEMONSTRATE_SECURITY_CONTEXT
	display_security_info(req, stdout);
#endif
	sprintf(BUFFER, "This is the sample ARDP server sending reply # %d.\n",
		i);
	length = strlen(BUFFER); 

	/* ardp_breply() will actually send the reply. */
        if ((retval= ardp_breply(req, ARDP_R_COMPLETE, BUFFER, length))) {
		rfprintf(stderr,"ardp_breply() failed witherror number %d \n",
			retval);
		exit(1);
        }
    }
}
	
#ifdef DEMONSTRATE_SECURITY_CONTEXT
/* The rest of this file is security-context specific */
static
void
display_security_info(RREQ req, FILE *output)
{
    ardp_sectype *sp;
    int found_kerberos_info = 0;
    int found_asrthost_info = 0;
    int found_kerberos_integrity = 0;
    int found_kerberos_privacy = 0;
    int found_crc_info = 0;
#ifdef ARDP_SEC_HAVE_LABELS_CLASS_OF_SERVICE
    /* Keep this typedef local, so we don't export it. */
    typedef struct ardp_class_of_service_tag *costag;
    costag ctg;
#endif

#ifdef ARDP_SEC_HAVE_LABELS_CLASS_OF_SERVICE
    
    rfprintf(output, "Labels for class of service:\n");
    if (!req->class_of_service_tags)
	rfprintf(output, "  No class of service labels for this request\n");
    for(ctg = req->class_of_service_tags; ctg; ctg = ctg->next) {
	rfprintf(output, "  Tag %s has value %s\n", ctg->tagname, ctg->value);
    }
#endif /* ARDP_SEC_HAVE_LABELS_CLASS_OF_SERVICE */

#ifdef ARDP_SEC_HAVE_AUTHENTICATION_ASRTHOST
    /* Just automatically say who we authenticated as for now. */
    fputs("Asrthost Authentication:\n", output);
    for (sp = req->secq; sp; sp = sp->next) {
	if (sp->service == ARDP_SEC_AUTHENTICATION &&
	    sp->mechanism == ARDP_SEC_AUTHENTICATION_ASRTHOST &&
	    (sp->processing_state == ARDP__SEC_PROCESSED_SUCCESS
	     /* Can't reach PREP_SUCCESS on the server unless the client's
		message was successfully processed, so this is OK.
		Obviously, we need a better interface. --swa, tryutov, 9/97
		*/
	     || sp->processing_state == ARDP__SEC_PREP_SUCCESS)) {
	    ++found_asrthost_info;
	    rfprintf(output, "  Message sender authenticated as the Asrthost"
		    " principal %s\n",
		    (char *) sp->mech_spec[AA_PEER_USERNAME].ptr);
	}
    }
    if (!found_asrthost_info)
	fputs("  none found\n", output);
#endif
#ifdef ARDP_SEC_HAVE_AUTHENTICATION_KERBEROS
    /* Just automatically say who we authenticated as for now. */
    fputs("Kerberos Authentication:\n", output);
    for (sp = req->secq; sp; sp = sp->next) {
	if (sp->service == ARDP_SEC_AUTHENTICATION &&
	    sp->mechanism == ARDP_SEC_AUTHENTICATION_KERBEROS &&
	    (sp->processing_state == ARDP__SEC_PROCESSED_SUCCESS
	     /* Can't reach PREP_SUCCESS on the server unless the client's
		message was successfully processed, so this is OK.
		Obviously, we need a better interface. --swa, tryutov, 9/97
		*/
	     || sp->processing_state == ARDP__SEC_PREP_SUCCESS)) {
	    ++found_kerberos_info;
	    rfprintf(output, "  Message sender authenticated as the Kerberos"
		    " principal %s\n",
		    (char *) sp->mech_spec[KA_CLIENTNAME].ptr);
	}
    }
    if (!found_kerberos_info)
	fputs("  none found\n", output);
#endif

#ifdef ARDP_SEC_HAVE_INTEGRITY_KERBEROS
    fputs("Kerberos Integrity:\n", output);
    for (sp = req->secq; sp; sp = sp->next) {
	if (sp->service == ARDP_SEC_INTEGRITY &&
	    sp->mechanism == ARDP_SEC_INTEGRITY_KERBEROS &&
	    (sp->processing_state == ARDP__SEC_PROCESSED_SUCCESS
	     /* Can't reach PREP_SUCCESS on the server unless the client's
		message was successfully processed, so this is OK.
		Obviously, we need a better interface that leaves
		some indicator of PROCESSED_SUCCESS, even if
		sending/preparation fails. --swa, tryutov, 9/97
		*/
	     || sp->processing_state == ARDP__SEC_PREP_SUCCESS)) {
	    ++found_kerberos_integrity;
	    rfprintf(output, "  Successfully authenticated the Kerberos \
principal %s as the sender of the message we are about to process.\n",
		    (char *) ((ardp_sectype *) sp->mech_spec[IK_AUTHREF].ptr)->
						mech_spec[KA_CLIENTNAME].ptr);
	}
    }
    if (!found_kerberos_integrity)
	fputs("  none found\n", output);
    
#endif

#ifdef ARDP_SEC_HAVE_PRIVACY_KERBEROS
    fputs("Kerberos Privacy:\n", output);
    for (sp = req->secq; sp; sp = sp->next) {
	if (sp->service == ARDP_SEC_PRIVACY &&
	    sp->mechanism == ARDP_SEC_PRIVACY_KERBEROS &&
	    (sp->processing_state == ARDP__SEC_PROCESSED_SUCCESS
	     /* Can't reach PREP_SUCCESS on the server unless the client's
		message was successfully processed, so this is OK.
		Obviously, we need a better interface that leaves
		some indicator of PROCESSED_SUCCESS, even if
		sending/preparation fails. --swa, tryutov, 9/97
		*/
	     || sp->processing_state == ARDP__SEC_PREP_SUCCESS)) {
	    ++found_kerberos_privacy;
	    rfprintf(output, "  Successfully authenticated the Kerberos \
principal %s as the sender of the message we are about to process.\n",
		    (char *) ((ardp_sectype *) sp->mech_spec[PK_AUTHREF].ptr)->
						mech_spec[KA_CLIENTNAME].ptr);
	}
    }
    if (!found_kerberos_privacy)
	fputs("  none found\n", output);
    
#endif

#ifdef ARDP_SEC_HAVE_INTEGRITY_CRC
    fputs("CRC Integrity:\n", output);
    for (sp = req->secq; sp; sp = sp->next) {
	if (sp->service == ARDP_SEC_INTEGRITY &&
	    sp->mechanism == ARDP_SEC_INTEGRITY_CRC &&
	    (sp->processing_state == ARDP__SEC_PROCESSED_SUCCESS
	     /* Can't reach PREP_SUCCESS on the server unless the client's
		message was successfully processed, so this is OK.
		Obviously, we need a better interface. --swa, tryutov, 9/97
		*/
	     || sp->processing_state == ARDP__SEC_PREP_SUCCESS)) {
	    ++found_crc_info;
	    fputs("  The message we received had a correctly verified CRC"
		  " checksum.\n", output);
	}
    }
    if (!found_crc_info)
	fputs("  none found\n", output);
#endif

}
#endif /* DEMONSTRATE_SECURITY_CONTEXT */
