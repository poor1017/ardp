/* vqslscanf.c
   Author: Steven Augart (swa@isi.edu)
   Designed, Documented, and Written: 7/18/92 -- 7/27/92
   Ported from Gnu C to full ANSI C & traditional C, 10/5/92
   & modifier added, detraditionalized: 2/16/93.
   3/2/93 converted from qsscanf() to vqscanf(); not all comments updated.
   Sorry. :)
   10/1/95:  Made in_readcahead() function local to this file; not used
   elsewhere.  Adapted it to reflect new  gl_parse.h library.
   Also made it more efficient.
   9/97: Got rid of generality; gone to vqslscanf.  This means we duplicate
   code.  Ick.
*/

/*#define ASSERT2*/                 /* expensive debugging assertions */
/* Copyright (c) 1992, 1993 by the University of Southern California. */
/* For copying and distribution information, please see the file
 * <usc-license.h> */

#include <usc-license.h>

#include <gl_parse.h>		/* for GL__STRING_PARSE */

#ifdef GL__STRING_PARSE

/*
  This is NOT the ordinary sscanf()!  It works somewhat like sscanf(), but it
  does *not* recognize all of sscanf()'s options.

  In order to understand the rest of this documentation, you should be familiar
  with the sscanf() function.

  Like sscanf(), qsscanf() returns a count of the number of successful
  assignment matches.  The return value is not as important as it is in
  sscanf(), though, since you can check whether you matched everything by
  making your final conversion specifier be %r, setting the corresponding
  pointer to NULL, then checking whether the corresponding pointer was reset.

  sscanf() ignores blanks and tabs in the input string.  qsscanf() considers
  those to match a stretch of at least one horizontal whitespace (space or tab)
  character.  This means that horizontal whitespace is NOT ignored in the input
  string, nor in the format string.  In other words, occurrence of one or more
  space or a tab in the format string is equivalent to %*[ \t].  This isn't
  really a terribly   creative thing to map whitespace onto, but it fits my
  intuition a little bit better than sscanf()'s approach of ignoring it
  altogether.  Other whitespace characters (such as newline ('\n')) which
  appear in the format string must literally match the corresponding character
  in the input string.

  The assignment suppression (the '*' modifier) feature of sscanf() is
  supported.

  The "long instead of int" (the 'l' modifier) feature of sscanf() is
  supported, but we have not yet had a need for the "short instead of int" (the
  'h' modifier) feature.

  The "'" modifier will unquote the input string.  It turns off normal
  processing of all field terminators inside a quoted string, until the quoting
  ends, at which point normal processing continues.  See qsprintf() for a
  discussion of quoting.  If we read only part of a quoted string, either
  because the input string terminated early or because the field width ended
  before the quoted part of the string ended or because the buffer ran out of
  room (even with a '_' modifier), then we consider the match to have failed.
  (I have thought about this quite a bit.  If you have an
  application for partial quoted string matches, send e-mail to swa@isi.edu;
  I'd like to hear about it.)

  Regular sscanf() has a notion of maximum field width.  For example, to read
  no more than 5 spaces or slashes into a string, one would give sscanf() the
  conversion specifier "%5[ /]".  "qsscanf()" also supports field width.
  Note that, according to the ANSI spec, all fields except for %n must contain
  at least one character that matches.  (In our extended sscanf(), all
  conversion specifiers except for %r, %~ and %( must match at least 1 single
  character of output field.  This means that negative or zero length field
  widths are meaningless, and should never be specified.  Similarly, buffer
  lengths (see below) must contain at least enough room for one character (and
  the trailing NUL ('\0') in all cases of NUL-terminated strings).

  The institution of quoted strings means that there is now a separation
  between input field width and number of characters written to the input
  buffer.  To give an example, should the quoted string "''''" have a field
  width of 4 (the number of input characters) or 1 (the number of characters
  written to the buffer?)

  I have decided "field width" represents the number of characters we are
  willing to read into the output buffer, exclusive of the terminating '\0'.
  Perhaps not including the '\0' is a bad decision, but it's backward
  compatible with sscanf(). 

  Output buffer size may be specified by using the '_' modifier, the '$'
  modifier, or the '!' modifier.

  _: If the '_' modifier is specified, this means that the output buffer size
     follows (including space for a terminating '\0', in the case of every
     string conversion).  After the output buffer is full, the match will
     succeed and we go on to the next format character.  It is quite similar to
     specifying maximum field width, except that maximum field width doesn't
     include space for the terminating '\0'.  If two '_' modifiers are
     specified, then the output buffer size is read from the next integer
     argument.

  $: If the '$' modifier is specified, the output buffer size follows.  The
     match will FAIL if the output buffer overflows.  A count of the # of
     successful matches is returned.  If two '$' modifiers are specified, the
     output buffer size is read from the next integer argument.  Note that '_'
     works just like '$' when we are scanning quoted strings, because *partial
     matches for quoted strings are not useful*.

  !: If the '!' modifier is specified, the output buffer size follows.  If the
     output buffer overflows, qsscanf() will return with the value of the
     integer constant GL_IO_BUFFER_OVERFLOW (guaranteed to be negative).  Note that
     buffer overflow and numeric overflow are the only situations in which
     qsscanf() returns negative values.  If two '!' modifiers are specified,
     the output buffer size is read from the next integer argument.  Note that
     '_' works just like '!'  when we are scanning quoted strings.


  A GOSTlib-specific modifier is '&'.  The '&' modifier is only implemented
  for the %s and %[ conversions.  The argument to %s or %[, instead of being a
  pointer to a buffer, is a char **.  The argument will have the GOSTlib
  stcopyr() function applied to it.  Therefore, the argument must be NULL or
  contain data previously allocated by GOSTlib's stcopy() or stcopyr() or
  stalloc() functions.  This allows us to read strings of unlimited size
  without overflow.  This works with the '\'' modifier.

  qsscanf() checks for integer overflow.  By default, or if the '!' modifier is
  specified, qsscanf() aborts processing and returns the value of the integer
  constant "GL_IO_NUMERIC_OVERFLOW" (guaranteed to be negative).  If the '_' modifier
  has been specified, then we will terminate the integer conversion upon
  potential overflow (just like specifying an input field width for an
  integer).  If the '$' modifier has been specified, then the match will fail
  upon integer overflow.

  It is meaningful to combine the '_', '$', and '!' modifiers with the '*'
  (assignment suppression) modifier.

  I expect that nobody will actually use the '_' modifier with numeric input,
  but the functionality is there.

  Conversion    Argument    Function
  Specifier     Type
  ------        -----       -----------

  %d            int *       Looks for an optionally signed decimal integer.
                            Note that we do NOT skip over leading whitespace
                            like sscanf() does, nor do we in any other
                            conversions.  Terminates on the first non-numeric
                            character, exclusive of an optional leading '-'.

			    The h modifier ("%hd")indicates a short, and the
			    l modifier ("%ld") indicates a long.  (Future
			    enhancement: If the compiler supports long-longs,
			    then %ll indicates a long-long.)

  %s            char *      Whitespace-terminated non-zero-length sequence of
                            characters.  In the context of "%s", "whitespace"
                            means any one or more of "\n\r\t\v\f ".  This
                            conversion adds a terminating '\0' to the output
                            string. .  Note one exception to the
                            non-zero-length rule:

                            "%'s" will return the zero-length string when fed
                            the input "''".  (This exception applies to all
                            cases where "non-zero-length" is mentioned in this
                            documentation.)

  %S            char *      Matches all of the remaining characters in the
                            input buffer.  Adds a terminating '\0' to the
                            output string.  Will return a zero-length string if
                            that's all that's left, (and won't fail).  In other
                            words, %S never fails if you get to it, unless you
			    run out of room in the output string.  "%'S" will
                            strip off a layer of quoting while it gobbles, if
                            you need it to.

  %r            char **     "Rest of the string".  Sets the pointer to point
                            to the portion of the input string beyond this
                            point.  This is useful, for instance, to check
                            whether there was any leftover input in the string
                            beyond a certain point.  Applying maximum
                            field-width to this construct is meaningless.
                            Applying assignment suppression to this construct
                            is not useful.  %r never fails, if you get to it.

  %R            char **     "Rest of the string, skipping to the next line".
                            Equivalent to specifying
                            "%*( \t)%*[\r\n]%*( \t)%r" or "%~%*[\r\n]%~%r".
                            We can use this to go onto the next Prospero
                            command in a Prospero protocol packet.

  %~            none        This conversion is automatically suppressed.
                            Equivalent to "%*( \t)".  Used to skip over
                            optional leading or trailing horizontal whitespace.
                            Ignores field width and buffer size specifiers.

  %c            char *      field_width characters are placed in the position
                            pointed to by the char *.  Default is 1, if no
                            field-width is specified.  (All of the other
                            constructs have an infinite default field-width.)
			    We don't null-terminate the output.  Doesn't take &
			    argument.

  %[ ... ]      char *      Matches the longest non-empty string of input
                            characters from the set between brackets.  A '\0'
                            is added.  [] ... ] includes ']' in the set.

  %[^ ... ]     char *      Matches the longest non-empty string of input
                            characters not from the set within brackets.  A
                            '\0' is added.  [^] ... ] behaves as expected.
                            It will *include* whitespace in the set of
                            acceptable input characters, unless you explicitly
                            exclude whitespace.

                            Note that %s is equivalent to %[^ \t\n\r\v\f]

                            Also, note that %'[^/ \t] will match a string of
                            characters up to the first *unquoted* slash.
                            In Prospero, will use this construct to disassemble
                            multi-component user-level filenames, which may
                            have components with (quoted) slashes.

  %( ... )      char *      Works just like %[, except that it accepts
  %(^ ... )                 zero-length matches.
                            The construct "%*( \t)" is useful for skipping over
                            zero or more whitespace characters at the start of
                            an input line.

  %%            none        Literally matches a '%'.  Does not increment the
                            counter of matches.

  Since the format string itself is a standard null-terminated C string, it
  cannot contain a NUL character (zero character).
*/

