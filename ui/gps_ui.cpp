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
#include <stdlib.h>
#include <math.h>

void w2a_gps(void *param)
{
	int n, i, j;
	conn_t *conn = (conn_t *) param;
	char cmd[256];
	u4_t ka_time = timer_sec();
	
	send_msg(conn, SM_NO_DEBUG, "GPS gps_chans=%d", gps_chans);
	
	while (TRUE) {
	
		n = web_to_app(conn, cmd, sizeof(cmd));
				
		if (n) {			
			cmd[n] = 0;

			ka_time = timer_sec();
			
			if (strcmp(cmd, "SET keepalive") == 0) {
				//printf("KA\n");
				continue;
			}
			
			//printf("GPS: <%s>\n", cmd);

			i = strcmp(cmd, "SET update");
			if (i == 0) {
				//printf("GPS: received update\n");
				gps_stats_t::gps_chan_t *c;
				
				send_msg(conn, SM_NO_DEBUG, "GPS update_init=%d", gps.FFTch);

				for (i=0; i < gps_chans; i++) {
					c = &gps.ch[i];
					int un = c->ca_unlocked;
					send_msg(conn, SM_NO_DEBUG, "GPS prn=%d snr=%d rssi=%d gain=%d hold=%d wdog=%d "
						"unlock=%d parity=%d sub=%d sub_renew=%d novfl=%d define_chan=%d",
						c->prn, c->snr, c->rssi, c->gain, c->hold, c->wdog,
						un, c->parity, c->sub, c->sub_renew, c->novfl, i);
					c->parity = 0;
					for (j = 0; j < SUBFRAMES; j++) {
						if (c->sub_renew & (1<<j)) {
							c->sub |= 1<<j;
							c->sub_renew &= ~(1<<j);
						}
					}
				}

				UMS hms(gps.StatSec/60/60);
				
				unsigned r = (timer_ms() - gps.start)/1000;
				if (r >= 3600)
					sprintf(gps.s_run, "%d:%02d:%02d", r / 3600, (r / 60) % 60, r % 60);
				else
					sprintf(gps.s_run, "%d:%02d", (r / 60) % 60, r % 60);

				if (gps.ttff)
					sprintf(gps.s_ttff, "%d:%02d", gps.ttff / 60, gps.ttff % 60);

				if (gps.StatDay != -1)
					esnprintf(gps.s_gpstime, sizeof(gps.s_gpstime), "%s %02d:%02d:%02.0f", Week[gps.StatDay], hms.u, hms.m, hms.s);

				if (gps.StatLat) {
					esnprintf(gps.s_lat, sizeof(gps.s_lat), "%8.6f %c", gps.StatLat, gps.StatNS);
					esnprintf(gps.s_lon, sizeof(gps.s_lon), "%8.6f %c", gps.StatLon, gps.StatEW);
					esnprintf(gps.s_alt, sizeof(gps.s_alt), "%1.0f m", gps.StatAlt);
					esnprintf(gps.s_map, sizeof(gps.s_map), "<a href='http://wikimapia.org/#lang=en&lat=%8.6f&lon=%8.6f&z=18&m=b' target='_blank'>wikimapia.org</a>",
						(gps.StatNS=='S')? -gps.StatLat:gps.StatLat, (gps.StatEW=='W')? -gps.StatLon:gps.StatLon);
				}
					
				send_msg(conn, SM_NO_DEBUG, "GPS track=%d good=%d fixes=%d run=%s ttff=%s gpstime=%s "
					"adc_clk=%.6f adc_corr=%d lat=%s lon=%s alt=%s map=%s update=%d",
					gps.tracking, gps.good, gps.fixes, gps.s_run, gps.s_ttff, gps.s_gpstime,
					adc_clock/1e6, gps.adc_clk_corr,
					gps.s_lat, gps.s_lon, gps.s_alt, gps.s_map,
					gps.FFTch);
				continue;
			}
		}
		
		conn->keep_alive = timer_sec() - ka_time;
		//if ((conn->keep_alive %4) == 0) printf("CK KA %d/%d\n", conn->keep_alive, KEEPALIVE_SEC);
		bool keepalive_expired = (conn->keep_alive > KEEPALIVE_SEC);
		if (keepalive_expired) {
			printf("GPS KEEP-ALIVE EXPIRED\n");
			rx_server_remove(conn);
			return;
		}

		TaskSleep(250000);
	}
}
