/*
 * Copyright (c) 1991-1994 by the University of Southern California
 *
 * For copying and distribution information, please see the file
 * <usc-license.h>.
 */

#include <usc-license.h>

/*
 * pmachine.h - Processor/OS specific definitions
 *
 * This file should be included (in the Prospero and ARDP sources) after 
 * all of the regular system include files have been included, and before
 * the Prospero/ARDP/GOSTLIB include files have been included.
 * This is because some definitions in those files rely on types that are
 * defined in this file.
 */

#ifndef PMACHINE_H_INCLUDED
#define PMACHINE_H_INCLUDED
/* Machine types and OS types now defined in ../Makefile.config */
/*
 * Machine types - Supported values
 *
 *   VAX, SUN, HP9000_S300, HP9000_S700, IBM_RTPC, ENCORE_NS32K,
 *   ENCORE_S93, ENCORE_S91, APOLLO, IBM_RS6000, INTELX86
 * 
 *   MIPS_BE - MIPS Chip (Big Endian Byte Order)
 *   MIPS_LE - MIPS Chip (Little Endian Byte Order)
 *
 * Add others as needed.  
 *
 * Files that check these defintions:
 *   include/pmachine.h
 */
/* lib/ardp/Makefile.config.ardp defines the P_MACHINE_TYPE make macro.
   Makefile.boilerplate will convert this into a C macro of the following
   form: */
/*   #define $(P_MACHINE_TYPE)               */

/* We include misc/Makefile.define_machine_os in files where we want to define
   an additional configuration variable as: */

/*   #define P_MACHINE_TYPE 		"$(P_MACHINE_TYPE)" */

/*
 * Operating system - Supported values
 * 
 * ULTRIX, BSD43, SUNOS (version 4), SUNOS_V3 (SunOS version 3), HPUX, SYSV,
 * MACH, DOMAINOS, AIX, SOLARIS (a.k.a. SunOS version 5), SCOUNIX.
 *
 * Ignored values, which are supported otherwise: Windows 3.1, Linux
 *
 * _WINDOWS is defined for us by the Microsoft Win. 3.1 compiler for the PC 
 * architecture; we just make use of it.
 *
 * "linux" is defined for us by GCC; we just make use of it.  
 *
 * Add others as needed.  
 *
 * Files that check these defintions:
 *   include/pmachine.h, lib/pcompat/opendir.c, lib/pcompat/readdir.c
 */

/* lib/ardp/Makefile.config.ardp defines the P_OS_TYPE make macro,
   Makefile.boilerplate will convert this into a C macro of the following
   form: */
/*   #define $(P_OS_TYPE)               */

/* We include misc/Makefile.define_machine_os in files where we want to define
   an additional configuration variable as: */

/*   #define P_OS_TYPE 		"$(P_OS_TYPE)" */


