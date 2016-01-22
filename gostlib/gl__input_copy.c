/*
 * Copyright (c) 1995 by the University of Southern California
 *
 * For copying and distribution information, please see the file
 * <usc-license.h>.
 */

#include <usc-license.h>

#include <gl_parse.h>
#include <gl_internal_err.h>
#include <memory.h>		/* under SunOS */

void
gl__input_copy(constINPUT src, /* Struct input that we copy from */
	       INPUT dest, /* struct input that we copy to. */
	       void *dat_aux, /* A block of memory we can use to copy auxiliary
				 data into. The caller promises to take care of
				 freeing it when "dest" is no longer needed. 
				 This is only looked at if src->sourcetype ==
				 GL__IO_CALLERDEF*/ 
	       int dat_auxsiz) /* How much space available in dat_aux */
{
    *dest = *src;		/* structure copying works in ANSI C! */

    if (src->sourcetype == GL__IO_CALLERDEF) {
	/* If GL__IO_CALLERDEF, copy the auxiliary data structure too. */
	if (!dat_aux || dat_auxsiz < src->u.c.datsiz) {
	    gl_function_arguments_error("gl__input_copy(): not enough room to \
copy auxiliary data.");
	    /* NOTREACHED */
	}
	memcpy(dat_aux, src->u.c.dat, src->u.c.datsiz);
	dest->u.c.dat = dat_aux;
    }
    /* All data copied! */
}
