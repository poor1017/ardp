#ifndef _mitra_macros_h
#define _mitra_macros_h

/*
 * This function is designed to be an internal part of Prospero; it is not
 * intended to be an exported part of the Prospero interface.
 *
 * These functions are used by the memory allocators.  They 
 * allocate and deallocate Prospero memory items in a thread-safe manner.
 * For efficiency, we store linked-lists of items allocated by the memory
 * allocator; each memory allocation routine has its own static variable,
 * "lfree", which stores the list of free items.
 *
 * Until prospero 5.4 alpha 2, the lfree list of items did not follow the
 * Prospero and GOST linked-list format.  This has been changed, for the sake
 * of consistency. 
 */



#include <stdlib.h>          /* For malloc free */
#include <gl_threads.h>	/* for mutex stuff */


#ifdef PURIFY
#include <purify.h>		/* for purify_is_running() */
#endif

#ifdef GL_THREADS
#define GL__IFTHR(stmt) stmt
#else
#define GL__IFTHR(stmt)
#endif

#define TH_STRUC_ALLOC1(prefix,PREFIX,instance) do { 		\
    GL__IFTHR(pthread_mutex_lock(&(p_th_mutex##PREFIX));)	\
    if(lfree) {							\
	    instance = lfree;					\
	    EXTRACT_ITEM(instance, lfree);			\
    } else {							\
	    instance = (PREFIX) malloc(sizeof(PREFIX##_ST));	\
	    if (!instance) out_of_memory();			\
	    prefix##_max++;					\
    }								\
    prefix##_count++;						\
    /* Initialize and fill in default values */			\
    instance->previous = NULL;					\
    instance->next = NULL; 					\
    GL__IFTHR(pthread_mutex_unlock(&(p_th_mutex##PREFIX));)	\
} while(0)


#ifdef P_ALLOCATOR_CONSISTENCY_CHECK
#define TH_STRUC_ALLOC(prefix,PREFIX,instance) do {			\
    TH_STRUC_ALLOC1(prefix,PREFIX,instance);				\
    instance->consistency = P__INUSE_PATTERN;				\
} while(0) 
#else /*!P_ALLOCATOR_CONSISTENCY_CHECK*/
#define TH_STRUC_ALLOC(prefix,PREFIX,instance) 				\
    TH_STRUC_ALLOC1(prefix,PREFIX,instance)
#endif /*P_ALLOCATOR_CONSISTENCY_CHECK*/



#ifdef PURIFY
/* Under Purify, use MALLOC and FREE normally, so Purify can help us find use
   of uninitialized memory. */
#define TH_STRUC_FREE2(prefix,PREFIX,instance) do {			\
    GL__IFTHR(pthread_mutex_lock(&(p_th_mutex##PREFIX));)		\
    if (purify_is_running())						\
	free(instance);							\
    else								\
	PREPEND_ITEM(instance, lfree);					\
    prefix##_count--;							\
    GL__IFTHR(pthread_mutex_unlock(&(p_th_mutex##PREFIX));)		\
} while(0)
#else
#define TH_STRUC_FREE2(prefix,PREFIX,instance) do {			\
    GL__IFTHR(pthread_mutex_lock(&(p_th_mutex##PREFIX));)		\
    PREPEND_ITEM(instance, lfree);					\
    prefix##_count--;							\
    GL__IFTHR(pthread_mutex_unlock(&(p_th_mutex##PREFIX));)		\
} while(0)
#endif


#ifdef P_ALLOCATOR_CONSISTENCY_CHECK
#define TH_STRUC_FREE(prefix,PREFIX,instance)	do {			\
    if (!instance)							\
	return;								\
    assert(instance->consistency == P__INUSE_PATTERN);			\
    instance->consistency = P__FREE_PATTERN;				\
    TH_STRUC_FREE2(prefix,PREFIX,instance);				\
} while(0) 
#else
#define TH_STRUC_FREE(prefix,PREFIX,instance)	do {			\
    if (!instance)							\
	return;								\
    TH_STRUC_FREE2(prefix,PREFIX,instance)				\
} while(0)
#endif

/*
 * instance_lfree - free a linked list of ATYPEDEF structures.
 *
 *    instance_lfree takes a pointer to a channel structure frees it and 
 *    any linked
 *    ATYPEDEF structures.  It is used to free an entire list of ATYPEDEF
 *    structures.
 */
#define TH_STRUC_LFREE(PREFIX,instance,instance_free) do {		\
    PREFIX	this;							\
    PREFIX	nxt;							\
    /* Set instance to 0 so others can access it while we trash the list */\
    nxt = instance; instance = NULL;					\
    while((this=nxt) != NULL) {						\
        nxt = this->next;						\
        instance_free(this);						\
    }									\
} while(0)

/* temp should be set to point to a list, cnt should be 0
   temp will be NULL afterwards and cnt the count*/
#define COUNT_LISTS(temp,cnt) 				    do {	\
    while (temp) {							\
	cnt++;								\
	temp = temp->next;						\
    }									\
} while(0)


/* Find val in field, instance set to match or NULL */
/* Need to mutex this, cos if struc changes while walking it could fail */
#define	FIND_LIST(instance,field,val) do {\
    while (instance != NULL)  {				\
	if (instance->field == val) 			\
		break;					\
	instance = instance->next;			\
    }							    \
} while(0)

#define	TH_FIND_LIST(instance,field,val,PREFIX) do {		\
    GL__IFTHR(pthread_mutex_lock(&(p_th_mutex##PREFIX));)	\
    FIND_LIST(instance,field,val);				\
    GL__IFTHR(pthread_mutex_unlock(&(p_th_mutex##PREFIX));)    	\
} while(0)

#define FIND_FNCTN_LIST(instance, field, val, fnctn) do {	\
    while(instance != NULL) {				    \
	    if (fnctn(instance->field, val))		    \
		    break;				    \
	    instance = instance->next;			    \
    }							    \
} while(0)

#define FIND_OBJFNCTN_LIST(instance, ob2, fnctn)	do {\
    while(instance != NULL) {			\
	if (fnctn(instance, ob2))		\
            break;				\
	instance = instance->next;		\
    }						    \
} while(0)

#define TH_FIND_FNCTN_LIST(instance, field, val, fctn, PREFIX) do { 	\
    GL__IFTHR(pthread_mutex_lock(&(p_th_mutex##PREFIX));)		\
    FIND_FNCTN_LIST(instance, field, val, fctn)				\
    GL__IFTHR(pthread_mutex_unlock(&(p_th_mutex##PREFIX));)		\
} while(0)
#define TH_FIND_OBJFNCTN_LIST(instance, ob2, fctn, PREFIX) do { 	\
    GL__IFTHR(pthread_mutex_lock(&(p_th_mutex##PREFIX));)		\
    FIND_OBJFNCTN_LIST(instance, ob2, fctn)				\
    GL__IFTHR(pthread_mutex_unlock(&(p_th_mutex##PREFIX));)   	    	\
} while(0)

#define TH_FIND_STRING_LIST_CASE(instance, field, val, PREFIX) 		\
    TH_FIND_FNCTN_LIST(instance, field, val, stcaseequal,PREFIX)

#define TH_FIND_STRING_LIST(instance, field, val, PREFIX) 		\
    TH_FIND_FNCTN_LIST(instance, field, val, stequal,PREFIX)


#define TH_FREESPARES(prefix,PREFIX)	do {				\
    PREFIX	this, next;						\
    GL__IFTHR(pthread_mutex_lock(&(p_th_mutex##PREFIX));)		\
    next = lfree ; lfree = NULL;					\
    GL__IFTHR(pthread_mutex_unlock(&(p_th_mutex##PREFIX));)		\
    while((this = next) != NULL) {					\
	next = this->next;						\
	free(this);	/* Matches malloc in STRUC_ALLOC1*/		\
	prefix##_max--;							\
    }									\
} while(0)

/* Can't take an offset from a null value; shortcuts to return NULL */
#define L2(a,b) (a ? a->b : NULL)
#define L3(a,b,c) (a ? (a->b ? a->b->c : NULL ) : NULL)
#define L4(a,b,c,d) (a ? (a->b ? (a->b->c ? a->b->c->d \
		: NULL) : NULL ) : NULL)
	  
/* String functions on some systems dont like operating on NULLS */
#define Strlen(a) (a ? strlen(a) : 0)

#endif /*_mitra_macros_h*/

