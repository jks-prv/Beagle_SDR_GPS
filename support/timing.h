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

#ifndef	_TIMING_H_
#define	_TIMING_H_

#include "types.h"
#include "timer.h"

#define	MINUTES_TO_SEC(min)	((min) * 60)
#define	SEC_TO_MINUTES(sec)	((sec) / 60)
#define	SEC_TO_MSEC(sec)	((sec) * 1000)
#define	SEC_TO_USEC(sec)	((sec) * 1000000)
#define	MSEC_TO_USEC(msec)	((msec) * 1000)

#define TIME_DIFF_MS(now, start) ((float) ((now) - (start)) / 1e3)

u4_t time_diff(u4_t next, u4_t prev);
s64_t time_diff_s(u4_t next, u4_t prev);
u64_t time_diff48(u64_t next, u64_t prev);

void spin_ms(u4_t msec);
void spin_us(u4_t usec);

void kiwi_usleep(u4_t usec);
#define kiwi_msleep(msec) kiwi_usleep((msec) * 1000)
#endif
