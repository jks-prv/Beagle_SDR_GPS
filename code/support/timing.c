//////////////////////////////////////////////////////////////////////////
// Homemade GPS Receiver
// Copyright (C) 2013 Andrew Holme
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// http://www.holmea.demon.co.uk/GPS/Main.htm
//////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <setjmp.h>
#include <sched.h>
#include <sys/time.h>

#include "types.h"
#include "config.h"
#include "misc.h"
#include "coroutines.h"

// assumes no phase wrap between t1 & t2
u4_t time_diff(u4_t t1, u4_t t2)
{
	u4_t diff;
	
	if (t1 >= t2)
		diff = t1 - t2;
	else
		diff = 0xffffffff - t2 + t1;
	
	return diff;
}

void yield_ms(u4_t msec)
{
	u4_t tref = timer_ms(), diff;
	
	do {
		NextTaskL("yield_ms");
		diff = time_diff(timer_ms(), tref);
	} while (diff < msec);
}

void yield_us(u4_t usec)
{
	u4_t tref = timer_us(), diff;
	
	do {
		diff = time_diff(timer_us(), tref);
		if (NextTaskM()) sprintf(NextTaskM(), "%d/%d", diff, usec);
		NextTaskL("yield_us");
	} while (diff < usec);
}

void spin_ms(u4_t msec)
{
	u4_t tref = timer_ms(), diff;
	
	do {
		diff = time_diff(timer_ms(), tref);
	} while (diff < msec);
}

void spin_us(u4_t usec)
{
	u4_t tref = timer_us(), diff;
	
	do {
		diff = time_diff(timer_us(), tref);
	} while (diff < usec);
}

unsigned Microseconds(void) {
    struct timespec ts;
    {
    	struct timeval tv;
    	gettimeofday(&tv, 0);
    	TIMEVAL_TO_TIMESPEC(&tv, &ts);
    }
    return ts.tv_sec*1000000 + ts.tv_nsec/1000;
}

unsigned nonSim_Microseconds(void) {
    struct timespec ts;
    {
    	struct timeval tv;
    	gettimeofday(&tv, 0);
    	TIMEVAL_TO_TIMESPEC(&tv, &ts);
    }
    return ts.tv_sec*1000000 + ts.tv_nsec/1000;
}

void TimerWait(unsigned ms) {
    unsigned finish = Microseconds() + 1000*ms;
    for (;;) {
        NextTaskL("TimerWait");
        int diff = finish - Microseconds();
        if (diff<=0) break;
    }
}

void nonSim_TimerWait(unsigned ms) {
    unsigned finish = nonSim_Microseconds() + 1000*ms;
    for (;;) {
        NextTaskL("nonSim_TimerWait");
        int diff = finish - nonSim_Microseconds();
        if (diff<=0) break;
    }
}

void TimeruWait(unsigned us) {
    unsigned finish = Microseconds() + us;
    for (;;) {
        NextTaskL("TimeruWait");
        int diff = finish - Microseconds();
        if (diff<=0) break;
    }
}
