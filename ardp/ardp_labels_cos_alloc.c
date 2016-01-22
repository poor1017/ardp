/*
 * Copyright (c) 1997 by the University of Southern California
 *
 * For copying and distribution information, please see the file
 * <usc-license.h>.
 */

#include <usc-license.h>
#include <stdlib.h>		/* malloc() and free() prototypes. */

#include <ardp_sec_config.h>
#include <ardp.h>
#include <ardp_sec.h>
#include <gl_strings.h>		/* For consistency checks */

#include <gl_threads.h>	/* multithreading stuff */
#include <mitra_macros.h>

#if !defined(ARDP_NO_SECURITY_CONTEXT) && defined(ARDP_SEC_HAVE_LABELS_CLASS_OF_SERVICE)

/* Internal use only */
typedef ardp_label_cos ARDP_COS_ST;
typedef ARDP_COS_ST *ARDP_COS;

static ardp_label_cos *lfree = NULL;   /* locked with p_th_mutexardp_label_cos */
int ardp_cos_count = 0;
int ardp_cos_max = 0;


ardp_label_cos *
ardp_cos_alloc(void)
{
    ardp_label_cos *tg;

    /* Sets CONSISTENCY for us. */
    TH_STRUC_ALLOC(ardp_cos, ARDP_COS, tg);
    /* Initialize and fill in default values. */
#if 1
    tg->tagname = NULL;
    tg->value = NULL;
#else
    memset(tg, '\000', sizeof *tg);
#endif

    return tg;
}

void
ardp_cos_free(ardp_label_cos * tg)
{
    if (tg) {
	GL_STFREE(tg->tagname);
	GL_STFREE(tg->value);
	TH_STRUC_FREE(ardp_cos, ARDP_COS, tg);
    }
}

void ardp_cos_lfree(ardp_label_cos * tg)
{
    TH_STRUC_LFREE(ARDP_COS, tg, ardp_cos_free);
}


#endif /* !defined(ARDP_NO_SECURITY_CONTEXT) &&
	  defined(ARDP_SEC_HAVE_LABELS_CLASS_OF_SERVICE) */
