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
#include "kiwi.h"
#include "misc.h"
#include "apps.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>

double app_get_sample_rateHz()
{
	return adc_clock / (RX1_DECIM * RX2_DECIM);
}

app_receive_iq_samps_t app_receive_iq_samps[RX_CHANS];

void app_register_receive_iq_samps(app_receive_iq_samps_t func, int rx_chan)
{
	app_receive_iq_samps[rx_chan] = func;
}

void app_unregister_receive_iq_samps(int rx_chan)
{
	app_receive_iq_samps[rx_chan] = NULL;
}

struct app_list_t {
	const char *name;
	app_receive_msgs_t func;
};

static int n_apps;
static app_list_t app_list[32];

app_receive_msgs_t app_receive_msgs[RX_CHANS];

void app_register_receive_msgs(const char *name, app_receive_msgs_t func)
{
	printf("app_register_receive_msgs: #%d app %s msgs func %p\n", n_apps, name, func);
	app_list_t *al = &app_list[n_apps++];
	al->name = name;
	al->func = func;
}

conn_t *app_conn[RX_CHANS];

int app_send_msg(int rx_chan, bool debug, const char *msg, ...)
{
	va_list ap;
	char *s;

	if (!app_conn[rx_chan]->mc) return -1;
	va_start(ap, msg);
	vasprintf(&s, msg, ap);
	va_end(ap);
	if (debug) printf("app_send_msg: RX%d-%p <%s>\n", rx_chan, app_conn[rx_chan], s);
	mg_websocket_write(app_conn[rx_chan]->mc, WS_OPCODE_BINARY, s, strlen(s));
	free(s);
	return 0;
}

int app_send_data_msg(int rx_chan, bool debug, u1_t cmd, u1_t *bytes, int nbytes)
{
	if (debug) printf("app_send_data_msg: RX%d-%p cmd %d nbytes %d\n", rx_chan, app_conn[rx_chan], cmd, nbytes);
	if (!app_conn[rx_chan]->mc) return -1;
	send_data_msg(app_conn[rx_chan], SM_NO_DEBUG, cmd, bytes, nbytes);
	return 0;
}

int app_send_encoded_msg(int rx_chan, bool debug, const char *dst, const char *cmd, const char *fmt, ...)
{
	va_list ap;
	char *s;

	if (cmd == NULL || fmt == NULL) return 0;
	if (!app_conn[rx_chan]->mc) return -1;
	
	va_start(ap, fmt);
	vasprintf(&s, fmt, ap);
	va_end(ap);
	
	char *buf = str_encode(s);
	free(s);
	app_send_msg(rx_chan, debug, "%s %s=%s", dst, cmd, buf);
	free(buf);
	return 0;
}


// private

void apps_setup()
{
	int i;
	for (i=0; i < RX_CHANS; i++) {
		app_receive_msgs[i] = NULL;
		app_conn[i] = NULL;
		app_receive_iq_samps[i] = NULL;
	}
	apps_init();
}

void w2a_apps(void *param)
{
	int n, i, j;
	conn_t *conn = (conn_t *) param;
	u4_t ka_time = timer_sec();
	
	// for this connection, ask client app who they are
	send_msg(conn, SM_DEBUG, "APP app_client_name_query");
	
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
				app_send_msg(conn->app_rx_chan, false, "APP keepalive");
				continue;
			}

			printf("w2a_apps: c %p RX%d-%p %d <%s>\n", conn, conn->app_rx_chan, (conn->app_rx_chan == -1)? 0:app_conn[conn->app_rx_chan], strlen(cmd), cmd);

			// answer from client app about who they are
			// match against list of known apps and register msg handler
			char ident[32];
			int rx_chan;
			i = sscanf(cmd, "SET app_client_name_reply=%s rx_chan=%d", ident, &rx_chan);
			if (i == 2) {
				for (i=0; i < n_apps; i++) {
					if (strcmp(ident, app_list[i].name) == 0) {
						printf("w2a_apps: found func %p conn %p for app %s RX%d\n", app_list[i].func, conn, ident, rx_chan);
						app_receive_msgs[rx_chan] = app_list[i].func;
						app_conn[rx_chan] = conn;
						conn->app_rx_chan = rx_chan;
						break;
					}
				}
				if (i == n_apps) panic("unknown app");

				// automatically let app server-side know the connection has been established and
				// our stream thread is running
				app_receive_msgs[rx_chan]((char *) "SET init", rx_chan);
				continue;
			}
			
			i = strcmp(cmd, "SET init");
			if (i == 0) {
				continue;
			}

			i = sscanf(cmd, "SERVER DE CLIENT %s", id);
			if (i == 1) {
				continue;
			}
			
			if (app_receive_msgs[conn->app_rx_chan]) {
				printf("w2a_apps: app_receive_msgs() c %p RX%d-%p %d <%s>\n", conn, conn->app_rx_chan, (conn->app_rx_chan == -1)? 0:app_conn[conn->app_rx_chan], strlen(cmd), cmd);
				if (app_receive_msgs[conn->app_rx_chan](cmd, conn->app_rx_chan))
					continue;
			}
			
			printf("w2a_apps: unknown command: <%s>\n", cmd);
			continue;
		}
		
		conn->keep_alive = timer_sec() - ka_time;
		bool keepalive_expired = (conn->keep_alive > KEEPALIVE_SEC);
		if (keepalive_expired) {
			printf("APP KEEP-ALIVE EXPIRED\n");
			rx_server_remove(conn);
			return;
		}

		TaskSleep(250000);
	}
}
