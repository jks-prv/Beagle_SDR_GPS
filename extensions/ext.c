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
#include "cfg.h"
#include "ext_int.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>

ext_users_t ext_users[RX_CHANS];

double ext_get_sample_rateHz()
{
	return adc_clock / (RX1_DECIM * RX2_DECIM);
}

void ext_register_receive_iq_samps(ext_receive_iq_samps_t func, int rx_chan)
{
	ext_users[rx_chan].receive_iq = func;
}

void ext_unregister_receive_iq_samps(int rx_chan)
{
	ext_users[rx_chan].receive_iq = NULL;
}

void ext_register_receive_real_samps(ext_receive_real_samps_t func, int rx_chan)
{
	ext_users[rx_chan].receive_real = func;
}

void ext_unregister_receive_real_samps(int rx_chan)
{
	ext_users[rx_chan].receive_real = NULL;
}

static int n_exts;
static ext_t *ext_list[N_EXT];

void ext_register(ext_t *ext)
{
	check(n_exts < N_EXT);
	ext_list[n_exts] = ext;
	printf("ext_register: #%d \"%s\"\n", n_exts, ext->name);
	n_exts++;
}

int ext_send_msg(int rx_chan, bool debug, const char *msg, ...)
{
	va_list ap;
	char *s;

	conn_t *conn = ext_users[rx_chan].conn;
	if (!conn->mc) return -1;
	va_start(ap, msg);
	vasprintf(&s, msg, ap);
	va_end(ap);
	if (debug) printf("ext_send_msg: RX%d-%p <%s>\n", rx_chan, conn, s);
	mg_websocket_write(conn->mc, WS_OPCODE_BINARY, s, strlen(s));
	free(s);
	return 0;
}

int ext_send_data_msg(int rx_chan, bool debug, u1_t cmd, u1_t *bytes, int nbytes)
{
	conn_t *conn = ext_users[rx_chan].conn;
	if (debug) printf("ext_send_data_msg: RX%d-%p cmd %d nbytes %d\n", rx_chan, conn, cmd, nbytes);
	if (!conn->mc) return -1;
	send_data_msg(conn, SM_NO_DEBUG, cmd, bytes, nbytes);
	return 0;
}

int ext_send_encoded_msg(int rx_chan, bool debug, const char *dst, const char *cmd, const char *fmt, ...)
{
	va_list ap;
	char *s;

	if (cmd == NULL || fmt == NULL) return 0;
	conn_t *conn = ext_users[rx_chan].conn;
	if (!conn->mc) return -1;
	
	va_start(ap, fmt);
	vasprintf(&s, fmt, ap);
	va_end(ap);
	
	char *buf = str_encode(s);
	free(s);
	ext_send_msg(rx_chan, debug, "%s %s=%s", dst, cmd, buf);
	free(buf);
	return 0;
}


// private

void extint_setup()
{
	extint_init();
}

void extint_send_extlist(conn_t *conn)
{
	int i;
	#define	MAX_EXT_NAME 32
	char *elist = (char *) malloc(N_EXT * MAX_EXT_NAME);
	strcpy(elist, "[");

	for (i=0; i < n_exts; i++) {
		ext_t *ext = ext_list[i];

		// by now all the extensions have long since registered via ext_register()
		// send a list of all extensions to an object on the .js side
		sprintf(elist + strlen(elist), "\"%s\",", ext->name);
	}
	strcpy(&elist[strlen(elist)-1], "]");
	printf("elist = %s\n", elist);
	send_encoded_msg_mc(conn->mc, "MSG", "extint_list_json", "%s", elist);
}

// create the <script> tags needed to load all the extension .js and .css files
char *extint_list_js()
{
	int i;
	int blen = 1;
	char *buf = (char *) malloc(blen);
	*buf = '\0';

	for (i=0; i < n_exts; i++) {
		ext_t *ext = ext_list[i];
		char *s1, *s2;
		blen += asprintf(&s1, "<script src=\"extensions/%s/%s.js\"></script>\n", ext->name, ext->name);
		blen += asprintf(&s2, "<link rel=\"stylesheet\" type=\"text/css\" href=\"extensions/%s/%s.css\" />\n", ext->name, ext->name);
		buf = (char *) realloc(buf, blen);
		strcat(buf, s1);
		strcat(buf, s2);
		free(s1);
		free(s2);
	}

	return buf;
}

void extint_load_extension_configs(conn_t *conn)
{
	int i;
	for (i=0; i < n_exts; i++) {
		ext_t *ext = ext_list[i];
		send_encoded_msg_mc(conn->mc, "ADM", "ext_config_html", "%s", ext->name);
	}
}