/*
  Implementation notes:

  This routine was written to be fast above all else; it is used to
  parse Prospero commands.  Therefore, there's a lot of in-line code and
  macros that I would have put into a separate function if I'd been working
  under different constraints.

  I also do a small amount of loop unrolling to avoid excessive tests.

  I really must apologize to the reader; I find this stuff pretty thick to get
  through.  I'm sorry.  The charset implementation is not great, and could
  be made more efficient.  We could also pre-build the charset for '%s', and
  save about 300 instructions per use of %s.  That would be a good quick
  speedup.

  The GNU C implementation of readc() and incc() is reasonably efficient, but
  the easiest translation to a non-GNU C system involves making the inline
  functions into static functions, and all variables they reference into file
  static variables.  Totally gross, and much less efficient to boot.

  One may #define NDEBUG to remove some internal consistency checking code and
  to remove code that checks for malformed format strings.
*/

/*
   ** Maintainer **
   Yes, we are actually maintaining qsprintf() and qsscanf() as part of the
   Prospero project.  We genuinely want your bug reports, bug fixes, and
   complaints.  We figure that improving qsprintf()'s portability will help us
   make all of the Prospero software more portable.  Send complaints and
   comments and questions to bug-prospero@isi.edu.
*/

#include <gl_threads.h>        /* for definitions of thread stuff, below. */
#include <gostlib.h>		/* prototypes for functions used -- stcopyr,
				   etc.  */
#include <stdarg.h>             /* ANSI variable arguments facility. */

#include <gl_parse.h>             /* for definition of INPUT & prototype for
				     vqslscanf(). */
#include "gl_charset.h"            /* character set stuff; shared with qsprintf()
                                   */
#include <ctype.h>


/* The next macro only works on ASCII systems.  It also involves a subtraction
   operation, which is likely to be as efficient as a table lookup, so I won't
   rewrite it. */

#ifdef __GNUC__
static inline int chartoi(char c)
{
    return c - '0';
}
#else
#define chartoi(c)  ((c) - '0')
#endif

/* being in a quotation is an automatic match, since we can't break up
   quotations across fields, EXCEPT that EOF is always a failure to match
   (otherwise we might run off the end of the string.) */

