/*
 * Copyright (c) 1992, 1993, 1994 by the University of Southern California
 *
 * For copying and distribution information, please see the file
 * <usc-license.h>.
 *
 * Written  by swa 1992   To manipulate lists in prospero and ardp 
 * Modified by bcn 1/93   Added EXTRACT ITEM and fixed several bugs
 * Modified by swa 9/93   Added APPEND_LISTS.
 * Modified by swa 10/93  Quick hack to make assert() in APPEND_LISTS ok.
 * Modified by swa 2/94   Added PREPEND_ITEM, renamed APPEND_LISTS to
 *                          CONCATENATE_LISTS 
 * Modified by swa 4/94   Got rid of mitra changes to check_offlist.
 * Modified by swa 7/94   Removed Mitra paranoia checks; not appropriate
 *                        for our purposes.    Replaced debugging with
 *                        DEBUG_ASSERT_HEAD(), DEBUG_SET_NULL(),
 *                        DEBUG_ASSERT_NULL().  Addded
 *                        INSERT_ITEM1_BEFORE_ITEM2(). 
 * Modified by swa 11/94 Added INSERT_ITEM1_AFTER_ITEM2_IN_LIST(). (needed by
 * ul_insert_ob() )
 */

#include <usc-license.h>

#ifndef LIST_MACROS_H

#define LIST_MACROS_H
#include <gl_threads.h>		/* for mutex stuff */
/* All the macros in this file are documented in the Prospero library reference
   manual. */

/* Note: In doubly linked lists of the structures used for GOSTLIB, ARDP, and
   Prospero, the  */
/* ->previous element of the head of the list is the tail of the list and */
/* the ->next element of the tail of the list is NULL.  This allows fast  */
/* addition at the tail of the list.                                      */


#ifdef NDEBUG
#define DEBUG_ASSERT_HEAD(a) do { } while(0)
#define DEBUG_SET_NULL(a) do { } while(0)
#define DEBUG_ASSERT_NULL(a) do { } while(0)
#else
#define DEBUG_ASSERT_HEAD(a) \
        assert(!(a) || ((a)->previous && (a)->previous->next == NULL))
#define DEBUG_SET_NULL(a) do { (a) = NULL; } while (0)
#define DEBUG_ASSERT_NULL(a) assert(!(a))
#endif

/* 
 * This macro appends an item to a doubly linked list.  The item goes
 * at the tail of the list. This macro modifies its second argument, HEAD.
 *
 * If HEAD is non-null, HEAD->previous will always be the TAIL of
 * the list. 
 */
#define APPEND_ITEM(new, head) do {         \
    DEBUG_ASSERT_HEAD(head);		    \
    if((head)) {                            \
        (new)->previous = (head)->previous; \
        (head)->previous = (new);           \
        (new)->next = NULL;                 \
        (new)->previous->next = (new);      \
    } else /* !(head) */ {                  \
        (head) = (new);                     \
        (new)->previous = (new);            \
        (new)->next = NULL;                 \
    }                                       \
} while(0) 

/*
 * This macro prepends the item NEW to the doubly-linked list headed by HEAD.
 * NEW should not be a member of HEAD or the results are unpredictable. 
 * This macro modifies HEAD to be the new head of the list.
 */

#define PREPEND_ITEM(new, head) do {                            \
    assert(new);      /* Must be something to insert */         \
    /* Make the item a one-element list. */                     \
    (new)->next = NULL;                                         \
    (new)->previous = (new);                                    \
    /* Concatenate the full list to the one-element list. */    \
    CONCATENATE_LISTS((new), (head));                           \
    (head) = (new);  /* reset the list head. */                 \
} while(0)

