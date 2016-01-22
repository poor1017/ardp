/*

 * Copyright (c) 1992, 1993 by the University of Southern California
 *
 * For copying and distribution information, please see the file
 *  <usc-license.h>.
 *
 * Written  by bcn 1991     as part of rdgram.c in Prospero distribution 
 * Modified by bcn 1/93     modularized and incorporated into new ardp library
 */


#include <usc-license.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>

#include <ardp.h>
#include "ardp__int.h"
#include <perrno.h>
#include <gl_log_msgtypes.h>

int (*ardp_pri_func)() = NULL;  /* Function to compare priorities       */
int ardp_pri_override = 0;	/* If 1, then oveeride value in request */

/* 
 * ardp_set_queueing_plicy - set function for queueing policy 
 *
 *    ardp_set_queuing_policy allows one to provide a function that will set 
 *    priorities for requests.  When passed two req structures, r1 and r2, the 
 *    function should  return a negative number if r1 should be executed first 
 *    (i.e. r1 has a lower numerical priority) and positive if r2 should be 
 *    executed first.  If the function returns 0, it means the two have the 
 *    same priority and should be executed FCFS.  If override is non-zero, then
 *    the priority function is to be applied to all requests.  If non-zero,
 *    it is only applied to those with identical a priori priorities (as
 *    specified in the datagram itself.
 */
enum ardp_errcode
ardp_set_queuing_policy(int (*pf)(), /* Function to compare priorities       */
			int override) /* If 1, then override value in request
				       */  
{
    ardp_pri_func = pf;
    ardp_pri_override = override;
    return(ARDP_SUCCESS);
}

enum ardp_errcode
ardp_set_prvport(int port)
{
    ardp_prvport = port;
    /* See the documentation of ardp_accept in lib/ardp/ardp_pr_actv.c for an
       understanding of why we are doing this form of late binding. */
    ardp__accept = ardp_accept;
    return(ARDP_SUCCESS);
}


/* Improvement 7/97: Listen on 'default port' if nothing passed. */
/* Something a little bit weird happens here.  We need to return both an error
   code and a port number. */
/* Change in interface: We now return an error code in perrno if we must. */
int
ardp_bind_port(const char *portname)
{
    struct sockaddr_in	s_in = {AF_INET};
    struct servent 	*sp;
#if 1
    int     		on = 1;
#endif
    int			port_no = 0;

    /* See the documentation of ardp_accept in lib/ardp/ardp_pr_actv.c for an
       understanding of why we are doing this form of late binding. */
    ardp__accept = ardp_accept;

    if (!portname || !*portname) {
	if (ardp_debug) {
	    rfprintf(stderr, "ardp_bind_port() did not get a valid port name \
as an argument; using the default server port: %d\n",
		    ardp_config.default_port);
	}
	s_in.sin_port = htons((u_int16_t) ardp_config.default_port);
    } else if(*portname == '#') {
	sscanf(portname+1,"%d",&port_no);
	if(port_no == 0) {
	    if (ardp_debug) {
		rfprintf(stderr, "ardp_bind_port: cannot bind: \"%s\" is an \
invalid port specifier; port number  must follow #\n", portname);
	    }
	    return -(perrno = ARDP_PORT_UNKN);
	}
	s_in.sin_port = htons((ushort) port_no);
    }
    else if((sp = getservbyname(portname, "udp")) != NULL) {
	s_in.sin_port = sp->s_port;
    }
    else if(strcmp(portname,ardp_config.default_peer) == 0) {
	if (ardp_debug) {
	    rfprintf(stderr, 
		    "ardp_bind_port: udp/%s unknown service - using %d\n", 
		    ardp_config.default_peer, ardp_config.default_port);
	}
	s_in.sin_port = htons((ushort) ardp_config.default_port);
    }
    else {
	if (ardp_debug) {
	    rfprintf(stderr, "ardp_bind_port: udp/%s unknown service\n",portname);
	    return -(perrno = ARDP_PORT_UNKN);
	}
    }
    
    if ((ardp_srvport = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
	ardp__log(L_STATUS,NOREQ,"Startup -- Can't open socket",0);
	if (ardp_debug)
	    rfprintf(stderr, "ardp_bind_port: Can't open socket\n");
	return -(perrno = ARDP_UDP_CANT);
    }
#if 0
    /* This was an attempt to do it more elegantly.  It did not work, despite
       the promises of the Solaris 2.5 (and SunOS 4.1.3, to be fair) manual
       page to the contrary.  We got an EINVAL (err # 22) when running this
       code under Solaris.  -- 8/1/96, sridhar@ISI.EDU, swa@ISI.EDU */
    /* We had believed that 
       passing any non-zero value as the fourth argument to setsockopt() would
       be enough to turn on the SO_REUSEADDR option; we believed that it did
       not need to point to 'on', an integer with a non-zero value.
       There are calls to setsockopt) in lib/pfs/opentcp.c which invoke it with
       a zero as the fourth and fifth arguments; no idea why they work (or even
       if that code is ever executed).  --swa, 8/1/96 */

    if (setsockopt(ardp_srvport, SOL_SOCKET, SO_REUSEADDR, (char *) 62065, 0) < 0)
	rfprintf(stderr, "dirsrv: setsockopt (SO_REUSEADDR)\n");
    
#else
    /* Old way of doing it: */
    if (setsockopt(ardp_srvport, SOL_SOCKET, SO_REUSEADDR, (char *) &on, sizeof(on)) < 0)
	rfprintf(stderr, "dirsrv: setsockopt (SO_REUSEADDR): %s; continuing \
as best we can.\n", unixerrstr());
    
#endif
    s_in.sin_addr.s_addr = (u_long) myaddress();
    if (bind(ardp_srvport, (struct sockaddr *) &s_in, S_AD_SZ) < 0) {
	ardp__log(L_STATUS,NOREQ,"ardp: Startup - Can't bind socket",0);
	if (ardp_debug)
	    rfprintf(stderr, "ardp_bind_port(): Can not bind socket\n");
	return -(perrno = ARDP_UDP_CANT);
    }
    return(ntohs(s_in.sin_port));
}

