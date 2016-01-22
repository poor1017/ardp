/*
 * Copyright (c) 1996 by the University of Southern California
 *
 * For copying and distribution information, please see the file
 * <usc-license.h>.
 */

#include <usc-license.h>


#if !defined(ARDP_NO_SECURITY_CONTEXT) 

#include "ardp_sec_members.h"
#include <ardp.h>
#include "ardp_sec.h"

#if defined(ARDP_SEC_HAVE_LABELS_CLASS_OF_SERVICE)	/* macro in ardp_sec.h  */

/* New implementation */

static enum ardp_errcode GL_UNUSED_C_ARGUMENT v_class_of_service_tags_parse_arguments(RREQ, ardp_sectype *, va_list ap);
static void v_class_of_service_tags_mech_spec_free(ardp_sectype *);
static enum ardp_errcode v_class_of_service_tags_commit(RREQ, ardp_sectype *);

ardp_sectype ardp_sec_labels_class_of_service = {
    UNINITIALIZED,
    ARDP_SEC_LABELS,
    ARDP_SEC_LABELS_CLASS_OF_SERVICE,
    v_class_of_service_tags_parse_arguments,
    v_class_of_service_tags_mech_spec_free,
    v_class_of_service_tags_commit,
    ARDP__SEC_STATIC_CONSTANT,	/* (original) processing_state */
    0,				/* (default) criticality */
    NULL,			/* args (default) */
    1,				/* me (I should provide this info.) */
				/* rest default to zero or null */
};


/* Here, we set the members of 's'.   This function is called only by
   ardp_req_security(). */
static
enum ardp_errcode
v_class_of_service_tags_parse_arguments(RREQ GL_UNUSED_C_ARGUMENT req, 
					ardp_sectype *s,
					va_list ap)
{
    char *tagname = va_arg(ap, char *);
    char *value = va_arg(ap, char *);
    if (tagname) {
	s->mech_spec[COS_LABEL] = stcopy(tagname);
    } else {
	s->processing_state = ARDP__SEC_PREP_FAILURE;
	return ARDP_FAILURE;
    }
    if (value)
	s->mech_spec[COS_VALUE] = stcopy(value);
	s->processing_state = ARDP__SEC_PREP_SUCCESS;
    return ARDP_SUCCESS;
    
}


/* We here are supporting having a tag name with a NULL (as opposed to
   zero-length) value, but we don't see any clear way of using this right now.
   Steve thinks we might use this when we have the YOU bit set without the ME
   bit. */
static
enum ardp_errcode
v_class_of_service_tags_commit(RREQ req, ardp_sectype *s)
{
    char *tagname = s->mech_spec[COS_LABEL].ptr;
    char *value = s->mech_spec[COS_VALUE].ptr;

    assert(tagname);
    ardp__sec_add2secdata4req(req, 0, tagname, 
			      p_bstlen(tagname) + 1);
    if (value)
	ardp__sec_add2secdata4req(req, 0, value,
			     p_bstlen(value) + 1);
    
    s->processing_state = ARDP__SEC_COMMITTMENT_SUCCESS;
    return ARDP_SUCCESS;
}

/* Keep this typedef local, so we don't export it. */
typedef struct ardp_class_of_service_tag *costag;


/* Used by recipients of the class_of_service tags, to read them. */
/* Return ARDP_FAILURE if can't parse. */
/* TAGS are printable strings, NULL terminated (why does this look
   familiar?). */ 
enum ardp_errcode
GL_UNUSED_C_ARGUMENT
ardp__sec_extract_class_of_service_tags(
    RREQ req, ardp_sectype *secref, const char *arg, int arglen)
{
    int namelen = -1, vallen = -1;
    costag cos_tag;
    cos_tag = ardp_cos_alloc();

    namelen = strnlen(arg, arglen);
    if (namelen >= arglen) {
	GL_STFREE((char *) cos_tag);
	
	return ARDP_FAILURE;
    }    
    cos_tag->tagname = stcopy(arg);
    /* +1 so we skip over the null byte terminating the string. */
    arg += namelen + 1, arglen -= (namelen + 1);

    vallen = strnlen(arg, arglen);
    if (vallen >= arglen) {
	/* In this case, we encountered a bogus argument -- it was not
	   NULL-terminated.  We fix this by simply assigning to NULL. */
#if 1
	cos_tag->tagname = NULL;
#else 				/* other way of handling it */
	GL_STFREE(cos_tag->tagname);
	GL_STFREE((char *) cos_tag);
	return ARDP_FAILURE;
#endif
    } else
	cos_tag->value = stcopy(arg);
    secref->mech_spec[COS_LABEL].ptr = stcopy(cos_tag->tagname);
    secref->mech_spec[COS_VALUE].ptr = stcopy(cos_tag->value);
    APPEND_ITEM(cos_tag, req->class_of_service_tags); /* important little
							 detail :)  */
    
    return ARDP_SUCCESS;
}



/* Common functions used by both old and new implementations. */

/* XXX later on we should free these off of the RREQ structure in
   ardp_rqfree(); not needed for prototype work :) 11/96 */

static
void
v_class_of_service_tags_mech_spec_free(ardp_sectype *s)
{
    GL_STFREE(s->mech_spec[COS_LABEL].ptr);
    GL_STFREE(s->mech_spec[COS_VALUE].ptr);
}


#endif /* !defined HAVE_CLASS_OF_SERVICE_TAGS */

#endif /* ndef ARDP_NO_SECURITY_CONTEXT, */
