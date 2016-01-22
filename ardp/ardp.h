#ifndef ARDP_H_INCLUDED
#define ARDP_H_INCLUDED

/*
 * Copyright (c) 1991-1997 by the University of Southern California
 *
 * For copying and distribution information, please see the file
 * <usc-license.h>.
 */

#include <usc-license.h>

/*
 * Written  by bcn 1989-92  as pfs.h in the Prospero distribution
 * Modified by bcn 1/93     separate include file and extended preq structure
 * Modified by swa 11/93    error reporting; less dependent on Prospero
 * Modified by cheang 10/94 incorporate configuration management
 * Modified by swa    10/95 ardp now severable from rest of Prospero.
 * Many modifications, swa & katia, 8/96
 */

#include <stdio.h>              /* for FILE */

#include <stdarg.h>             /* for va_list */
#include <gl_threads.h>        /* PFS threads package. */
#include <gostlib.h>	
#include <gl_parse.h>		/* for INPUT type. */
#include <gl_strings.h>		/* for consistency-checking code */
#include <sys/time.h>
#include <sys/types.h>
#include <pmachine.h>		/* for u_int16_t and u_int32_t types */

/* Unfortunately, some buggy compilers (in this case, gcc 2.1 under
   VAX BSD 4.3) get upset if <netinet/in.h> is included twice. */
/* Note that the IEEE C standard specifies that multiple inclusions of an
   include file should not matter. */
#if !defined(IP_OPTIONS) || defined(linux)
#include <netinet/in.h> 
#endif

#include <list_macros.h>

/* Constants used in the implementation.  */
/* Configurable stuff is at the end. */

#define      ARDP_BACKOFF(x)   (2 * x)   /* (C)Backoff algorithm               */
#define	     ARDP_PTXT_HDR	    64   /* (CS)Max offset for start            */
#define	     ARDP_PTXT_LEN	  1250   /* (CS)Max length for data to send     */
#define	     ARDP_PTXT_LEN_R	  1405	 /* (CS)Max length for received data    */

/* Rationale for MAX_PTXT_LEN_R: According to IEEE std. 802.5, 1492 is the  */
/* max data length for an ethernet packet.  IP implementers tend to use     */
/* this information in deciding how large a packet could be sent without IP */
/* fragmentation occurring.  Subtract 16 octets for the IP header.          */
/* Subtract 8 octets for UDP header. The maximum ARDP header size is 6 bits */
/* or 64, so subtract 64 more for the ARDP header.  This leaves 1404 bytes. */
/* Note we only generate 1250 bytes because there are preV5 implementations */
/* out there with smaller limits. We also add one to MAX_PTXT_LEN_R to make */
/* sure there is always room to insert a null if needed.                    */

/* Must leave room to insert header when sending and to strip it on receipt */
#define		ARDP_PTXT_DSZ	ARDP_PTXT_LEN_R+2*ARDP_PTXT_HDR

/* Note: In doubly linked lists of the structures used for ARDP (and other
 * GOST group projects), the  */
/* ->previous element of the head of the list is the tail of the list and */
/* the ->next element of the tail of the list is NULL.  This allows fast  */
/* addition at the tail of the list.                                      */

/* Definition of text structure used to pass around each packet */
/* As with RREQ, 
   <NB>: Network byte order
   <HB>: Host byte order.
   */
struct ptext {
#ifdef P_ALLOCATOR_CONSISTENCY_CHECK
    enum p__consistency consistency;
#endif
    u_int16_t           seq;		  /* <HB> Packet sequence number.
					   */ 
    int		       length;		  /* <HB> Length of text (from start).
					     An explicit value of -1 means a 
					     sequenced control
					     packet. order. */  
    char	       context_flags;	  /* contexts; from ARDP v1 header */
    char	       *start;		  /* Start of packet		    */
    char	       *text;		  /* Start of text                  */
    char	       *ioptr;		  /* Current position for i/o       */
    char	       dat[ARDP_PTXT_DSZ];/* The data itself incl headers   */
    unsigned long      mbz;		  /* ZERO to catch runaway strings  */
    struct ptext       *previous;         /* Previous element in list       */
    struct ptext       *next;		  /* Next element in linked list    */
};

typedef struct ptext *PTEXT;
typedef struct ptext PTEXT_ST;
#define NOPKT   ((PTEXT) 0)               /* NULL pointer to ptext          */

/* Used below; must appear before it. */
/* *rr1: this is also used for consistency checking. */
/* Values for this field: */
enum ardp_status {
    ARDP_STATUS_NOSTART = -1, /* not started or inactive */
    ARDP_STATUS_INACTIVE = -1,
    ARDP_STATUS_COMPLETE = -2, /* done */
    ARDP_STATUS_ACTIVE = -3, /* running */
    ARDP_STATUS_ACKPEND = -4,
    ARDP_STATUS_GAPS = -5,
    ARDP_STATUS_ABORTED = -6,
    ARDP_STATUS_FREE = -7, /* used for consistency checking; indicates this
                              RREQ is free. */
    ARDP_STATUS_FAILED = 255
};

