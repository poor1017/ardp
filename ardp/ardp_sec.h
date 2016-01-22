#ifndef ARDP__SEC_H_INCLUDED
#define ARDP__SEC_H_INCLUDED
#ifndef ARDP_NO_SECURITY_CONTEXT
/*
 * Copyright (c) 1991-1998 by the University of Southern California
 *
 * For copying and distribution information, please see the file
 * <usc-license.h>.
 */

/* This header file contains interfaces to the ARDP library Security Context
   routines and data types and definitions. */

#include <usc-license.h>
#include <ardp_sec_config.h>	/* Configuration variables for ARDP.  Not
				   needed outside ARDP, usually. */

#include <stdarg.h>

/* Various defaults that might have been set in ardp_sec_config.h.  If they
   weren't, we make reasonable assumptions. */

#ifndef ARDP_SERVER_PRINCIPAL_NAME_DEFAULT
#define ARDP_SERVER_PRINCIPAL_NAME_DEFAULT "sample"
#endif

#ifndef ARDP_KERBEROS_SRVTAB_DEFAULT
/* This is the V5 srvtab default. */
#define	    ARDP_KERBEROS_SRVTAB_DEFAULT "/etc/krb5.srvtab"
#endif
/* Here is the definition of the ardp_sectype object, and its associated
   enumerated types. */

enum ardp__context {
    ARDP_SECURITY = 1,
    ARDP_NAMING = 2,
    ARDP_MESSAGE = 3
};


enum ardp__sec_service {
    ARDP_SEC_SERVICE_UNDEFINED = 0,
    ARDP_SEC_PAYMENT = 1,
    ARDP_SEC_INTEGRITY = 2,
    ARDP_SEC_AUTHENTICATION = 3,
    ARDP_SEC_PRIVACY = 4,
    /* It is not clear to us that CLASS_OF_SERVICE really
       should be a security mechanism.  However, pending
       discussion and consensus, we'll put this here
       anyway.   -- katia & steve, 11/96 */
    ARDP_SEC_LABELS = 5 ,
};



/* The mechanisms all are one byte long (this is defined in the protocol) */

/* No payment mechanisms implemented. Yet. */
enum ardp_sec_payment_mechanism {
    ARDP_SEC_PAYMENT_UNDEFINED = 0x00 /* Dummy filler. */
};

/* No authentication needed has high-nibble of 0 */
/* CRC checksums have a low-nibble of 1 */
/* Kerberos authentication has high-nibble of 2;
   Kerberos integrity has low-nibble of 2 */
/* INTEGRITY mechanism: */

enum ardp__sec_integrity_mechanism {
    ARDP_SEC_INTEGRITY_UNDEFINED = 0x00,
    ARDP_SEC_INTEGRITY_CRC = 0x01,
    ARDP_SEC_INTEGRITY_KERBEROS = 0x22
};


enum ardp__sec_authentication_mechanism {
    ARDP_SEC_AUTHENTICATION_UNDEFINED = 0x00,
    ARDP_SEC_AUTHENTICATION_KERBEROS = 1,
    ARDP_SEC_AUTHENTICATION_ASRTHOST = 2
};


enum ardp__sec_privacy_mechanism {
    ARDP_SEC_PRIVACY_UNDEFINED = 0x00,
    ARDP_SEC_PRIVACY_ROT13 = 0x01, /* this is a silly mechanism for debugging
				      and as a sample implementation. */
    ARDP_SEC_PRIVACY_KERBEROS = 0x22
};


/* CLASS_OF_SERVICE mechanism -- only one such mechanism (right now) */
enum ardp__sec_labels_mechanism {
    ARDP_SEC_LABELS_UNDEFINED = 0,
    ARDP_SEC_LABELS_CLASS_OF_SERVICE = 1,
};



#define ARDP__SEC_NUM_MECH_SPEC	5	/* for starters */

/* We developed this structure assuming that we would need to do call-backs.
   this degree of work is not necessary if we defer the processing of the
   security context until we have all the data. */

