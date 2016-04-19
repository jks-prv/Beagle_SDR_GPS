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

// Copyright (c) 2016 John Seamons, ZL/KF6VO

#include "types.h"
#include "config.h"
#include "kiwi.h"
#include "misc.h"
#include "web.h"
#include "peri.h"
#include "spi.h"
#include "gps.h"
#include "coroutines.h"
#include "pru_realtime.h"
#include "debug.h"
#include "printf.h"

#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sched.h>
#include <math.h>

void w2a_mfg(void *param)
{
	int n, i;
	conn_t *conn = (conn_t *) param;
	char cmd[256];
	u4_t ka_time = timer_sec();
	
	int next_serno = eeprom_next_serno(SERNO_READ, 0);
	int serno = eeprom_check();
	send_msg(conn, SM_DEBUG, "MFG next_serno=%d serno=%d", next_serno, serno);
	
	while (TRUE) {
	
		n = web_to_app(conn, cmd, sizeof(cmd));
				
		if (n) {			
			cmd[n] = 0;

			ka_time = timer_sec();
			
			if (strcmp(cmd, "SET keepalive") == 0) {
				//printf("KA\n");
				continue;
			}
			
			printf("MFG: <%s>\n", cmd);

			i = strcmp(cmd, "SET write");
			if (i == 0) {
				printf("MFG: received write\n");
				eeprom_write();

				serno = eeprom_check();
				next_serno = eeprom_next_serno(SERNO_READ, 0);
				send_msg(conn, SM_DEBUG, "MFG next_serno=%d serno=%d", next_serno, serno);
			}

			int next_serno = -1;
			i = sscanf(cmd, "SET set_serno=%d", &next_serno);
			if (i == 1) {
				printf("MFG: received set_serno=%d\n", next_serno);
				eeprom_next_serno(SERNO_WRITE, next_serno);

				serno = eeprom_check();
				next_serno = eeprom_next_serno(SERNO_READ, 0);
				send_msg(conn, SM_DEBUG, "MFG next_serno=%d serno=%d", next_serno, serno);
			}
		}
		
		conn->keep_alive = timer_sec() - ka_time;
		//if ((conn->keep_alive %4) == 0) printf("CK KA %d/%d\n", conn->keep_alive, KEEPALIVE_SEC);
		bool keepalive_expired = (conn->keep_alive > KEEPALIVE_SEC);
		if (keepalive_expired) {
			printf("MFG KEEP-ALIVE EXPIRED\n");
			rx_server_remove(conn);
			return;
		}

		TaskSleep(250000);
	}
}
