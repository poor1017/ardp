#ifndef GL_PARSE_H_INCLUDED
#define GL_PARSE_H_INCLUDED
/* gostlib/gl_parse.h */

/* -*-c-*- */
/*
 * Copyright (c) 1991-1998 by the University of Southern California
 *
 * For copying and distribution information, please see the file
 * <usc-license.h>.
 */

#include <usc-license.h>



/* 9/95: Moved from include/pparse.h; substantial rewriting to handle
   dependencies on structures defined in include/ardp.h.  */

/* This structure is used by the parsing code.  It defines a standard IO type
   that lets the parsing code know where to read its input from.  This code is
   shared among the client protocol interface, server protocol interface, and
   server directory format reading mechanism.  All information about the next
   location to read input from is contained INSIDE this structure.
   That means that if we make a duplicate of the structure (using input_copy())
   and feed it to a function that resets the pointer (e.g., in_inc(), or
   in_line_GSP()), the input pointer will be reset for the copy but not for the
   original. 

   If there are any data structures which have been allocated for an INPUT,
   then input_freelinks() will free them.   At the moment, this is a null
   macro. 

   The user is responsible for freeing any memory that the user passed to
   rreqtoin() or filetoin().  The sending functions will normally do this, as
   will fclose().

   The only low-level functions that actually read from this all eventually
   call in_readc() and in_readcahead() and in_incc().
*/  

#include <stddef.h>		/* for size_t type. */
#include <stdarg.h>		/* for va_list */
#include <stdio.h>		/* for FILE type (used in args.); for EOF
				   (returned by in_readc()). */

#include <gl_function_attributes.h> /* for function attributes we use here. */
#include <gl_internal_err.h>	/* for internal_err() prototype. */

enum gl__iotype {GL__IO_UNSET = 0, /* undefined */
		 /* This caller-defined IO is currently (9/97) used  by the
		    ARDP library */
		 GL__IO_CALLERDEF, /* caller defined IO.  Uses offset. */
		 /* GL__IO_BSTRING is now used for any known-length array of
		    bytes.  There is nothing in the code that actually requires
		    that it be a bstring. 
		    It is even used for regular UNIX strings.  It was
		    originally just for the GOSTLIB binary strings. */
		 GL__IO_BSTRING};

enum {
    GL_IO_NUMERIC_OVERFLOW = (-1),   /* Return value from qsscanf() */
    GL_IO_BUFFER_OVERFLOW = (-2)    /* Return value from qsscanf() */
};


typedef struct input INPUT_ST;
typedef struct input *INPUT;
typedef const struct input *constINPUT;

struct input {
    enum gl__iotype sourcetype;
    int flags;			/* see below */
    long offset;	/*  Offset from start of file or input stream.

			    Formerly used in in_line() to allocate appropriate
			    memory.  
			    Formerly, used for BSTRINGs to see how much data is
			    still readable. 
			    Still available for any types of GL__IO_CALLERDEF.
			    */ 
    union {
	/* sourcetype == GL__IO_BSTRING. */
    	struct {
	    const unsigned char *head; /* head of string or bstring */
	    const unsigned char *s; /* Current position in string or bstring */  
	    const unsigned char *pastend; /* head + bstring_length */
	    size_t bstring_length; /* Length of the bstring.  */
	} s;			/* for bstrings. */
	/* sourcetype == GL__IO_CALLERDEF */
	struct {
	    int (*readc)(constINPUT in);
	    void (*incc)(INPUT in);
	    void *dat;		/* when initialized, set to a data area. No
				   freeing function is needed for the data
				   area, for the same reason that there is no
				   allocator/deallocator for struct input:
				   these are all allocated on the stack and
				   automatically freed. */
	    size_t datsiz;	/* # of bytes needed to copy dat, if necessary.
				 */ 
	} c;			/* callerdef */
    } u;			/* for union */
};

/* Flags for Struct input */
#define GL__IO_PERCENT_R_TARGET_IS_STRING  0x2 /* if set, the target for %r is a
                                          string.  Otherwise, it's an IN.
                                          This is temporarily set by the
                                          qsscanf() interface to qscanf()
                                          */ 