/* *rr3: There are several odd numbers you might find. */
/* 0 is the current version of the ARDP library. */
/* -1 is an older version (pre-ARDP).   This is used only on the servers when
   speaking to old archie clients.   There is no ARDP header; the words
   MULTI-PACKET are printed into the packet instead.*/
/* -2 is a newer version than -1, but still old enough that bugs appeared.  
   This is used only on the servers when speaking to V1 archie clients. */
/* Version 1 of the ARDP library has been designed and is being implemented;
   see URL: http://PROSPERO.ISI.EDU/info/ardp. */

enum ardp_version {ARDP_VERSION_NEW_V0 = 0, ARDP_VERSION_1 = 1,
		   ARDP_VERSION_PRE_ARDP = -1, ARDP_VERSION_OLD_V0 = -2};

/* This is for debugging only. */
#define ARDP__DBG_MBZ_SIZE	30

extern const int ardp__mbz[ARDP__DBG_MBZ_SIZE];

#ifdef GL_THREADS
#define ONE_YEAR (3600 * 24 * 365) /* One year in terms of seconds */

/* The following structure is to establish a communication between daemon
   thread and other threads.  The threads inform the daemon by signalling
   daemon's associated condition variable. --Nader Salehi 4/98 */
typedef struct _DAEMON {
    pthread_mutex_t *mutex;	/* Used for guaranteeing mutual exclusion */
    pthread_cond_t *cond;	/* Used for sending a signal to Daemon thread
				 */ 
    char inputReady,		/* These three flags speicify the reason(s) */
	outputReady,		/* for signaling Daemon thread */
	timerUpdate,
	timeout;
} DAEMON;

extern DAEMON daemon;
#endif /* GL_THREADS */

/* Request structure: maintains information about pending requests          */
/* (S): members of this structure used only on SERVERs.
   (C): Members of this structure used only on CLIENTs
   (CS): used on both 
   (H) used only by higher layers, although initialized here.   Might be used
   by the logging library (or any other libraries we make upcalls to).  
   <NB>: Network byte order
   <HB>: Host byte order.
*/
typedef struct rreq *RREQ;
typedef struct rreq RREQ_ST;
struct rreq {
#ifdef GL_THREADS
    pthread_mutex_t	*mutex;		  /*(C)Used in blocking mode */
    pthread_cond_t	*cond;		  /*(C)Used in blocking mode */
    pthread_t		thread_id;	  /*(C)Client's thread ID */
#endif /* GL_THREADS */
    enum ardp_status   	status;		  /*(CS)Status of request     *rr1 */
    int			flags;		  /*(CS)Options for ARDP library 
                                           *rr2*/
    u_int16_t      	cid;		  /*(CS) Connection ID - net byte
                                             order on CLIENT, host byte order
					     on SERVER */ 
    /* Goes across the network: */
    int16_t		priority;	  /*(CS) <HB> Priority */ 
    int			pf_priority;	  /*(S)<HB>Priority assigned by
						pri_func */
    /* This variable does not go across the network. */
    enum ardp_version		peer_ardp_version;/* (CS) Peer ARD protocol version *rr3 */
    struct ptext	*inpkt;		  /* (CS) Packet in process by applic    */
    u_int16_t      	rcvd_tot;	  /* (CS) Total # of packets to receive */
    struct ptext	*rcvd;		  /* (CS) Received packets */
    u_int16_t      	rcvd_thru;	  /* (CS) Received all packets through # */
    struct ptext	*comp_thru;	  /* (CS)Pointer to RCVD_THRUth packet  */
    struct ptext	*outpkt;	  /* (CS)Packets not yet sent
					   */
    u_int16_t      	prcvd_thru;	  /* (CS)Peer's rcvd_thru (host byte ord)*/
    u_int16_t            trns_thru;        /* (CS)Total # of packets already sent.
                                             prcvd_thru <= trns_thru <=
                                             trns_tot  (host byte ord) */
    u_int16_t      	trns_tot;	  /* (CS)Total # of packets for trns    */
    struct ptext	*trns;		  /* (CS)Transmitted packets            */
#ifndef NDEBUG
    int			mbz1[30]; /* Some MBZ space for testing errors. */
#endif
    u_int16_t            window_sz;        /* (CS)My window size (default: 0 (sender
                                             does whatever they choose)
                                             (hostorder) */
    u_int16_t            pwindow_sz;       /* (CS)Peer window (dflt ARDP_WINDOW_SZ).
                                             0 means no limit on # to send. 
                                             (hostorder)*/ 
    struct sockaddr_in	peer;   	  /* (CS)Sender/Destination		    */
#define peer_addr       peer.sin_addr	  /* Address in network byte order  */
#define peer_port       peer.sin_port	  /* Port in network byte order     */
    char *		peer_hostname;    /* (C) Hostname provided by sender
					     from which we got the peer's
					     name. This never has to be filled
					     in.  */ 
#ifdef GL_THREADS
    int			ttwait;		  /* (C)Time to wait in microseconds,
					     only to be used in MT environments
					  */ 
#endif /* GL_THREADS */
    struct timeval	rcvd_time;	  /* (S)Time request was received      */
    struct timeval	svc_start_time;	  /* (S)Time service began on request  */
    struct timeval	svc_comp_time;	  /* (S)Time service was completed     */
    struct timeval	timeout;	  /* (C)Initial time to wait for resp  */
    struct timeval	timeout_adj;      /* (C)Adjusted time to wait for resp */
    struct timeval	wait_till;	  /* (C)Time at which timeout expires  */
    u_int16_t      	retries;	  /* (C)Number of times to retry       */
    u_int16_t      	retries_rem;	  /* (C)Number of retries remaining    */
    u_int16_t      	svc_rwait;	  /* (C)Svc suggested tm2wait b4 retry */
    u_int16_t      	svc_rwait_seq;	  /* (C)Seq # when scv_rwait changed   */
    /* inf_ in the following two items means 'informational' */
    u_int16_t      	inf_queue_pos;    /* (C)Service queue pos if available */
    u_int32_t		inf_sys_time;     /* (C)Expected time til svc complete */
    char		*client_name;     /* (S)Client name string for logging */
    char		*peer_sw_id;	  /* (H) Peer's software specific
                                             ident. This is set and used only
                                             by by a higher protocol level;
                                             under Prospero it is set as an
                                             argument to the VERSION command.
                                             It is also used by the higher
                                             layers to fix the
                                             SERVER_MIGHT_APPEND_NULL_TO_PACKET
                                             bug.  This bug is fixed entirely
                                             in higher layers (in prospero it
                                             appears in lib/pfs), and it was
                                             elimniated in versions of
                                             Prospero after Alpha.5.2a. 

                                             This string is also used for
                                             logging by the Prospero PLOG
					     routines, which are called here
					     through the (*ardp__log)()
					     interface. 
                                             --swa, 7/15/94, 9/96 */