/*
 * Miscellaneous definitions
 *
 * Even within a particular hardware and OS type, not all systems
 * are configured identically.  Some systems may need additinal
 * definitions, some of which are included below.   Note that for
 * some system types, these are automatically defined.
 *
 * We define SYSV_DERIVED_OPERATING_SYSTEM for systems such as SOLARIS.
 *   This definition automatically turns on some of the definitions below.
 *   Special cases can then be left out.  To do this, you would use a line such
 *   as :
 *   #if defined(SYSV_DERIVED_OPERATING_SYSTEM) && !defined(SOME_WEIRD_SYSV_SYSTEM)
 * define ARDP_MY_WINDOW_SZ if your system has some sort of internal limit
 *   on the number of UDP packets that can be queued.  SOLARIS has a limit of
 *   9 or less (6 seems to work for Pandora Systems), which inspired 
 *   this change.  If defined, the client will explicity request that 
 *   the server honor this window size.  This reduces retries and wasted
 *   messages. 
 * define NEED_MODE_T if mode_t is not typedefed on your system
 * define DD_SEEKLEN if your system doesn't support dd_bbase and dd_bsize
 * define DIRECT if direct is the name of your dirent structure
 *      This generally goes along with using sys/dir.h.
 * define USE_SYS_DIR_H if your system doesn't include sys/dirent.h, and
 *      sys/dir.h should be used instead.
 * Define P__USE_UTMPX if your system should use the SysV-derived extended 
 *	"struct utmpx" instead of the BSD-like "struct utmp".  This was added
 *	for Solaris 2.5
 * define CLOSEDIR_RET_TYPE_VOID if your closedir returns void
 * Define GETDENTS if your system supports getdents instead of getdirentries
 * Define OPEN_MODE_ARG_IS_INT if your system has the optional third argument
 *   to open() as an int.  Don't #define it if the optional third argument to
 *   open() is a mode_t.  You will need to #undef this if you're using the
 *   sysV interface to SunOS.
 * Define signal_handlers_have_one_argument if your system's sigcontext structure lacks
 *   an sc_pc member.  This appears to be the case on HPUX version 8.07 on the
 *   HP 9000 series 700 workstations.  I don't know if it's the case anywhere
 *   else, and I suspect it's a bug in the release that will be fixed later.
 *   It's also the case in Linux.
 * Define PROTOTYPE_FOR_OPEN_HAS_EMPTY_ARGLIST if your system prototypes the
 *   open() function with an empty argument list.  This is necessary because
 *   gcc requires the prototype to match the function invocation.
 * Define P__DO_NOT_HAVE_STRSPN if your system needs its own version of strspn().
 *   This is obsolete now; we do not think it required on any modern system.

 *
 * Several of the following definitions refer to prototypes being missing.  When
 * these prototypes are missing, gcc -Wimplicit will generate warnings.  These 
 * warnings, in turn, clutter the output and distract us from legitimate
 * problems.  Therefore, we have added these definitions to turn off the
 * compiler warnings as needed.  -swa, 7/97
 *
 *
 * Define PROTOTYPE_FOR_SELECT_USES_INT_POINTER if your system prototypes the
 *   select() function with 'int *' for the second, third, and fourth arguments
 *   instead of 'fd_set *'.  
 * Define BSD_UNION_WAIT if your system doesn't support the new POSIX wait()
 *   interface and does support the old BSD wait() interface and 'union wait'
 *   member.   We currently do not know of any systems which require this.
 * Define DO_NOT_HAVE_WAITPID if your system does not have the waitpid()
 *   interface.  (Don't know of any systems which require this.)
 * Define INCLUDE_FILES_LACK_FDOPEN_PROTOTYPE if your system's <stdio.h> lacks
 *   an fdopen() prototype.
 * Define INCLUDE_FILES_LACK_POPEN_PROTOTYPE if your system's <stdio.h> lacks
 *   an popen() prototype.
 * Define INCLUDE_FILES_LACK_TEMPNAM_PROTOTYPE if your system's <stdio.h> lacks
 *   a prototype for tempnam().
 * Define INCLUDE_FILES_LACK_FPRINTF_PROTOTYPE if your system's <stdio.h> lacks
 *   a prototype for fprintf().
 * Additional INCLUDE_FILES_LACK_*_PROTOTYPE definitions added to list now; see
 *   below.  
 * POSIX_SIGNALS is currently defined for all architectures by default.  We do
 *   not know any systems which need it turned off (to use the old BSD signals).
 * Define P__USE_TZNAME_NOT_TM_ZONE if the name of your time zone is in the
 *   global variable 'char *tzname[2]'.  Don't define it if your system's 
 *   'struct tm'  (from <time.h> includes the 'tm_zone' member.
 */

#if defined(SOLARIS) || defined(SYSV) || defined (SCOUNIX) || defined(HPUX)
#define SYSV_DERIVED_OPERATING_SYSTEM
#endif

/*****************************************************************/
/* If your machine and OS type are listed above, and if your     */
/* configuration is relatively standard for your machine and     */
/* OS, there should be no need for any changes below this point. */
/*****************************************************************/



/*
 * Machine or OS dependent parameters
 *
 *  The comment at the head of each section names the paramter
 *  and the files that use the definition
 */

/* u_int16_t, u_int32_t: unsigned 16 and 32 bit types.
 * These are names for types the ARDP library uses to manipulate 2 and 4 byte
 * quantities.  
 *
 * These are the same as the type names Bob Braden uses in his networking code.
 *
 * Feature tests:
 * _U_INT32_T_ (etc.):  Chris Provenzano's pthreads implementation
 * __BIT_TYPES_DEFINED__: a Linux/glibc term
 * _MACHTYPES_H_: the FreeBSD equivalent
 */

