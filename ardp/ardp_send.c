/*
 * Copyright (c) 1991-1993 by the University of Southern California
 *
 * Written  by bcn 1989-92  as dirsend.c in the Prospero distribution
 * Modified by bcn 1/93     separate library and add support for asynchrony 
 *
 * For copying and distribution information, please see the file
 * <usc-license.h>.
 */

#include <usc-license.h>

#include <unistd.h>		/* close() prototype. */
#include <stdlib.h>		/* rand() prototype */
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>		/* struct in_addr prototype */
#include <arpa/inet.h>		/* prototype inet_ntoa(),inet_addr() */
#include <netdb.h>
#ifdef GL_THREADS
#include <pthread.h>
#include <fcntl.h>
#endif /* GL_THREADS */

#include <ardp.h>
#include <ardp_sec.h>		/* ardp_sec_commit() prototype. */
#include <ardp_time.h>		/* internals routines; add_times() */
#include "ardp__int.h"		/* ardp__gettimeofday() */
#include <perrno.h>

/* Old way is not thread-safe. --swa, 7/97 */
/* #define OLD_GETHOSTBYNAME       */
/* do it the old way, since we're on the client                                   */ 

/* The default port number is configured by ardp_config.default_peer, and
   ardp_config.default_port.
   ardp_def_port_no is set by ardp_init(), based upon those two values. */
static u_int16_t  ardp_def_port_no = 0;	/* Default UDP port to use.  0 means no
					   default was enabled.   If there is
					   no default, then requests without a
					   port # will get ARDP_PORT_UNKN
					   returned. */ 

int ardp_port = -1;		/* UNIX descriptor of the opened UDP port */ 

/* The address and port for the UDP port bound to file descriptor
   'ardp_port'. */ 
struct sockaddr_in ardp_client_address_port; /* initialized to zero by
						compiler.  */
static u_int16_t ardp_next_cid(void);
static void ardp__set_def_port_no(void);

/*
 * ardp_send - send a request and possibly wait for response
 *
 *   ardp_send takes a pointer to a structure of type RREQ, an optional 
 *   hostname, an optional pointer to the desination address, and the time to
 *   wait before returning in microseconds.
 *
 *   If a destination address was specified, the address is inserted
 *   into the RREQ structure.  If not, but a hostname was specified, the
 *   hostname is resolved, and its address inserted into the RREQ
 *   structure.  The hostname may be followed by a port number in
 *   parentheses in which case the port field is filled in.  If not
 *   specified, the Prospero directory server port is used as the default. 
 *   If the host address is a non-null pointer to an empty address, then
 *   the address is also filled in.
 *
 *   ardp_send then sends the packets specified by the request structure 
 *   to the address in the request structure.  If the time to wait is
 *   -1, it waits until the complete response has been received and
 *   returns PSUCCESS or PFAILURE depending on the outcome.  Any
 *   returned packets are left in the RREQ strucure.  If the time to
 *   wait is 0, ardp_send returns immediately.  The PREQ strucure will
 *   be filled in as the response is received if calls are made to
 *   ardp_check_messages (which may be called by an interrupt, or
 *   explicitly at appropriate points in the application.  If the time
 *   to wait is positive, then ardp_send waits the specified lenght of
 *   time before returning.
 *
 *   If ardp_send returns before the complete response has been
 *   received, it returns ARDP_PENDING (-1).  This means that only
 *   the status field in the RREQ structure may be used until the status
 *   field indicates ARDP_STATUS_COMPLETE.  In no event shall it be legal 
 *   for the application to modify fields in the RREQ structure while a
 *   request is pending.  If the request completes during the call to
 *   ardp_send, it returns ARDP_SUCCESS (0).  On error, a positive
 *   return or status value indicates the error that occured.
 *
 *   In attempting to obtain the response, the ARDP library will wait
 *   for a response and retry an appropriate number of times as defined
 *   by timeout and retries (both static variables).  It will collect
 *   however many packets form the reply, and return them in the
 *   request structue.
 *
 *   ARGS:      req        Request structure holding packets to send 
 *			   and to receive the response
 *              hname      Hostname including optional port in parentheses
 *              dest       Pointer to destination address
 *              ttwait     Time to wait in microseconds
 *
 *   MODIFIES:  dest	   If pointer to empty address
 *              req	   Fills in ->recv and frees ->trns
 *
 *   NOTE:      In preparing packets for transmission, the packets
 *              are modified.  Once the full response has been received,
 *              the packets that were sent are freed.
 *
 *   In all events (error or not), the caller is responsible for freeing the
 *   RREQ passed to ardp_send().
 */
