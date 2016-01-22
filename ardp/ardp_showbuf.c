/*
 * Copyright (c) 1993, 1996 by the University of Southern Calfornia
 *
 * For copying and distribution information, please see the file
 * <usc-license.h>.
 */

#include <usc-license.h>

#include <stdio.h>
#include <ctype.h>
#include <ardp.h>

/* ardp_showbuf:
   Kinda, but not at all really, like fputs, but for bstrings.
   8/21/96: Used only to display ARDP packets before they're sent.
   If debugging level >= 11, newlines, tabs, and carriage returns are displayed
   specially.
   */
   

static void showc(unsigned char c, FILE *outf);

/* Possible future improvement: Return non-zero if string ends in a newline. */
void
ardp_showbuf(const unsigned char *bst, int len, FILE *out)
{
    while (len-- > 0) {
        showc(*bst++, out);
    }
}

#include <ctype.h>

/* Needed for debugging here.  Can be reset if we need to.   Useful if
   debugging with GCC. */
int ardp_showbuf_dump_like_gdb = 0;

static void 
showc(unsigned char c, FILE *outf)
{
    if (ardp_showbuf_dump_like_gdb && (c == '"')) {
	putc('\\', outf);
	putc(c, outf);
    } else if (c == '\\') {
        fputc('\\', outf);
        fputc('\\', outf);
    } else if (isprint(c) 
	       || (ardp_showbuf_dump_like_gdb && isprint(c & 0x7f))) {
        putc(c, outf);
    } else if ((c == '\n')) {
        if (ardp_debug >= 11 || ardp_showbuf_dump_like_gdb) {
            putc('\\', outf);
            putc('n', outf);
        } else {
            putc('\n', outf);
        }
    } else if ((c == '\t')) {
        if (ardp_debug >= 11 || ardp_showbuf_dump_like_gdb) {
            putc('\\', outf);
            putc('t', outf);
        } else {
            putc('\t', outf);
        }
    } else if ((c == '\f')) {
        if (ardp_debug >= 11 || ardp_showbuf_dump_like_gdb) {
            putc('\\', outf);
            putc('f', outf);
        } else {
            putc('\f', outf);
        }
    } else if ((c == '\b')) {
        if (ardp_debug >= 11 || ardp_showbuf_dump_like_gdb) {
            putc('\\', outf);
            putc('b', outf);
        } else {
            putc('\b', outf);
        }
    } else if (c == 0377 && ardp_showbuf_dump_like_gdb) {
	putc(c, outf);
     } else if (c == '\r') {
        if (ardp_debug >= 11 || ardp_showbuf_dump_like_gdb) {
            putc('\\', outf);
            putc('r', outf);
        } else {
            putc('\r', outf);
        }
    } else {
        rfprintf(outf, "\\%03o", c);
    }
}
