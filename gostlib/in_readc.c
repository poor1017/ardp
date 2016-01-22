/*
 *  Copyright(c) 1993, 1995 by the University of Southern California
 *  For copying and distribution information, please see the file
 * <usc-license.h>
 */

/* Major change:
   9/95: [swa@ISI.EDU]: many changes; here moved RREQ handling to
   lib/ardp/ardp_rreq_to_in.c. 
   9/97: <swa@ISI.EDU> Efficiency improvements in bstring handling.
   (Probably not necessary). 
   9/97: Now using GNU EMACS's INLINE facility
*/

#include <usc-license.h>

#include <gl_function_attributes.h> /* for GL_HAVE_INLINE, set up variables. */
#include <gl_internal_err.h>

#ifdef GL_HAVE_INLINE
#if 0				/* I thought this would 
				   shut up GNU C's warnings about lack of
				   prototypes; led to more problems, though.
				   */
#undef GL_PARSE_H_INCLUDED
#endif


#undef GL_INLINE_EXTERN
#define GL_INLINE_EXTERN inline


#endif
#define GL_INLINE_GET_FUNCTION_BODY /* always true, here, inlining or not. */

#include <gl_parse.h>