    /* (H) These are unused by the ARDP library.  They are set by higher-level
       layers, and the cfunction is not called within the RDP library.
       No callers of the ARDP library, to the best of my knowledge,
       use this facility.   XXX It is of questionable utility.  --swa 4/14/95
       */ 
    int		        (*cfunction)();   /* (H)Function to call when done  */
    char		*cfunction_args;  /* (H)Additional args to cfunction*/
    /* SECURITY CONTEXT stuff */
    u_int32_t		seclen;	/* current length of sec. context */
    struct ptext	*sec;		  /* (CS) structure to hang security
					     context packets off of until we're
					     committed to sending. */
    struct ardp__sectype *secq;	/* This is the only one USED */
    /* Application-specific pointer or flag.  Used by higher level callers
       to this library.  (H) */
    /* Specifically, this is used by the higher-level Prospero source code
       (in lib/psrv and in server).  You're free to use it for
       what you like, unless you're using ardp as a client of Prospero. */
    union {
        int flg;		/* Flag or number */
        void *ptr;              /* Generic Pointer */
    } app;                      /* app-specific (H) */
    /* Standard list management stuff; at the end of each Prospero structure.
     */ 
    struct rreq		*previous;        /* (CS)Previous element in list    */
    struct rreq		*next;		  /* (CS)Next element in linked list */
#ifndef NDEBUG
    int			mbz2[30]; /* Some MBZ space for testing errors. */
#endif
    /* For development purposes, add new members that are likely to change
       to the end of the RREQ structure.  (This keeps things from breaking in a
       version skew situation).  These are moved up into the middle of the
       structure (more appropriate places?) in the code cleanup phase before a
       real external release.  -- swa, katia, 11/96 */
    /* struct ardp__cos_tag defined in ardp_sec.h */
    struct ardp_class_of_service_tag *class_of_service_tags;
};
/* *rr2: There is only one flag currently defined.  */
/* Send the client's preferred window size along with the next packet you
   transmit.  At the moment, this flag is never cleared once it is set, so all
   packets are tagged with the preferred window size.  There are other
   strategies that will be tried later.  */
/* At the moment, this flag is only set in ardp_rqalloc(). */
enum { ARDP_FLAG_SEND_MY_WINDOW_SIZE = 0x1};
#ifdef PROSPERO
/* This is used only on the clients.  It means they're when speaking to a
   Version 0 Prospero server, but one which used the Prospero 5.0, 5.1, or 5.2
   releases. */
enum { ARDP_FLAG_PEER_HAS_EXCESSIVE_ACK_BUG = 0x2};
#endif


/* *rr3: See above the RREQ def. */

/* Here is the datatype which the ARDP library gives to the gl_parse family of
   functions, ardp_rreq_to_in_aux_INPUT.  */
/* This is the additional argument to  ardp_rreq_to_in();  */
/* It is always allocated on the stack, just as the "INPUT" typedef is. */

struct ardp_rreq_to_in_aux_input {
    RREQ rreq;	/* server or client reading */
    PTEXT inpkt;	/* packet in process.  Null iff no more input. */ 
    char *ptext_ioptr;          /* position of next character from within ptxt.
                                   This will point to a null iff there is no
                                   more input. */

};
typedef struct ardp_rreq_to_in_aux_input *ardp_rreq_to_in_aux_INPUT;
typedef struct ardp_rreq_to_in_aux_input ardp_rreq_to_in_aux_INPUT_st;

#define S_AD_SZ           sizeof(struct sockaddr_in)