#ifdef __GNUC__
#define match(cs, c)    _match((cs), (c), active_quoting)

static inline int 
_match(charset cs, int c, int active_quoting)
{
    return  c != EOF && (active_quoting || is_in_charset(cs,(c)));
}
#else
/* This macro uses its argument twice.  That should be OK. */
#define match(cs, c)    ((c) != EOF && (active_quoting || is_in_charset(cs,(c))))
#endif

static int NO_SIDE_EFFECTS strsetspn(charset cs, const char *str, const char *toofar);
static int NO_SIDE_EFFECTS qstrsetspn(
    charset cs, const char *str, const char *toofar, int active_quoting);

#define incc() _incc(&str, toofar, respect_quoting, &active_quoting)
#define build_charset(cs, endc) _build_charset(&(cs), (endc), &fmt)

/* Names changed from in to _in and from respect_quoting to _respect_quoting in order
   to avoid getting bogus GCC warnings with -Wshadow under
   gcc version 2.5.8:
   lib/pfs/vqscanf.c:470: warning: declaration of `in' shadows a parameter
   lib/pfs/vqscanf.c:470: warning: declaration of `respect_quoting' shadows previous local
   */

static void _incc(const char **sp, const char * const toofar, int _respect_quoting, int *active_quotingp);
static void _build_charset(charset *csp,  char endc, const char **fmtp);





/* Might return EOF or a character. */
/* When called, str might well be past 'toofar'.  
   We do not specially flag malformed quoting, other than by getting an eof in
   the middle of a string. */
/* Use GL_VQSSCANF_READC_SPECIALLY_INCREMENTS; it makes vqslscanf() faster by
   over 1.025 times. (ok, not a lot, but still... :) */
#define GL_VQSLSCANF_READC_SPECIALLY_INCREMENTS
/* COMPILATION OPTION; not yet ready. */

#ifndef GL_VQSLSCANF_READC_SPECIALLY_INCREMENTS
/* UNMAINTAINED VERSION */
#define GL_VQSSCANF_FULLY_INDEPENDENT_INCC /* needs this one */
#undef GL_VQSSCANF_INCC_ONLY_AFTER_ONE_OR_MORE_CALLS_TO_READC

#define readc() _readc(str, toofar, respect_quoting, active_quoting)
static int NO_SIDE_EFFECTS _readc(const char *str, const char *toofar, int _respect_quoting, int active_quoting);
/* Here, we do not increment the input stream while reading.  */
static int
GL_INLINE
NO_SIDE_EFFECTS
_readc(register const char *str, register const char * const toofar, 
       int respect_quoting, 
       register int active_quoting)
{
    if (str >= toofar)
	return EOF;
    if (!respect_quoting)
        return *str;
 redo:
    if (*str != '\'')
        return *str;
    /* *str == '\'' */
    if (!active_quoting) {
        ++active_quoting;	/* set it to 1; maybe that would be faster?
			   Who knows.  WE'll try it on diff. compilers
			   one day.  --swa, 9/29/97 */
	/* Processed the quotation-starting ' ; pass over it now. */
        if (++str >= toofar)
	    return EOF;
        goto redo;
    }
    /* We *are* quoting, & on top of a '\'' */
    if (str + 1 < toofar && str[1] == '\'') { /* examine next char. */
        return '\''; /* do NOT advance input stream, since we might call
                        readc() again. */
    } else {
	/* didn't get two successive ' marks; still on top of one -- must turn
	   off quoting now.  */ 
#if 0
	/* this can be commented out, because this is the function-valued
	   version, and the last mod will not make any difference. */
	active_quoting = 0; 
#endif
	
        return *++str;  /* quoting gone; return real char.  */
    }
}

#else
/* ACTIVELY MAINTAINED VERSION, 12/97 */
/* This is the side-effecting _readc().  
   According to time tests I ran on 10/1/97, it alone leads to a speedup 
   of 1.025 in dsrobject(), reading from a 100-entry file. */

#define GL_VQSSCANF_INCC_ONLY_AFTER_ONE_OR_MORE_CALLS_TO_READC
#define readc() _readc(&str, toofar, respect_quoting, &active_quoting)
static int NO_SIDE_EFFECTS _readc(const char **sp, const char *toofar, int _respect_quoting, int *active_quotingp);

static int
GL_INLINE
NO_SIDE_EFFECTS			/* This may be a lie.  What is true, however,
				   is that it won't have side-effects,
				   irrespective of the order we call it in.
				   Interleaved computation, on the other
				   hand, would be a Bad Thing. */
_readc(register const char **sp, register const char * const toofar, 
       int respect_quoting, 
       register int *active_quotingp)
{
    if (*sp >= toofar)
	return EOF;
    if (!respect_quoting)
        return **sp;
 redo:
    if (**sp != '\'')
        return **sp;
    /* **sp == '\'' */
    if (!*active_quotingp) {
        ++*active_quotingp;	/* set it to 1; maybe that would be faster?
			   Who knows.  WE'll try it on diff. compilers
			   one day.  --swa, 9/29/97 */
	/* Processed the quotation-starting ' ; pass over it now. */
        if (++*sp >= toofar)
	    return EOF;
        goto redo;
    }
    /* We *are* quoting, & on top of a '\'' */
    if (*sp + 1 < toofar && (*sp)[1] == '\'') { /* examine next char. */
        return '\''; /* do NOT advance input stream, since we might call
                        readc() again. */
    } else {
	/* didn't get two successive ' marks; still on top of one -- must turn
	   off quoting now.  */ 
        *active_quotingp = 0; 
        return *++*sp;  /* quoting gone; return real char.  */
    }
}



#endif

/* Check for consistency. */
#if !defined(GL_VQSSCANF_FULLY_INDEPENDENT_INCC) \
    && !defined(GL_VQSSCANF_INCC_ONLY_AFTER_ONE_OR_MORE_CALLS_TO_READC)
#error bad #defines
#endif

#ifdef GL_VQSSCANF_FULLY_INDEPENDENT_INCC
/* 9/30/97: I have rewritten _incc() so that it can be called irrespective of
   whether _readc() was ever called.  This makes it independent of _readc(). 
   This does, however, impose efficiency penalties.  We'll run more experiments
   later. */