/* POSIX?   Works on SunOS, linux. */
/* If we're running LINUX or GNU libc, then we get the int32_t type, and friends, included here, so we won't make a conflicting definition below. */
#include <sys/types.h>


#if (defined(__BIT_TYPES_DEFINED__) || defined(_MACHTYPES_H_))
  /* already got them */
#else

#ifndef _U_INT32_T_
#define _U_INT32_T_
#ifdef _WINDOWS			/* MS Windows 3.1 (ewww) */
typedef unsigned long		u_int32_t;
#else
typedef unsigned int		u_int32_t;
#endif
#endif

#ifndef _U_INT16_T_
#define _U_INT16_T_
typedef unsigned short		u_int16_t;
#endif

#ifndef _INT32_T_
#define _INT32_T_
#ifdef _WINDOWS
typedef long int32_t;
#else
typedef  int int32_t;
#endif
#endif /* _INT32_T_ */

#ifndef _INT16_T_
#define _INT16_T_
typedef  short int16_t;
#endif

#endif /* !(defined(__BIT_TYPES_DEFINED__) || defined(_MACHTYPES_H_)) */

/*
 *  BYTE_ORDER: lib/psrv/plog.c, lib/psrv/check_acl.c
 */
/* The GNU C library has conflicting definitions for these values
 * (In endian.h, conditional on the __USE_BSD feature---so this
 * may occur under BSD as well.)
 * Use their definitions when we can.)
 */
#if defined(linux)
#include <endian.h>

#else
/* These numbers (1 and 2) are not magic; they just need to be different
   from each other.  --swa, 4/97 */

#define BIG_ENDIAN		1
#define LITTLE_ENDIAN		2

#if defined(SUN)        || defined(HP9000_S300) || defined(HP9000_S700) || \
    defined(IBM_RTPC) || defined(IBM_RS6000) || \
    defined(ENCORE_S91) || defined(ENCORE_S93)  || defined(APOLLO)   || \
    defined(MIPS_BE)
#define BYTE_ORDER BIG_ENDIAN
#else
#define BYTE_ORDER LITTLE_ENDIAN
#endif

#endif


#ifdef SOLARIS
#define ARDP_MY_WINDOW_SZ 6
#endif

/* Not sure if we need this as well, we certainly need the definition
   of direct as dirent below */
#ifdef SCOUNIX
#define DIRECT
#endif

/*
 * PUTENV: lib/pfs/vfsetenv.c
 *
 * PUTENV must be defined if your C library supports the putenv
 * call instead of setenv (e.g. Ultrix and SunOS).
 */
#if defined(ULTRIX) || defined(SUNOS) || defined(SUNOS_V3) || defined(HPUX) \
    || defined(SOLARIS) || defined(SCOUNIX)
#define PUTENV
#endif

/*
 * BADSETENV: lib/pfs/penviron.c
 *
 * Older BSD 4.3 systems have a bug in the C library routine setenv.
 * Define BADSETENV if you wish to compile a working version of this
 * this routine.
 * 
 * #define BADSETENV
 */

/*
 * P__USE_UTMPX: used in the lib/psrv/host/libhost.a Prospero server library
 *
 * Define P__USE_UTMPX if your system should use the SysV-derived extended 
 *	"struct utmpx" instead of the BSD-like "struct utmp".  This was added
 *	for Solaris 2.5
 */
#if defined(SOLARIS)
#define P__USE_UTMPX
#endif


/* P__USE_TZNAME_NOT_TM_ZONE: Used in server/dirsrv.c to format status
 *  messages.
 * Define P__USE_TZNAME_NOT_TM_ZONE if the name of your time zone is in the
 *  global variable 'char *tzname[2]'.  Don't define it if your system's 
 *  'struct tm'  (from <time.h> includes the 'tm_zone' member.
 */
#if defined(SOLARIS) || defined(linux)
#define P__USE_TZNAME_NOT_TM_ZONE
#endif


/*
 * P__NOREGEX: lib/pfs/re_comp_exec.c
 *
 * P__NOREGEX must be defined if your C library does not support the
 * re_comp and re_exec regular expression routines.
 */
#if defined(HPUX) || defined(SCOUNIX)
#define P__NOREGEX
#endif

