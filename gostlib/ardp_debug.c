/*
 * Copyright (c) 1991-1994 by the University of Southern California
 *
 * For copying and distribution information, please see the file
 * <usc-license.h>.
 */

#include <usc-license.h>

/* Original Author: Cliff Neuman (1989)
   11/94:  swa rewrote all the comments (except for one line), but did not 
       change the one line of code :).  Moved it to ARDP library.
   5/29/96 changed that last one line of code; pfs_debug became ardp_debug.
*/

/*
 * ardp_debug - a global variable containing debugging flag
 *
 *          This variable is provided in the ARDP library, since it is used
 *          within the ARDP library.  
 *	    This variable is also provided in GOSTLIB, since the parsing code
 *	    specially recognizes the -D flag when it sets this variable.
 *	    UGGGGLLYYYY.
 *
 *          Originally, it was customary for Prospero clients to define the
 *          variable themselves.   It is not necessary to do so, unless
 *          you want it to be automatically initialized to some value other 
 *          than zero.  (I expect this will only be wanted during development
 *          or for demonstrations.)  --swa
 * 
 *          Customary practice now is for clients to not define this variable
 *          themselves.
 */

#include <gostlib.h>               /* for prototype for ardp_debug variable. */

int	ardp_debug = 0;
