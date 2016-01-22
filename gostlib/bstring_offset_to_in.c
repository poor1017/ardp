/*
 * Copyright (c) 1997 by the University of Southern California
 *
 * For copying and distribution information, please see the file
 * <usc-license.h>.
 */

#include <usc-license.h>

#include <gl_parse.h> 
#include <gl_internal_err.h>	/* assert() */
#include <gl_strings.h>

/* STR is a bstring.
   POS is an offset into that bstring.
   IN is the resultant INPUT structure. */

void 
gl_bstring_offset_to_in(const char *str, const char *pos, INPUT in)
{
    size_t len = p_bstlen(str);
    assert(pos >= str);
    in->sourcetype = GL__IO_BSTRING;
    in->flags = 0;
#ifdef GL_BSTRING_SUPPORT_OFFSET
    in->u.s.offset = pos - str;
#endif
    in->u.s.s = pos;
    in->u.s.head = str; 
    in->u.s.pastend = str + len; 
    in->u.s.bstring_length = len;
}