/*
 * String and byte manipulating procedures.
 * XXX These should be changed throughout the source, from the BSD index(), rindex() to the ANSI
 * XXX strchr(), strrchr().  
 */
#if defined(HPUX) || defined(SYSV) || defined(SOLARIS)
#define index(a,b)		strchr((a),(b))
#define rindex(a,b)		strrchr((a),(b))
#endif


/*
 * getwd: server/pstart.c
 */
#if defined(HPUX) || defined(SYSV) || defined(SOLARIS)
#define getwd(d)	getcwd(d, MAXPATHLEN)
#endif

/*
 * SETSID:  server/dirsrv.c
 *
 * SETSID is to be defined if the system supports the POSIX
 * setsid() routine to create a new session and set the process 
 * group ID.
 *
 * SETSID is *on* by default; non-POSIX systems should undef it.
 */
#define SETSID
#if defined(CruftyOldOs)
#undef SETSID
#endif

/*
 * NFILES: user/vget/pclose.c
 *
 * NFILES is the size of the descriptor table.
 */
#if defined(HPUX)
#define NFILES _NFILE
#elif defined (SOLARIS)
#define NFILES sysconf(_SC_OPEN_MAX)
#else
#define NFILES getdtablesize()
#endif

/*
 * SIGNAL_RET_TYPE: user/vget/ftp.c, user/vget/pclose.c
 *
 * This is the type returned by the procedure returned by
 * signal(2) (or signal(3C)).  In some systems it is void, in others int.
 *
 */
#if defined (BSD43) || defined(SUNOS_V3)
#define SIGNAL_RET_TYPE int
#else
#define SIGNAL_RET_TYPE void
#endif

/*
 * CLOSEDIR_RET_TYPE_VOID: lib/pcompat/closedir.c
 *
 * If set, closedir() returns void.
 */
#if defined (CruftyOldOS) 
#define CLOSEDIR_RET_TYPE_VOID
#endif

/*
 * DIRECT: lib/pcompat/ *dir.c app/ls.c
 *
 *  Use direct as the name of the dirent struct
 */
#if defined(DIRECT)
#define dirent direct
#endif

/*
 * USE_SYS_DIR_H: lib/pcompat/ *dir.c app/ls.c
 *
 *  Include the file <sys/dir.h> instead of <dirent.h>
 */
#if defined (CruftyOldOS)
#define USE_SYS_DIR_H           /* slowly fading out of necessity */
                                /* Hopefully it's finally dead. */
#endif

/*
 * DIR structure definitions: lib/pcompat/telldir.c,opendir.c
 */
#if defined (SUNOS) || defined(SUNOS_V3)
#define dd_bbase dd_off
#endif

#if defined (DD_SEEKLEN)
#define dd_bbase dd_seek
#define dd_bsize dd_len
#endif
 
/*
 * GETDENTS: lib/pcompar/readdir.c
 *
 * Define GETDENTS if your system supports getdents instead of
 * getdirentries.
 */

#if defined (SOLARIS)
#define GETDENTS
#endif
#if defined (GETDENTS)
#define getdirentries(F,B,N,P) getdents(F,B,N)
#endif

/*
 * NEED MODE_T typedef: ls.c
 *
 * Define this if mode_t is not defined by your system's include
 * files (sys/types.h or sys/stdtypes.h or sys/stat.h).
 */

#if defined (NEED_MODE_T)
typedef unsigned short mode_t;
#endif

/*
 * OPEN_MODE_ARG_IS_INT: used: lib/pcompat/open.c
 * Define OPEN_MODE_ARG_IS_INT if your system has the optional third argument
 *   to open() as an int.  Don't #define it if the optional third argument to
 *   open() is a mode_t.  You will need to #undef this if you're using the
 *   sysV interface to SunOS.
 */
/* Not sure how MACH and DOMAINOS actually need it; this is a guess. */
#if defined(ULTRIX) || defined(BSD43) || defined(SUNOS) || defined(SUNOS_V3) \
	|| defined(MACH) || defined(DOMAINOS)
#define OPEN_MODE_ARG_IS_INT
#endif

