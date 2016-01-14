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

// Copyright (c) 2015 John Seamons, ZL/KF6VO

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

void w2a_admin(void *param)
{
	int n, i;
	conn_t *conn = (conn_t *) param;
	char cmd[256];
	u4_t ka_time = timer_ms();
	
	send_msg(conn, SM_DEBUG, "MSG admin_init=%d", RX_CHANS);
	
	while (TRUE) {
	
		n = web_to_app(conn, cmd, sizeof(cmd));
				
		if (n) {			
			cmd[n] = 0;

			ka_time = timer_ms();
			printf("ADMIN: <%s>\n", cmd);

			i = strcmp(cmd, "SET init");
			if (i == 0) {
				printf("ADMIN: received init\n");
			}
		}
		
		bool keepalive_expired = ((timer_ms() - ka_time) > KEEPALIVE_MS);
		if (keepalive_expired) {
			printf("ADMIN KEEP-ALIVE EXPIRED\n");
			rx_server_remove(conn);
			return;
		}

		TaskSleep(250000);
	}
}
