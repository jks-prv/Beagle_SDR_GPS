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

// Copyright (c) 2016 John Seamons, ZL4VO/KF6VO

#include "types.h"
#include "config.h"
#include "kiwi.h"
#include "system.h"
#include "rx.h"
#include "rx_util.h"
#include "mem.h"
#include "misc.h"
#include "net.h"
#include "nbuf.h"
#include "web.h"
#include "eeprom.h"
#include "spi.h"
#include "gps.h"
#include "coroutines.h"
#include "debug.h"
#include "printf.h"
#include "kiwi_ui.h"
#include "fpga.h"

#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sched.h>
#include <stdlib.h>
#include <math.h>

bool sd_copy_in_progress;

static void mfg_send_info(conn_t *conn)
{
	int next_serno = eeprom_next_serno(EE_SERNO_READ, 0);
	model_e model;
	int serno = eeprom_check(&model);
	send_msg(conn, SM_NO_DEBUG, "MFG next_serno=%d serno=%d model=%d", next_serno, serno, model);
}

void c2s_mfg_setup(void *param)
{
	conn_t *conn = (conn_t *) param;

	send_msg(conn, SM_NO_DEBUG, "MFG ver_maj=%d ver_min=%d dna=%08x%08x",
	    version_maj, version_min, PRINTF_U64_ARG(fpga_dna()));
	mprintf_ff("MFG interface\n");
	mfg_send_info(conn);
}

void c2s_mfg(void *param)
{
	int n, i;
	conn_t *conn = (conn_t *) param;
	rx_common_init(conn);
	
	int next_serno;

	nbuf_t *nb = NULL;
	while (TRUE) {
	
		if (nb) web_to_app_done(conn, nb);
		n = web_to_app(conn, &nb);
				
		if (n) {			
			char *cmd = nb->buf;
			cmd[n] = 0;		// okay to do this -- see nbuf.c:nbuf_allocq()

    		TaskStat(TSTAT_INCR|TSTAT_ZERO, 0, "cmd");
			
			// SECURITY: this must be first for auth check
			if (rx_common_cmd(STREAM_MFG, conn, cmd))
				continue;
			
			//printf("MFG: <%s>\n", cmd);

			int _type, _serno, _model = 0;
			char *_key = NULL;
			i = sscanf(cmd, "SET eeprom_write=%d serno=%d model=%d key=%64ms", &_type, &_serno, &_model, &_key);
			if ((i == 3 || i == 4) && _model > 0) {
				printf("MFG: received write, type=%d serno=%d model=%d key=%s\n", _type, _serno, _model, _key);
				eeprom_write(_type? EE_SERNO_WRITE : EE_SERNO_ALLOC, _serno, _model, _key);
				kiwi_asfree(_key);
				mfg_send_info(conn);
				continue;
			}

			next_serno = -1;
			i = sscanf(cmd, "SET set_serno=%d", &next_serno);
			if (i == 1) {
				printf("MFG: received set_serno=%d\n", next_serno);
				eeprom_next_serno(EE_SERNO_WRITE, next_serno);
                mfg_send_info(conn);
				continue;
			}

			i = strcmp(cmd, "SET microSD_write");
			if (i == 0) {
				mprintf_ff("MFG: received microSD_write\n");
			    sd_backup(conn, false);
				continue;
			}

			i = strcmp(cmd, "SET mfg_restart");
			if (i == 0) {
				kiwi_restart();
				while (true)
					kiwi_usleep(100000);
			}

			i = strcmp(cmd, "SET mfg_power_off");
			if (i == 0) {
                system_poweroff();
				while (true)
					kiwi_usleep(100000);
			}

			char *cmd_p, *user_m = NULL, *host_m = NULL;
			n = sscanf(cmd, "SET rev_config user=%256ms host=%256ms", &user_m, &host_m);
			if (n == 2) {
                const char *proxy_server = admcfg_string("proxy_server", NULL, CFG_REQUIRED);
                asprintf(&cmd_p, "sed -e s/SERVER/%s/ -e s/USER/%s/ -e s/HOST/%s/ -e s/PORT/%d/ %s >%s",
                    proxy_server, user_m, host_m, net.port_ext, DIR_CFG "/frpc.template.ini", DIR_CFG "/frpc.ini");
                printf("proxy register: %s\n", cmd_p);
                system(cmd_p);
                kiwi_asfree(cmd_p);
                kiwi_asfree(user_m); kiwi_asfree(host_m);
                admcfg_string_free(proxy_server);
				continue;
			}

			printf("MFG: unknown command: <%s>\n", cmd);
			continue;
		}
		
		conn->keep_alive = timer_sec() - conn->keepalive_time;
		//if ((conn->keep_alive %4) == 0) printf("CK KA %d/%d\n", conn->keep_alive, KEEPALIVE_SEC);
		bool keepalive_expired = (conn->keep_alive > KEEPALIVE_SEC);
		if (keepalive_expired || conn->kick) {
			printf("MFG connection closed\n");
			rx_server_remove(conn);
			return;
		}

		TaskSleepMsec(250);
	}
}