#define PEER_PORT(req)  (ntohs((req)->peer_port)) /* host byte order        */

#define NOREQ   ((RREQ) 0)                /* NULL pointer to rreq           */

/* ARDP library error status codes. */
/* These must remain in the range 0-20 for compatibility with the Prospero
   File System. */
/* If you change these, also modify lib/pfs/perrmesg.c */
/* Note that ARDP_SUCCESS must remain 0, due to code implementation. */
enum ardp_errcode {
    ARDP_SUCCESS = 0,		/* Successful completion of call    */
    ARDP_PORT_UNKN = 1,		/* UDP port unknown                 */
    ARDP_UDP_CANT = 2,		/* Can't open local UDP port        */
    ARDP_BAD_HOSTNAME = 3,	/* Can't resolve hostname           */
    ARDP_NOT_SENT = 4,		/* Attempt to send message failed   */
    ARDP_SELECT_FAILED = 5,	/* Select failed	            */
    ARDP_BAD_RECV = 6,		/* Recvfrom failed 	            */
    ARDP_BAD_VERSION = 7,       /* bad version # in rdgram protocol */
    ARDP_BAD_REQ = 8,		/* Inconsistent request structure   */
    ARDP_TIMEOUT = 9,		/* Timed out - retry count exceeded */
    ARDP_REFUSED = 10,		/* Connection refused by server     */
    ARDP_FAILURE = 11,		/* Unspecified ARDP failure         */
    ARDP_TOOLONG = 12,		/* Buffer too long for packet       */
    ARDP_TIMER_FAILED = 13,	/* A call to gettimeofday() failed.
				   (this is ARI's addition; I'm not
				   sure that it's needed --swa) */
    ARDP_CRITICAL_CONTEXT_NOT_SUPPORTED = 14, /* for ardp-v1 contexts */
    ARDP_CRITICAL_SERVICE_FAILED = 15, /* Critical request failed (returned by
					  sender or redeiver)  */
    ARDP_CRITICAL_SERVICE_UNRECIPROCATED = 16, /* Critical request did not get
						  an expected matching
						  response. */
    ARDP_CONTEXT_FAILURE = 17,	/* more general than the CONTEXT ones above;
				   more specific than ARDP_FAILURE (or
				   ARDP_REFUSED, in some situations)  */
};

typedef enum ardp_errcode ardp_errcode;

#ifdef SWA_DO_NOT_USE_ENUMS
#define ARDP_PENDING           -1       /* The request is still pending     */
#define ARDP_WAIT_TILL_TO      -1       /* Wait until timeout occurs        */

#define ARDP_A2R_SPLIT       0x00       /* OK to split across packets       */
#define ARDP_A2R_NOSPLITB    0x01       /* Don't split across packets       */
#define ARDP_A2R_NOSPLITL    0x02       /* Don't split lines across packets */
#define ARDP_A2R_NOSPLITBL   0x03       /* NOSPLITB|NOSPLITL                */
#define ARDP_A2R_TAGLENGTH   0x04       /* Include length tag for buffer    */
#define ARDP_A2R_COMPLETE    0x08       /* This is the final packet to add  */

#define ARDP_R_INCOMPLETE    0x00       /* More messages will follow        */
#define ARDP_R_NOSEND        0x02       /* Add to req->trns but don't send  */
#define ARDP_R_COMPLETE      0x08       /* This is the final packet to send */

/* Queuing priorities for requests */
#define	       ARDP_MAX_PRI   32765  /* Maximum user proiority          */
#define	       ARDP_MAX_SPRI  32767  /* Maximum priority for system use */
#define	       ARDP_MIN_PRI  -32765  /* Maximum user proiority          */
#define	       ARDP_MIN_SPRI -32768  /* Maximum priority for system use */
#else /* SWA_DO_NOT_USE_ENUMS */

enum { ARDP_PENDING= -1,	/* The request is still pending */
       ARDP_WAIT_TILL_TO = -1,	/* Wait until timeout occurs */


       ARDP_A2R_SPLIT = 0x00,	/* OK to split across packets */
       ARDP_A2R_NOSPLITB=    0x01,       /* Don't split across packets       */
       ARDP_A2R_NOSPLITL =   0x02,       /* Don't split lines across packets */
       ARDP_A2R_NOSPLITBL =   0x03, /* NOSPLITB|NOSPLITL */
       ARDP_A2R_TAGLENGTH =   0x04, /* Include length tag for buffer    */
       ARDP_A2R_COMPLETE =    0x08, /* This is the final packet to add  */
       
       ARDP_R_INCOMPLETE =    0x00, /* More messages will follow        */
       ARDP_R_NOSEND=        0x02, /* Add to req->trns but don't send  */
       ARDP_R_COMPLETE =      0x08, /* This is the final packet to send */
       /* ***Queuing priorities for requests ****/
       ARDP_MAX_PRI=   32765,  /* Maximum user proiority          */
       ARDP_MAX_SPRI=  32767,  /* Maximum priority for system use */
       ARDP_MIN_PRI=  -32765,  /* Maximum user proiority          */
       ARDP_MIN_SPRI = -32768,  /* Maximum priority for system use */
};
#endif

