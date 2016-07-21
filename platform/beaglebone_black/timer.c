#include "types.h"
#include "config.h"
#include "kiwi.h"
#include "timer.h"
#include "misc.h"

#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>

static bool timer_init = false;
static u4_t timer_epoch_sec;

static void timer_epoch()
{
	struct timeval tv;

	gettimeofday(&tv, 0);
	timer_epoch_sec = tv.tv_sec;
	timer_init = true;
}

// overflows 136 years after timer_epoch_sec
u4_t timer_sec(void)
{
	struct timeval tv;

	if (!timer_init) timer_epoch();
	gettimeofday(&tv, 0);
    return tv.tv_sec - timer_epoch_sec;
}

// overflows 49.7 days after timer_epoch_sec
u4_t timer_ms(void)
{
	struct timeval tv;

	if (!timer_init) timer_epoch();
	gettimeofday(&tv, 0);
	int msec = tv.tv_usec/1000;
	assert(msec >= 0 && msec < 1000);
    return (tv.tv_sec - timer_epoch_sec)*1000 + msec;
}

// overflows 1.2 hours after timer_epoch_sec
u4_t timer_us(void)
{
	struct timeval tv;

	if (!timer_init) timer_epoch();
	gettimeofday(&tv, 0);
	int usec = tv.tv_usec;
	assert(usec >= 0 && usec < 1000000);
    return (tv.tv_sec - timer_epoch_sec)*1000000 + usec;	// ignore overflow
}

// never overflows (effectively)
u64_t timer_us64(void)
{
	struct timeval tv;
	u64_t t;

	if (!timer_init) timer_epoch();
	gettimeofday(&tv, 0);
	int usec = tv.tv_usec;
	assert(usec >= 0 && usec < 1000000);
	t = tv.tv_sec - timer_epoch_sec;
	t *= 1000000;
	t += usec;
    return t;
}

unsigned Microseconds(void) {
    struct timespec ts;
    struct timeval tv;
    
	if (!timer_init) timer_epoch();
    gettimeofday(&tv, 0);
    TIMEVAL_TO_TIMESPEC(&tv, &ts);
	int usec = ts.tv_nsec/1000;
	assert(usec >= 0 && usec < 1000000);
    return (ts.tv_sec - timer_epoch_sec)*1000000 + usec;
}

unsigned nonSim_Microseconds(void) {
    struct timespec ts;
    struct timeval tv;
    
	if (!timer_init) timer_epoch();
    gettimeofday(&tv, 0);
    TIMEVAL_TO_TIMESPEC(&tv, &ts);
	int usec = ts.tv_nsec/1000;
	assert(usec >= 0 && usec < 1000000);
    return (ts.tv_sec - timer_epoch_sec)*1000000 + usec;
}
