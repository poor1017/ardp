#include <ardp.h>

const char *ardp_err_text[] = {
    /*   0 */ "Success (ARDP)",
#include "ardp_errmesg.c.h"	/* from the ARDP sources */
};