void extint_w2a(void *param)
{
	int n, i, j;
	conn_t *conn = (conn_t *) param;
	u4_t ka_time = timer_sec();
	
	// initialize extension for this connection
	send_msg_mc(conn->mc, false, "EXT ext_client_init");
	
	nbuf_t *nb = NULL;
	while (TRUE) {
		int rx_chan, ext_rx_chan;
		ext_t *ext;
	
		if (nb) web_to_app_done(conn, nb);
		n = web_to_app(conn, &nb);
				
		if (n) {
			char *cmd = nb->buf;
			cmd[n] = 0;		// okay to do this -- see nbuf.c:nbuf_allocq()
			char id[64];

			ka_time = timer_sec();

			// receive and send a roundtrip keepalive
			i = strcmp(cmd, "SET keepalive");
			if (i == 0) {
				ext_send_msg(conn->ext_rx_chan, false, "EXT keepalive");
				continue;
			}

			ext_rx_chan = conn->ext_rx_chan;
			printf("extint_w2a: CONN%d-%p RX%d-%p %d <%s>\n", conn->self_idx, conn, ext_rx_chan, (ext_rx_chan == -1)? 0:ext_users[ext_rx_chan].conn, strlen(cmd), cmd);

			// answer from client ext about who they are
			// match against list of known extensions and register msg handler
			char ext_name[32];
			i = sscanf(cmd, "SET ext_client_reply=%s rx_chan=%d", ext_name, &rx_chan);
			if (i == 2) {
				for (i=0; i < n_exts; i++) {
					if (strcmp(ext_name, ext_list[i]->name) == 0) {
						printf("ext_client_reply: found func %p CONN%d-%p for ext %s RX%d\n", ext_list[i]->receive_msgs, conn->self_idx, conn, ext_name, rx_chan);
						ext_users[rx_chan].ext = ext_list[i];
						ext_users[rx_chan].conn = conn;
						conn->ext_rx_chan = rx_chan;
						break;
					}
				}
				if (i == n_exts) panic("ext_client_reply: unknown ext");

				// automatically let extension server-side know the connection has been established and
				// our stream thread is running
				ext_users[rx_chan].ext->receive_msgs((char *) "SET ext_server_init", rx_chan);
				continue;
			}
			
			char client[32];
			i = sscanf(cmd, "SET ext_switch_to_client=%s rx_chan=%d", client, &rx_chan);
			if (i == 2) {
				for (i=0; i < n_exts; i++) {
					if (strcmp(client, ext_list[i]->name) == 0) {
						printf("ext_switch_to_client: found func %p CONN%d-%p for ext %s RX%d\n", ext_list[i]->receive_msgs, conn->self_idx, conn, client, rx_chan);
						ext_users[rx_chan].ext = ext_list[i];
						ext_users[rx_chan].conn = conn;
						conn->ext_rx_chan = rx_chan;
						break;
					}
				}
				if (i == n_exts) panic("ext_client_reply: unknown ext");
				continue;
			}
			
			int blur;
			i = sscanf(cmd, "SET ext_blur=%d", &blur);
			if (i == 1) {
				ext_users[blur].ext = NULL;
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
			
			ext_rx_chan = conn->ext_rx_chan;
			ext = ext_users[ext_rx_chan].ext;
			if (ext->receive_msgs) {
				printf("extint_w2a: ext_receive_msgs() %p c %p RX%d-%p %d <%s>\n", ext->receive_msgs, conn, ext_rx_chan, (ext_rx_chan == -1)? 0:ext_users[ext_rx_chan].conn, strlen(cmd), cmd);
				if (ext->receive_msgs(cmd, ext_rx_chan))
					continue;
			}
			
			printf("extint_w2a: unknown command: <%s> ======================================================\n", cmd);
			continue;
		}
		
		conn->keep_alive = timer_sec() - ka_time;
		bool keepalive_expired = (conn->keep_alive > KEEPALIVE_SEC);
		if (keepalive_expired) {
			ext_rx_chan = conn->ext_rx_chan;
			ext = ext_users[ext_rx_chan].ext;
			printf("EXT KEEP-ALIVE EXPIRED RX%d %s\n", ext_rx_chan, ext->name);
			if (ext->close_conn != NULL)
				ext->close_conn(ext_rx_chan);
			rx_server_remove(conn);
			return;
		}

		TaskSleep(250000);
	}
}
