/*
 * Copyright (c) 1993 by the University of Southern California
 *
 * For copying and distribution information, please see the file
 * <usc-license.h>.
 */

#include <usc-license.h>

#include <gostlib.h>
#include <gl_parse.h>
#include <perrno.h>

#include <sys/types.h>
#include <sys/stat.h>

/* Convert an entire file into a single input structure.
 */
/* Reuses or frees the buffer on subsequent calls. */
int
wholefiletoin(FILE *file, INPUT in)
{
    struct stat st_buf;
    /* This function is not thread-safe.  It is only called during the
       configuration time and MUST not be simultanously used by two or more
       threads. -- swa, salehi 4/98 */
    static char *buf = NULL;
    char **bufp = &buf;
    int tmp;                    /* temp. return value from subfunctions */
    
    if(fstat(fileno(file), &st_buf)) {
        p_err_string = qsprintf_stcopyr(p_err_string,
                 "wholefiletoin(): Couldn't fstat input file");
        return perrno = PFAILURE;
    }
    /* If buffer sufficiently large, don't bother calling malloc() twice. */
    if (p__bstsize(*bufp) < st_buf.st_size + 1) {
        if (*bufp) stfree(*bufp);
        if((*bufp = stalloc(st_buf.st_size + 1)) == NULL) {
        p_err_string = qsprintf_stcopyr(p_err_string,
            "wholefiletoin(): out of memory");
            return perrno = PFAILURE;
        }
    }
    tmp = fread(*bufp, sizeof (char), st_buf.st_size, file);
    if (tmp != st_buf.st_size) {
        p_err_string = qsprintf_stcopyr(p_err_string,
                 "wholefiletoin(): fread() returned %d, not the %d expected.",
                 tmp, st_buf.st_size);
        return perrno = PFAILURE;
    }
    (*bufp)[st_buf.st_size] = '\0'; /* Most code will be able to treat it as a
				       null-terminated string, although that
				       code will have problems if we miss the
				       count thing.  In general, it has been an
				       objective of this code to work
				       gracefully for callers who do not plan
				       to use it for bstrings, and who don't
				       want to know anything about bstrings. */
    gl_stringlen_to_in(*bufp, st_buf.st_size, in);
#ifdef GL__IO_SERVER_DATA_FILE
    /* Can be useful for Prospero's own parsing routines; they can test for
       additional validity (specifically, some link types should appear only in
       the server data files.)  */ 
    in->flags = GL__IO_SERVER_DATA_FILE;
#endif /* GL__IO_SERVER_DATA_FILE */
#if 0
    in->sourcetype = GL__IO_BSTRING;
    in->u.s.s = *bufp;
#endif

    return PSUCCESS;
}
        
                 
