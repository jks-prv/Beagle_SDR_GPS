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

u4_t time_diff(u4_t next, u4_t prev)
{
	u4_t diff;
	
	if (next >= prev)
		diff = next - prev;
	else
		diff = 0xffffffffU - prev + next;	// i.e. amount outside prev - next
	
	return diff;
}

// difference of two u4_t values expressed as a signed quantity
// presuming the unsigned difference fits in 31 bits
s4_t time_diff_s(u4_t next, u4_t prev)
{
	s4_t diff;
	
	if (next >= prev)
		diff = next - prev;
	else
		diff = -(prev - next);
	
	return diff;
}

u64_t time_diff48(u64_t next, u64_t prev)
{
	u64_t diff;
	
	if (next >= prev)
		diff = next - prev;
	else
		diff = 0xffffffffffffULL - prev + next;	// i.e. amount outside prev - next
	
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
