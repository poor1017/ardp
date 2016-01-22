#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include <pconfig.h>		/* Prospero config routines in GOSTLIB */
#include "ardp_sec_config.h"
#include <ardp.h>

#ifdef DEMONSTRATE_SECURITY_CONTEXT
#include <ardp_sec.h>
#endif

#include "sample.h"

#define MAX_THREADS 200
#define Stringize(num) #num
#define SStringize(val) Stringize(val)

void *client_thread(void *args);

char *usage = "client -tn -mn\n\tc: Indicates how many of processes should "
	      "run (including the parent)\n\t"
              "t: Indicates the total number of threads\n\t"
              "m: Indicates the total number of requests\n\t"
	      "n: The number";

/* client_thread():
	Each thread issues an ARDP send command and waits for the response.
	The transmission is in blocking mode.
 */
void *client_thread(void *args)
{
    RREQ req;			/* Request to be sent */
    int length,
	numofLoop;
    int ret_val;
    int flag = ARDP_A2R_COMPLETE;
    char mach_name[80];
    char BUFFER[256];
    int i = 0;
    pthread_t threadID;

    /* If you leave this set to NULL, then the ardp_send() function will
       resolve the hostname anew each time you call ardp_send.  Setting this to
       a pointer to a struct sockaddr_in lets you cache the information about
       the IP address and port number, saving some steps.
       This is no longer as important as it was once, since the
       ardp_hostname2addr() function that ardp_send() calls now does its own
       internal caching.  */

    struct sockaddr_in *destination_address_and_port = NULL;

#ifdef SAMPLE_CLIENT_CACHE_DESTINATION_ADDRESS_AND_PORT
    struct sockaddr_in cache;
    destination_address_and_port = &cache;
#endif

    numofLoop = (int)args;
    threadID = pthread_self();
    sprintf(mach_name, "%s(%d)", SAMPLE_DEFAULT_SUPER_SERVER_HOST,
	    SAMPLE_DEFAULT_SERVER_PORT);
    /* Loop N times */
    for (i=1; i < numofLoop + 1; ++i) {
	/* This is OK because a pthread_t is always going to be an integer
	   type.   If you find an implementation treating a pthread_t as other
	   than an integer type, please tell us about it.  --swa, salehi */
	sprintf(BUFFER, "This is the client #%lu sending to super server msg"
		" #%d\n", (long) threadID, i);
	length = strlen(BUFFER);

	if ((req = ardp_rqalloc()) == NULL) {
	    rfprintf(stderr, "Out of memory\n");
	    exit(1);
	}
	/* Add text in BUFFER to request req. */

	if ((ret_val = ardp_add2req(req, flag, BUFFER, length))) {
	    rfprintf(stderr, "ardp_add2req, error number %d: %s\n", ret_val,
		     ardp_err_text[ret_val]);
	    exit(1);
	}

#if 1 && defined(DEMONSTRATE_SECURITY_CONTEXT)

	if (ardp_config.preferred_ardp_version >= 1) {
	    int tmp;
	    ardp_sectype *crc_integ, /* reference for Integrity CRC */
		*kerb_integref,	/*  refers to Kerberos integrity.*/
		*kerb_privref,	/*  refers to Kerberos privacy.*/
		*kerb_authref,	/* reference for Kerberos authentication  */
		*asrthost_authref, /* reference for ASRTHOST authentication  */
		*label_ref;	/* Reference for labels. */

#if 1 && defined(ARDP_SEC_HAVE_AUTHENTICATION_KERBEROS)
           /* AUTHENTICATION_KERBEROS */
	    if((tmp = ardp_req_security(req, ardp_sec_authentication_kerberos, 
					&kerb_authref,
					mach_name /* remote host */, 
					SERVER_SERVICE_NAME /* service */))) {
		rfprintf(stderr, 
			 "sample client(%lu): ardp_req_security() "
			"failed: ARDP error # %d\n", (long) threadID, tmp);
		exit(1);
	    } /* if */
	    
	    /* developer: Change the ioctl argument to 1 if you want a
	       response. */ 
	    /* Note to demonstrator: You do not need to 
	       set the kerb_authref ARDP_SEC_YOU ioctl to 1 if you are using
	       Kerberos privacy or integrity, since both of those methods
	       automatically guarantee that the peer is authenticated. */
	    if((tmp = ardp_sectype_ioctl(kerb_authref, ARDP_SEC_YOU, 1))) {
		/* Note: If you link in with Prospero, then you can use
		   perrmesg() to get a nicer error message.  We should have
		   this facility available directly within ARDP, but that's for
		   future development.  (If you feel like being nice, move a
		   similar thing into it for us.) */
		rfprintf(stderr, 
			"sample client(%d): ardp_sectype_ioctl(ARDP_SEC_YOU) "
			"failed: ARDP error # %d\n", threadID, tmp);
		exit(1);
	    }    

	    if((tmp = ardp_sectype_ioctl(kerb_authref, ARDP_SEC_CRITICALITY, 1))) {
		/* Note: If you link in with Prospero, then you can use
		   perrmesg() to get a nicer error message.  We should have
		   this facility available directly within ARDP, but that's for
		   future development.  (If you feel like being nice, move a
		   similar thing into it for us.) */
		rfprintf(stderr, 
			"sample client(%d): ardp_sectype_ioctl(ARDP_SEC_YOU) "
			"failed: ARDP error # %d\n", threadID, tmp);
		exit(1);
	    }    


#endif /* 1 && defined(ARDP_SEC_HAVE_AUTHENTICATION_KERBEROS) */

#if 0 && defined(ARDP_SEC_HAVE_AUTHENTICATION_ASRTHOST)
           /* AUTHENTICATION_ASRTHOST */
	    if((tmp = ardp_req_security(req, ardp_sec_authentication_asrthost, 
					&asrthost_authref))) {
		rfprintf(stderr, "sample client(%d):"
			 "ardp_req_security(authentication_asrthost) "
			 "failed: ARDP error # %d\n", threadID, tmp);
		exit(1);
	    }
	    /* developer: Change the ioctl argument to 1 if you want a
	       response. */ 
	    if((tmp = ardp_sectype_ioctl(asrthost_authref, ARDP_SEC_YOU, 0))) {
		/* Note: If you link in with Prospero, then you can use
		   perrmesg() to get a nicer error message.  We should have
		   this facility available directly within ARDP, but that's for
		   future development.  (If you feel like being nice, move a
		   similar thing into it for us.) */
		rfprintf(stderr, 
			 "sample client(%d): ardp_sectype_ioctl(ARDP_SEC_YOU) "
			 "failed: ARDP error # %d\n", threadID, tmp);
		exit(1);
	    }    
	    /* This option can also be set.  Later we should provide an
	       IOCTL for this. */
	    asrthost_authref->reject_if_peer_does_not_reciprocate = 1;
#endif /* 1 && defined(ARDP_SEC_HAVE_AUTHENTICATION_ASRTHOST) */



#if 0 && defined(ARDP_SEC_HAVE_INTEGRITY_CRC)
	    /* INTEGRITY_CRC */

	    if((tmp = ardp_req_security(req, ardp_sec_integrity_crc, 
				       &crc_integ))) {
		rfprintf(stderr, "sample client(%d): "
			 "ardp_req_security(ardp_sec_integrity_crc) "
			"failed: ARDP error # %d\n", threadID, tmp);
		exit(1);
	    }
	    
	    /* Change the ioctl to 1 if you want to. */
	    if((tmp = ardp_sectype_ioctl(crc_integ, ARDP_SEC_YOU, 1))) {
		rfprintf(stderr, "sample client(%d):"
			 " ardp_sectype_ioctl(ARDP_SEC_YOU) on integrity"
			 " failed: ARDP error # %d\n", threadID, tmp);
		exit(1);
	    }    
#endif
	    
	    
	    /* You don't need to turn on both INTEGRITY_KERBEROS and
	       PRIVACY_KERBEROS --- privacy subsumes integrity. */
	    
#if 1 && defined(ARDP_SEC_HAVE_INTEGRITY_KERBEROS)
	    /* INTEGRITY KERBEROS */
	    if (ardp_debug > 3)
		rfprintf(stderr, "ardp sample client(%d): trying"
			 "  integrity_kerberos\n", threadID);
	    if((tmp = ardp_req_security(req, ardp_sec_integrity_kerberos,
					&kerb_integref, kerb_authref))) {
		rfprintf(stderr, "%s: ardp_req_security() failed,"
			 " ARDP error #%d: %s", argv[0], tmp, 
			 ardp_err_text[tmp]);
		exit(1);
	    }
#endif
	    
#if 0 && defined(ARDP_SEC_HAVE_PRIVACY_KERBEROS)
	    /* PRIVACY KERBEROS */
	    if (ardp_debug > 3)
		rfprintf(stderr, "ardp sample client(%d): trying"
			 " privacy_kerberos\n", threadID);
	    if((tmp = ardp_req_security(req, ardp_sec_privacy_kerberos,
					&kerb_privref, kerb_authref))) {
		rfprintf(stderr, "%s: ardp_req_security() failed,"
			 " ARDP error #%d:", argv[0], tmp);
		exit(1);
	    }
	    if((tmp = ardp_sectype_ioctl(kerb_privref, ARDP_SEC_YOU, 1))) {
		rfprintf(stderr, "sample client(%d):"
			 " ardp_sectype_ioctl(ARDP_SEC_YOU) on"
			" privacy failed: ARDP error # %d\n", threadID, tmp);
		exit(1);
	    }    
	    if((tmp = 
		ardp_sectype_ioctl(kerb_privref, ARDP_SEC_CRITICALITY, 1))) {
		rfprintf(stderr, "sample client(%d):"
			 " ardp_sectype_ioctl(ARDP_SEC_CRITICALITY) on"
			 " privacy failed: ARDP error # %d\n", threadID, tmp);
		exit(1);
	    }    
#endif

#if 1 && defined(ARDP_SEC_HAVE_LABELS_CLASS_OF_SERVICE)
	    /* LABELS_CLASS_OF_SERVICE */ 
	    if((tmp = ardp_req_security(req, ardp_sec_labels_class_of_service, 
				       &label_ref,
				       "FIRST-LABEL",
				       "Value-Of-First-Label"))) {
		rfprintf(stderr, 
			 "sample client(%d): ardp_req_security() "
			 "failed: ARDP error # %d\n", threadID, tmp);
		exit(1);
	    }
	    
	    if((tmp = ardp_sectype_ioctl(label_ref, ARDP_SEC_YOU, 0))) {
		rfprintf(stderr, 
			 "sample client(%d): ardp_sectype_ioctl(ARDP_SEC_YOU) "
			 "failed: ARDP error # %d: %s\n", threadID, tmp,
			ardp_err_text[tmp]);
		exit(1);
	    }
#endif /*  1 && defined(ARDP_SEC_HAVE_LABELS_CLASS_OF_SERVICE) */
	}
#endif


	/* Send request req to server name given by mach_name. The -1
	 * value indicates that ardp_send returns as soon as it gets a
	 * complete response (or times out after ARDP's default timeout).
	 */
	
	if ((ret_val = ardp_send(req, mach_name, 
				 destination_address_and_port, -1))) {
	    rfprintf(stderr, "ardp_send, status (or err) number %d:%s\n",
		     ret_val, ardp_err_text[ret_val]);
	    exit(1);
	}
	
	if (req->status == ARDP_STATUS_COMPLETE) {
/* 	    get_data(req, stdout); */
	} else {
	    if (req->status == ARDP_STATUS_ACTIVE) {
		rfprintf(stderr, "REQUEST pending.  (This should not happen in
this sample client, since we are running in the standard synchronous mode.)  Aborting\n
");
		exit(1);
	    } else {
		rfprintf(stderr, "client: This shouldn't happen: ardp_send \
reported that the request completed successfully, yet its status was set to \
%d; aborting \n", req->status);
		exit(1);
	    }
	}
    }
    /* NOTREACHED */
    return 0;
    
} /* client_thread */

