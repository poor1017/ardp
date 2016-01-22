/* -*-c-*- */
/*
 * Copyright (c) 1991-1998 by the University of Southern California
 *
 * For copying and distribution information, please see the file
 * <usc-license.h>.
 */

#include <usc-license.h>

#include <stdlib.h>
#include <sys/conf.h>
#include <pwd.h>

#include <gostlib.h>


#ifndef DISABLE_PFS
#define DISABLE_PFS(x) (x)
#endif
/* Converts a uid into a username. If unknown uid, returns a string of */
/* the form "uid#<uid>" */
char *
gl_uid_to_name_GSP(uid_t uid, char **namep)
{
    register char *retval = NULL;
    struct passwd *whoiampw;
    char *name = NULL;		/* Dummy value in case passed NULL */
#ifdef GL_THREADS
    struct passwd pswd,
	*pwd = &pswd;
    int buff_size = _SC_GETPW_R_SIZE_MAX;
    char *buff = stalloc(buff_size);
#endif /* GL_THREADS */

    if (!namep)
	namep = &name;

#ifdef GL_THREADS
    if (getpwuid_r(uid, pwd, buff, buff_size, &whoiampw)) {
	perror("getpwuid_r:");
	internal_error("buffer is too small in getpwuid_r");
    } /* if */
#else
    DISABLE_PFS(whoiampw = getpwuid(uid));
#endif
    if (whoiampw == 0) {
	/* uid_t is long or short */
	retval = qsprintf_GSP(namep, "uid#%ld", (long) uid);
    }
    else {
	retval = stcopy_GSP(whoiampw->pw_name, namep);
    }
#ifdef GL_THREADS
    stfree(buff);		/* done with it. */
#endif /* GL_THREADS */
    return retval;
}

