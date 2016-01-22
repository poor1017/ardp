/* -*-c-*- */
/*
 * Copyright (c) 1991-1997 by the University of Southern California
 *
 * For copying and distribution information, please see the file
 * <usc-license.h>.
 */

#include <usc-license.h>
#include <gl_strings.h>

inline 
char * 
stcopy_GSP(const char *s, char **gp)
{
    if (gp)
	return *gp = stcopyr(s, *gp);
    else
	return stcopy(s);
};
