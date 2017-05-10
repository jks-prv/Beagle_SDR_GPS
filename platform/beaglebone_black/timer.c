#include "types.h"
#include "config.h"
#include "kiwi.h"
#include "timer.h"
#include "misc.h"

#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>

static bool init = false;
static u4_t epoch_sec;
static time_t server_start_unix_time;

static void set_epoch()
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	epoch_sec = ts.tv_sec;
	time(&server_start_unix_time);
	init = true;
}

u4_t timer_epoch_sec(void)
{
	if (!init) set_epoch();
	return epoch_sec;
}

u4_t timer_server_start_unix_time(void)
{
	if (!init) set_epoch();
	return server_start_unix_time;
}

// overflows 136 years after timer epoch
u4_t timer_sec(void)
{
	struct timespec ts;

	if (!init) set_epoch();
	clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec - epoch_sec;
}

// overflows 49.7 days after timer epoch
u4_t timer_ms(void)
{
	struct timespec ts;

	if (!init) set_epoch();
	clock_gettime(CLOCK_MONOTONIC, &ts);
	int msec = ts.tv_nsec/1000000;
	assert(msec >= 0 && msec < 1000);
    return (ts.tv_sec - epoch_sec)*1000 + msec;
}

// overflows 1.2 hours after timer epoch
u4_t timer_us(void)
{
	struct timespec ts;

	if (!init) set_epoch();
	clock_gettime(CLOCK_MONOTONIC, &ts);
	int usec = ts.tv_nsec / 1000;
	assert(usec >= 0 && usec < 1000000);
    return (ts.tv_sec - epoch_sec)*1000000 + usec;	// ignore overflow
}

// never overflows (effectively)
u64_t timer_us64(void)
{
	struct timespec ts;
	u64_t t;

	if (!init) set_epoch();
	clock_gettime(CLOCK_MONOTONIC, &ts);
	int usec = ts.tv_nsec / 1000;
	assert(usec >= 0 && usec < 1000000);
	t = ts.tv_sec - epoch_sec;
	t *= 1000000;
	t += usec;
    return t;
}
