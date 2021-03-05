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

// Copyright (c) 2021 John Seamons, ZL/KF6VO

// TODO
//  check that:
//      kiwiclient still gets "too busy"
//      request of adpcm state doesn't effect other camps

#include "types.h"
#include "kiwi.h"
#include "clk.h"
#include "misc.h"
#include "str.h"
#include "cfg.h"
#include "gps.h"
#include "rx.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>

void c2s_mon_setup(void *param)
{
	conn_t *conn_mon = (conn_t *) param;
}

void c2s_mon(void *param)
{
	int n, i, j;
	conn_t *conn_mon = (conn_t *) param;
	bool init = false;
	rx_common_init(conn_mon);
	
	nbuf_t *nb = NULL;
	while (TRUE) {
		int rx_chan;
	
		if (nb) web_to_app_done(conn_mon, nb);
		n = web_to_app(conn_mon, &nb);
				
		if (n) {
			char *cmd = nb->buf;
			cmd[n] = 0;		// okay to do this -- see nbuf.c:nbuf_allocq()

			printf("c2s_mon: CONN-%d(%p) unknown command: sl=%d %d|%d|%d [%s] ip=%s ==================================\n",
			    conn_mon->self_idx, conn_mon, strlen(cmd), cmd[0], cmd[1], cmd[2], cmd, conn_mon->remote_ip);

			continue;
		}
		
		conn_mon->keep_alive = timer_sec() - conn_mon->keepalive_time;
		bool keepalive_expired = (!conn_mon->internal_connection && conn_mon->keep_alive > KEEPALIVE_SEC);
		if (keepalive_expired || conn_mon->kick) {
			rx_server_remove(conn_mon);
			panic("shouldn't return");
		}
		
		TaskSleepReasonMsec("mon-cmd", 250);
	}
}
