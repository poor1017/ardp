/*
 * Copyright (c) 1991-1994 by the University of Southern California
 *
 * For copying and distribution information, please see the file
 * <usc-license.h>.
 */

#include <usc-license.h>

#ifndef PFS_PERRNO_H
#define PFS_PERRNO_H

/* XXX This file is awkwardly placed.  It is updated, and supports, the PFS
   library, but it is actually included as part of the GOSTLIB library.  This
   modularity is poor. It is done because the error codes of the GOSTLIB, ARDP,
   and PFS libraries share a name-space.   */

/* this file and lib/pfs/perrmesg.c should be updated simultaneously */

/*
 * perrno.h - definitions for perrno
 *
 * This file contains the declarations and defintions of of the external
 * error values in which errors are returned by the pfs and psrv
 * libraries.
 */

#ifdef GL_THREADS
extern int *gl__perrno(void);
extern int *gl__pwarn(void);
extern char **gl__p_err_string(void);
extern char **gl__p_warn_string(void);

#define perrno		(*gl__perrno())
#define pwarn		(*gl__pwarn())
#define p_err_string	(*gl__p_err_string())
#define p_warn_string	(*gl__p_warn_string())

#else
extern enum p_errcode perrno;
/* pwarn and p_warn_string are not set or read in ARDP. */
extern enum p_warncode pwarn;
extern char *p_warn_string;	/* This is a GOST string */
/* p_err_string is set in ARDP. */
extern char *p_err_string;	/* This is a GOST string */
#endif


/* Error codes returned or found in perrno */


enum p_errcode {

#undef PSUCCESS
    PSUCCESS = 0,
    P_SUCCESS = PSUCCESS,
    /* Error codes 1 through 20 are reserved for the ardp library */
    /* Defined in include/ardp.h */
    /* We shadow those codes here, because it is useful for us to have that
       information available to display in the debugger when error codes are
       returned.  We cannot duplicate them, regrettably.  
       Lines like the following lead to complaints about conflicting types,
       even though the definitions are identical: */
    /* ARDP_SUCCESS = 0, */
    P_ARDP_PORT_UNKN = 1,	/* UDP port unknown                 */
    P_ARDP_UDP_CANT = 2,	/* Can't open local UDP port        */
    P_ARDP_BAD_HOSTNAME = 3,	/* Can't resolve hostname           */
    P_ARDP_NOT_SENT = 4,	/* Attempt to send message failed   */
    P_ARDP_SELECT_FAILED = 5,	/* Select failed	            */
    P_ARDP_BAD_RECV = 6,	/* Recvfrom failed 	            */
    P_ARDP_BAD_VERSION = 7,	/* bad version # in rdgram protocol */
    P_ARDP_BAD_REQ = 8,		/* Inconsistent request structure   */
    P_ARDP_TIMEOUT = 9,		/* Timed out - retry count exceeded */
    P_ARDP_REFUSED = 10,	/* Connection refused by server     */
    P_ARDP_FAILURE = 11,	/* Unspecified ARDP failure         */
    P_ARDP_TOOLONG = 12,	/* Buffer too long for packet       */
    P_ARDP_TIMER_FAILED = 13,	/* A call to gettimeofday() failed.
				   (this is ARI's addition; I'm not
				   sure that it's needed --swa) */
    P_ARDP_CRITICAL_CONTEXT_NOT_SUPPORTED = 14, /* for ardp-v1 contexts */
    P_ARDP_CRITICAL_SERVICE_FAILED = 15,
    P_ARDP_CONTEXT_FAILURE = 16, /* more general than the CONTEXT ones above;
				    more specific than P_ARDP_FAILURE (or
				    P_ARDP_REFUSED, in some situations)  */
    /* #s 16 -- 20 are unused. */
    
    
    /* These are error codes from the PFS library routines. */
    /* vl_insert */
    VL_INSERT_ALREADY_THERE = 21,	/* Link already exists	        */
    VL_INSERT_CONFLICT = 22,	/* Link exists with same name   */
    
    /* ul_insert */
    UL_INSERT_ALREADY_THERE = 25,	/* Link already exists		*/
    UL_INSERT_SUPERSEDING = 26,	/* Replacing existing link	*/
    /* This has been superseded by an gl_function_arguments_error() call in
       ul_insert_ob(). */
    UL_INSERT_POS_NOTFOUND = 27,	/* Prv entry not in dir->ulinks */
    
