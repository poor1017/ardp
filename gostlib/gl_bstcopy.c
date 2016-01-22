/* gl_bstcopy.c */
/* -*-c-*- */
/*
 * Copyright (c) 1998 by the University of Southern California
 *
 * For copying and distribution information, please see the file
 * <usc-license.h>.
 */

#include <usc-license.h>

#include <gl_strings.h>
#include <stdlib.h>		/* memcpy() */

char *
gl_bstcopy(const char *bst)
{
    size_t len = p_bstlen(bst);
    char *newstr = stalloc(len);
    return memcpy(newstr, bst, len);
}