typedef struct ardp__sectype {
    enum p__consistency consistency;
    enum ardp__sec_service service;
    int mechanism;		/* one of several types of enum */
    /* ardp_req_security() freshly allocates an ardp_sectype and passes it to
       the sectype's parse_arguments function. */
    enum ardp_errcode (*parse_arguments)(RREQ, struct ardp__sectype *,
					va_list ap);
    /* Free all the mech_spec members of this structure, when it is a
       dynamically-allocated structure.  This is part of the mechanism.
       I'd recommend that this (freeing) function set those members to zero
       too, just for good luck. */
    void (*mech_spec_free)(struct ardp__sectype *);
    enum ardp_errcode (*commit)(RREQ, struct ardp__sectype*);
    /* The processing_state field is staticly initialized and later reset by
       ardp_sec_commit(), do_security_context(), and other functions. See *as1
       */
    /* 1/98: I've removed all of the unused states now.   These states are all
       used; none are RFU --swa */
    /* IF you add a state, must also modify ardp/processing_state_str.c */
    enum { ARDP__SEC_NONE = 0,
	   ARDP__SEC_JUST_ALLOCATED,
	   ARDP__SEC_STATIC_CONSTANT,
	   ARDP__SEC_PREP_SUCCESS,
	   ARDP__SEC_PREP_FAILURE,
	   ARDP__SEC_COMMITTMENT_SUCCESS,
  	   ARDP__SEC_COMMITTMENT_FAILURE,
	   ARDP__SEC_RECEIVED,
	   ARDP__SEC_IN_PROCESS,
	   ARDP__SEC_IN_PROCESS_DEFERRED,
	   ARDP__SEC_PROCESSED_SUCCESS,
	   ARDP__SEC_PROCESSED_FAILURE,
    } processing_state;

    /* **************************************** */

    /* These fields are all set by do_security_context() (called ardp_accept()).  */
    /* We use these fields only when dynamically allocating these
       structures on the peer_requests_sec member of the RREQ structure. */
    int criticality;		/* flag sent across network with this request
				 */
    char *args;			/* arguments given to us.  Only used on receipt
				 */
    /* INDEPENDENT; used on server in dynam. alloc. structs */
    int me;			/* I should provide this. */
    /* These two used to be a single member called 'you'. */
    int requesting_from_peer;	/* (C) Client sets this */
    int requested_by_peer;	/* (S) Server sets this */
    int privacy;		/* privacy requested */
    enum ardp_errcode error_code; /* only looked at if a FAILURE state.  */
    /* INDEPENDENT: Each mechanism may use these fields as it wishes.  This is
       useful so that you can extend this to other types of authentication
       without needing to recompile the rest of ARDP; thus, it is possible to
       link in new modules for new auth. types, without changing this
       structure. */
    /* If you need more than five of them, you'll have to allocate more memory
       on your own and make this point to such a structure. */
    union {
        int flg;		/* Flag or number */
        void *ptr;              /* Generic Pointer */
    } mech_spec[ARDP__SEC_NUM_MECH_SPEC]; /* mechanism-specific */

    /* KERBEROS specific */
    /* XXX This is not as clean as we would like; kerberos's special stuff is
       different from that which other methods might use. */
    /* We use the 'struct' versions so we don't have to include <krb5.h>
       unless we are actually using Kerberos. */
    struct _krb5_ticket *ticket; /* krb5_ticket *ticket; */
    struct _krb5_auth_context *auth_context; /* krb5_auth_context auth_context; */
    struct _krb5_context *k5context; /* This is irrelevant, since the
				      context now represents per-process
				      state.  This should probably be unused,
				      and should certainly never be freed. */
    u_int32_t seq;		/* Sequence number. */
    struct ardp__sectype *previous;
    struct ardp__sectype *next;
    int peer_set_me_bit;	/* (CS) Did the peer set the ME bit on a
				   request?  This is used inside routines to
				   test whether any verification/acceptance
				   work actually needs to be done. */
    PTEXT pkt_context_started_in;
    int reject_if_peer_does_not_reciprocate;  /* Reject this message if the
						 peer does not reciprocate by
						 honoring the YOU bit we send
						 it.   It does not make sense
						 to set this if the
						 requesting_from_peer member is
						 unset. */
    struct ardp__sectype *mate; /* The original mesg to which this is a
				       response.  Setting this only makes sense
				       on the client side, and only if this
				       message was a received response
				   OR, the returned message for which this
				       was the original.  Setting this only
				       makes sense on the client side and only
				       if this message was a successfully sent
				       original. */
    /* This may track down some memory problems */
    int mbz[ARDP__DBG_MBZ_SIZE];
} ardp_sectype;

