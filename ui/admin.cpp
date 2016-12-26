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
#include "str.h"
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
#include "ext_int.h"

#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sched.h>
#include <math.h>

void c2s_admin_setup(void *param)
{
	conn_t *conn = (conn_t *) param;

	// send initial values
	send_msg(conn, SM_NO_DEBUG, "ADM init=%d", RX_CHANS);
}

void c2s_admin(void *param)
{
	int n, i, j;
	conn_t *conn = (conn_t *) param;
	char *sb, *sb2;
	u4_t ka_time = timer_sec();
	
	nbuf_t *nb = NULL;
	while (TRUE) {
	
		if (nb) web_to_app_done(conn, nb);
		n = web_to_app(conn, &nb);
				
		if (n) {
			char *cmd = nb->buf;
			cmd[n] = 0;		// okay to do this -- see nbuf.c:nbuf_allocq()

			ka_time = timer_sec();

			// SECURITY: this must be first for auth check
			if (rx_common_cmd("ADM", conn, cmd))
				continue;
			
			//printf("ADMIN: %d <%s>\n", strlen(cmd), cmd);

			i = strcmp(cmd, "SET init");
			if (i == 0) {
				continue;
			}

			i = strcmp(cmd, "SET gps_update");
			if (i == 0) {
				gps_stats_t::gps_chan_t *c;
				
				asprintf(&sb, "{\"FFTch\":%d,\"ch\":[", gps.FFTch);
				
				for (i=0; i < gps_chans; i++) {
					c = &gps.ch[i];
					int un = c->ca_unlocked;
					asprintf(&sb2, "%s{ \"ch\":%d,\"prn\":%d,\"snr\":%d,\"rssi\":%d,\"gain\":%d,\"hold\":%d,\"wdog\":%d"
						",\"unlock\":%d,\"parity\":%d,\"sub\":%d,\"sub_renew\":%d,\"novfl\":%d}",
						i? ", ":"", i, c->prn, c->snr, c->rssi, c->gain, c->hold, c->wdog,
						un, c->parity, c->sub, c->sub_renew, c->novfl);
					sb = kiwi_strcat(sb, sb2);
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
				if (r >= 3600) {
					asprintf(&sb2, "],\"run\":\"%d:%02d:%02d\"", r / 3600, (r / 60) % 60, r % 60);
				} else {
					asprintf(&sb2, "],\"run\":\"%d:%02d\"", (r / 60) % 60, r % 60);
				}
				sb = kiwi_strcat(sb, sb2);

				if (gps.ttff) {
					asprintf(&sb2, ",\"ttff\":\"%d:%02d\"", gps.ttff / 60, gps.ttff % 60);
				} else {
					asprintf(&sb2, ",\"ttff\":null");
				}
				sb = kiwi_strcat(sb, sb2);

				if (gps.StatDay != -1) {
					asprintf(&sb2, ",\"gpstime\":\"%s %02d:%02d:%02.0f\"", Week[gps.StatDay], hms.u, hms.m, hms.s);
				} else {
					asprintf(&sb2, ",\"gpstime\":null");
				}
				sb = kiwi_strcat(sb, sb2);

				if (gps.StatLat) {
					asprintf(&sb2, ",\"lat\":\"%8.6f %c\"", gps.StatLat, gps.StatNS);
					sb = kiwi_strcat(sb, sb2);
					asprintf(&sb2, ",\"lon\":\"%8.6f %c\"", gps.StatLon, gps.StatEW);
					sb = kiwi_strcat(sb, sb2);
					asprintf(&sb2, ",\"alt\":\"%1.0f m\"", gps.StatAlt);
					sb = kiwi_strcat(sb, sb2);
					asprintf(&sb2, ",\"map\":\"<a href='http://wikimapia.org/#lang=en&lat=%8.6f&lon=%8.6f&z=18&m=b' target='_blank'>wikimapia.org</a>\"",
						gps.sgnLat, gps.sgnLon);
				} else {
					asprintf(&sb2, ",\"lat\":null");
				}
				sb = kiwi_strcat(sb, sb2);
					
				asprintf(&sb2, ",\"acq\":%d,\"track\":%d,\"good\":%d,\"fixes\":%d,\"adc_clk\":%.6f,\"adc_corr\":%d}",
					gps.acquiring? 1:0, gps.tracking, gps.good, gps.fixes, (adc_clock - adc_clock_offset)/1e6, gps.adc_clk_corr);
				sb = kiwi_strcat(sb, sb2);

				send_msg_encoded_mc(conn->mc, "ADM", "gps_update", "%s", sb);
				free(sb);
				continue;
			}

			int firsttime;
			i = sscanf(cmd, "SET log_update=%d", &firsttime);

			if (i == 1) {
				int start;
				log_save_t *ls = log_save_p;
				
				if (ls->not_shown == 0) {
					start = firsttime? 0 : conn->log_last_sent;
					//if (start < ls->idx)
					//	real_printf("ADM-%d log_update: ft=%d last=%d st/idx=%d-%d\n",
					//		conn->self_idx, firsttime, conn->log_last_sent, start, ls->idx);
					for (i = start; i < ls->idx; i++) {
						send_msg(conn, SM_NO_DEBUG, "ADM log_msg_idx=%d", i);
						send_msg_encoded_mc(conn->mc, "ADM", "log_msg_save", "%s", ls->arr[i]);
					}
					conn->log_last_sent = ls->idx;
				} else
				
				if (ls->not_shown != conn->log_last_not_shown) {
					send_msg(conn, SM_NO_DEBUG, "ADM log_msg_not_shown=%d", ls->not_shown);
					start = firsttime? 0 : MIN(N_LOG_SAVE/2, conn->log_last_sent);
					//if (start < ls->idx)
					//	real_printf("ADM-%d log_update: ft=%d half=%d last=%d st/idx=%d-%d\n",
					//		conn->self_idx, firsttime, N_LOG_SAVE/2, conn->log_last_sent, start, ls->idx);
					for (i = start; i < ls->idx; i++) {
						send_msg(conn, SM_NO_DEBUG, "ADM log_msg_idx=%d", i);
						send_msg_encoded_mc(conn->mc, "ADM", "log_msg_save", "%s", ls->arr[i]);
					}
					conn->log_last_not_shown = ls->not_shown;
				}
				
				continue;
			}
			
			i = strcmp(cmd, "SET sdr_hu_update");
			if (i == 0) {
				gps_stats_t::gps_chan_t *c;
				
				if (gps.StatLat) {
					asprintf(&sb, "{\"lat\":\"%8.6f\",\"lon\":\"%8.6f\"}", gps.sgnLat, gps.sgnLon);
				} else {
					asprintf(&sb, "{}");
				}
				send_msg_encoded_mc(conn->mc, "ADM", "sdr_hu_update", "%s", sb);
				free(sb);
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

			i = strcmp(cmd, "SET extint_load_extension_configs");
			if (i == 0) {
				extint_load_extension_configs(conn);
				continue;
			}

			i = strcmp(cmd, "SET restart");
			if (i == 0) {
				clprintf(conn, "ADMIN: restart requested by admin..\n");
				exit(0);
			}

			i = strcmp(cmd, "SET reboot");
			if (i == 0) {
				clprintf(conn, "ADMIN: reboot requested by admin..\n");
				system("reboot");
				while (true)
					usleep(100000);
			}

			i = strcmp(cmd, "SET power_off");
			if (i == 0) {
				clprintf(conn, "ADMIN: power off requested by admin..\n");
				system("poweroff");
				while (true)
					usleep(100000);
			}

			int use_static_ip;
			i = strcmp(cmd, "SET use_DHCP");
			if (i == 0) {
				clprintf(conn, "eth0: USE DHCP\n");
				system("cp /etc/network/interfaces /etc/network/interfaces.bak");
				system("cp /root/" REPO_NAME "/unix_env/interfaces.DHCP /etc/network/interfaces");
				continue;
			}

			char static_ip[32], static_nm[32], static_gw[32];
			i = sscanf(cmd, "SET static_ip=%s static_nm=%s static_gw=%s", static_ip, static_nm, static_gw);
			if (i == 3) {
				clprintf(conn, "eth0: USE STATIC ip=%s nm=%s gw=%s\n", static_ip, static_nm, static_gw);
				system("cp /etc/network/interfaces /etc/network/interfaces.bak");
				FILE *fp;
				scallz("/tmp/interfaces.kiwi fopen", (fp = fopen("/tmp/interfaces.kiwi", "w")));
					fprintf(fp, "auto lo\n");
					fprintf(fp, "iface lo inet loopback\n");
					fprintf(fp, "auto eth0\n");
					fprintf(fp, "iface eth0 inet static\n");
					fprintf(fp, "    address %s\n", static_ip);
					fprintf(fp, "    netmask %s\n", static_nm);
					fprintf(fp, "    gateway %s\n", static_gw);
					fprintf(fp, "iface usb0 inet static\n");
					fprintf(fp, "    address 192.168.7.2\n");
					fprintf(fp, "    netmask 255.255.255.252\n");
					fprintf(fp, "    network 192.168.7.0\n");
					fprintf(fp, "    gateway 192.168.7.1\n");
				fclose(fp);
				system("cp /tmp/interfaces.kiwi /etc/network/interfaces");
				continue;
			}

			printf("ADMIN: unknown command: <%s>\n", cmd);
			continue;
		}
		
		conn->keep_alive = timer_sec() - ka_time;
		bool keepalive_expired = (conn->keep_alive > KEEPALIVE_SEC);
		if (keepalive_expired) {
			cprintf(conn, "ADMIN KEEP-ALIVE EXPIRED\n");
			rx_server_remove(conn);
			return;
		}

		TaskSleep(250000);
	}
}
