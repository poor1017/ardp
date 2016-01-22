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
   9/95:    Moved to gostlib.  --swa
*/

/*
 * pfs_debug - a global variable containing debugging flag
 *
 *          This variable is provided in the GOSTLIB library, since it is used
 *          within the GOSTLIB library.  
 *          
 *          Originally, it was customary for Prospero clients to define the
 *          variable themselves.   It is not necessary to do so, unless
 *          you want it to be automatically initialized to some value other 
 *          than zero.  (I expect this will only be wanted during development
 *          or for demonstrations.)  --swa
 * 
 *          Customary practice now is for clients to not define this variable
 *          themselves.
 *
 *  	    If we were writing from scratch, we wouldn't put it here.
 */

#include <gostlib.h>               /* for prototype for pfs_debug variable. */

int	pfs_debug = 0;
