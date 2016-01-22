/*
 * Copyright (c) 1993             by the University of Southern California
 *
 * For copying and distribution information, please see the file
 * <usc-license.h>
 */

#include <usc-license.h>
#include <stdio.h>
#include <string.h>
#include <gostlib.h>                /* for defs. of functions in this file. */
    	    	    	    	    /* For P_ALLOCATOR_CONSISTENCY_CHECK */
#include <stdlib.h>             /*SOLARIS: for malloc, free etc*/


/* See gostlib.h for discussion of the string format we use and for macros that
   manipulate it. . */

int	string_count = 0;
int	string_max = 0;

/*
 * stcopy - allocate space for and copy a string
 *
 *     STCOPY takes a string as an argument, allocates space for
 *     a copy of the string, copies the string to the allocated space,
 *     and returns a pointer to the copy.
 *	Cannot fail - calls out_of_memory
 */

char *
stcopy(const char *st)
{
    return(stcopyr(st,(char *)0));
}

/*
 * stcopyr - copy a string allocating space if necessary
 *
 *     STCOPYR takes a conventional string, S, as an argument, and a pointer to
 *     a second  string, R, which is to be replaced by S.  If R is long enough
 *     to hold S, S is copied.  Otherwise, new space is allocated, and R is
 *     freed.  S is then copied to the newly allocated space.  If S is
 *     NULL, then R is freed and NULL is returned.
 *
 *     In any event, STCOPYR returns a pointer to the new copy of S,
 *     or a NULL pointer.
 */
char *
stcopyr(const char *s, char *r)
{
    int	sl;
    int	rl;

    assert(p__bst_consistent(r));
    if(!s && r) {
        stfree(r);
        return(NULL);
    }
    else if (!s) return(NULL);

    sl = strlen(s);

    if(r) {
        rl = p__bstsize(r);
        if(rl < sl) {
            stfree(r);
	    goto morecore;
        }
    } else {
    morecore:
	/* Normally, we just pass SL down.  This is because stalloc() actually
	   allocates an extra byte to stop runaway strings.  However, since
	   stalloc() specially handles requests for zero bytes, we bump it up
	   by one. */
	if (sl == 0)
	    ++sl;
	r = stalloc(sl);
    }
    strcpy(r,s);
    return(r);
}

/*
 * stalloc - Allocate space for a string
 *
 *     STALLOC allocates space for a string by calling malloc.
 *     STALLOC guarantees never to honor requests for zero or fewer bytes of
 *      memory. 
 */
char *
stalloc(int size)
{
    char	*st;

    if (size <= 0) return NULL;
    /* Allocate one additional byte; this stops runaway strings. */
    st = malloc(size + p__bst_header_sz + 1);
    if(!st) out_of_memory();
    string_count++;
    st += p__bst_header_sz;
    p__bst_size_fld(st) = size;
#ifdef P_ALLOCATOR_CONSISTENCY_CHECK
    p__bst_consistency_fld(st) = P__INUSE_PATTERN;
#endif
    p__bst_length_fld(st) = P__BST_LENGTH_NULLTERMINATED;
    st[size] = '\0';		/* to stop runaway strings. */
    if(string_max < string_count) string_max = string_count;
    return(st);
}

/* Ensure that the gostlib string GP will have at least NBYTES of storage
   allocated for it.  Allocates the storage if unavailable and frees the
   previous storage. */  
void
stalloc_GSP(int nbytes, char **gp)
{
    /* Now, unquote the user-level name represented by compp */
    if (p_bstsize(*gp) < nbytes) {
	stfree(*gp);
	*gp = stalloc(nbytes);
    }
}


/*
 * stfree - free space allocated by stcopy or stalloc
 *
 *     STFREE takes a string that was returned by stcopy or stalloc 
 *     and frees the space that was allocated for the string.
 */
void
stfree(void *st)
{
    assert(p__bst_consistent(st));
    if(st) {
#ifdef P_ALLOCATOR_CONSISTENCY_CHECK
        p__bst_consistency_fld(st) = P__FREE_PATTERN;
#endif
        free((char *) st - p__bst_header_sz);
        string_count--;
    }
}


/* Take the length of a Prospero bstring.  Must be allocated by GOSTLIB. */
int
p_bstlen(const char *s)
{
    assert(p__bst_consistent(s));
    if (!s) return 0;
    else if (p__bst_length_fld(s) == P__BST_LENGTH_NULLTERMINATED) 
        return strlen(s);
    else
	return p__bst_length_fld(s);
}


/* Mark the length field of a buffer appropriately, when BUFLEN amount of data
   has been read into it.  Give preference to using a raw length. */
/* This is what you do after allocating a buffer with stalloc(), and it might
   be seen as part of the interface. */
void
p_bst_set_buffer_length_nullterm(char *buf, int buflen)
{
    register int i;

    assert(buf && p__bst_consistent(buf));
    assert(p__bstsize(buf) >= buflen);
    for (i = 0; i < buflen; ++i) {
        if (buf[i] == '\0') {
            /* If a null is present in the data, have to set an explicit
               count. */
            p__bst_length_fld(buf) = buflen;
            goto done;
        }
    }
    p__bst_length_fld(buf) = P__BST_LENGTH_NULLTERMINATED;
    
 done:
    buf[buflen] = '\0';		/* one extra byte allocated to stop runaway
				   strings.  */
}


/* This function would not have worked; it must not have been being called. */
void
p_bst_set_buffer_length_explicit(char *buf, int buflen)
{
    if ((buf == NULL) && (buflen == 0))
	return;			/* This is just fine and is, indeed, consistent
				 */ 
    assert(buf && p__bst_consistent(buf));
    assert(p__bstsize(buf) >= buflen);

    p__bst_length_fld(buf) = buflen;
    buf[buflen] = '\0';
}