    /* rd_vdir */
    RVD_DIR_NOT_THERE = 41,	/* Temporary NOT_FOUND		    */
    RVD_NO_CLOSED_NS = 42,	/* Namespace not closed w/ object:: */
    RVD_NO_NS_ALIAS = 43,	/* No alias for namespace NS#:      */
    RVD_NS_NOT_FOUND = 44,	/* Specified namespace not found    */
    
    /* pfs_access */
    PFSA_AM_NOT_SUPPORTED = 51,      /* Access method not supported  */

    /* p__map_cache */
    PMC_DELETE_ON_CLOSE = 55,	/* Delete cached copy on close   */
    PMC_RETRIEVE_FAILED = 56,      /* Unable to retrieve file       */

    /* mk_vdir */
    /* Superseded by DIRSRV_ALREADY_EXISTS, DIRSRV_NAME_CONFLICT */
    /* MKVD_ALREADY_EXISTS = 61,     * Directory already exists */ 
    /* MKVD_NAME_CONFLICT = 62,  * Link with name already exists */ 
    
    /* vfsetenv */
    VFSN_NOT_A_VS = 65,	/* Not a virtual system          */
    VFSN_CANT_FIND_DIR = 66,	/* Not a virtual system          */
    
    /* add_vlink */
    /* We use the DIRSRV_ALREADY_EXISTS, DIRSRV_NAME_CONFLICT return codes */
    /* ADDVL_ALREADY_EXISTS = 71,	    * Directory already exists */ 
    /* ADDVL_NAME_CONFLICT = 72,	* Link with name already exists */
    
    /* pset_at */
    PSET_AT_TARGET_NOT_AN_OBJECT = 81,/* The link passed to PSET_AT() has a
					 TARGET such that it does not
					 refer to an object, so we can't set
					 any object attributes on it. */

    /* Error codes related to Prospero library security */
    P_CANNOT_GUARANTEE_INTEGRITY = 91, /* Unable to provide integrity
					  protection, either on the client or
					  server end. */ 
    P_CANNOT_PROVIDE_PRIVACY = 92,/* Unable to provide privacy-protection */

    /* Error codes for parsing problems. */
    PARSE_ERROR = 101,     /* General parsing syntax error . */

    /* Local error codes on server */

    /* dsrdir */
    /* Now superseded by DIRSRV_NOT_FOUND */
    /* or by NOT_A_DIRECTORY, depending upon context */
    DSRDIR_NOT_A_DIRECTORY = 111,	/* Not a directory name		*/
    /* Superseded by DIRSRV_NOT_FOUND */
    /* dsrfinfo */
    DSRFINFO_NOT_A_FILE = 121,      /* Object not found             */
    OBJECT_FORWARDED = 122,  /* Object has moved */ 
    /* deprecated */
    DSRFINFO_FORWARDED = OBJECT_FORWARDED,

    /* Additional server error codes starting at # 200; ran out of 'em past 255
       --swa & dongho, 8/96 */
    DIRSRV_AUTHENTICATION_FAILED = 200,
    DIRSRV_BAD_VALUE = 201,
    DIRSRV_FILTER_APPLICATION_ERROR = 202,

    /* Error codes that may be returned by various procedures               */
    PFS_MALFORMED_USER_LEVEL_NAME = 228,
    /* Some user-level name (of the form parsed by rd_vdir()) was malformed
       and could not be read. */
    PFS_NEED_DIRECTORY_AND_LINK_NAME = 229,
    /* Means the user-level-name passed to some procedure
       in the PFS (or higher-level) library does not name a link within a
       directory. This is important for del_vlink(), where we must have a
       directory and the name of a link within that directory.  Only returned
       by p_uln_to_diruln_comp_GSP */  
    PFS_FILE_NOT_FOUND = 230,      /* File not found               */
    PFS_DIR_NOT_FOUND = 231,      /* Directory in path not found  */
    PFS_SYMLINK_DEPTH = 232,	/* Max sym-link depth exceeded  */
    PFS_ENV_NOT_INITIALIZED = 233,	/* Can't read environment	*/
    PFS_EXT_USED_AS_DIR = 234,	/* Can't use externals as dirs  */
    PFS_MAX_FWD_DEPTH = 235,	/* Exceeded max forward depth   */
    /* added for p_open() with payment. */
    PFS_PAYMENT_REQUIRED = 236, /* how to transmit info up now? */
    PFS_PAYMENT_INSUFFICIENT = 237, /* too little */
    PFS_PAYMENT_BAD_INSTRUMENT = 238, /* couldn't parse or already paid */
    
