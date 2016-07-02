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

// Copyright (c) 2015-2016 John Seamons, ZL/KF6VO

#include "types.h"
#include "config.h"
#include "kiwi.h"
#include "misc.h"
#include "nbuf.h"
#include "web.h"
#include "peri.h"
#include "spi.h"
#include "gps.h"
#include "coroutines.h"
#include "pru_realtime.h"
#include "debug.h"
#include "printf.h"
#include "cfg.h"

#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sched.h>
#include <math.h>

void w2a_admin(void *param)
{
	int n, i, j;
	conn_t *conn = (conn_t *) param;
	u4_t ka_time = timer_sec();
	
	// send initial values
	int chans = RX_CHANS;
	char *json = cfg_get_json(NULL);

	if (json == NULL) {
		chans = -1;
	} else {
		send_encoded_msg_mc(conn->mc, "ADM", "load", "%s", json);
	}

	send_msg(conn, SM_NO_DEBUG, "ADM init=%d", chans);
	
	nbuf_t *nb = NULL;
	while (TRUE) {
	
		if (nb) web_to_app_done(conn, nb);
		n = web_to_app(conn, &nb);
				
		if (n) {
			char *cmd = nb->buf;
			cmd[n] = 0;		// okay to do this -- see nbuf.c:nbuf_allocq()
			char id[64];

			ka_time = timer_sec();

			i = strcmp(cmd, "SET keepalive");
			if (i == 0) {
				continue;
			}

			//printf("ADMIN: %d <%s>\n", strlen(cmd), cmd);

			i = strncmp(cmd, "SET save=", 9);
			if (i == 0) {
				json = cfg_realloc_json(strlen(cmd));	// a little bigger than necessary
				i = sscanf(cmd, "SET save=%s", json);
				printf("ADMIN: SET save=...\n");
				int slen = strlen(json);
				mg_url_decode(json, slen, json, slen+1, 0);		// dst=src is okay because length dst always <= src
				cfg_save_json(json);
				continue;
			}
			
			i = strcmp(cmd, "SET init");
			if (i == 0) {
				continue;
			}

			i = strcmp(cmd, "SET gps_update");
			if (i == 0) {
				static char gps_json[16384];
				gps_stats_t::gps_chan_t *c;
				
				char *cp = gps_json;
				n = sprintf(cp, "{ \"FFTch\":%d, \"ch\":[ ", gps.FFTch); cp += n;
				
				for (i=0; i < gps_chans; i++) {
					c = &gps.ch[i];
					int un = c->ca_unlocked;
					n = sprintf(cp, "%s{ \"ch\":%d, \"prn\":%d, \"snr\":%d, \"rssi\":%d, \"gain\":%d, \"hold\":%d, \"wdog\":%d"
						", \"unlock\":%d, \"parity\":%d, \"sub\":%d, \"sub_renew\":%d, \"novfl\":%d }",
						i? ", ":"", i, c->prn, c->snr, c->rssi, c->gain, c->hold, c->wdog,
						un, c->parity, c->sub, c->sub_renew, c->novfl); cp += n;
					c->parity = 0;
					for (j = 0; j < SUBFRAMES; j++) {
						if (c->sub_renew & (1<<j)) {
							c->sub |= 1<<j;
							c->sub_renew &= ~(1<<j);
						}
					}
				}

				n = sprintf(cp, " ]"); cp += n;

				UMS hms(gps.StatSec/60/60);
				
				unsigned r = (timer_ms() - gps.start)/1000;
				if (r >= 3600) {
					n = sprintf(cp, ", \"run\":\"%d:%02d:%02d\"", r / 3600, (r / 60) % 60, r % 60); cp += n;
				} else {
					n = sprintf(cp, ", \"run\":\"%d:%02d\"", (r / 60) % 60, r % 60); cp += n;
				}

				if (gps.ttff) {
					n = sprintf(cp, ", \"ttff\":\"%d:%02d\"", gps.ttff / 60, gps.ttff % 60); cp += n;
				} else {
					n = sprintf(cp, ", \"ttff\":null"); cp += n;
				}

				if (gps.StatDay != -1) {
					n = sprintf(cp, ", \"gpstime\":\"%s %02d:%02d:%02.0f\"", Week[gps.StatDay], hms.u, hms.m, hms.s); cp += n;
				} else {
					n = sprintf(cp, ", \"gpstime\":null"); cp += n;
				}

				if (gps.StatLat) {
					n = sprintf(cp, ", \"lat\":\"%8.6f %c\"", gps.StatLat, gps.StatNS); cp += n;
					n = sprintf(cp, ", \"lon\":\"%8.6f %c\"", gps.StatLon, gps.StatEW); cp += n;
					n = sprintf(cp, ", \"alt\":\"%1.0f m\"", gps.StatAlt); cp += n;
					n = sprintf(cp, ", \"map\":\"<a href='http://wikimapia.org/#lang=en&lat=%8.6f&lon=%8.6f&z=18&m=b' target='_blank'>wikimapia.org</a>\"",
						(gps.StatNS=='S')? -gps.StatLat:gps.StatLat, (gps.StatEW=='W')? -gps.StatLon:gps.StatLon); cp += n;
				} else {
					n = sprintf(cp, ", \"lat\":null"); cp += n;
				}
					
				n = sprintf(cp, ", \"acq\":%d, \"track\":%d, \"good\":%d, \"fixes\":%d, \"adc_clk\":%.6f, \"adc_corr\":%d",
					gps.acquiring? 1:0, gps.tracking, gps.good, gps.fixes, adc_clock/1e6, gps.adc_clk_corr); cp += n;

				n = sprintf(cp, " }"); cp += n;
				send_encoded_msg_mc(conn->mc, "ADM", "gps_update", "%s", gps_json);

				continue;
			}

			int force_check;
			i = sscanf(cmd, "SET force_check=%d force_build=%d", &force_check, &force_build);
			if (i == 2) {
				check_for_update(force_check);
				continue;
			}

			i = strcmp(cmd, "SET reload_index_params");
			if (i == 0) {
				reload_index_params();
				continue;
			}

			i = strcmp(cmd, "SET restart");
			if (i == 0) {
				lprintf("ADMIN: restart requested by admin..\n");
				exit(0);
				continue;
			}

			i = sscanf(cmd, "SERVER DE CLIENT %s", id);
			if (i == 1) {
				continue;
			}
			
			printf("ADMIN: unknown command: <%s>\n", cmd);
			continue;
		}
		
		conn->keep_alive = timer_sec() - ka_time;
		bool keepalive_expired = (conn->keep_alive > KEEPALIVE_SEC);
		if (keepalive_expired) {
			printf("ADMIN KEEP-ALIVE EXPIRED\n");
			rx_server_remove(conn);
			return;
		}

		TaskSleep(250000);
	}
}
