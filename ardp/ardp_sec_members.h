/* ardp_sec_members.h.h */
/* This is for members of the ardp_sectype structure, specific to each
   security context mechanism. */

/* authentication_asrthost */
enum {AA_PEER_USERNAME = 0,	/* to an allocated string. */
      AA_MY_USERNAME = 1,	/* to an allocated string. */
      AA_TRSTHOST = 2,		/* Set or unset -- yes or no. */
};

/* These indices are for the mech_spec member of the ardp_sectype structure for
   authentication_kerberos. */
enum {KA_AUTHENTICATOR = 0, 
      /* Client only */
      KA_REMOTE_SERVICENAME = 1,
      /* Client only. */
      KA_REMOTE_SERVERNAME = 2, 
      /* This is used for our own client name if we're the
	 client, and for the remote client name if we're the server. */
      KA_CLIENTNAME = 3,
};

/* These indices are for the mech_spec member of the ardp_sectype structure for
   integrity_kerberos. */

enum { 
    /* An ardp_sectype *; the authref passed to the integrity_kerberos
       client and server handlers. */ 
    IK_AUTHREF = 0,
};

/* These indices are for the mech_spec member of the ardp_sectype structure for
   privacy_kerberos. */

enum { 
    /* An ardp_sectype *; the authref passed to the privacy_kerberos
       client and server handlers. */ 
    PK_AUTHREF = 0,
};

/* Defining mech_spec members for ardp_sec_labels_class_of_service. */
enum { COS_LABEL = 0 ,
       COS_VALUE = 1,
};
