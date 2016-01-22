/* -*-c-*- */
/*
 * Copyright (c) 1991-1997 by the University of Southern California
 *
 * For copying and distribution information, please see the file
 * <usc-license.h>.
 */

#include <usc-license.h>

#include <gl_strings.h>

void
GL_INLINE
gl_set_bstring_length(char *bstr, size_t len)
{
    p__bst_length_fld(bstr) = len;
}
