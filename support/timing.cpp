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

// Copyright (c) 2014-2017 John Seamons, ZL4VO/KF6VO

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <setjmp.h>
#include <sched.h>
#include <errno.h>
#include <sys/time.h>

#include "types.h"
#include "config.h"
#include "misc.h"
#include "coroutines.h"

/*

n = next, p = prev

How unsigned difference (n-p) works where n could have
wrapped around from u_max to zero without exceeding
dynamic range u_max (i.e. n is still "ahead" of p in second case).

(max-p) represents how far n is ahead of p before n wrapped,
and (n-0) represents how much n is ahead after.
So total amount is (max-p)+n

0                                                 u_max
+-----------------------------------------------------|
|
|-----------------------------------| n     n >= p case (easy)
|-----------------------| p
|                       |==========>| ans = n-p
|
|-----------------------| n
|-----------------------------------| p     n < p case (slightly strange)
|                       |-----------| p-n   
|======================>|           |================>| ans = (max-p)+n
|
+-----------------------------------------------------|

*/

u4_t time_diff(u4_t next, u4_t prev)
{
	u4_t diff;
	
	if (next >= prev)
		diff = next - prev;
	else
		diff = (0xffffffffU - prev) + next;	// i.e. amount outside prev - next
	
	return diff;
}

// difference of two u4_t values expressed as a signed quantity
s64_t time_diff_s(u4_t a, u4_t b)
{
	s64_t diff;
	
	if (a >= b)
		diff = a - b;
	else
		diff = -(b - a);
	
	return diff;
}

u64_t time_diff48(u64_t next, u64_t prev)
{
	u64_t diff;
	
	if (next >= prev)
		diff = next - prev;
	else
		diff = (0x0000ffffffffffffULL - prev) + next;	// i.e. amount outside prev - next
	
	return diff;
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

void kiwi_usleep(u4_t usec)
{
    struct timespec tv;
    memset(&tv, 0, sizeof(tv));
    tv.tv_sec = usec / 1000000;
    tv.tv_nsec = (usec % 1000000) * 1000;

    if (tv.tv_nsec < 0 || tv.tv_nsec > 999999999) {
        real_printf("kiwi_usleep tv_nsec=%d\n", tv.tv_nsec);
    }

    if (nanosleep(&tv, NULL) < 0 && errno != EINTR)
        sys_panic("nanosleep");
}