enum ardp_errcode
ardp_send(RREQ		req,	/* Request structure to use (in, out) */
	  const char		*dname,	/* Hostname (and port) of destination
					 */ 
	  struct sockaddr_in *dest, /* Pointer to destination address */
	  int		ttwait) /* Time to wait in microseconds       */
{
#ifdef GL_THREADS
    int  retval;
#endif /* GL_THREADS */

    PTEXT	ptmp;		/* Temporary packet pointer	      */
    enum ardp_errcode aerr;	/* To temporarily hold return values  */
#ifdef GL_THREADS
    static pthread_once_t set_def_port_no_once = PTHREAD_ONCE_INIT;
#endif

    /* Throw out a status message right away. */
    if(ardp_debug >= 9) {
	rfprintf(stderr, "ardp: In ardp_send() -- going to send to %s\n", 
		dname);
        for(ptmp = req->outpkt; ptmp; ptmp = ptmp->next) {
            rfprintf(stderr,"Packet %d:\n",ptmp->seq);
            ardp_showbuf(ptmp->text, ptmp->length, stderr);
            putc('\n', stderr);
        }
    }

    p_clear_errors();

    if (dname) 
	stcopy_GSP(dname, &req->peer_hostname);

    /* We don't ever call ardp_init() from here in the multi-threaded case,
       since we'll have already called ardp_init() from ardp_initialize(). */ 
    if(ardp_port < 0) {
#ifdef GL_THREADS
	char msg[] = "ardp: In ardp_send(): Already failed to initialize"
	    " the ARDP client port (ardp_port); no way to send a message.\n";
	if (ardp_debug)
	    rfprintf(stderr, "%s", msg);
	return gl_set_err(ARDP_UDP_CANT, "%s", msg);
#else
	if ((aerr = ardp_init()))
	    return aerr;
#endif	
    }

#ifdef GL_THREADS
    /* Set the default ARDP port no, in the multi-threaded case.  
       We do this as part of ardp_init() in the single-threaded case. */
    if(pthread_once(&set_def_port_no_once, ardp__set_def_port_no)) {
	internal_error("Failure in pthread_once().");
    } /* if */
#endif

    if(req->status == ARDP_STATUS_FREE) {
	gl_function_arguments_error("Attempt to send free RREQ\n");
	/* NOTREACHED */
	return(perrno = ARDP_BAD_REQ);
    }

#ifdef ARDP___CANONICALIZE_OUTPKT
    ardp___canonicalize_ptexts(req->outpkt);
#endif
    
#ifndef ARDP_NO_SECURITY_CONTEXT
    /* Write out the bytes of the security context.  
       ardp_sec_commit() returns an error indication only if a critical context
       could not be generated. 
       ardp_sec_commit() guarantees to write out only complete security context
       blocks; no half-written blocks will be left. */
    if((aerr = ardp_sec_commit(req)))
	return aerr;
#endif
    while(req->outpkt) {
        req->outpkt->seq = ++(req->trns_tot);
        ptmp = req->outpkt;
        EXTRACT_ITEM(ptmp,req->outpkt);
        APPEND_ITEM(ptmp,req->trns);
    }


    /* Assign connection ID */
    req->cid = ardp_next_cid();

    /* Resolve the host name, address, and port arguments          */
    /* If we were given the host address, then use it.  Otherwise  */
    /* look up the hostname.  If we were passed a host address of   */
    /* 0, we must look up the host name, then replace the old value */

    if(!dest || (dest->sin_addr.s_addr == 0)) {
        /* If we have no destination and a null host name, return an error */
        if((dname == NULL) || (*dname == '\0')) {
            if (ardp_debug >= 1)
                rfprintf(stderr, "ardp_send: Null hostname specified\n");
            return(perrno = ARDP_BAD_HOSTNAME);
        }
        /* New way of doing things.  ardp_hostname2addr() will set the port and
	   address.  */
        if (ardp_hostname2addr(dname, &(req->peer)))
            return perrno = ARDP_BAD_HOSTNAME;
    }
    else { 
	memcpy( &(req->peer),dest, S_AD_SZ);
    }

    /* If no port set, use default port */
    if(req->peer_port == 0)  {
        if (!ardp_def_port_no)
            return perrno = ARDP_PORT_UNKN;
        req->peer_port = htons(ardp_def_port_no);
    }
    /* If dest was set, but zero, fill it in */
    if(dest && (dest->sin_addr.s_addr == 0)) 
	memcpy(dest, &(req->peer), S_AD_SZ);

    if((aerr = ardp_headers(req)))
        return(aerr);

#ifdef GL_THREADS
    req->status = ARDP_STATUS_NOSTART;

    /* In order to avoid deadlock, we should lock daemon mutex before locking
       the ardp_activeQ mutex. --Nader Salehi 5/98 */
    retval = pthread_mutex_lock(daemon.mutex);
    if (retval) {
	rfprintf(stderr, "%s\n", strerror(retval));
	internal_error("ardp_send(): Failure in locking a mutex");
    } /* if */
#else
    req->status = ARDP_STATUS_ACTIVE;
#endif /* GL_THREADS */

#ifdef GL_THREADS
#endif /* GL_THREADS */

    EXTERN_MUTEXED_LOCK(ardp_activeQ);

#ifdef GL_THREADS
    /* Create mutex and condition variable only if operating in blocking mode.
       -- Nader Salehi 4/98 */
    /* A problem here is the need to resend messages to a different server --
       e.g., when forwarding.  Therefore, we must test whether it has been
       done. */
    req->thread_id = pthread_self();
    if (ttwait) {
	req->mutex = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
	if (!req->mutex)
	    out_of_memory();
	pthread_mutex_init(req->mutex, NULL);
	req->cond = (pthread_cond_t *)malloc(sizeof(pthread_cond_t));
	if (!req->cond)
	    out_of_memory();
	pthread_cond_init(req->cond, NULL);
    } /* if */
    else {
	req->mutex = NULL;
	req->cond = NULL;
    } /* else */
#endif /* GL_THREADS */

    APPEND_ITEM(req,ardp_activeQ);
    ++ardp_activeQ_len;

    req->wait_till = add_times(ardp__gettimeofday(), req->timeout_adj);
#ifdef GL_THREADS
    req->ttwait = ttwait;	/* Keep the time to wait */

    /* Now, we have to inform daemon thread of the new request.  This is done
       by signaling daemon's associated condition variable. */
    daemon.outputReady = 1;	/* There is something to be sent */
    retval = pthread_cond_signal(daemon.cond);
    if (retval) {
	rfprintf(stderr, "%s\n", strerror(retval));
	internal_error("ardp_send(): Failure in sending a signal");
    } /* if */
    pthread_mutex_unlock(daemon.mutex);
#else
    if ((aerr = ardp_xmit(req, req->pwindow_sz)))
	return perrno = aerr;
#endif /* GL_THREADS */
    EXTERN_MUTEXED_UNLOCK(ardp_activeQ);

    /* XXX REQ is still on the activeQ.   It may be moved to ardp_completeQ by
       something we don't call. */

    /* This is a performance improvement (Santosh Rao's, I believe) that
       prevents us from calling ardp_retriev() if there's no time to wait. This
       saves us a call to select() and a few other calls when we're operating
       in an asynchronous mode. --swa, 4/13/95 */ 
    if (ttwait)
        return ardp_retrieve(req,ttwait);
    else 
        return ARDP_PENDING;
}


