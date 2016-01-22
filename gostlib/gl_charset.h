#ifndef GL_CHARSET_H_INCLUDED
#define GL_CHARSET_H_INCLUDED
/* charset.h */
/* Written by Steven Augart <swa@isi.edu> July, 1992 */
/* This implements a simple character set recognizer. */
/* It is used by qsprintf.c and qsscanf.c */
/* must include limits.h to use this. */
#include <limits.h>		/* need UCHAR_MAX */
/* Must include <stdio.h> to use this, since we need EOF. */
#include <stdio.h>

/* These macros define an abstraction for a set of characters where one can add
   members, remove them, and test for membership.  There are several ways in
   which this might be made more efficient, but I'm tired. */

#include <string.h>
#include <stdlib.h>

#if 0				/* We try other things now. */
/* <string.h> should include a definition of memset(), but the one on our
   system doesn't have it, so I have to define memset below. */ 
extern void *memset(void *, int, size_t);
#endif

#ifdef GL_CHARSET_SAVE_SPACE
typedef char charset[UCHAR_MAX + 1];    /* what we'll work on */
#else
/* this will optimize for time */
typedef int charset[UCHAR_MAX];    /* what we'll work on */
#endif
#define new_full_charset(cs)      do { memset((cs), 1, sizeof (cs)); \
                                           /* cs['\0'] = 0; */} while (0)
#define new_empty_charset(cs)     memset((cs), 0, sizeof (cs))
#define add_char(cs, c)             do {    \
         assert(c != EOF);                 \
         ((cs)[(unsigned char) (c)] = 1);   \
     } while (0)
#define remove_char(cs, c)          ((cs)[(unsigned char) (c)] = 0)
#define is_in_charset(cs,c)        (((c) != EOF) && (cs)[(unsigned char) (c)])

#endif /* GL_CHARSET_H_INCLUDED*/
