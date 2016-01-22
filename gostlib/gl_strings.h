#ifndef GL_STRINGS_H_INCLUDED
#define GL_STRINGS_H_INCLUDED

#include <stdarg.h>		/* for va_alist */
#include <stddef.h>		/* for size_t */
#include <gl_threads.h>	/* for pthread_mutex_t definition. */

/* Interfaces to string handling functions */
/* Returns the # of bytes it *wanted* to write. */
size_t qsprintf(char *buf, size_t buflen, const char fmt[], ...);
extern char *qsprintf_stcopyr(char *buf, const char *fmt, ...);
extern char *qsprintf_GSP(char **bufp, const char *fmt, ...);
extern char *stalloc(int size);
extern void stalloc_GSP(int nbytes, char **gp);
extern char *stcopy(const char *s);
extern char *stcopyr(const char *, char *); 
/* The prototype immediately preceding another prototype?  This is weird but
   will work. */
extern char * stcopy_GSP(const char *s, char **gp);
extern inline char * 
stcopy_GSP(const char *s, char **gp)
{
    if (gp)
	return *gp = stcopyr(s, *gp);
    else
	return stcopy(s);
};
extern void stfree(void *s);
extern int string_count;
extern int string_max;
extern size_t strnlen(const char s[], const int maxdist);
extern size_t vqsprintf(char *buf, size_t buflen, const char fmt[], va_list);
extern char *vqsprintf_stcopyr(char *buf, const char *fmt, va_list ap);
extern char *vqsprintf_GSP(char **bufp, const char *fmt, va_list ap);

#define GL_STFREE(s) do {					\
    if (s)							\
	stfree(s);						\
    (s) = NULL;							\
} while(0)

/* Code for bstrings */
/*
 *     The string format we use has the size of the data are encoded as an
 *     integer starting sizeof (int) bytes before the start of the string.
 *     When safety checking is in place, it also has a consistency check
 *     encoded as an integer 3* sizeof (int) bytes before the start of the
 *     string.
 *
 *     The stsize() macro in <pfs.h> gives the size field of the string.
 */

/*     The bst_consistency_fld() macro is a reference to the consistency check
 *     field.   bst_size_fld() is a reference to the size of the buffer.
 *     bst_length_fld() is a reference to the length of the data.  A
 *     distinguished value of the maximum possible unsigned integer (UINT_MAX
 *     in <limits.h>) 
 *     indicates that the null termination convention should be used to 
 *     check for string length. 
 */     
#include <limits.h>

/* #defining this turns on extra consistency checking code in the various
   memory allocators that checks for double freeing.  Turn on this compilation
   option to make sure you are not making this common mistake. 
   You can turn it off for slight added speed once your code is stable. 
   */
/* This consistency checking mechanism is used by the GOST, ARDP, and PFS
   libraries. as well as by general Prospero data structures.  The only
   exception is the RREQ structure (in libARDP), which does consistency
   checking differently. */ 
#ifndef NDEBUG
#define P_ALLOCATOR_CONSISTENCY_CHECK 
#endif
#ifdef P_ALLOCATOR_CONSISTENCY_CHECK /* patterns to set the 'consistency' member
                                      to.  */
enum p__consistency {
    UNINITIALIZED = 0x0,        /* should never appear */
    P__FREE_PATTERN = 0xf4ee, /* pattern to use if memory is now free. */
    P__INUSE_PATTERN = 0X0100b15e,  /* Pattern to set consistency member to
                                       if memory is in use. */
    /* Any other patterns are utterly bogus. */
};
#endif /* P_ALLOCATOR_CONSISTENCY_CHECK */

#define p__bst_size_fld(s) (((unsigned int *)(s))[-1])
#define p__bst_length_fld(s) (((unsigned int *)(s))[-2])
#define P__BST_LENGTH_NULLTERMINATED UINT_MAX /* distinguished value */
#ifndef P_ALLOCATOR_CONSISTENCY_CHECK
#define p__bst_header_sz (2 * sizeof (unsigned int))
#define p__bst_consistent(s) 1
#else
#define p__bst_consistency_fld(s) (((enum p__consistency *)(s))[-3])
#define p__bst_header_sz (3 * (sizeof (unsigned int)))
#define p__bst_consistent(s) \
    ((s == NULL) || p__bst_consistency_fld(s) == P__INUSE_PATTERN) 
#endif
void p_bst_set_buffer_length_explicit(char *buf, int buflen);
void p_bst_set_buffer_length_nullterm(char *buf, int buflen);
#ifdef __GNUC__
extern inline void gl_set_bstring_length(char *bstr, size_t len);
extern inline void
gl_set_bstring_length(char *bstr, size_t len)
{
    p__bst_length_fld(bstr) = len;
}
#else
#define gl_set_bstring_length(bstr, len) p_bst_set_buffer_length_explicit((bstr), (len))
#endif
extern char *gl_bstcopy(const char *bst);


/* String length of a GOST family binary string --- published interface. */
extern int p_bstlen(const char *s);
/* How much buffer space was allocated?  In other words, how many bytes
   allocated for a string's payload area?   Published interface as of 1/97*/
#define p_bstsize(s)   ((size_t) ((s) == NULL ? 0 : p__bst_size_fld((s))))
#define p__bstsize(s) p_bstsize((s)) /* Old interface.  This had been intended
					for internal use; it turned out to be
					generally useful, so p__bstsize() got
					promoted to p_bstsize()  */


/* Used internally in gostlib.*/
#ifdef GL_THREADS
extern pthread_mutex_t        p_th_mutexPFS_VQSCANF_NW_CS;
#endif


#endif