/* Constant values for for (1) information returned by
 * ardp_get_nxt_all_timeout() and by ardp_get_nxt_all().
 *  (2) arguments passed to ardp_get_nxt_all_timeout()
 *  (3) potential argument to ardp_get_nxt_all()
 */
enum ardp_gna_rettype {
    ARDP_CLIENT_PORT = 0,
    ARDP_RESPONSE = ARDP_CLIENT_PORT,
    ARDP_REPLY = ARDP_CLIENT_PORT,
    ARDP_SERVER_PORT = 1,
    ARDP_NEW_REQUEST = ARDP_SERVER_PORT,
    ARDP_GNA_SELECT_FAILED = ARDP_SELECT_FAILED,
    ARDP_GNA_TIMEOUT = ARDP_TIMEOUT
};



/* LONG_TO_SHORT_NAME is needed for linkers that can't handle long names */
/* This hasn't been tested recently; we don't have easy access to a platform
   which requires it.  If you do, we'll work with you to resolve any problems
   that occur; please pass patches back.  --katia & swa, 5/7/96 */

#ifdef LONG_TO_SHORT_NAME
/* Please keep this list alphabetically sorted. */
#define ardp_abort                RDABOR
#define ardp_abort_on_int         RDABOI
#define ardp_accept               RDACPT
#define ardp_accept_and_wait      RDACAW

#define ardp_acknowledge          RDACKN
#define ardp_activeQ              RDACTV
#define ardp_add2req		  RDA2RQ
#define ardp_bind_port            RDBPRT
#define ardp_breply               RDBREP
#define ardp_completeQ            RDCMPQ
#define ardp_def_port_no          RDDPNO
#define ardp_default_retry        RDDFRT
#define ardp_default_timeout      RDDFTO
#define ardp_doneQ                RDDONQ
#define ardp_get_nxt              RDGNXT
#define ardp_get_nxt_all          RDGNAL
#define ardp_get_nxt_nonblocking  RDGNNB
#define ardp_headers              RDHDRS
#define ardp_init	          RDINIT
#define ardp_initialize           RDINLZ
#define ardp_next_cid	          RDNCID
#define ardp_partialQ             RDPRTQ
#define ardp_pendingQ             RDPNDQ
#define ardp_port	          RDPORT
#define ardp_pri_func             RDPRIF
#define ardp_pri_override         RDOVRD
#define ardp_priority             RDPRIO
#define ardp_process_active       RDPACT
#define ardp_prvport              RDPPRT
#define ardp_ptalloc              RDPTAL
#define ardp_ptfree               RDPTFR
#define ardp_ptlfree              RDPTLF
#define ardp_redirect             RDREDR
#define ardp_refuse               RDRFSE
#define ardp_reply                RDREPL
#define ardp_respond              RDRESP
#define ardp_retrieve             RDRETR
#define ardp_retrieve_nxt         RDRETN
#define ardp_retrieve_nxt_nonblocking RDRNNB
#define ardp_rqalloc              RDRQAL
#define ardp_rqappfree            RDRQAF
#define ardp_rqfree               RDRQFR
#define ardp_rq_partialfree       RDRQPF
#define ardp_rqlfree              RDRQLF
#define ardp_runQ                 RDRUNQ
#define ardp_rwait                RDRWAI
#define ardp_send                 RDSEND
#define ardp_set_prvport          RDSPPT
#define ardp_set_queuing_policy   RDSQPL
#define ardp_set_retry            RDSETR
#define ardp_showbuf              RDSHBF
#define ardp_snd_pkt              RDSPKT
#define ardp_srvport              RDSPRT
#define ardp_trap_int             RDTINT
#define ardp_update_cfields       RDUPCF
#define ardp_xmit                 RDXMIT
#define ptext_count               PTXCNT
#define ptext_max                 PTXMAX
#define rreq_count                RRQCNT
#define rreq_max                  RRQMAX
#endif /* LONG_TO_SHORT_NAME */

/* unalphabetized interfaces XXX fix later */

/* Info about active requests (CLIENT) */
EXTERN_MUTEXED_DECL(RREQ, ardp_activeQ);
extern int             ardp_activeQ_len;   /* Length of ardp_activeQ     */
extern int	       ardp_port;    /* file descriptor of CLIENT's Opened UDP
					port -- unix file 
					descriptor # (NOT UDP/IP port #) */
/* The address and port for the UDP port bound to file descriptor
   ARDP_PORT. */  
extern struct sockaddr_in	      ardp_client_address_port;

extern int         ardp_srvport;      /* file descriptor of SERVER
					 non-privileged port -- file descriptor
					 */ 
extern int         ardp_prvport;      /* file descriptor of SERVER privileged
					 port */ 

extern int	       ardp_debug; /* (CLIENT) Debug level. This actually ends
				      up showing information on the server,
				      too, if you have stdout going somewhere
				      you can read. */

/* Info about completed reqs.  (CLIENT) */
EXTERN_MUTEXED_DECL(RREQ, ardp_completeQ);

/* Interfaces to ARDP library */
/* Please keep this list alphabetically sorted. */