static void
GL_INLINE
_incc(const char ** sp, const char * const toofar, 
      int respect_quoting, int *active_quotingp)
{
    /* Skip single ', if it's there. */
    if (!respect_quoting) {
        ++*sp;            /* easy case :) */
    } else if (*sp < toofar) {
        if (**sp == '\'') {
            if (!(*active_quotingp)) {
		/* turn on quoting if we're entering a quotation. */
                ++(*active_quotingp);
                ++*sp;
		/* We're now past the turn-on-quoting quotation mark.  
		   If the next character is another quotation mark, we handle
		   it specially; otherwise, we can just increment past the
		   'real character', the one that _readc() previously returned
		   to the caller of this function. */
		if (*sp < toofar && **sp != '\'') {
		    goto increment_past_real_character;
#if 0
		    ++*sp;
		    return;	/* we've incremented past; no further to go. */
#endif
		}
            }
            /* We're in a quotation (active_quoting == 1) && we're on
               top of a quotation mark.  Still need to increment
               past the 'real' character (the one that readc has already
	       returned). */
            assert(*active_quotingp);
            if (*sp + 1 < toofar && (*sp)[1] == '\'') {
		*sp += 2;	/* we skip over the '' */
                /* When inside a quotation, the two-character
                   sequence '' is treated as one character */
            } else {
                /* We're on top of a quotation mark that must close
                   the current quoted section.  Go past it.  */
                /* Additionally, skip over the character following the
		   quotation mark; that is the one that _readc()
		   must have returned before the user called _incc(). */
		/* This character (the one we skip over) is not a quotation
		   mark. */ 
		/* The other possibility is that the mark we're on top of is
		   the end of the input string.  In that case, we handle it the
		   same way, by incrementing.  By incrementing, we push *sp
		   past 'toofar', and future calls to in_readc() will return
		   EOF. */ 
		*sp +=2;
                (*active_quotingp) = 0;
	    }
	} else { /* We're not on a single quote.  Just increment past this
		    character. . */
	increment_past_real_character:
            ++*sp;
	    if(*active_quotingp) {
		/* At this point, we're past the character that we were trying
		   to increment past.  However, we might now encounter a
		   quoting-terminating single-quote.  If we encounter one, we
		   need to move past it and reset active_quotingp
		   appropriately.  Otherwise, a subsequent call to _readc()
		   could return a non-matching character that we incorrectly
		   match.  (This is because _match() always matches non-EOF
		   characters if we are actively quoting.)
		   
		   This change was not necessary before because _readc() used
		   to be a side-effecting function (and might still be one
		   day).  --swa, 9/97 */
		if (*sp + 1 == toofar && **sp == '\'') {
		    /* This single quote terminates the input string. */
		    *active_quotingp = 0;
		    /* Don't need it in the read buffer any more. */
		    ++*sp;
		} else if (*sp + 1 < toofar && **sp == '\'' && (*sp)[1] != '\'') {
		    /* We've encountered a non-doubled terminator of
                       quoting. */
		    *active_quotingp = 0;
		    ++*sp;
		    /* We are now no longer actively quoting and are on top of
		       the next character _readc() will return. */
		}
	    } /* (*active_quotingp) */
        }
    } else {
	/* We're too far. */
	/* (Since we're already too far, we certainly don't need to increment
	   any more!) */ 
	assert(*sp >= toofar);
    }

}
#endif /* #ifdef GL_VQSSCANF_FULLY_MODULAR_INCC */

#if defined(GL_VQSSCANF_INCC_ONLY_AFTER_ONE_OR_MORE_CALLS_TO_READC)
/* copied from 1994 vqsscanf(), then modified. */
/* quote is respect_quoting, am_quotingp is active_quotingp */
static 
inline
void
_incc(const char **sp, const char * const toofar,
      int quote, int *am_quotingp)
{

    /* Skip single ', if it's there. */
    if (!quote)
        ++*(sp);
    else {
    redo:
	if (*sp >= toofar)
	    return;
        if ((*sp)[0] == '\'') {
            if (!(*am_quotingp)) {
                ++(*am_quotingp);
                ++*(sp);
                goto redo;
            }
            /* We're in a quotation  */
            if (*sp + 1 < toofar && (*sp)[1] == '\'') {
                (*sp) += 2; /* '' is one character */
            } else { 
                ++(*sp);
                (*am_quotingp) = !(*am_quotingp);
            }
        } else { /* Quoting enabled, but we're not on a single
                    quote.  Just increment. */
            ++(*sp);
        }
    } /* if (!quote) */
}

#endif /* defined(GL_VQSSCANF_INCC_ONLY_AFTER_ONE_OR_MORE_CALLS_TO_READC) */



/* strsetspn() returns the length of the initial segment of a string whose
   characters are in set CS.  The characters of the string start at STR and
   ends just before TOOFAR. */
static int
GL_INLINE
NO_SIDE_EFFECTS
strsetspn(charset cs, register const char *str, register const char *toofar)
{
    register int retval = 0;

    while (str < toofar && is_in_charset(cs, *str)) {
        ++retval;
        ++str;
    }
    return retval;
}

/* qstrsetspn() returns the length of the initial segment of s whose unquoted
   characters are in set CS.  -1 is returned if the quoting is ill-formed.
   The characters of the string start at STR and end just before TOOFAR */
static int
GL_INLINE
NO_SIDE_EFFECTS
qstrsetspn(charset cs, register const char *str, register const char *toofar, int active_quoting)
{
    /* code swiped from qindex() */
    int count = 0;

    enum { OUTSIDE_QUOTATION, IN_QUOTATION,
               SEEN_POSSIBLE_CLOSING_QUOTE } state;

    state = active_quoting? IN_QUOTATION : OUTSIDE_QUOTATION;

    for (; str < toofar; ++str) {
        switch (state) {
        case OUTSIDE_QUOTATION:
            if (*str == '\'')
                state = IN_QUOTATION;
            else if (is_in_charset(cs, *str)) {
                ++count;
            } else  {
                return count;   /* failure to match. */
            }
            break;
        case IN_QUOTATION:
            if (*str == '\'')
                state = SEEN_POSSIBLE_CLOSING_QUOTE;
            else
                ++count;
            break;
        case SEEN_POSSIBLE_CLOSING_QUOTE:
            if (*str == '\'') {
                ++count;
                state = IN_QUOTATION;
            } else {
                state = OUTSIDE_QUOTATION;
                if (!is_in_charset(cs, *str))
                    return count;
                ++count;
            }
            break;
#ifndef NDEBUG
        default:
            internal_error("strsetspn(): impossible state!");
#endif
        }
    }
    if (state == IN_QUOTATION)  /* unbalanced quoting */
        return -1;
    return count;
}


