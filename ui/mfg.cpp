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
#include "nbuf.h"
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

bool sd_copy_in_progress;

void c2s_mfg_setup(void *param)
{
	conn_t *conn = (conn_t *) param;

	send_msg(conn, SM_NO_DEBUG, "MFG ver_maj=%d ver_min=%d", VERSION_MAJ, VERSION_MIN);

	int next_serno = eeprom_next_serno(SERNO_READ, 0);
	int serno = eeprom_check();
	send_msg(conn, SM_NO_DEBUG, "MFG next_serno=%d serno=%d", next_serno, serno);
}

void c2s_mfg(void *param)
{
	int n, i;
	conn_t *conn = (conn_t *) param;
	u4_t ka_time = timer_sec();
	
	int next_serno, serno;

	nbuf_t *nb = NULL;
	while (TRUE) {
	
		if (nb) web_to_app_done(conn, nb);
		n = web_to_app(conn, &nb);
				
		if (n) {			
			char *cmd = nb->buf;
			cmd[n] = 0;		// okay to do this -- see nbuf.c:nbuf_allocq()

			ka_time = timer_sec();
			
			// SECURITY: this must be first for auth check
			if (rx_common_cmd("MFG", conn, cmd))
				continue;
			
			printf("MFG: <%s>\n", cmd);

			i = strcmp(cmd, "SET write");
			if (i == 0) {
				printf("MFG: received write\n");
				eeprom_write(SERNO_ALLOC, 0);

				serno = eeprom_check();
				next_serno = eeprom_next_serno(SERNO_READ, 0);
				send_msg(conn, SM_NO_DEBUG, "MFG next_serno=%d serno=%d", next_serno, serno);
				continue;
			}

			next_serno = -1;
			i = sscanf(cmd, "SET set_serno=%d", &next_serno);
			if (i == 1) {
				printf("MFG: received set_serno=%d\n", next_serno);
				eeprom_next_serno(SERNO_WRITE, next_serno);

				serno = eeprom_check();
				next_serno = eeprom_next_serno(SERNO_READ, 0);
				send_msg(conn, SM_NO_DEBUG, "MFG next_serno=%d serno=%d", next_serno, serno);
				continue;
			}

#define SD_CMD "cd /root/" REPO_NAME "/tools; ./kiwiSDR-make-microSD-flasher-from-eMMC.sh --called_from_kiwi_server"
			i = strcmp(cmd, "SET microSD_write");
			if (i == 0) {
				mprintf_ff("MFG: received microSD_write\n");
				#define NBUF 256
				char *buf = (char *) kiwi_malloc("c2s_mfg", NBUF);
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
					TaskSleep(250000);
				} while (n > 0);
				err = non_blocking_cmd_pclose(&p);
				sd_copy_in_progress = false;
				
				err = (err < 0)? err : WEXITSTATUS(err);
				mprintf("MFG: system returned %d\n", err);
				kiwi_free("c2s_mfg", buf);
				#undef NBUF
				send_msg(conn, SM_NO_DEBUG, "MFG microSD_done=%d", err);
				continue;
			}

			i = strcmp(cmd, "SET mfg_power_off");
			if (i == 0) {
				system("halt");
				while (true)
					usleep(100000);
			}

			printf("MFG: unknown command: <%s>\n", cmd);
			continue;
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
