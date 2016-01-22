/*
 * Copyright (c) 1997 by the University of Southern California
 *
 * For copying and distribution information, please see the file
 * <usc-license.h>.
 */

#include <usc-license.h>

#include <gl_parse.h>

void 
gl_string_to_in(const char *str, INPUT in)
{
    size_t len = strlen(str);
    in->sourcetype = GL__IO_BSTRING;
    in->flags = 0;
#ifdef GL_BSTRING_SUPPORT_OFFSET
    in->u.s.offset = 0;
#endif
    in->u.s.s = str;
    in->u.s.head = str; 
    in->u.s.pastend = str + len; 
    in->u.s.bstring_length = len;
}