/* Note *as1:
   Here is a state diagram of the life of the processing_state, for any
   particular allocated ardp_sectype structure.

   Terminal states are marked with (T).  Note that, in the case of an error
	causing full processing to be aborted, most states may be terminal.
   Transitions that only the Client makes are marked with (C)
   Transitions that only the server makes are marked with (S)
   Start nodes are prefixed with (s)

   (s) NONE -----------------------+
				   |
				   |
				   |
				   |
				   v
   (s) STATIC_CONSTANT(T)--> JUST_ALLOCATED
				   |
+---------------<-------------<--  |
|       			 \ |
|       			  \v
v  PREP_SUCCESS <--------<---------+-----------> PREP_FAILURE (T)
|       |			   ^
|       |                          |
|       |                          +----<---------<---------<-----+
|       |							  |
|       v							  ^
|       +------> COMMITTMENT_FAILURE(T)                           |
|       |                                                         |
|       V                                                         ^
|  COMMITTMENT_SUCCESS (T)                                        |
|       							  |
|               						 (S)
|                                               		 (E)
|               			        		 (R)
+-->---> RECEIVED --> IN_PROCESS ---> IN_PROCESS_DEFERRED        (V)
			|                     |                  (E)
			v                     v 		 (R)
		        +-<---------------<---+ 		  |
			v                       		  ^
			+--> PROCESSED_FAILURE (T)                |
			|                                         |
			v                                         |
		   PROCESSED_SUCCESS (T)                          ^
		        |					  |
			v                                         |
			+-------(SERVER)---->--------------->-----+


*/



#define \
ardp_sectype_free_members(st) do {	\
    stfree(st.args);			\
    if (st.mech_spec_free)		\
	st.mech_spec_free(st);		\
    /* at some point, will check freeing on the auth context and k5context */ \
} while(0)

extern ardp_sectype *ardp_sealloc(void);
extern ardp_sectype * ardp_secopy(const ardp_sectype * orig);
extern void ardp_sefree(ardp_sectype * se);
extern void ardp_selfree(ardp_sectype * se);



/* ################ */
/* Security mechanisms */
/* ################ */

/* XXX We will probably later move the ARDP_SEC_HAVE_xxx macro definitions to a
   configuration file of some sort where we define what methods to include.  */

#ifdef ARDP_SEC_HAVE_AUTHENTICATION_ASRTHOST
extern const ardp_sectype	ardp_sec_authentication_asrthost;
#endif

#ifdef ARDP_SEC_HAVE_KERBEROS

#ifdef ARDP_SEC_HAVE_AUTHENTICATION_KERBEROS
extern const ardp_sectype	ardp_sec_authentication_kerberos;
#endif

#ifdef ARDP_SEC_HAVE_INTEGRITY_KERBEROS
extern const ardp_sectype	ardp_sec_integrity_kerberos;
#endif /* def ARDP_SEC_HAVE_INTEGRITY_KERBEROS */

#endif /* ARDP_SEC_HAVE_KERBEROS */


#ifdef ARDP_SEC_HAVE_INTEGRITY_CRC
extern const ardp_sectype	 ardp_sec_integrity_crc;
extern enum ardp_errcode  verify_message_checksum(
    PTEXT rcvd, ardp_sectype *secref, const char *arg, int arglen);

#endif /* def ARDP_SEC_HAVE_INTEGRITY_CRC */


#if 0
extern const ardp_sectype	payment_netcheque;
extern const ardp_sectype	payment_netcash;
#endif