/*
 * This is a sample client program using the ARDP library.
 *
 * The client sends requests to the server.
 * Rewritten: Steve Augart <swa@ISI.EDU> 6/30/97
 * Modified:  Gayatri Chugh (chugh@isi.edu) 9/30/97
 * Modified:  Nader Salehi <salehi@isi.edu> 4/30/98
 *
 */

int
main(int argc, char **argv)
{
    int i, 
	j,
	retval,
	totalMessages,
	totalThreads;
    pthread_t *threadID;
    void **status;

    /* This parses the standard Prospero and GOST and ARDP arguments.
       This handles the -pc configuration arguments and the -D debugging
       flag.   Try -D9 for lots of detail about ARDP. */
    p_command_line_preparse(&argc, argv);
    ardp_initialize();
    ardp_abort_on_int();

    totalMessages = 0;
    totalThreads = 0;
    for (j = 0; j < argc; j++) {
	if (argv[j][0] == '-') {
	    if (argv[j][1] == 't')
		totalThreads = atoi(&(argv[j][2]));
	    else if (argv[j][1] == 'm')
		totalMessages = atoi(&(argv[j][2]));
	} /* if */
    } /* for */

    if (!totalMessages) {
	puts(usage);
	exit(0);
    } /* if */
    
    threadID = (pthread_t *)malloc(sizeof(pthread_t) * totalThreads);
    memset(threadID, 0, sizeof(pthread_t) * totalThreads);
    status = (void **)malloc(sizeof (void *) * totalThreads);
    for (i = 0; i < totalThreads; i++) {
	retval = pthread_create(&(threadID[i]), NULL, client_thread, 
				(void *)(totalMessages / totalThreads));
	if (retval) {
	    rfprintf(stderr, "mt-client: Cannot create a thread: %s\n",
		    strerror(retval));
	    exit(0);
	} /* if */
    } /* for */

    for (i = 0; i < totalThreads; i++) {
	retval = pthread_join(threadID[i], &(status[i]));
	if (retval) {
	    rfprintf(stderr, "mt-client: Cannot join thread #%d: %s\n",
		    threadID[i], strerror(retval));
	    exit(0);
	} /* if */
    } /* for */
    return 0;
}