#define GL__IO_PERCENT_R_TARGET_IS_INPUT 0x1 /* struct input */
/* New coding: This should never be unset. */
#define GL__IO_PERCENT_R_FLAGS 0x3

/* A server data file, whether cached by the wholefiletoin() or treated in raw
   form by the (unused) filetoin() interface. */
/* This #define can be turned off for a tiny efficiency gain in the Prospero
   parser.   It just serves to help out Protocol syntax checkers; there are
   some Link constructions legal in data files that are not acceptable in
   protocol messages. */
#define GL__IO_SERVER_DATA_FILE                 0x8 


#define gl_bstring_to_in(bstr, in) \
    gl_stringlen_to_in((bstr), p_bstlen(bstr), (in))
extern void gl_bstring_offset_to_in(const char *bstr, const char *pos, INPUT in);
extern void gl_stringlen_to_in(const char *str, size_t len, INPUT in);
extern void gl_string_to_in(const char *str, INPUT in);

extern int wholefiletoin(FILE *file, INPUT in);

/* *** String Scanning Functions **** */
/* This could be used to replace in_line_GSP(), in a rewrite of the parsing
   code. */
extern int qscanf(INPUT in, const char fmt[], ...);
extern int vqscanf(INPUT in, const char fmt[], va_list ap);
/* Unimplemented at the moment, but convenient. */
/* These are in gostlib.h:
   gl_qsscanf(), gl_qslscanf(), gl_qbscanf()
 */

GL_INLINE_EXTERN int NO_SIDE_EFFECTS in_readc(constINPUT in);
/* Returns the distinguished value (from <stdio.h>) EOF if the end of the input
   is detected. */
GL_INLINE_EXTERN int 
NO_SIDE_EFFECTS
in_readc(constINPUT in)
#ifndef GL_INLINE_GET_FUNCTION_BODY
    ;
#else
{
    switch(in->sourcetype) {
    case GL__IO_BSTRING:
	return in->u.s.s < in->u.s.pastend ? *(in->u.s.s) : EOF;
    case GL__IO_CALLERDEF:
	return (in->u.c.readc)(in);
    default:
        internal_error("invalid in->sourcetype");
    }
    /* NOTREACHED */
    return EOF; /* Unreached - keeps gcc happy */
}
#endif

/* This function may legally be called on a stream which has already reached
   EOF.  In that case, it's a no-op. */

extern void in_incc(INPUT in);

GL_INLINE_EXTERN void 
in_incc(INPUT in)
#ifndef GL_INLINE_GET_FUNCTION_BODY
    ;
#else
{
    switch(in->sourcetype) {
    case GL__IO_BSTRING:
	++in->u.s.s;		/* ok to increment past the end. */
        break;
    case GL__IO_CALLERDEF:
	(in->u.c.incc)(in);
	break;
    default:
        internal_error("invalid in->sourcetype");
    }
}

#endif

/*
 * Returns EOF if EOF has been reached (non-zero; C true).
 * Returns 0 (C false) if still stuff to read.
 * 9/95: I made this a simple wrapper around in_readc(), despite the
 *  	overhead of the additional function call.   	--swa
 * 9/97: Will have to test this.
 */
extern int NO_SIDE_EFFECTS in_eof(constINPUT in);
GL_INLINE_EXTERN int NO_SIDE_EFFECTS in_eof(constINPUT in)
#ifndef GL_INLINE_GET_FUNCTION_BODY
    ;
#else
{
    return (in_readc(in) == EOF) ? EOF : 0;
}
#endif


#if 0
/* # of bytes available to read */
#error not yet implemented: extern size_t in_nunread(constINPUT in);
#endif
/* This is used by functions in gostlib; not intended to be exported */
/* Nevertheless, it is used by lib/pfs/in_line.c */
extern void gl__input_copy(constINPUT src, INPUT dest, void *dat_aux,
			   int dat_auxsiz);

/* #define GL__STRING_PARSE	*/
#ifdef GL__STRING_PARSE		/* Then qsscanf() needs this.... */
/* use the string-specific version of qsscanf(). */
/* See defs. in gostlib.h */
extern int vqslscanf(const char *s, ptrdiff_t nbytes, const char *fmt, va_list ap);
#endif

#endif