/* Append an item to a list.  Lock the list's associated mutex.. */
#define TH_APPEND_ITEM(new, head, PREFIX) do		\
{							\
     GL__IFTHR(pthread_mutex_lock(&(p_th_mutex##PREFIX));)	\
     APPEND_ITEM(new, head);				\
     GL__IFTHR(pthread_mutex_unlock(&(p_th_mutex##PREFIX));)	\
} while(0)


/* Insert, possibly at the head of a list.  More general version of 
	INSERT_ITEM1_BEFORE_ITEM2(). */ 

#define INSERT_NEW_ITEM1_BEFORE_ITEM2_IN_LIST(item1, item2, list)	\
do {									\
    assert((list));							\
    assert((item1));							\
    assert((item2));							\
    if ((item2) == (list)) {						\
        PREPEND_ITEM((item1), (list));					\
    } else {								\
	INSERT_ITEM1_BEFORE_ITEM2((item1), (item2));			\
    }									\
} while(0)

/* Insert in the middle of a list.  List can't be empty; can't be at the end.
   Also can't do it at the head. */
#define INSERT_ITEM1_BEFORE_ITEM2(item1, item2) do               \
{								 \
     assert(item1);      /* Must be something to insert */       \
     assert(item2);      /* not empty list */                    \
     assert((item2)->previous->next); /* Item2 not at the head of the list */ \
     (item2)->previous->next = (item1);                          \
     (item1)->previous = (item2)->previous;                      \
     (item1)->next = (item2);                                    \
     (item2)->previous = (item1);                                \
} while(0)

/* Insert in the middle of a list, following another point.  Item2 cannot be
   NULL. */
/* This is used in ul_insert_ob(). */
#define INSERT_NEW_ITEM1_AFTER_ITEM2_IN_LIST(item1, item2, list) do     \
{                                                                       \
    assert(item1);             /* Must be something to insert */        \
    assert(item2);		/* item2 should point somewhere real. */\
    assert(list);             /* list not empty */                      \
    (item1)->next = (item2)->next;                                      \
    (item1)->previous = (item2);                                        \
    if((item2)->next)                                                   \
        (item2)->next->previous = (item1);                              \
    else                                                                \
        (list)->previous = (item1);                                     \
    (item2)->next = (item1);                                            \
} while(0)

/* 
 * This macro removes an item from a doubly-linked list headed by HEAD.
 * If ITEM is not a member of the list, the results are not defined. 
 * The extracted item will NOT be freed.
 * Minor efficiency (code size) improvement by Mitra
 */

/* Use this if only know head, and we know its the last item.*/
/* From Mitra. */
#define  EXTRACT_HEAD_LAST_ITEM(head) do                        \
{								\
     DEBUG_ASSERT_HEAD(head);					\
     assert(head); /* must be an item in the list. */		\
     (head)->previous = (head)->previous->previous;		\
     (head)->previous->next->previous = NULL;		        \
     (head)->previous->next = NULL;				\
} while(0)

/* Use this if both item and head are known, and we know it's the last item. */
/* From Mitra. */
/* This code has appeared in inline form in parts of Prospero.  It should be
   replaced with the macro as those source files are changed. */
#define  EXTRACT_LAST_ITEM(item,head) do                \
{\
     (head)->previous = (item)->previous;		\
     (head)->previous->next = NULL;			\
     DEBUG_SET_NULL((item)->previous);                  \
} while(0)
	
/* swa tends to use just this one. */
#define EXTRACT_ITEM(item, head) do                     \
{							\
    if ((head) == (item)) {                             \
        (head) = (item)->next;                          \
        if (head)                                       \
            (head)->previous = (item)->previous;        \
    } else {                                            \
        (item)->previous->next = (item)->next;          \
        if ((item)->next)                               \
            (item)->next->previous = (item)->previous;  \
	else (head)->previous = (item)->previous;       \
    }                                                   \
     DEBUG_SET_NULL((item)->previous);                  \
     DEBUG_SET_NULL((item)->next);                      \
} while(0)

#define TH_EXTRACT_ITEM(item,head,PREFIX) do {		\
	GL__IFTHR(pthread_mutex_lock(&(p_th_mutex##PREFIX));)	\
	EXTRACT_ITEM(item, head);			\
	GL__IFTHR(pthread_mutex_unlock(&(p_th_mutex##PREFIX));)	\
} while(0)


/* Set LIST1 to point to a doubly-linked list consisting of LIST1 with
   LIST2 appended.  
   LIST2 must not already be a part of LIST1 or the results are unpredictable.
   LIST2 will be a garbage value at the end of this exercise, since it will no
   longer point to a valid doubly-linked list. 
   LIST1 and LIST2 must already be valid doubly-linked lists. */
/* This performs an O(1) list appending operation. */
#define CONCATENATE_LISTS(list1,list2) do {\
    DEBUG_ASSERT_HEAD(list1);                                                \
    DEBUG_ASSERT_HEAD(list2);                                                \
    if (!(list1))                                                           \
        (list1) = (list2);                                                  \
    else if (!(list2))                                                      \
        /* If 2nd list is empty, concatenation is a no-op. */               \
        ;                                                                   \
    else {                                                                  \
        /* OLDL1TAIL is (list2)->previous->next (scratchpad value) --       \
           guaranteed to be NULL right now */                               \
        /* Read next line as: OLDL1TAIL = (list1)->previous */              \
        (list2)->previous->next = (list1)->previous; /* scratchpad value  */\
        (list1)->previous->next = (list2); /* was NULL, or should've been */\
        (list1)->previous = (list2)->previous;                              \
        /* OLDL1TAIL is now list1->previous->next too */                    \
        (list2)->previous = (list1)->previous->next;                        \
        (list1)->previous->next = NULL; /* reset scratch value */           \
    }                                                                       \
    DEBUG_SET_NULL((list2));   /* for security and safety. */               \
} while (0)

#define TH_CONCATENATE_LISTS(list1,list2,PREFIX) do {	\
	GL__IFTHR(pthread_mutex_lock(&(p_th_mutex##PREFIX));)	\
	CONCATENATE_LISTS(list1, list2);		\
	GL__IFTHR(pthread_mutex_unlock(&(p_th_mutex##PREFIX));)	\
} while(0)



#endif /* LIST_MACROS_H */