    /* Error codes returned by directory server                    */
    /* some of these duplicate errors from individual routines     */
    /* some of those error codes should be eliminated              */
    DIRSRV_HTTP = 239,		/* HTTP gateway failure */
    DIRSRV_WAIS = 240,		/* WAIS gateway failure */
    DIRSRV_GOPHER = 241,	/* Gopher gateway failure */
    DIRSRV_AUTHENT_REQ = 242,	/* Authentication required       */
    DIRSRV_NOT_AUTHORIZED = 243, /* Not authorized                */
    DIRSRV_NOT_FOUND = 244,	/* Not found                     */
    DIRSRV_BAD_VERS = 245,	/* Bad version  */
    NOT_A_DIRECTORY = 246,	/* Can't perform a directory operation 
				   (LIST or EDIT-LINK-INFO) on an object that
				   is not a directory. */
    DIRSRV_NOT_DIRECTORY = NOT_A_DIRECTORY, /* OBSOLESCENT name */

    DIRSRV_ALREADY_EXISTS = 247, /* Identical link already exists */
    DIRSRV_NAME_CONFLICT = 248,	/* Link with name already exists */
    DIRSRV_TOO_MANY = 249,	/* Too many matches to return    */
    /* 250 unused */
    DIRSRV_UNIMPLEMENTED = 251,      /* Unimplemented command         */
    DIRSRV_BAD_FORMAT = 252,	/* how is this different from PROTOCL error
				   messages?  */
    DIRSRV_ERROR = 253, /* PROTOCOL error messages */
    DIRSRV_SERVER_FAILED = 254,      /* Unspecified server failure    */
#undef PFAILURE
    PFAILURE = 255,      /*  Random other complaint. */
};


#if 0
/* If DEBUG_PFAILURE is defined, then the no-op function it_failed() will be
   called right before any function originates a PFAILURE return.  This can be
   handy in tracking down an error in library usage when a library call is
   returning PFAILURE to you.  This is normally not enabled so that we can
   avoid the overhead of the unnecessary call to it_failed().  */
/* it_failed() is in lib/pfs/returning_error.c */
#ifdef DEBUG_PFAILURE
#define RETURNPFAILURE do { it_failed();  return(PFAILURE); } while(0)
#else  /* DEBUG_PFAILURE */
#define RETURNPFAILURE return(PFAILURE)
#endif /* DEBUG_PFAILURE */
#endif /* 0 */

/* XXX This is deprecated; will be eliminated as soon as I extirpate it from
   the code.  Could be years though... --swa, 11/22/94 */
#define RETURNPFAILURE  P_ERROR_RETURN(PFAILURE)

/* The above interface is now deprecated.  Superseded by: */
#ifdef DEBUG_PFAILURE
extern void returning_error(int errcode); /* will be in its own function. */
#define P_ERROR_RETURN(errcode) do {\
    returning_error(errcode);  \
    return gl_set_err(errcode, NULL); \
} while(0)
#else  /* DEBUG_PFAILURE */
/* Empty normal version. */
#define P_ERROR_RETURN(errcode)  return gl_set_err(errcode, NULL)
#endif /* DEBUG_PFAILURE */


/* Warning codes */

enum p_warncode {
    PNOWARN = 0,	/* No warning indicated		 */
    PWARN_OUT_OF_DATE = 1,	/* Software is out of date       */
    PWARN_MSG_FROM_SERVER = 2,      /* Warning in p_warn_string      */
    PWARN_UNRECOGNIZED_RESP = 3,	/* Unrecognized line in response */
    PWARNING = 255,	/* Warning in p_warn_string      */
};
/* Function to reset error and warning codes */
extern void p_clear_errors(void);
/* I intend this function to be the new standard method for returning error
   messages. --swa */
extern enum p_errcode gl_set_err(enum p_errcode perr, const char *, ...);

/* These two tables of error codes are in the PFS library.  They are prototyped
      here, but not defined in GOSTlib. */
extern const char	*p_err_text[];
extern const char	*p_warn_text[];

#endif /*PFS_PERRNO_H*/
