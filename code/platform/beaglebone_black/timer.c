#include "types.h"
#include "config.h"
#include "wrx.h"
#include "timer.h"

#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>

u4_t timer_ms(void)
{
	struct timeval tv;

	gettimeofday(&tv, 0);
    return tv.tv_sec*1000 + tv.tv_usec/1000;
}

u4_t timer_us(void)
{
	struct timeval tv;

	gettimeofday(&tv, 0);
    return tv.tv_sec*1000000 + tv.tv_usec;	// ignore overflow
}
