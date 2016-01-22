#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <unistd.h>

#include <pconfig.h>		/* Prospero config routines in GOSTLIB */
#include "ardp_sec_config.h"
#include <ardp.h>

#ifdef DEMONSTRATE_SECURITY_CONTEXT
#include <ardp_sec.h>
#endif

#include "sample.h"


#define Stringize(num) #num
#define SStringize(val) Stringize(val)

void child_process(int num_loops);

char *usage = "client -pn -mn\n\tp: Indicates how many of processes should "
	      "run\n\t"
              "m: Indicates the total number of requests\n\t"
	      "n: The number";

void child_process(int num_loops)
{
    RREQ req;			/* Request to be sent */
    int length;
    int ret_val;
    int flag = ARDP_A2R_COMPLETE;
    char mach_name[80];
    char BUFFER[256];
    int i = 0;
    pid_t pid;
    void *dummy;

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

    sprintf(mach_name, "%s(%d)", SAMPLE_DEFAULT_SUPER_SERVER_HOST,
	    SAMPLE_DEFAULT_SERVER_PORT);
    /* Loop N times */
    pid = getpid();
    for (i=1; i < num_loops + 1; ++i) {
	sprintf(BUFFER, "This is the client #%d sending to super server msg"
		" #%d\n", (int)pid, i);
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
			 "sample client(%d): ardp_req_security() "
			"failed: ARDP error # %d\n", (int)pid, tmp);
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
			"failed: ARDP error # %d\n", (int)pid, tmp);
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
			"failed: ARDP error # %d\n", (int)pid, tmp);
		exit(1);
	    }    


#endif /* 1 && defined(ARDP_SEC_HAVE_AUTHENTICATION_KERBEROS) */

#if 0 && defined(ARDP_SEC_HAVE_AUTHENTICATION_ASRTHOST)
           /* AUTHENTICATION_ASRTHOST */
	    if((tmp = ardp_req_security(req, ardp_sec_authentication_asrthost, 
					&asrthost_authref))) {
		rfprintf(stderr, "sample client(%d):"
			 "ardp_req_security(authentication_asrthost) "
			 "failed: ARDP error # %d\n", (int)pid, tmp);
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
			 "failed: ARDP error # %d\n", (int)pid, tmp);
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
			"failed: ARDP error # %d\n", (int)pid, tmp);
		exit(1);
	    }
	    
	    /* Change the ioctl to 1 if you want to. */
	    if((tmp = ardp_sectype_ioctl(crc_integ, ARDP_SEC_YOU, 1))) {
		rfprintf(stderr, "sample client(%d):"
			 " ardp_sectype_ioctl(ARDP_SEC_YOU) on integrity"
			 " failed: ARDP error # %d\n", (int)pid, tmp);
		exit(1);
	    }    
#endif
	    
	    
	    /* You don't need to turn on both INTEGRITY_KERBEROS and
	       PRIVACY_KERBEROS --- privacy subsumes integrity. */
	    
#if 1 && defined(ARDP_SEC_HAVE_INTEGRITY_KERBEROS)
	    /* INTEGRITY KERBEROS */
	    if (ardp_debug > 3)
		rfprintf(stderr, "ardp sample client(%d): trying"
			 "  integrity_kerberos\n", (int)pid);
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
			 " privacy_kerberos\n", (int)pid);
	    if((tmp = ardp_req_security(req, ardp_sec_privacy_kerberos,
					&kerb_privref, kerb_authref))) {
		rfprintf(stderr, "%s: ardp_req_security() failed,"
			 " ARDP error #%d:", argv[0], tmp);
		exit(1);
	    }
	    if((tmp = ardp_sectype_ioctl(kerb_privref, ARDP_SEC_YOU, 1))) {
		rfprintf(stderr, "sample client(%d):"
			 " ardp_sectype_ioctl(ARDP_SEC_YOU) on"
			" privacy failed: ARDP error # %d\n", (int)pid, tmp);
		exit(1);
	    }    
	    if((tmp = 
		ardp_sectype_ioctl(kerb_privref, ARDP_SEC_CRITICALITY, 1))) {
		rfprintf(stderr, "sample client(%d):"
			 " ardp_sectype_ioctl(ARDP_SEC_CRITICALITY) on"
			 " privacy failed: ARDP error # %d\n", (int)pid, tmp);
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
			 "failed: ARDP error # %d\n", (int)pid, tmp);
		exit(1);
	    }
	    
	    if((tmp = ardp_sectype_ioctl(label_ref, ARDP_SEC_YOU, 0))) {
		rfprintf(stderr, 
			 "sample client(%d): ardp_sectype_ioctl(ARDP_SEC_YOU) "
			 "failed: ARDP error # %d: %s\n", (int) pid, tmp,
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
} /* child_process */
/*
 * This is a sample client program using the ARDP library.
 *
 * The client sends requests to the server.
 * Rewritten: Steve Augart <swa@ISI.EDU> 6/30/97
 * Modified:  Gayatri Chugh (chugh@isi.edu) 9/30/97
 *
 */

int
main(int argc, char **argv)
{
    int j,
	num_of_children,
	status,
	total_messages;
    pid_t *child_pid,
	pid;

    /* This parses the standard Prospero and GOST and ARDP arguments.
       This handles the -pc configuration arguments and the -D debugging
       flag.   Try -D9 for lots of detail about ARDP. */
    p_command_line_preparse(&argc, argv);
    ardp_initialize();

    num_of_children = 0;
    total_messages = 0;
    for (j = 0; j < argc; j++) {
	if (argv[j][0] == '-') {
	    if (argv[j][1] == 'p')
		num_of_children = atoi(&(argv[j][2]));
	    else if (argv[j][1] == 'm')
		total_messages = atoi(&(argv[j][2]));
	} /* if */
    } /* for */

    if (!total_messages) {
	puts(usage);
	exit(0);
    } /* if */
    child_pid = (pid_t *)malloc(sizeof(pid_t) * num_of_children);
    memset(child_pid, 0, sizeof(pid_t) * num_of_children);
    for (j = 0; j < num_of_children; j++) {
	child_pid[j] = fork();
	if (!child_pid[j]) {	/* Child process */
	    child_process(total_messages / num_of_children);
	    exit(0);
	} /* if */
    } /* for */

    for (j = 0; j < num_of_children; j++) {
	pid = wait(&status);
	if ((pid == -1) || (status % 256)) {
	    perror("client");
	    exit(1);
	} /* if */
    } /* for */
    exit(0);
}