/*
 * signal_handlers_have_one_argument typedef: server/dirsrv.c
 * 
 * Define signal_handlers_have_one_argument if your system's sigcontext structure lacks
 *   an sc_pc member.  This appears to be the case on HPUX version 8.07 on the
 *   HP 9000 series 700 workstations, as does SOLARIS, SCO, and linux.
 *
 * SIGNAL_HANDLERS_HAVE_ONE_ARGUMENT is *defined* by default (i.e., we assume
 * that most systems don't have sc_pc.  If you have this feature,
 * enable it by undef'ing it.
 */

#define SIGNAL_HANDLERS_HAVE_ONE_ARGUMENT
#if defined(SUNOS)
#undef SIGNAL_HANDLERS_HAVE_ONE_ARGUMENT
#endif

/*
 * PROTOTYPE_FOR_OPEN_HAS_EMPTY_ARGLIST: lib/pcompat/open.c
 * See the file for how this is used.  In GCC, an old-style function prototype
 * is not compatible with a full ANSI function definition containing a ...
 * (variable argument list).  This definition makes sure that we use the
 * appropriate definition of open() to correspond with the system include
 * files.
 */
/* 
 * This is triggered if we're using the GCC fixed <sys/fcntlcom.h> under an
 * ANSI C compiler; means that open() will be fully prototyped.
 */

#if !defined(HPUX) && !defined(SOLARIS) && (!defined(_PARAMS) && !defined(__STDC__))
#define PROTOTYPE_FOR_OPEN_HAS_EMPTY_ARGLIST
#endif

/*
 * Define PROTOTYPE_FOR_SELECT_USES_INT_POINTER if your system prototypes the
 *   select() function with 'int *' for the second, third, and fourth arguments
 *   instead of 'fd_set *'.   This avoids compilation warnings in libardp.
 * The action for this occurs at the end of this include file.
 */
#if defined(HPUX)
#define PROTOTYPE_FOR_SELECT_USES_INT_POINTER
#endif


/*
 * Define BSD_UNION_WAIT if your system doesn't support the new POSIX wait()
 *   interface and does support the old BSD wait() interface and 'union wait'
 *   member. 
 * Used in: lib/pcompat/pmap_cache.c, lib/pcompat/pmap_nfs.c, user/vget.c
 */

#if defined(CruftyOldOS)
#define BSD_UNION_WAIT
#endif

/*
 * Define DO_NOT_HAVE_WAITPID if your system does not have the waitpid()
 *   interface.
 *  (Don't know of any systems which require this.)
 */
#if defined(CruftyOldOS)
#define DO_NOT_HAVE_WAITPID
#endif

#if defined(SYSV_DERIVED_OPERATING_SYSTEM)
/* This was necessary for Solaris (we had problems without it).   It's the case
   for any SysV derived system, according to the book "Unix Network
   Programming" by W. Richard Stevens. 
   Used in lib/pfs/pmap_cache() */
#define TURNING_OFF_SIGCLD_MEANS_PARENT_PROCESS_CAN_NOT_WAIT_FOR_CHILDREN
#endif

/************************************************************************/
/* MISSING PROTOTYPES  */
/********************************/
/* If you want to turn off the missing-prototype fixit stuff (for example, if
   you don't care about the compiler warnings, and don't want to mess with
   these, then define this.  Preferrably in lib/ardp/Makefile.config.ardp. */
/* #define P__DO_NOT_FIX_MISSING_PROTOTYPES */

#ifndef P__DO_NOT_FIX_MISSING_PROTOTYPES

/* Put missing prototypes in alphabetical order. */
#if defined(SOLARIS) 

/* In theory these should be defined, but they're not present (at least in
 * Solaris 2.3 they're not) and GCC doesn't fix this correctly right now. 
 */
#define INCLUDE_FILES_LACK_FDOPEN_PROTOTYPE /* stdio.h */
#define INCLUDE_FILES_LACK_GETHOSTNAME_PROTOTYPE
#define INCLUDE_FILES_LACK_MEMSET_PROTOTYPE /* stdlib.h */

#define INCLUDE_FILES_LACK_POPEN_PROTOTYPE /* stdio.h */
#define INCLUDE_FILES_LACK_TEMPNAM_PROTOTYPE /* stdio.h */
#endif /* SOLARIS */