/* NB: Format strings should never have a '\0' in them.   They are always null
   terminated */
#ifndef NDEBUG
#define check_null(fmtc) if ((fmtc) == '\0') \
gl_function_arguments_error("improperly specified character set given as qsscanf() format")
#else
/* Assume format strings are always properly formed.   This is reasonable
behavior, since it's a programming error to submit a malformed format
string. */
#define check_null(fmtc)
#endif

static void
GL_INLINE
_build_charset(charset *csp, char endc, const char **fmtp)
{
    int negation;
    if (*++(*fmtp) == '^') {
        negation = 1;
        new_full_charset(*csp);
	/* We deliberately leave '\0' in these negated character sets, since 
	   Prospero now supports attribute values that contain null bytes. --swa */
#if 0
        remove_char(*csp, '\0'); /* don't ever want to match \0 as being
                                    valid. */
#endif
        remove_char(*csp, *++(*fmtp));
    } else {
        negation = 0;
        new_empty_charset(*csp);
        add_char(*csp, *(*fmtp));
    }
    check_null(*(*fmtp));
    while (*++(*fmtp) != endc) {
        check_null(*(*fmtp));
        if (negation)
            remove_char(*csp, *(*fmtp));
        else
            add_char(*csp, *(*fmtp));
    }
    /* (*fmtp) now points to closing bracket or paren.  Done! */
}
#undef check_null