enum ardp_errcode   ardp_abort(RREQ);
enum ardp_errcode   ardp_abort_on_int(void);
enum ardp_errcode   ardp_accept(void);
enum ardp_errcode   ardp_accept_and_wait(int timeout, int usec);
enum ardp_errcode   ardp_add2req(RREQ,int,const char*,int);
enum ardp_errcode   ardp_acknowledge(RREQ req);
int		ardp_bind_port(const char*);
enum ardp_errcode   ardp_breply(RREQ, int, const char*, int);
/* Internal */
void		ardp__bwrite_cid(u_int16_t       newcid, PTEXT ptmp);
extern int      ardp_debug;
extern const char	*ardp_err_text[]; /* Error messages */
RREQ		ardp_get_nxt(void);
RREQ            ardp_get_nxt_all(enum ardp_gna_rettype *);
RREQ            ardp_get_nxt_all_timeout(enum ardp_gna_rettype *, int, int);
RREQ		ardp_get_nxt_nonblocking(void);
enum ardp_errcode   ardp_headers(RREQ);
/* Variable name "hostname" Commented out to shut up GCC -Wshadow */
enum ardp_errcode   ardp_hostname2addr(const char * /* hostname */, struct sockaddr_in *hostaddr);
void            ardp_hostname2addr_initcache(void);
enum ardp_errcode   ardp_hostname2name_addr(const char *hostname_arg, char **official_hnameGSP, struct sockaddr_in *hostaddr_arg);

enum ardp_errcode ardp_init(void);
void            ardp_initialize(void);
extern void     ardp__log(int, RREQ, const char *, ...);
struct timeval	ardp__next_activeQ_timeout(const struct timeval now);
void ardp__next_cid_initialize_if_not_already(void);
extern void     (*ardp_newly_received_additional)(RREQ nreq);
extern int	(*ardp_pri_func)(); /* Function to compare priorities */
extern int	ardp_pri_override; /* If 1, overide value in request */
extern int	ardp_priority;	/* default priority for outgoing requests. */
extern enum ardp_errcode	ardp_process_active(void); 
extern int      ardp_prvport; /* retrying a request */
PTEXT		ardp_ptalloc(void);
void		ardp_ptfree(PTEXT);
void		ardp_ptlfree(PTEXT);
extern enum ardp_errcode		ardp_refuse(RREQ req);
extern enum ardp_errcode		ardp_redirect(RREQ req, struct sockaddr_in *target);
extern enum ardp_errcode		ardp_reply(RREQ,int,const char *);
extern enum ardp_errcode		ardp_respond(RREQ,int);
extern enum ardp_errcode		ardp_retrieve(RREQ,int);
RREQ		ardp_retrieve_nxt(int);
RREQ            ardp_retrieve_nxt_nonblocking(void); /* ari did this --swa */
RREQ		ardp_rqalloc(void);
void            ardp_rqappalloc(void * (* appallocfunc)(void));
void            ardp_rqappfree(void (* appfreefunc)(void *));
void		ardp_rqfree(RREQ);
#ifndef NDEBUG
void		ardp_rq_partialfree(RREQ);
#endif
void		ardp_rqlfree(RREQ);
void	    	ardp_rreq_to_in(RREQ rreq, INPUT in, ardp_rreq_to_in_aux_INPUT in_aux);

extern enum ardp_errcode		ardp_rwait(RREQ,int,u_int16_t,u_int32_t);
extern enum ardp_errcode		ardp_send(RREQ, const char *, struct sockaddr_in*,int);
extern enum ardp_errcode		ardp_set_prvport(int);
extern enum ardp_errcode		ardp_set_queuing_policy(int(*pf)(),int);
extern enum ardp_errcode		ardp_set_retry(int,int);
extern void ardp_showbuf(const unsigned char * /* st */, int /* length */,
			     FILE */* out */);
extern enum ardp_errcode		ardp_snd_pkt(PTEXT,RREQ);
extern int         ardp_srvport;      /* server ports, in case client is */
/* This is an external interface */
extern void (*ardp_vlog)(int, RREQ, const char *, va_list);
extern enum ardp_errcode		ardp_version_not_supported_by_server(RREQ creq);
extern enum ardp_errcode		ardp_xmit(RREQ,int);

#ifdef GL_THREADS
extern pthread_t daemon_id;
extern void ardp_init_daemon(void);
extern enum ardp_errcode ardp_thr_retrieve(RREQ req, int ttwait_arg);
extern void *daemon_thread(void *args);
extern void *select_thread(void *args);
extern void *retrieval_thread(void *args);
RREQ first_timeout(void);
#endif /* GL_THREADS */

/** NEW LIST: INTERNAL or STATUS CHECKING interfaces to ARDP library. */

/* these are used to look for memory leaks.   Currently used by dirsrv.c to
   return STATUS information.  Internal to ARDP library.  */
extern int dnscache_count;     
extern int dnscache_max;
/* Used to see how many cached items are in */
extern int alldnscache_count;

extern int	   pfs_debug;         /* Debug level                */
/* these are used to look for memory leaks.   Currently used by dirsrv.c to
   return STATUS information. */