/* Put missing prototypes in alphabetical order. */
#if defined(SUNOS)
/* Additional prototypes are missing as well; see top-level BUGS file. */
#define INCLUDE_FILES_LACK_FPRINTF_PROTOTYPE
#define INCLUDE_FILES_LACK_FCLOSE_PROTOTYPE
#define INCLUDE_FILES_LACK_FFLUSH_PROTOTYPE
#define INCLUDE_FILES_LACK_FPUTS_PROTOTYPE
#define INCLUDE_FILES_LACK_FSEEK_PROTOTYPE
#define INCLUDE_FILES_LACK_IOCTL_PROTOTYPE
#define INCLUDE_FILES_LACK_MEMSET_PROTOTYPE
#define INCLUDE_FILES_LACK_PRINTF_PROTOTYPE
#define INCLUDE_FILES_LACK_SSCANF_PROTOTYPE
#define INCLUDE_FILES_LACK_TIME_PROTOTYPE
#endif

#endif /* defined(P__DO_NOT_FIX_MISSING_PROTOTYPES) */

/*
 * Catch any definitions not in system include files
 *
 *  The comment at the head of each section names the paramter
 *  and the files that use the definition
 */

/*
 * OPEN_MAX: Maximum number of files a process can have open
 */
#if defined(linux)   /* XXX - include limits unconditionally? --johnh, 31-Jan-96 */
#include <limits.h>
#endif
#ifndef OPEN_MAX
#define OPEN_MAX 64
#endif

/* MAXPATHLEN is still needed despite recommended change to pprot.h
   since some files include sys/param.h but not pprot.h and use MAXPATHLEN

   XXX MAXPATHLEN will eventually disappear from Prospero.   SCO doesn't
   provide that interface, and neither does Posix.  All the fixed-length
   code that uses MAXPATHLEN sized buffers will have to be rewritten to use
   flexible-length names.  -swa, Mar 10, 1994
*/

#ifdef linux
#include <sys/param.h>  /* for MAXPATHLEN */
#endif /* linux */
#ifndef MAXPATHLEN
#define MAXPATHLEN 1024
#endif

#ifdef SCOUNIX
/* Mitra: sys/types.h is needed to define u_long, it must be before the FD_SET stuff*/
/* Mitra: I dont know why this is so, but remember it from another port */
#define vfork fork
#endif

/* End of stuff required for SCO Unix */





#if defined(linux)
#include <sys/param.h>  /* for MAXHOSTNAMELEN */
#endif

#if !defined(MAXHOSTNAMELEN)  && defined(SOLARIS)
#include <netdb.h>              /* defines MAXHOSTNAMELEN in SOLARIS.  Might do
                                   it in others too.  --swa, 1/3/95 */
#endif

#if (!defined(MAXHOSTNAMELEN) && (defined(SUN) || defined(HPUX)))
#include <sys/param.h>          /* defines MAXHOSTNAMELEN in SunOS 4.1.3.
                                   Might do it in others too.  --swa, 1/3/95 */
#endif

#if !defined (MAXHOSTNAMELEN)
#define MAXHOSTNAMELEN 256      /* 64 is standard under berkeley unix.  256
                                   should be more than enough. */
#endif


/* FD_SET is provided in SOLARIS by <sys/select.h>.  This include file
   does not exist on SunOS 4.1.3; on SunOS and Linux (and POSIX?) it's
   provided in <sys/types.h> and <sys/time.h>.  No <sys/select.h> on
   HP-UX.  Don't know about other versions of UNIX. */

#include <sys/time.h>

#ifdef SOLARIS
#include <sys/select.h>		/* Include it before override it */
#endif

#ifndef FD_SET
/*
 * howmany: app/ls.c.  Also used by fd_set definition below.
 */
/* We insert this here because on the HP howmany() is defined in <sys/types.h>. */ 

/* On SunOS, howmany() is defined in <sys/param.h>.  Therefore, we move the
   inclusion of <sys/param.h> up above so that we won't conflict if we define
   howmany() down here. */
#ifndef	howmany
#define	howmany(x, y)   ((((u_int)(x))+(((u_int)(y))-1))/((u_int)(y)))
#endif

/* #error FD_SET undefined; test for presence of fd_set type.  Something wrong
   here. */
#if 0
/* Is it ever the case that we need fd_set and fd_mask also defined? */
   typedef long fd_mask;
   typedef struct fd_set {
       fd_mask fds_bits[howmany(FD_SETSIZE, NFDBITS)];
   } fd_set;