#ifdef ARDP_SEC_HAVE_LABELS_CLASS_OF_SERVICE
extern ardp_sectype	ardp_sec_labels_class_of_service;
#endif


#ifdef ARDP_SEC_HAVE_KERBEROS
#ifdef ARDP_SEC_HAVE_PRIVACY_KERBEROS
extern const ardp_sectype	ardp_sec_privacy_kerberos;
#endif
#endif


/* Only include KRB5.H if we have support for Kerberos built in. */
#if defined(ARDP_SEC_HAVE_KERBEROS)
#include <krb5.h>

/* There is only one Kerberos context needed for each process. */
EXTERN_MUTEXED_DECL(krb5_context,ardp__sec_k5context);
#define SERVER_SERVICE_NAME (ardp_config.server_principal_name)
#define SERVER_KEYTAB_FILE (ardp_config.kerberos_srvtab)
#endif



/** FUNCTION PROTOTYPES **/
/* EXTERNAL INTERFACE */
/* ardp_req_security should be called after the message text is all there. */
extern enum ardp_errcode
ardp_req_security(RREQ req,
		  const ardp_sectype typ,
		  ardp_sectype **ref,
		  ...);

/* #define ARDP__SEC_ARDP__IOCTL_T_CONST_POINTERS_TO_STRUCTS */
/* #define ARDP__SEC_ARDP__IOCTL_T_ENUM */
#define ARDP__SEC_ARDP__IOCTL_T_ENUM

#ifdef ARDP__SEC_ARDP__IOCTL_T_CONST_POINTERS_TO_STRUCTS

typedef struct ardp__ioctl_t {
    int value;
} *ardp_ioctl_t;

extern const
ardp_ioctl_t ARDP_SEC_CRITICALITY, ARDP_SEC_ME, ARDP_SEC_YOU, ARDP_SEC_PRIVACY_REQUESTED;
#endif

#ifdef ARDP__SEC_ARDP__IOCTL_T_ENUM
typedef enum {
    ARDP_SEC_CRITICALITY,
    ARDP_SEC_ME,
    ARDP_SEC_YOU,
    ARDP_SEC_PRIVACY_REQUIRED,
    ARDP_SEC_RECIPROCATION_REQUIRED,
} ardp_ioctl_t;
#endif




extern enum ardp_errcode
ardp_sectype_ioctl(ardp_sectype *ref, ardp_ioctl_t ioctlname,
		   /* IOCTLs may take arguments. */
		   ...);

const char *ardp_sec_processing_state_str(const ardp_sectype *sp);
const char *ardp_sec_mechanism_name(const ardp_sectype *sp);

extern enum ardp_errcode ardp_sec_commit(RREQ req);

/** INTERNAL INTERFACES */
extern void ardp__sec_add2secdata4req(RREQ req, int flags, const void *buf, int buflen);

struct ardp__sec_mark_position {
    RREQ req;			/* Needed only for sanity checks. */
    PTEXT pkt;
    char *old_ioptr;
};
struct ardp__sec_mark_position ardp__sec_mark_position(RREQ req);
void ardp__sec_restore_position(RREQ req, struct ardp__sec_mark_position posit);

extern enum ardp_errcode
ardp__sec_dispatch_service_receiver(
    RREQ req,
    ardp_sectype *newctxt,
    PTEXT pkt_context_started_in,
    enum ardp__sec_service sec_service, int sec_mechanism,
    char *arg, int arglen);

extern enum ardp_errcode
ardp__sec_reject_failed_ardp_context(
    RREQ req, char service, char mechanism, enum ardp_errcode errcode);
extern const ardp_sectype *
ardp__sec_look_up_service(enum ardp__sec_service sec_service,
		unsigned char sec_mechanism);

extern enum ardp_errcode
ardp__sec_process_contexts(RREQ req, ardp_sectype **failed_ctxtp);

enum ardp_errcode
ardp__sec_server_process_received_security_contexts(RREQ creq);

