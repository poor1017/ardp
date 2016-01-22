#include <ardp.h>
#include "ardp__int.h"

void
ardp__adjust_backoff(struct timeval *tv)
{
    tv->tv_sec = ARDP_BACKOFF(tv->tv_sec);
    tv->tv_usec = ARDP_BACKOFF(tv->tv_usec);
    while (tv->tv_usec >= UFACTOR) {
	tv->tv_usec -= UFACTOR;
	++(tv->tv_sec);
    }
}


