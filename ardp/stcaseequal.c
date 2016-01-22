/*
 * Copyright (c) 1992-1994 by the University of Southern California
 *
 * For copying and distribution information, please see the file
 * <usc-license.h>.
 */

#include <usc-license.h>



#include <ardp.h>
#include <string.h>

/* Check for case-blind string equality, where s1 or s2 may be null
   pointers. */


int
stcaseequal(const char *s1,const char *s2)
{
    if (s1 == s2)               /* test for case when both NULL*/
        return TRUE;
    if (!s1 || !s2)             /* test for one NULL */
        return FALSE;
    return (strcasecmp(s1, s2) == 0);
}

