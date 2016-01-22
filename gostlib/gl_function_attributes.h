/*
 * Copyright (c) 1997 by the University of Southern California
 *
 * For copying and distribution information, please see the file
 * <usc-license.h>.
 */
#ifndef GL_FUNCTION_ATTRIBUTES_H_INCLUDED
#define GL_FUNCTION_ATTRIBUTES_H_INCLUDED

#include <usc-license.h>

/* Function attributes.  Useful for compiler and warnings. */
#ifdef __GNUC__
/* This is not implemented in GNU C versions earlier than 2.5 */
/* Note that these should not be the first type qualifiers; it should follow
   the type name ('int', 'static' whatever.  Perhaps it goes right before the 
   function name?) */ 
#define GL_UNUSED_C_ARGUMENT __attribute__((unused))
#define NEVER_RETURNS __attribute__ ((noreturn))
#define NO_SIDE_EFFECTS __attribute__((const))

/* INLINE */
/* Defined if we have the INLINE facility */
#define GL_INLINE	    inline
#define GL_HAVE_INLINE
#define GL_INLINE_EXTERN extern inline
#define GL_INLINE_GET_FUNCTION_BODY

#else  /* __GNUC__ */

#define GL_UNUSED_C_ARGUMENT
#define NEVER_RETURNS
#define NO_SIDE_EFFECTS

/* INLINE */
#undef GL_HAVE_INLINE
#define GL_INLINE_EXTERN extern
#undef GL_INLINE_GET_FUNCTION_BODY
#endif /* __GNUC__ */


#endif /* GL_FUNCTION_ATTRIBUTES_H_INCLUDED */