#endif /* 0 */
#define	NFDBITS		32
#define	FD_SETSIZE	32
#define	FD_SET(n, p)	((p)->fds_bits[(n)/NFDBITS] |= (1 << ((n) % NFDBITS)))
#define	FD_CLR(n, p)	((p)->fds_bits[(n)/NFDBITS] &= ~(1 << ((n) % NFDBITS)))
#define	FD_ISSET(n, p)	((p)->fds_bits[(n)/NFDBITS] & (1 << ((n) % NFDBITS)))
#define FD_ZERO(p)	memset((char *)(p), '\000', sizeof(*(p)))
#endif /* ndef FD_SET */
 
/*
 * O_ACCMODE: lib/pcompat/open.c
 */
#include <sys/fcntl.h>	/* Make sure include before overridden */
#ifndef O_ACCMODE
#define O_ACCMODE         (O_RDONLY|O_WRONLY|O_RDWR)
#endif

/*
 * Definitions from stat.h: app/ls.c lib/pfs/mkdirs.c
 */
#include <sys/stat.h>	/* Make sure included before overridden */
#ifndef S_IFMT
#define S_IFMT	 070000
#endif
#ifndef S_IFDIR
#define S_IFDIR	 040000
#endif
#ifndef S_IFCHR
#define S_IFCHR 020000
#endif
#ifndef S_IFBLK
#define S_IFBLK 060000
#endif
#ifndef S_IXUSR
#define S_IXUSR 0100
#endif
#ifndef S_IXGRP
#define S_IXGRP 0010
#endif
#ifndef S_IXOTH
#define S_IXOTH 0001
#endif
#ifndef S_ISDIR
#define	S_ISDIR(mode)  (((mode) & S_IFMT) == S_IFDIR)
#endif
#ifndef S_ISCHR
#define S_ISCHR(m)  ((S_IFLNK & m) == S_IFCHR)
#endif  
#ifndef S_ISBLK
#define S_ISBLK(m)  ((S_IFLNK & m) == S_IFBLK)
#endif

#define POSIX_SIGNALS

/* Define this if your system doesn't support the readv() and writev()
   scatter/gather I/O primitives.  If they're present, a couple of minor
   optimizations happen.
*/
#ifdef SCOUNIX
#define NO_IOVEC
#else  /* have WRITEV().  Should get rid of these two separate definitions;
	  only one is necessary. */
#define HAVE_WRITEV
#endif


/* SOLARIS doesn't have sys_nerr or sys_errlist.  It provides the strerror()
   interface instead.  All references to strerror() or to sys_nerr and
   sys_errlist in Prospero now go through the unixerrstr() function in
   lib/ardp/unixerrstr.c --swa
*/
#if defined(SOLARIS) || defined(linux)
#define HAVESTRERROR
#else
#undef HAVESTRERROR
#endif

/* We have our own versions of STRPBRK, for old compatibility routines, but
   don't need these on modern systems. */
#if defined(CruftyOldOS)
#define P__DO_NOT_HAVE_STRPBRK
#endif


/* We have our own versions of STRSPN, for old compatibility routines, but
   don't need these on modern systems. */
#if defined(CruftyOldOS)
#define P__DO_NOT_HAVE_STRSPN
#endif



/* Missing prototypes: */
/* Put missing prototypes in alphabetical order. */

#ifdef  INCLUDE_FILES_LACK_FCLOSE_PROTOTYPE 
#ifndef FILE
#include <stdio.h>              /* make sure we have FILE */
#endif
/* This seems to have been the case on SunOS 4.1.3 and SOLARIS 2.3 (fixed by
   2.5) */
/*Should have been defined in stdio.h */
extern int fclose(FILE *);
#endif



#ifdef  INCLUDE_FILES_LACK_FDOPEN_PROTOTYPE 
#ifndef FILE
#include <stdio.h>              /* make sure we have FILE */
#endif
/* This seems to be the case only on SOLARIS, at least through 2.3*/
/*Should be defined in stdio.h */
extern FILE *fdopen(const int fd, const char *opts);
#endif

#ifdef INCLUDE_FILES_LACK_FFLUSH_PROTOTYPE
#ifndef FILE
#include <stdio.h>              /* make sure we have FILE */
#endif
/* SunOS 4.1.3: not in <stdio.h>  */
extern int fflush(FILE *);
#endif

