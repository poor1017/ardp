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
       9/95: added ardp_priority variable to (new) gostlib library.  Did not
       delete any other copies that may exist; they will not cause problems.
       Ultimately this will probably 
*/

/*
 * ardp_priority - a global variable containing ardp priority
 *
 *          This variable is provided in the GOSTLIB library, since it is
 *  	    modified  by the p_config routines within gostlib.
 *          
 *          Originally, it was customary for Prospero clients to define the
 *          variable themselves.   It is not necessary to do so, unless
 *          you want it to be automatically initialized to some value other 
 *          than zero.  (I expect this will only be wanted during development
 *          or for demonstrations.)  If you do this, your def. will be used,
 *  	    not the one here.--swa
 * 
 *          Customary practice now is for clients to not define this variable
 *          themselves.
 *  	    
 *  	    Note that this variable will do no harm if you are not using
 *  	    the ARDP library with gostlib.
 */

#include <gostlib.h>               /* for prototype for ardp_priority
				      variable. */ 

int	ardp_priority = 0;