extern int ptext_count;         
extern int ptext_max;
extern int rreq_count;
extern int rreq_max;
/* Check for case-blind string equality, where s1 or s2 may be null
   pointer. */
extern int stcaseequal(const char *s1, const char*s2);

/* These three are more in the class of utility functions, rather than
   ARDP-specific.  They are used by the ARDP library internally, however. */
extern const char*unixerrstr(void);
extern u_int32_t myaddress(void);
extern const char *myhostname(void);


/* Mutex stuff for gl_threads on server side only still.. */
extern void ardp_init_mutexes(void); /* need not be called. */
#if !defined(NDEBUG)
extern void ardp__diagnose_mutexes(void); /* need not be called. */
#endif /*NDEBUG*/
EXTERN_MUTEXED_DECL(RREQ, ardp_runQ);
EXTERN_MUTEXED_DECL(RREQ, ardp_doneQ);
extern int	dQlen;          /* mutexed with ardp_doneQ */
extern int	dQmaxlen;
/* XXX This should probably be moved to the config. structure. --swa */
extern int	ardp_clear_doneQ_loginfo; /* User option.  Clear log info from
					     the doneQ. In Prospero, set by the
					     -n option to dirsrv.  Examined
					     ardp_respond().*/   
EXTERN_MUTEXED_DECL(RREQ, ardp_pendingQ);
/* These two fall under the ardp_pendingQ mutex; don't need their own. */
extern int		pQlen;	/* Length of pending queue */
extern int		pQNlen;	/* Number of niced requests */
#ifdef GL_THREADS
extern pthread_mutex_t p_th_mutexARDP_ACCEPT; /* declared in ardp_mutexes.c */
#ifndef ARDP_NO_SECURITY_CONTEXT
#if defined(ARDP_SEC_HAVE_LABELS_CLASS_OF_SERVICE)
extern pthread_mutex_t p_th_mutexARDP_COS; /* declaration */
#endif /* defined(ARDP_SEC_HAVE_LABELS_CLASS_OF_SERVICE) */
extern pthread_mutex_t p_th_mutexARDP_SECTYPE; /* declaration */
#endif /* ndef ARDP_NO_SECURITY_CONTEXT */
extern pthread_mutex_t p_th_mutexPTEXT; /* declared in ardp_mutexes.c */
extern pthread_mutex_t p_th_mutexARDP_RQALLOC; /* declared in ardp_mutexes.c */
extern pthread_mutex_t p_th_mutexARDP_SELFNUM; /* declared in ardp_mutexes.c */
#endif

/* The internal error handling code that was moved from pfs.h to ardp.h has now
   been moved again to gl_internal_err.h, in the GOSTLIB area. */
#include <gl_internal_err.h>


/* XXX There appears to be a bug in GCC 2.7.2. under Solaris 2.5.1, such that
   the #define below is necessary.  There should, of course, be no
   difference. 
   A simple test (see ardp/devtools/test_ardp_memcpy2.c) did not reveal any problem.  
   However, ARDP clients failed spectacularly for us when we used the (const
   char *) cast version.  Look at lib/ardp/ardp_headers.c for where it fails.
   */

#define APPARENT_BUG_IN_CAST_TO_CONST_CHAR_STAR
#ifdef  APPARENT_BUG_IN_CAST_TO_CONST_CHAR_STAR
/* Some short macros that really hike the efficiency of this code. */
/* Like memcpy( b,a, 1), but much faster. */
#define memcpy1(b, a) do {                       \
    ((char *) (b))[0] = ((char *) (a))[0];      \
} while(0)                              

#define memcpy2(b, a) do {                        \
    memcpy1(b, a);                                 \
    /* Next line depends on unary cast having higher precedence than \
       addition.   (Guaranteed to be true.) */ \
    memcpy1((char *) (b) + 1, (char *) (a) + 1);   \
} while(0)

#define memcpy4(b, a) do {   \
    memcpy2((b), (a));       \
    memcpy2((char *) (b) + 2, (char *) (a) + 2);\
} while(0)

#else /* APPARENT_BUG_IN_CAST_TO_CONST_CHAR_STAR */
/* Some short macros that really hike the efficiency of this code. */
/* Like memcpy( b,a, 1), but much faster. */
#define memcpy1(b, a) do {                       \
    ((char *) (b))[0] = ((const char *) (a))[0];      \
} while(0)                              

#define memcpy2(b, a) do {                        \
    memcpy1(b, a);                                 \
    /* Next line depends on unary cast having higher precedence than \
       addition.   (Guaranteed to be true.) */ \
    memcpy1((char *) (b) + 1, (const char *) (a) + 1);   \
} while(0)

#define memcpy4(b, a) do {   \
    memcpy2((b), (a));       \
    memcpy2((char *) (b) + 2, (const char *) (a) + 2);\
} while(0)
#endif /* APPARENT_BUG_IN_CAST_TO_CONST_CHAR_STAR */

#define bzero1(a) do {                  \
    ((char *) (a))[0] = '\0';           \
} while(0)

#define bzero2(a) do {                  \
    bzero1(a);                          \
    bzero1((char *) (a) + 1);           \
} while (0)

#define bzero3(a) do {              \
    bzero2(a);                      \
    bzero1((char *) (a) + 2);       \
} while(0)