#ifdef INCLUDE_FILES_LACK_FPUTS_PROTOTYPE
#ifndef FILE
#include <stdio.h>              /* make sure we have FILE */
#endif
/* SunOS 4.1.3: not in <stdio.h>  */
extern int fputs(const char *, FILE *);
#endif

#ifdef INCLUDE_FILES_LACK_FPRINTF_PROTOTYPE
#ifndef FILE
#include <stdio.h>              /* make sure we have FILE */
#endif
/* SunOS 4.1.3: not in <stdio.h>  */
extern int fprintf(FILE *, const char *, ...);
#endif

#ifdef INCLUDE_FILES_LACK_FSEEK_PROTOTYPE
#ifndef FILE
#include <stdio.h>              /* make sure we have FILE */
#endif
/* SunOS 4.1.3: not in <stdio.h>.  POSIX says it should be.  */
extern int fseek(FILE *, long int, int);
#endif

#ifdef INCLUDE_FILES_LACK_GETHOSTNAME_PROTOTYPE
/* Solaris 2.5.1 (SunOS 5.5.1): not in any include file I can find -- I grepped
   through the /usr/include directory tree. --swa, 7/97 */
extern int gethostname(char *, int);
#endif

#ifdef INCLUDE_FILES_LACK_IOCTL_PROTOTYPE
/* SunOS 4.1.3: not in any include file I can find.  IOCTL() is not standard C,
   and may not be POSIX */
/* This is the version given in the Sun OS 4.1.3 manpage, but we prefer 
   the Solaris version --swa, egim, 7/97 */
/* extern int ioctl(int, int, caddr_t); */
extern int ioctl(int, int, ...);
#endif

#ifdef  INCLUDE_FILES_LACK_MEMSET_PROTOTYPE 
/* Supposed to be defined in stdlib.h under ANSI C.  
   Not there in: SunOS 4.1.3; SunOS 5.5.1 (Solaris 2.5.1)  */
void *memset(void *s, int c, size_t n);
#endif

#ifdef  INCLUDE_FILES_LACK_POPEN_PROTOTYPE 
#ifndef FILE
#include <stdio.h>              /* make sure we have FILE */
#endif
/* This seems to be the case only on SOLARIS, at least through 2.3. */
/*Should be defined in stdio.h */
extern FILE *popen(const char *, const char *);
#endif


#ifdef INCLUDE_FILES_LACK_PRINTF_PROTOTYPE
/* SunOS 4.1.3: not in <stdio.h>  */
extern int printf(const char *, ...);
#endif

#ifdef INCLUDE_FILES_LACK_TEMPNAM_PROTOTYPE
/* Supposed to be defined in stdio.h, not there in Solaris2.3 */
extern char	*tempnam(const char *, const char *);
#endif


#ifdef INCLUDE_FILES_LACK_SSCANF_PROTOTYPE
/* Supposed to be defined in stdio.h under POSIX.  Not there in SunOS 4.1.3. */
extern int sscanf(const char *, const char *, ...);
#endif


#ifdef INCLUDE_FILES_LACK_TIME_PROTOTYPE
/* Supposed to be defined in time.h under POSIX.  Not there in SunOS 4.1.3 */
extern long time(long *);
#endif

#if defined(SCOUNIX) 
#define TCPTIMEOUTS
#else
/* Definitely not working on SOLARIS yet */
#undef TCPTIMEOUTS
#endif

#if defined(SOLARIS)
/* Currently used only in lib/psrv/ppasswd.c.
   Solaris prototypes the library function crypt() in the crypt.h
   include file. */
#define CRYPT_FUNCTION_PROTOTYPE_IN_CRYPT_H
#endif

/* Stuff at the end.  The redefinition of select() is here because it cannot
   occur before the prototype for select() is read in.  This means here that,
   (when developing on the HP), <sys/time.h> must not be included after this
   redefinition occurs.  If this proves problematic, then we can redo this by
   having an internal routine, p__select(), for compatibility.
*/
#ifdef PROTOTYPE_FOR_SELECT_USES_INT_POINTER
#define select(width, readfds, writefds, exceptfds, timeout) \
  select(width, (int *) readfds, (int *) writefds, (int *) exceptfds, timeout)
#endif

#endif /* #ifndef PMACHINE_H_INCLUDED */