/*
 * ardp_init - Open socket and bind port for network operations
 *
 *    Sets the variables:
 *	ardp_port, ardp_client_address_port, ardp_def_port_no (local), 
 *	    
 *    ardp_init attempts to determine the default destination port.
 *    It then opens a socket for network operations and attempts
 *    to bind it to an available privleged port.  It tries to bind to a
 *    privileged port so that its peer can tell it is communicating with
 *    a "trusted" program.  If it can not bind a privileged port, then
 *    it also returns successfully since the system will automatically 
 *    assign a non-priveleged port later, in which case the peer will
 *    assume that it is communicating with a non-trusted program.  It
 *    is expected that in the normal case, we will fail to bind the
 *    port since most applications calling this routine should NOT
 *    be setuid.  On success, ardp_port will be set to the file descriptor of
 *    the socket to be used for communication.
 */

/*
 * This also sets the local variable ardp_def_port_no, used by ardp_send
 * above. 
 */

/* This also sets the global variable ardp_client_address_port */
enum ardp_errcode
ardp_init(void)
{
    int			tmp;	/* For stepping through ports; also temp dummy
				   value. */

    /* This code must be called before we go multi-threaded.
       1) Mitra points out that under Solaris, getservbyname() is not
       thread-safe.
        2) This code modifies global variables without performing any sort of
       locking; if called twice it would be bad.   --swa
       */

    /* 9/96: Make it legal and reasonable to call ardp_init() twice. */
    if (ardp_port != -1) {
	close(ardp_port);
	ardp_port = -1;
	if (ardp_debug)
	    rfprintf(stderr, "ardp_init(): closing port # %d; opening new"
		    " one...", ntohs(ardp_client_address_port.sin_port));
	memset(&ardp_client_address_port, '\000', 
	       sizeof ardp_client_address_port);
    }
        
#ifndef GL_THREADS
    /* In the single-threaded case, we set the default port number here, in
       ardp_init() .  In the multi-threaded case, we use a pthread_once in
       ardp_send; the reason for that is to avoid committing to a default ARDP
       port until as late as possible. */ 
    ardp__set_def_port_no();
#endif

   /* Open the local socket from which packets will be sent */
    if((ardp_port = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        if (ardp_debug >= 1)
            rfprintf(stderr, "ardp: Can't open client's sending socket: %s"
		    " (error #%d)\n", unixerrstr(), errno);
	return perrno = ARDP_UDP_CANT;
    }

#ifdef GL_THREADS
/*     fcntl(ardp_port, F_SETFL, O_NDELAY); */
#endif /* GL_THREADS */
    /* Now bind the port. */
    memset((char *)&ardp_client_address_port, '\000', 
	   sizeof(ardp_client_address_port));
    ardp_client_address_port.sin_family = AF_INET;
    ardp_client_address_port.sin_addr.s_addr =  myaddress();
#ifndef ARDP_NONPRIVED
    /* Try to bind it to a privileged port - loop through candidate   */
    /* ports trying to bind.  If failed, that's OK, we will let the   */
    /* system assign a non-privileged port later                      */
    for(tmp = ardp_config.first_privp; 
        tmp < ardp_config.first_privp + ardp_config.num_privp; 
        tmp++) {
	ardp_client_address_port.sin_port = htons((u_short) tmp);
	if(bind(ardp_port, (struct sockaddr *)&ardp_client_address_port,
		sizeof(ardp_client_address_port)) == 0) {
	    goto bind_worked;
#if 0
	    /* old code: */
	    /* return (ARDP_SUCCESS); */
#endif
	}
	/* If the privileged address is already in use, go on to the next port
	   in the list. */ 
	if(errno == EADDRINUSE) 
	    continue;

	/* Any errors except for EADDRINUSE are worth breaking out of the loop
	   for; none of this loop's subsequent bind()s will succeed. */

	/* The most common failure will be EACCES (Permission Denied to
	   access the privileged ports) */
	if (errno == EACCES) {
	    if (ardp_debug)
		rfprintf(stderr, "ardp_init(): We are not allowed to bind to a"
			" privileged port; going to try a nonprived one."
			" (this is usually nothing to worry about)\n");
	    ardp_client_address_port.sin_port = 0; /* try to go on */
	    break;		/* go on to regular bind()  */
	} 

	/* Any error but EACCES and EADDRINUSE winds up here. */
	if (ardp_debug) {
	    rfprintf(stderr, "ARDP: Attempt to bind() to privileged"
		    " port #%d completed with nasty error # %d: %s):"
		    " (The client address(port) are: %s(%d));"
		    " aborting ardp_init()\n", 
		    tmp, errno, unixerrstr(),
		    inet_ntoa(ardp_client_address_port.sin_addr),
		    ntohs(ardp_client_address_port.sin_port));
	    }
	close(ardp_port);
	ardp_port = -1;
	memset((char *)&ardp_client_address_port, '\000', 
	       sizeof(ardp_client_address_port));
	return perrno = ARDP_UDP_CANT;
    }
#endif /* ndef ARDP_NONPRIVED */
    /* Unless we have a privileged port, now do a normal bind where we do NOT
       specify the port.   The sendto() will normally do an implicit bind for
       us, but (in this bit of experimental code) we want to try to get the
       port # (and hopefully IP address) so we will have something to pass to
       the Kerberos v5beta6 auth_context (if we are doing the security
       context).  Perhaps this should be called on an as-needed basis; for now,
       we'll just try to do what we can here. */
    /* NOTE: we do not need to bind an address normally.  However, if we are
       using the ARDP security context with kerberos, then we do need to know
       what our port number and host address are.  Therefore, we'll continue.
       */
    if(bind(ardp_port, (struct sockaddr *)&ardp_client_address_port,
	    sizeof ardp_client_address_port)) {
	if (ardp_debug) {
	    rfprintf(stderr, "ARDP: bind() completed with error # %d: %s):"
		    " client address(port) are: %s(%d)", errno, unixerrstr(),
		    inet_ntoa(ardp_client_address_port.sin_addr),
		    ntohs(ardp_client_address_port.sin_port));
	}
	close(ardp_port);
	ardp_port = -1;
	memset((char *)&ardp_client_address_port, '\000', 
	       sizeof(ardp_client_address_port));
	return perrno = ARDP_UDP_CANT;
    }
    /* OK, we now have successfully bound, either to a prived or non-priv'd
       port. */
bind_worked:
#if 0				/* bind() under SunOS (at least) never returns 
				   any useful information; just takes 'in'
				   parameters.  */
    if (ardp_debug)
	rfprintf(stderr, "Bind says our host & port # are %s(%d)\n", 
		inet_ntoa(ardp_client_address_port.sin_addr),
		ntohs(ardp_client_address_port.sin_port));
#endif


#if 0				/* nobody to actually connect to; I expect
				   getsockname() to fail. */
     /* "connect" the datagram socket; this is necessary to get a local address
       properly bound for getsockname() below. */

    if (connect(ardp_port, (struct sockaddr *)&ardp_client_address_port, 
		sizeof(ardp_client_address_port)) == -1) {
	if (ardp_debug) {
	    rfprintf(stderr, "ARDP: connect() completed with error # %d: %s):"
		    " client address(port) are: %s(%d)\n", errno, unixerrstr(),
		    inet_ntoa(ardp_client_address_port.sin_addr),
		    ntohs(ardp_client_address_port.sin_port));
	}
	close(ardp_port);
	ardp_port = -1;
	memset((char *)&ardp_client_address_port, '\000', 
	       sizeof(ardp_client_address_port));
	return perrno = ARDP_UDP_CANT;
    }
#endif

    memset((char *) &ardp_client_address_port, 0, 
	   sizeof(ardp_client_address_port));
    errno = 0;
    tmp = sizeof(ardp_client_address_port);
    /* Returns 0 on success, -1 on failure. */
    if (getsockname(ardp_port, (struct sockaddr *)&ardp_client_address_port,
		    &tmp)) {
	if (ardp_debug) {
	    rfprintf(stderr, "ARDP: getsockname() completed with error # %d:"
		    "%s): client address(port) are: %s(%d)", 
		    errno, unixerrstr(),
		    inet_ntoa(ardp_client_address_port.sin_addr),
		    ntohs(ardp_client_address_port.sin_port));
	}
	close(ardp_port);
	ardp_port = -1;
	memset((char *)&ardp_client_address_port, '\000', 
	       sizeof(ardp_client_address_port));
	return perrno = ARDP_UDP_CANT;
    } 
    if (ardp_debug)
	rfprintf(stderr, "ardp_client_address_port set to %s(%d)"
		" [%lx(%d)]\n", inet_ntoa(ardp_client_address_port.sin_addr),
		ntohs(ardp_client_address_port.sin_port),
		/* On some systems (LINUX, maybe others) s_addr is just an 
		   int; the system assumes that all ints are 32 bits.  We 
		   cast it to a long so that we can just use the "%l" 
		   format specifier in all cases, without getting 
		   warnings about mismatches.  If such a system were to ever
		   make longs into 64-bit quantities, then the code would 
		   break, not just generate warnings. --swa, 3/98 */
		(unsigned long) ardp_client_address_port.sin_addr.s_addr,
		ntohs(ardp_client_address_port.sin_port));
    return ARDP_SUCCESS;
}


static pid_t			last_pid = 0;     /* Reset after forks    */

void
ardp__next_cid_initialize_if_not_already()
{
    if (last_pid == 0)
	last_pid = getpid();
}

/*
 * ardp_next_cid - return next connection ID in network byte order
 *
 *    ardp_next_cid returns the next connection ID to be used
 *    after first converting it to network byte order.
 */
static 
u_int16_t
ardp_next_cid(void)
{
    static u_int16_t	next_conn_id = 0; /* Next conn id to use  */

    /* The following mutex was was is added to ensure synchronization among
       threads for altering next_conn_id. -- Nader Salehi 4/98 */
#if GL_THREADS
    static pthread_mutex_t p_th_mutexNEXT_CONN_ID = PTHREAD_MUTEX_INITIALIZER;
#endif /* GL_THREADS */
    int				pid = getpid();
    
    /* If we did a fork, reinitialize.  I wonder how much this slows down ARDP.
      We'll run the profiler on it at some point and see how much time is spent
      in getpid().  This is probably inefficient; could be fixed 
      --swa, katia, 2/97 */ 
    /* This code (the call to getpid()) was done because of a problem
       encountered by archie client writers, where clients would fork, and then
       each child process started with the same client port, and the same
       connection ID.  This could easily confuse the poor ARDP server, which
       assumed it was seeing duplicate requests.   Also, if we do multiplexing
       (forking clients) then we need to make sure each client listens on its
       own port so they don't receive replies meant for others. */
    if (!last_pid) {
	if (ardp_debug)
	    rfprintf(stderr, "ardp_next_cid(): strange behavior:"
		    " ardp__next_cid_initialize_if_not_already()"
		    " has not been called yet!");
	ardp_init();
    }
    if(last_pid != pid) {
#ifdef GL_THREADS
	/* Use of fork in a multi-threaded environment is strongly discouraged.
	   At the time of this modification, I am not aware of repercussions of
	   allowing threading and forking together.  For now the system stops
	   if it detects any forking within the program. -- Nader Salehi 4/98
	*/ 
	internal_error("ardp_next_cid():  Use of fork() in a multi-threaded"
		       " environment is strongly discouraged.  If your system"
		       " MUST fork, please use the single thread version of"
		       " the ARDP client library");
#else
	if(ardp_port >= 0) close(ardp_port);
	ardp_port = -1;
	next_conn_id = 0;
	ardp_init();
	last_pid = pid;
#endif /* GL_THREADS */
    }
    /* Find first connection ID */
#if 0
    assert(gl_is_this_thread_master()); /* rand and srand are unsafe */
#endif /* 0 */

#ifdef GL_THREADS
    /* Critical Area:  next_conn_id should be accessed by one thread at a
       time.  -- Nader Salehi 4/98 */
    pthread_mutex_lock(&p_th_mutexNEXT_CONN_ID);
#endif /* GL_THREADS */
    if(next_conn_id == 0) {
	srand(pid+time(0));
	next_conn_id = rand();
	last_pid = pid;
    }
    if(++next_conn_id == 0) ++next_conn_id;

#ifdef GL_THREADS
    pthread_mutex_unlock(&p_th_mutexNEXT_CONN_ID); /* Unlock the mutex */
#endif /* GL_THREADS */
    return(htons(next_conn_id));
}


/* Determine default udp port to use */
/* This code will try to use the default_peer ardp_config option, if one has
   been set. Otherwise, it will use ardp_config's default_port option, if none
   had been previously set. */ 
/* If no information letting us choose a default port is provided, then... no
   information letting us choose a default port is provided.  */ 
/* This function was broken off from ardp_init() so that we can still select
   the default port after calling ardp_initialize().   (In the multi-threaded
   case, we call ardp_init() as part of ardp_initialize()).
   */

static
void
ardp__set_def_port_no(void)
{
    if (ardp_config.default_peer && *(ardp_config.default_peer)) {
	struct servent 	*sp;	/* Entry from services file           */
#ifdef GL_THREADS
	struct servent thrdsafe_servent_st;
	char thrdsafe_buffer[256]; /* XXXX Need a better way of figuring out
				      the right size. */
#endif
#ifdef GL_THREADS
#if 0				/* We did this while working with Purify; did
				   not fix our problems.  */
	memset(&thrdsafe_servent_st, 0, sizeof thrdsafe_servent_st);
	memset(thrdsafe_buffer, 0, sizeof thrdsafe_buffer);
#endif
	sp = getservbyname_r(ardp_config.default_peer,"udp", 
			     &thrdsafe_servent_st, thrdsafe_buffer,
			     sizeof thrdsafe_buffer);
#else
	sp = getservbyname(ardp_config.default_peer,"udp");
#endif
	if (sp) {
	    ardp_def_port_no = sp->s_port;
        } else {
            if (ardp_debug >= 10)
                rfprintf(stderr, "ardp: udp/%s unknown service\n", 
                        ardp_config.default_peer);
        }
    }
    if (!ardp_def_port_no) /* if still unset */ {
        ardp_def_port_no = htons((u_short) ardp_config.default_port);
    }

    if (ardp_def_port_no) {
	if (ardp_debug >= 10) {
            rfprintf(stderr,"ardp: default udp port is %d\n", 
		    ntohs(ardp_def_port_no));
        }
    }else {
	if (ardp_debug >= 7)
            rfprintf(stderr, "ardp: Client's default udp port is unset.\n");
    }
}