#define bzero4(a) do {              \
    bzero2(a);                      \
    bzero2((char *) (a) + 2);       \
} while(0)

/* Configuration routines. */
/* These will go into gostlib when it's created.. */


/*
 * Definition of the ardp configuration data structure
 *
 * The configuration structure should include all the configurable
 * items below. So remember to make corresponding changes if you
 * add or delete any of them AND make changes to ardp_config_table.
 *
 * The original #DEFINE items are redefined and point to individual 
 * fields in this structure. A _DEFAULT definition is added to define 
 * compile time defaults.
 */

/* Ardp Configuration Structure */
 /* Documented in doc/working_notes/pconfig* */
extern struct ardp_config_type {
    char                        *default_peer;
    int				default_port;
    int				default_timeout;
    int				default_retry;
    int                         default_window_sz;
    int                         max_window_sz;
    int                         first_privp;
    int				num_privp;
    int                                my_receive_window_size;
    int                                preferred_ardp_version;
    int                                wait_time;
    int                                client_request_queue_status;
    char                       *server_redirect_to; /* string incl. optional
                                                       portnum */ 

    /* Server's principal name in a security system such as Kerberos or X-509.
	This will need to be defined differently in the rare case that
	you are running two different security systems at the same time. */
    /* 9/97: In Kerberos, this service name helps form the principal that the
       Kerberos client will request a ticket for.  For instance, for the
       Kerberos principal:
	nari.isi.edu/sample@ISI.EDU
       this will actually be the string 'sample'.  So this should probably be
       renamed, whenever we come up with a better one -- suggestions
       welcome. */ 
    /* Right now, to request authentication for a different principal, you have
       to reset this value to a different string before calling
       ardp_req_security().  This can be done dynamically in a program.  We
       could change this by adding options to ardp_req_security() when called
       for Kerberos, and will do so if there is demand. */
    char			*server_principal_name;
    /* This is the location of the Kerberos SRVTAB file.  It is only used on
       the server.  */
    /* The comment above about changing server_principal_name applies to
       changing kerberos_srvtab too. */
    char			*kerberos_srvtab;
} ardp_config;


/* These set defaults used in the structure above. */

/* We currently do not set any default PEER or PORT for the ARDP library.  We
   prefer to have these items set dynamically through the ARDP configuration
   table (ardp_config).  This allows the ARDP library to be used by several
   different callers. */
/* However, if you wish to set a particular default, one of the definitions
   below is what you'd like. */
#if 0
#ifdef PROSPERO
#define	     ARDP_DEFAULT_PEER_DEFAULT	 "dirsrv"
                                      /* (C)Default destination port name */
#define	     ARDP_DEFAULT_PORT_DEFAULT    1525   
                                      /* (C)Default destination port number */
#endif /* PROSPERO */
#ifdef PRM
#define ARDP_DEFAULT_PEER_DEFAULT "prm-nm"
#define ARDP_DEFAULT_PORT_DEFAULT 409
#endif /* PRM */
#ifdef NETCHEQUE_ACC            /* new #define I just made up --swa */
#define ARDP_DEFAULT_PEER_DEFAULT "accsrv"
#define ARDP_DEFAULT_PORT_DEFAULT 4008
#endif
#endif /* 0 -- examples */

/* Default is unset. */
#ifndef ARDP_DEFAULT_PEER_DEFAULT
#define ARDP_DEFAULT_PEER_DEFAULT (char *) NULL
#define ARDP_DEFAULT_PORT_DEFAULT 0
#endif

#define      ARDP_DEFAULT_TIMEOUT_DEFAULT    4
                                      /* (C)Default time before retry (sec) */
#define      ARDP_DEFAULT_RETRY_DEFAULT	     3
                                      /* (C)Default number of times to try  */

#define	     ARDP_DEFAULT_WINDOW_SZ_DEFAULT 16
                                      /* (CS)Default maximum packets to send at
                                            once, unless special request
                                            received from client. */
#define      ARDP_MAX_WINDOW_SZ_DEFAULT    256   
                                      /* (CS)Maximum # of packets we'll send
                                            out, no matter what a client
                                            requests.   Might be redefined
                                            locally if you're at the far end of
                                            a slow link. */

#define       ARDP_FIRST_PRIVP_DEFAULT     901
                                      /* (C)First prived local port to try  */
#define      ARDP_NUM_PRIVP_DEFAULT         20
                                      /* (C)Number of prived ports to try   */
/* These are temporary internals; we will yank that out later.
   This is for backwards compatibility with an older ARDP library user that 
   generates singly-linked lists instead of doubly-linked. */
/* This function turns singly-linked lists into doubly. */
#define ARDP___CANONICALIZE_OUTPKT
#ifdef  ARDP___CANONICALIZE_OUTPKT
/* We'll try a macro implementation */
/* extern void ardp___canonicalize_ptexts(PTEXT pt); */
#define ardp___canonicalize_ptexts(pt) do {			    \
    if ((pt) && !((pt)->next) && !((pt)->previous))		    \
	pt->previous = pt;					    \
} while(0)
#endif

#endif /* not ARDP_H_INCLUDED */