/* #################### */
#ifdef ARDP_SEC_HAVE_LABELS_CLASS_OF_SERVICE
/* The class of service tags are a linked list of attribute/value pairs.
   The values are always strings. They hang off of the RREQ structure, as they
   are parsed. */
typedef struct ardp_class_of_service_tag {
    enum p__consistency consistency;
    char *tagname;
    char *value;
    struct ardp_class_of_service_tag *previous;
    struct ardp_class_of_service_tag *next;
} ardp_label_class_of_service;
typedef ardp_label_class_of_service ardp_label_cos;

extern int ardp_cos_count;
extern int ardp_cos_max;
extern struct ardp_class_of_service_tag * ardp_cos_alloc(void);
extern void ardp_cos_free(struct ardp_class_of_service_tag *);
extern void ardp_cos_free(struct ardp_class_of_service_tag *);
extern void ardp_cos_lfree(struct ardp_class_of_service_tag *);



extern enum ardp_errcode GL_UNUSED_C_ARGUMENT ardp__sec_extract_class_of_service_tags(
    RREQ req, ardp_sectype *secref, const char *arg, int arglen);
#endif	    /* ARDP_SEC_HAVE_LABELS_CLASS_OF_SERVICE */


#ifdef ARDP_SEC_HAVE_AUTHENTICATION_ASRTHOST
/* SPECIFIC to AUTHENTICATION_ASRTHOST; we may want to change this later
   (a separate include file for each security mechanism, so we can more easily
   mix and match). */
extern enum ardp_errcode
ardp__sec_authentication_asrthost_accept_message(
    RREQ req, ardp_sectype *newctxt,
    const char *arg, int arglen);
#endif /*  ARDP_SEC_HAVE_AUTHENTICATION_ASRTHOST */


#ifdef ARDP_SEC_HAVE_AUTHENTICATION_KERBEROS
/* SPECIFIC to AUTHENTICATION_KERBEROS; we may want to change this later
   (a separate include file for each security mechanism, so we can more easily
   mix and match). */
extern enum ardp_errcode
ardp__sec_authentication_kerberos_client_verify_message(
    RREQ req, ardp_sectype *newctxt,
    const char *arg, int arglen);
extern enum ardp_errcode
ardp__sec_authentication_kerberos_server_verify_message(
    RREQ req,
    ardp_sectype *newctxt,
    const char *arg, /* This is the output buffer generated by
			the client's call to krb5_mk_req() */
    int arglen);
#endif /*  ARDP_SEC_HAVE_AUTHENTICATION_KERBEROS */

/* #################### */

#ifdef ARDP_SEC_HAVE_INTEGRITY_KERBEROS
/* SPECIFIC to INTEGRITY_KERBEROS; we may want to change this later
   (a separate include file for each security mechanism, so we can more easily
   mix and match). */
extern enum ardp_errcode ardp__sec_integrity_kerberos_verify_message(
    RREQ req, ardp_sectype *integref,
    const char *arg, int arglen);
#endif /* ARDP_SEC_HAVE_INTEGRITY_KERBEROS */

#if defined(ARDP_SEC_HAVE_KERBEROS)
extern void ardp__sec_make_krb5_data_from_PTEXTs(PTEXT ptl, krb5_data *outbufp);
extern void ardp__sec_make_PTEXTs_from_krb5_data(krb5_data inbuf, PTEXT *pptl);
extern ardp_sectype * ardp__sec_find_krbauth_secref(RREQ req);
extern void ardp__sec_krb5_show_sequence_nums(krb5_auth_context auth_context);
#endif



/* #################### */
#ifdef ARDP_SEC_HAVE_PRIVACY_KERBEROS
/* SPECIFIC to PRIVACY_KERBEROS; we may want to change this later
   (a separate include file for each security mechanism, so we can more easily
   mix and match). */
extern enum ardp_errcode
ardp__sec_privacy_kerberos_decrypt_message(
    RREQ req, ardp_sectype *newctxt, const char *arg, int arglen);
#endif



#endif /* ARDP_NO_SECURITY_CONTEXT */
#endif /* ARDP__SEC_H_INCLUDED */
