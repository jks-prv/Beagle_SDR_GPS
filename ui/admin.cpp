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
#include "net.h"
#include "peri.h"
#include "spi.h"
#include "gps.h"
#include "coroutines.h"
#include "pru_realtime.h"
#include "debug.h"
#include "printf.h"
#include "cfg.h"
#include "ext_int.h"
#include "wspr.h"

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

bool backup_in_progress, DUC_enable_start;

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
    		TaskStatU(TSTAT_INCR|TSTAT_ZERO, 0, "cmd", 0, 0, NULL);

			// SECURITY: this must be first for auth check
			if (rx_common_cmd("ADM", conn, cmd))
				continue;
			
			//printf("ADMIN: %d <%s>\n", strlen(cmd), cmd);

			i = strcmp(cmd, "SET init");
			if (i == 0) {
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
						send_msg_encoded(conn, "ADM", "log_msg_save", "%s", ls->arr[i]);
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
						send_msg_encoded(conn, "ADM", "log_msg_save", "%s", ls->arr[i]);
					}
					conn->log_last_not_shown = ls->not_shown;
				}
				
				continue;
			}
			
			i = strcmp(cmd, "SET sdr_hu_update");
			if (i == 0) {
				gps_stats_t::gps_chan_t *c;
				
				if (gps.StatLat) {
					latLon_t loc;
					char grid[8];
					loc.lat = gps.sgnLat;
					loc.lon = gps.sgnLon;
					if (latLon_to_grid(&loc, grid))
						grid[0] = '\0';
					else
						grid[6] = '\0';
					asprintf(&sb, "{\"lat\":\"%8.6f\",\"lon\":\"%8.6f\",\"grid\":\"%s\"}",
						gps.sgnLat, gps.sgnLon, grid);
				} else {
					asprintf(&sb, "{}");
				}
				send_msg_encoded(conn, "ADM", "sdr_hu_update", "%s", sb);
				free(sb);
				continue;
			}

			int force_check, force_build;
			i = sscanf(cmd, "SET force_check=%d force_build=%d", &force_check, &force_build);
			if (i == 2) {
				check_for_update(force_build? FORCE_BUILD : FORCE_CHECK, conn);
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
				send_msg(conn, SM_NO_DEBUG, "ADM auto_nat=%d", ddns.auto_nat);
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

			int server_enabled;
			i = sscanf(cmd, "SET server_enabled=%d", &server_enabled);
			if (i == 1) {
				clprintf(conn, "ADMIN: server_enabled=%d\n", server_enabled);
				
				if (server_enabled) {
					down = false;
				} else {
					down = true;
					rx_server_user_kick();		// kick everyone off
				}
				continue;
			}

			i = strcmp(cmd, "SET user_kick");
			if (i == 0) {
				rx_server_user_kick();
				continue;
			}

            // FIXME: support wlan0
			int use_static_ip;
			i = strcmp(cmd, "SET use_DHCP");
			if (i == 0) {
				clprintf(conn, "eth0: USE DHCP\n");
				system("cp /etc/network/interfaces /etc/network/interfaces.bak");
				system("cp /root/" REPO_NAME "/unix_env/interfaces.DHCP /etc/network/interfaces");
				continue;
			}

            // FIXME: support wlan0
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

#define SD_CMD "cd /root/" REPO_NAME "/tools; ./kiwiSDR-make-microSD-flasher-from-eMMC.sh --called_from_kiwi_server"
			i = strcmp(cmd, "SET microSD_write");
			if (i == 0) {
				mprintf_ff("ADMIN: received microSD_write\n");
				backup_in_progress = true;
				rx_server_user_kick();		// kick everyone off to speed up copy
				
				#define NBUF 256
				char *buf = (char *) kiwi_malloc("c2s_admin", NBUF);
				int n, err;
				
				sd_copy_in_progress = true;
				non_blocking_cmd_t p;
				p.cmd = SD_CMD;
				non_blocking_cmd_popen(&p);
				do {
					n = non_blocking_cmd_read(&p, buf, NBUF);
					if (n > 0) {
						mprintf("%s", buf);
						//real_printf("mprintf %d %d <%s>\n", n, strlen(buf), buf);
					}
					TaskSleepMsec(250);
				} while (n > 0);
				err = non_blocking_cmd_pclose(&p);
				sd_copy_in_progress = false;
				
				err = (err < 0)? err : WEXITSTATUS(err);
				mprintf("ADMIN: system returned %d\n", err);
				kiwi_free("c2s_admin", buf);
				#undef NBUF
				send_msg(conn, SM_NO_DEBUG, "ADM microSD_done=%d", err);
				backup_in_progress = false;
				continue;
			}

			// FIXME: hardwired to eth0 -- needs to support wlans
			char *args_m = NULL;
			n = sscanf(cmd, "SET DUC_start args=%ms", &args_m);
			if (n == 1) {
				system("killall -q noip2; sleep 1");
			
				str_decode_inplace(args_m);
				char *cmd_p;
				asprintf(&cmd_p, "%s/noip2 -C -c " DIR_CFG "/noip2.conf -k %s -I eth0 2>&1",
					background_mode? "/usr/local/bin" : "./pkgs/noip2", args_m);
				free(args_m);
				printf("DUC: %s\n", cmd_p);
				char buf[1024];
				int stat;
				n = non_blocking_cmd(cmd_p, buf, sizeof(buf), &stat);
				free(cmd_p);
				if (stat < 0 || n <= 0) {
					lprintf("DUC: noip2 failed?\n");
					send_msg(conn, SM_NO_DEBUG, "ADM DUC_status=300");
					continue;
				}
				int status = WEXITSTATUS(stat);
				printf("DUC: status=%d\n", status);
				printf("DUC: <%s>\n", buf);
				send_msg(conn, SM_NO_DEBUG, "ADM DUC_status=%d", status);
				if (status != 0) continue;
				
                if (background_mode)
                    system("/usr/local/bin/noip2 -c " DIR_CFG "/noip2.conf");
                else
                    system("./pkgs/noip2/noip2 -c " DIR_CFG "/noip2.conf");
				
				continue;
			}
		
			i = strcmp(cmd, "SET DUC_status_query");
			if (i == 0) {
				if (DUC_enable_start) {
					send_msg(conn, SM_NO_DEBUG, "ADM DUC_status=301");
				}
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

		TaskSleepMsec(250);
	}
}