int
vqslscanf(const char *str, ptrdiff_t len, const char *fmt, va_list ap)
/* in: source
   fmt: format describing what to scan for.
   remaining args: pointers to places to store the data we read, or
      integers (field widths).
*/
{
    int nmatches = 0;           /* no assignment-producing directives matched
                                   so far! */
    const char * const toofar = str + len; /* one byte too far */

    for (;;) {			/* check current format character */
        /* Each case in this switch statement is responsible for leaving fmt
           and s pointing to the next format and input characters to process.
        */
        switch (*fmt) {
        case ' ':
        case '\t':
            if (*str != ' ' && *str != '\t')
                goto this_match_fails;      /* must match at least 1 space; failure
                                   otherwise  */
            do { /* eat up any remaining spaces in the input */
                ++str;
            } while (str < toofar && (*str == ' ' || *str == '\t'));
            /* Eat up any remaining spaces in the format string and leave it
               poised at the next formatting character */
            while (*++fmt == ' ' || *fmt == '\t')
                ;
            break;
        case '%':
            /* This is the big long part! Handle the conversion specifiers. */
        {
            int use_long = 0;   /* Use long instead of int for %d conversion?
				 */
            int use_short = 0;   /* Use short instead of int for %d?  */
            int respect_quoting = 0;      /* quoting modifier? */
	    int suppress = 0;   /* suppression modifier? */
	    int maxfieldwidth = 0; /* max field width specified?  0 would be
                                      meaningless; if the user does specify 0,
                                      that would be strange, and I don't know
                                      what it would mean.  It will be ignored.
                                      */
            /* outbuf_size could be set to this too: */
#define READ_FROM_NEXT_ARGUMENT ((size_t) -1)
	    /* We need to be able to do subtraction and compute valid negative
	       numbers on this, so a size_t (unsigned on every architecture I
	       know) is inappropriate. */
            long outbuf_size = 0; /* output buffer size */
            int seen_underscore = 0; /* How many underscores have we seen? */
            int seen_bang = 0;  /* How many bangs ('!') have we seen? */
            int seen_dollar = 0;  /* How many dollar signs ('$') have we seen? */
	    int seen_ampersand = 0; /* How many ampersands ('&') have we
                                       seen? */
	    int active_quoting = 0; /* Are we actively in the middle of a
				   quotation?  */


        more:
            switch(*++fmt) {
                /* Process the modifiers (options) */
            case '\'':
                ++respect_quoting;
                goto more;
            case '_':
#ifndef NDEBUG
                if (seen_bang)
                    gl_function_arguments_error("qsscanf(): can't use ! and _ modifiers \
together");
                if (seen_dollar)
                    gl_function_arguments_error("qsscanf(): can't use $ and _ modifiers \
together");
                if (seen_underscore > 1)
                    gl_function_arguments_error("qsscanf(): can't use > 2 underscore\
  modifiers together.");
#endif
                if (seen_underscore++)
                    outbuf_size = READ_FROM_NEXT_ARGUMENT;
                goto more;

            case '!':
#ifndef NDEBUG
                if (seen_underscore)
                    gl_function_arguments_error("qsscanf(): can't use ! and _ modifiers \
together");
                if (seen_dollar)
                    gl_function_arguments_error("qsscanf(): can't use $ and ! modifiers \
together");
                if (seen_bang > 1)
                    gl_function_arguments_error("qsscanf(): can't use > 2 bang\
  modifiers together.");
#endif
                if (seen_bang++)
                    outbuf_size = READ_FROM_NEXT_ARGUMENT;
                goto more;
            case '$':
#ifndef NDEBUG
                if (seen_underscore)
                    gl_function_arguments_error("qsscanf(): can't use $ and _ modifiers \
together");
                if (seen_bang)
                    gl_function_arguments_error("qsscanf(): can't use $ and ! modifiers \
together");
                if (seen_dollar > 1)
                    gl_function_arguments_error("qsscanf(): can't use > 2 dollar sign\
  modifiers together.");
#endif
                if (seen_dollar++)
                    outbuf_size = READ_FROM_NEXT_ARGUMENT;
                goto more;
            case '&':
                seen_ampersand++;
                goto more;
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                if (seen_underscore == 1 || seen_bang == 1 || seen_dollar == 1)
                    outbuf_size = outbuf_size * 10 + chartoi(*fmt);
                else
                    maxfieldwidth = maxfieldwidth * 10 + chartoi(*fmt);
                goto more;
            case 'h':
                ++use_short;
                goto more;
            case 'l':
                ++use_long;
                goto more;
            case '*':
                ++suppress;
                goto more;


                /* commands */
            case '%':           /* literal match */
                if (seen_ampersand)
                    gl_function_arguments_error("& modifier cannot be combined \
with %% conversion.");
                if (*str != '%')
                    goto this_match_fails;
		++str;
                break;
            case '~':
            {
                int r;
                if (seen_ampersand)
                    gl_function_arguments_error("& modifier cannot be combined \
with %~ conversion.");
                /* Match zero or more whitespace characters. */
                while ((r = readc()) == ' ' || r == '\t')
                    incc();
            }
                break;
            case 'R':
            {
                register int r;
                if (seen_ampersand)
                    gl_function_arguments_error("& modifier cannot be combined \
with %R conversion.");
                /* strip trailing whitespace from previous line. */
                while ((r = readc()) == ' ' || r == '\t')
                    incc();
                /* skip the newline character.  Be generous and accept \r. 
		   Be even more generous and accept blank lines. */
                if (r != '\n' && r != '\r')
                    goto this_match_fails;
                do {
                    incc();
                } while ((r = readc()) == '\n' && r == '\r');
                /* Skip leading whitespace from next line. */
                /* This could be rewritten to eliminate a call to readc(). */
                while (r == ' ' || r == '\t') {
                    incc();
		    r = readc();
		}
            }
                /* DELIBERATE FALLTHROUGH */
            case 'r':           /* ptr. to rest of string */
                if (seen_ampersand)
                    gl_function_arguments_error("& modifier cannot be combined \
with %r conversion.");
                if (!suppress) { /* suppression is really stupid in this case,
                                    but we'll support stupidity. */
		    /* cast to char * to throw away CONST */
		    *va_arg(ap, char **) = (char *) str;
                    ++nmatches;
                }
                break;
            case 'c':           /* char */
		/* Output need be only one char wide. */
            {
                char *out = 0; /* assignment quiets gcc -Wall */
		register int r;	/* current read character.  Need to buffer this
				 since we must call readc() before checking
				 against 'toofar'.*/

                if (seen_ampersand)
                    gl_function_arguments_error("& modifier cannot be combined \
with %c conversion.");
                if (!suppress)
                    out = va_arg(ap, char *);
                if (outbuf_size == READ_FROM_NEXT_ARGUMENT)
                    outbuf_size = va_arg(ap,int);
                else if (outbuf_size == 0)
                    outbuf_size = -1; /* ignore outbuf size by default */
                if (maxfieldwidth < 1)
                    maxfieldwidth = 1; /* Read 1 character by default. */
		r = readc();
		if (str >= toofar)
                    goto this_match_fails;  /* must match at least 1 character. */
                do {
                    if (!suppress)
                        *out++ = r;
                    incc();
		    r = readc();
                    if (outbuf_size-- == 0) {
                        if (seen_dollar)
                            goto this_match_fails;
                        else if (seen_bang)
                            return GL_IO_BUFFER_OVERFLOW;
                        else
                            break; /* on to the next field */
                    }
                } while (--maxfieldwidth && str < toofar);
                if (!suppress)
                    ++nmatches;
            }
		break;


            case 'd':
	    {
                long d = 0;      /* decimal # we're generating. */

                /* Use these 2 definitions to check for overflow. */
                /* Save the last return from readc() for reuse.  We must use an
                   explicit temporary variable because the compiler won't know
                   that readc() always returns the same value without an
                   intervening incc(). */
                int negative;   /* non-zero if negative #. */

                register int r;

		long div, mod;

                if (seen_ampersand)
                    gl_function_arguments_error("& modifier cannot be combined \
with %d conversion.");

		if (use_long) {
		    div = LONG_MAX / 10;
		    mod = LONG_MAX % 10;
		} else if (use_short) {
		    div = SHRT_MAX / 10;
		    mod = SHRT_MAX % 10;
		} else {
		    div = INT_MAX / 10;
		    mod = INT_MAX % 10;
		}


                if ((r = readc()) == '-')
                    negative = -1, incc(), r = readc();
                else
                    negative = 0;
                if (!isdigit(r))
                    goto this_match_fails;
                do {
                    register i = chartoi(r);

                    if (d > div || (d == div && i > mod)) {
                        /* Integer overflow! */
                        if (seen_dollar)
                            goto this_match_fails; /* failure to match */
                        else if (seen_underscore)
                            break; /* conversion done */
                        else
                            return -1; /* abort */
                    }
                    d = d * 10 + i;
                    incc();
                } while ((r = readc()) && --maxfieldwidth && isdigit(r = readc()));
                /* s points to the next non-digit now. */
                if (!suppress) {
		    if (use_long)
			*va_arg(ap, long *) = (negative ? -d : d);
		    else if (use_short)
			*va_arg(ap, short *) = (negative ? -d : d);
		    else
			*va_arg(ap, int *) = (negative ? -d : d);
                    ++nmatches;
                }
            }
		break;

            case '(':
            {
                char *out = 0; /* assignment quiets gcc -Wall */      /* output buffer */
                register int r;
                charset cs;

                if (seen_ampersand)
                    gl_function_arguments_error("& modifier cannot currently be combined \
with %( conversion.");
                if (!maxfieldwidth)
                    maxfieldwidth = -1;
                if (!suppress)
                    out = va_arg(ap, char *);
                if (outbuf_size == READ_FROM_NEXT_ARGUMENT)
                    outbuf_size = va_arg(ap,int);
                build_charset(cs, ')');

                /* don't have to match any characters. */
                for (; maxfieldwidth-- && (r = readc(), match(cs, r)); incc()) {
                    if (--outbuf_size == 0) {
                        if (seen_dollar)
                            goto this_match_fails;
                        else if (seen_bang)
                            return GL_IO_BUFFER_OVERFLOW;
                        else
                            break;
                    }
                    if (!suppress)
                        *out++ = r;
                }
                /* if we stopped while still quoting, there are problems! */
                if (active_quoting)
                    goto this_match_fails;
                if (!suppress)
                    *out = '\0', ++nmatches;
            }
                break;
            case '[':
            {
                char *out = 0; /* assignment quiets gcc -Wall */      /* output buffer */
                char **outp = 0; /* assignment quiets gcc -Wall */     /* Pointer to place to stash output. */
                register int r;
                charset cs;

                if (suppress) {
                    seen_ampersand = 0; /* ignore the seen_ampersand flag */
                } else {
                    if (seen_ampersand) {
                        outp = va_arg(ap, char **);
                    } else {
                        out = va_arg(ap, char *);
                    }
                }
                if (outbuf_size == READ_FROM_NEXT_ARGUMENT)
                    outbuf_size = va_arg(ap,int);
                if ((suppress || outbuf_size) && seen_ampersand)
                    gl_function_arguments_error("qsscanf(): Specifying an output buffer \
size or suppression  and the ampersand conversion together is ridiculous. You \
don't know what you're doing.");
                build_charset(cs, ']');

                /* must match at least 1 character if not quoting.  If quoting,
                   we might see the (quoted) null string. */
                /* Treat the quoted null string (also, by the way, a
                   common case) as a special case. */
		/* Note that we must test this case before we call readc();
		   readc() would skip over the quoted null string. */
                if (respect_quoting && str + 2 < toofar && *str == '\''
                    && str[1] == '\''
                    && str[2] != '\''
                    && !match(cs, str[2])) {
                    /* quoted null string. */
		    str += 2;	/* skip '' in null string. */
                    if (!suppress) {
                        ++nmatches;
                        if (seen_ampersand)
                            *outp = stcopyr("", *outp);
                        else
                            *out = '\0';
                    }
                    break;
                }
		r = readc();
                if (str>= toofar || !match(cs, r))
                    goto this_match_fails;      /* did not match any characters */
                if (seen_ampersand) {
                    /* Set out to start of string; we can increment out, but
                       should leave *outp always at the start of the string. */
                    if ((out = *outp) == NULL) {
                        /* passing null pointer to strcpy is legal. */
                        outbuf_size = 1 + (respect_quoting ?
                                           qstrsetspn(cs, str, toofar, active_quoting)
                                           : strsetspn(cs, str, toofar));
                        if (outbuf_size <= 1) /* unbalanced quoting or failed
                                                 to match any characters. */
                            goto this_match_fails;
                        out = *outp = stalloc(outbuf_size);
#ifndef NDEBUG
                        /* should have allocated exactly enough memory for this
                           conversion. */
                        seen_ampersand = -1;
#endif
                    } else {
                        /* stalloc() guarantees not to honor requests for 0 or
                           fewer bytes of memory, so outbuf_size > 0. */
                        outbuf_size = p__bstsize(out);
                        assert(outbuf_size > 0);
                    }
                }
                do {
                    if (--outbuf_size == 0) {
                        if (seen_ampersand) {
                            int oldsize = p__bstsize(*outp);
                            char *oldstart = *outp;

#ifdef ASSERT2                  /* expensive assertion */
                            assert(oldsize == strlen(oldstart) + 1);
#endif
#ifndef NDEBUG
                            /* We should never run through this code twice for
                               the same conversion. */
                            assert(seen_ampersand > 0);
                            seen_ampersand = -1;
#endif
                            /* Don't need room for the trailing null, since
                               the current outbuf_size allocated space for it.
                               */
                            outbuf_size = respect_quoting ?
                                qstrsetspn(cs, str, toofar, active_quoting) :
                                    strsetspn(cs, str, toofar);
                            if (outbuf_size < 1) /* unbalanced quoting */
                                goto this_match_fails;

                            out = *outp = stalloc(outbuf_size + oldsize);
                            strcpy(out, oldstart);
                            out += oldsize - 1;
                        } else if (seen_dollar)
                            goto this_match_fails;
                        else if (seen_bang)
                            return GL_IO_BUFFER_OVERFLOW;
                        else {
                            break;
                        }
                    }
                    if (!suppress) {
                        *out++ = r;
                    }
                    incc();
		    r = readc();
                } while (--maxfieldwidth && str < toofar && match(cs, r));
                /* if we stopped while still quoting, there are problems! */
                if (active_quoting)
                    goto this_match_fails;
#ifndef NDEBUG
                if (seen_ampersand == -1) /* if we allocated just enough memory
                                             to fit */
                    assert(*outp + p__bstsize(*outp) - 1 == out);
#endif
                if (!suppress) {
                    *out = '\0';
                    ++nmatches;
                }
            }
                break;

            case 'b':           /* just like %s, except '\0' is OK */
            case 's':           /* Just like %b, except that encountering '\0'
				   in a string (quoted or not) is a failure to
				   match.  */
		/* This distinction might be implemented, but isn't right now.
		   At this moment, '\0' is a character, just like any other,
		   and never triggers such a failure to match. */
            {
                static charset nw_cs; /* character set for not-whitespace
					 characters. */
                static int nw_cs_initialized = 0;
                register int r;
                char *out = 0; /* assignment quiets gcc -Wall */      /* output buffer */
                char **outp = 0; /* assignment quiets gcc -Wall */     /* Pointer to place to stash output. */
                int expected_inputlen;


                /* Have we already built a charset for %s?  If not, build it
                   now and flag it as having been initialized. */
                if (!nw_cs_initialized) {
#ifdef GL_THREADS
                    pthread_mutex_lock(&(p_th_mutexPFS_VQSCANF_NW_CS));
#endif /* GL_THREADS */
                    new_full_charset(nw_cs);
                    remove_char(nw_cs, '\n');
                    remove_char(nw_cs, ' ');
                    remove_char(nw_cs, '\t');
                    remove_char(nw_cs, '\r');
                    remove_char(nw_cs, '\v');
                    remove_char(nw_cs, '\f');

                    ++nw_cs_initialized;
#ifdef GL_THREADS
                    pthread_mutex_unlock(&(p_th_mutexPFS_VQSCANF_NW_CS));
#endif /* GL_THREADS */
                }
                if (suppress) {
                    seen_ampersand = 0; /* ignore the seen_ampersand flag */
                } else {
                    if (seen_ampersand) {
                        outp = va_arg(ap, char **);
                    } else {
                        out = va_arg(ap, char *);
                    }
                }
                if (outbuf_size == READ_FROM_NEXT_ARGUMENT)
                    outbuf_size = va_arg(ap,int);
                if ((suppress || outbuf_size) && seen_ampersand)
                    gl_function_arguments_error(
			"qsscanf(): Specifying an output buffer size or"
			" suppression  and the ampersand conversion together"
			" is ridiculous. You don't know what you're doing.");
                /* must match at least 1 character if not quoting.  If quoting,
                   we might see the (quoted) null string. */
                /* Treat the quoted null string (also, by the way, a
                   common case) as a special case. */
                if (respect_quoting && str + 1 < toofar && *str == '\''
                    && str[1] == '\''
                    && (str + 2 >= toofar || 
			(str[2] != '\'' && !match(nw_cs, str[2])))) {
                    /* quoted null string. */
		    str += 2;	/* skip '' in string.  */
                    if (!suppress) {
                        ++nmatches;
                        if (seen_ampersand)
                            *outp = stcopyr("", *outp);
                        else
                            *out = '\0';
                    }
                    break;	/* done with this switch case -- on to next
				   format character. */
		}
		r = readc();	/* prepare to test STR against TOOFAR */
		if (str >= toofar)
		    goto this_match_fails;
                if (!match(nw_cs, r ))
                    goto this_match_fails;      /* FAIL; did not match any characters */
                expected_inputlen = 
		    respect_quoting ? qstrsetspn(nw_cs, str, toofar, active_quoting) 
		    : strsetspn(nw_cs, str, toofar);
                if (expected_inputlen <= 0) /* unbalanced quoting or failed
                                                 to match any characters. */
                    goto this_match_fails;
                if (seen_ampersand) {
                    /* Set out to start of string; we can increment out, but
                       should leave *outp always at the start of the string. */
                    /* Stick on the +1 for trailing NUL for safety. */
                    if (expected_inputlen + 1 > p__bstsize(*outp)) {
                        stfree(*outp); /* might be NULL.  Ok if it is; passing
					  NULL to stfree() is legal. */ 
                        outbuf_size = 1 + expected_inputlen;
                        out = *outp = stalloc(outbuf_size);
#ifndef NDEBUG
                        /* should have allocated exactly enough memory for this
                           conversion. */
                        seen_ampersand = -1;
#endif
                    } else {
                        outbuf_size = p__bstsize(out = *outp);
                        /* This assertion is ok, since we tested above. */
                        assert(outbuf_size > 0);
                    }
    
#if 0				/*  XXXX FIX THIS SOON*/
                    gl_set_bstring_length(*outp, expected_inputlen);
#endif
                }
                do {
                    if (--outbuf_size == 0) {
                        if (seen_ampersand) {
                            gl_function_arguments_error("somehow failed to allocate enough \
memory for the output in a %%s or %%b conversion.");
                        } else if (seen_dollar) {
                            goto this_match_fails;
                        } else if (seen_bang) {
                            return GL_IO_BUFFER_OVERFLOW;
                        } else {
                            break;
                        }
                    }
                    if (!suppress)
                        *out++ = r;
                    incc();
		    /* Need to call readc() here so we skip past any closing
		       quotation marks; incc() won't necessarily do it. */
		    r = readc();
                } while (str < toofar && --maxfieldwidth &&  match(nw_cs, r));
                /* if we stopped while still quoting, there are problems! */
                if (active_quoting)
                    goto this_match_fails;
#ifndef NDEBUG
                if (seen_ampersand == -1) /* if we allocated just enough memory
                                             to fit */
                    assert(*outp + p__bstsize(*outp) - 1 == out);
#endif
                if (seen_ampersand)
                    p_bst_set_buffer_length_nullterm(*outp, out - *outp);
                if (!suppress) {
                    *out = '\0';
                    ++nmatches;
                }
            }
                break;
            case 'S':
            {
                register int r;
                char *out = 0; /* assignment quiets gcc -Wall */      /* output buffer */
                char **outp = 0; /* assignment quiets gcc -Wall */     /* Pointer to place to stash output. */

                if (suppress) {
                    seen_ampersand = 0; /* ignore the seen_ampersand flag */
                } else {
                    if (seen_ampersand) {
                        outp = va_arg(ap, char **);
                    } else {
                        out = va_arg(ap, char *);
                    }
                }
                if (seen_ampersand)
                    gl_function_arguments_error("& modifier cannot currently be combined \
with %S conversion.");
                if (outbuf_size == READ_FROM_NEXT_ARGUMENT)
                    outbuf_size = va_arg(ap,int);
                if ((suppress || outbuf_size) && seen_ampersand)
                    gl_function_arguments_error("qsscanf(): Specifying an output buffer \
size or suppression  and the ampersand conversion together is ridiculous. You \
don't know what you're doing.");

		for (;;) {
		    r = readc();
		    if (!(--maxfieldwidth && str < toofar))
			break;
                    if (--outbuf_size == 0) {
                        if (seen_dollar)
                            goto this_match_fails;
                        else if (seen_bang)
                            return GL_IO_BUFFER_OVERFLOW;
                        else
                            goto conversion_S_success;
                    }
                    if (!suppress) {
                        *out++ = r;
                    }
                    incc();
                }
                /* if we stopped while still quoting, there are problems! */
                if (active_quoting)
                    goto this_match_fails;
                if (!suppress)
                    *out = '\0', ++nmatches;
            }
	    conversion_S_success:
                break;

#ifndef NDEBUG
            default:
                gl_function_arguments_error("malformed format string passed to qsscanf()");
                /* NOTREACHED */
#endif
            }
            ++fmt;
        }                       /* end of case '%' */
            break;


        case '\0':              /* no more format specifiers to match. */
            goto done;
        default:                /* literal character match */
            if (*fmt++ != *str)
                goto this_match_fails;
            ++str;
        }
    }

 done:
 this_match_fails:
    return nmatches;
}




#endif /* GL__STRING_PARSE */
