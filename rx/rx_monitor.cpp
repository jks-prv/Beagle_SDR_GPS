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

			// SECURITY: this must be first for auth check
			if (rx_common_cmd("MON", conn_mon, cmd)) {
			    if (!init) {
			        printf("<%s>\n", cmd);
			        if (kiwi_str_begins_with(cmd, "SET auth")) {
                        send_msg(conn_mon, false, "MSG monitor");
			            init = true;
			        }
			    }
				continue;
			}
			
			//printf("c2s_mon: CONN%d(%p) Q=%d <%s>\n", conn_mon->self_idx, conn_mon, conn_mon->queued? 1:0, cmd);
			
			int rx;
			if (sscanf(cmd, "SET MON_CAMP=%d", &rx) == 1) {
			    cprintf(conn_mon, "SET MON_CAMP=%d\n", rx);

                // remove any previous camping
                bool stop = false;
                for (i = 0; i < MAX_RX_CHANS; i++) {
			        rx_chan_t *rxc = &rx_channels[i];
                    for (j = 0; j < N_CAMP; j++) {
                        conn_t *c = rxc->camp_conn[j];
                        if (c == conn_mon) {
                            c->camp_init = false;
                            rxc->camp_conn[j] = NULL;
			                rxc->n_camp--;
                            cprintf(conn_mon, ">>> CAMP rem rx%d slot=%d/%d\n", i, rxc->camp_id[i], j+1, N_CAMP);
                            stop = true;
                        }
                    }
                }
                
                if (stop) send_msg(conn_mon, false, "MSG audio_camp=1,0");
                if (rx == -1) continue;

			    int okay = 0;
			    rx_chan_t *rxc = &rx_channels[rx];
			    if (rxc->n_camp < N_CAMP) {
                    for (i = 0; i < N_CAMP; i++) {
                        conn_t *c = rxc->camp_conn[i];
                        if (c != NULL) continue;
                        rxc->camp_conn[i] = conn_mon;
                        rxc->camp_id[i] = conn_mon->remote_port;
                        cprintf(conn_mon, ">>> CAMP add rx%d id=%d slot=%d/%d\n", rx, rxc->camp_id[i], i+1, N_CAMP);
                        okay = 1;
                        break;
                    }
			        rxc->n_camp++;
			    }
			    
                send_msg(conn_mon, false, "MSG camp=%d,%d", okay, rx);
			    continue;
			}

			if (sscanf(cmd, "SET MON_QSET=%d", &i) == 1) {
			    conn_mon->queued = (i != 0);
			    conn_mon->arrival = conn_mon->queued? timer_sec() : 0;
			    continue;
			}

			if (strcmp(cmd, "SET MON_QPOS") == 0) {
			    int pos = 0, waiters = 0;
                conn_t *c = conns;
                for (i = 0; i < N_CONNS; i++, c++) {
                    if (c->type == STREAM_MONITOR && c->queued && c->arrival) {
                        if (c->arrival <= conn_mon->arrival)
                            pos++;
                        waiters++;
                    }
                }

                int rx_free = rx_chan_free_count(RX_COUNT_ALL);
		        static u4_t lockout;
		        bool locked = (lockout > timer_sec())? 1:0;
		        int reload = (!locked && pos == 1 && rx_free)? 1:0;
		        if (reload) lockout = timer_sec() + 5;
		        cprintf(conn_mon, "QPOS=%d waiters=%d rx_chans=%d rx_free=%d locked=%d reload=%d\n",
		            pos, waiters, rx_chans, rx_free, locked, reload);

                send_msg(conn_mon, false, "MSG qpos=%d,%d,%d", pos, waiters, reload);
			    continue;
			}

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
