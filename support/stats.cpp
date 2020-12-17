/*
--------------------------------------------------------------------------------
This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public
License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.
This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.
You should have received a copy of the GNU Library General Public
License along with this library; if not, write to the
Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
Boston, MA  02110-1301, USA.
--------------------------------------------------------------------------------
*/

// Copyright (c) 2014-2016 John Seamons, ZL/KF6VO

#include "types.h"
#include "config.h"
#include "kiwi.h"
#include "misc.h"
#include "web.h"
#include "gps.h"
#include "coroutines.h"
#include "debug.h"
#include "printf.h"
#include "non_block.h"
#include "eeprom.h"

void stat_task(void *param)
{
	u64_t stats_deadline = timer_us64() + SEC_TO_USEC(1);
	u64_t secs = 0;
	
	while (TRUE) {

		if ((secs % STATS_INTERVAL_SECS) == 0) {
			if (do_sdr) {
				webserver_collect_print_stats(print_stats & STATS_TASK);
				if (!do_gps) nbuf_stat();
			}

            cull_zombies();
		}

		NextTask("stat task");
		eeprom_test();

		if ((print_stats & STATS_TASK) && !(print_stats & STATS_GPS)) {
			if (!background_mode) {
				if (do_sdr) {
					lprintf("ECPU %4.1f%%, cmds %d/%d, malloc %d, ",
						ecpu_use(), ecpu_cmds, ecpu_tcmds, kiwi_malloc_stat());
					ecpu_cmds = ecpu_tcmds = 0;
				}
				//TaskDump(PRINTF_REG);
				TaskDump(PRINTF_LOG);
				printf("\n");
			}
		}

		// update on a regular interval
		u64_t now_us = timer_us64();
		s64_t diff = stats_deadline - now_us;
		if (diff > 0) TaskSleepUsec(diff);
		stats_deadline += SEC_TO_USEC(1);
		secs++;
	}
}
