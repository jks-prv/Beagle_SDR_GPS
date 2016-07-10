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
#include "ext.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>

double ext_get_sample_rateHz()
{
	return adc_clock / (RX1_DECIM * RX2_DECIM);
}

ext_receive_iq_samps_t ext_receive_iq_samps[RX_CHANS];

void ext_register_receive_iq_samps(ext_receive_iq_samps_t func, int rx_chan)
{
	ext_receive_iq_samps[rx_chan] = func;
}

void ext_unregister_receive_iq_samps(int rx_chan)
{
	ext_receive_iq_samps[rx_chan] = NULL;
}

ext_receive_real_samps_t ext_receive_real_samps[RX_CHANS];

void ext_register_receive_real_samps(ext_receive_real_samps_t func, int rx_chan)
{
	ext_receive_real_samps[rx_chan] = func;
}

void ext_unregister_receive_real_samps(int rx_chan)
{
	ext_receive_real_samps[rx_chan] = NULL;
}

struct ext_list_t {
	const char *name;
	ext_receive_msgs_t func;
};

static int n_exts;
#define	N_EXTS	256
static ext_t *ext_list[N_EXTS];

ext_receive_msgs_t ext_receive_msgs[RX_CHANS];

void ext_register(ext_t *ext)
{
	check(n_exts < N_EXTS);
	ext_list[n_exts] = ext;
	printf("ext_register: #%d \"%s\" msgs %p\n", n_exts, ext->name, ext->msgs);
	n_exts++;
}

conn_t *ext_conn[RX_CHANS];

int ext_send_msg(int rx_chan, bool debug, const char *msg, ...)
{
	va_list ap;
	char *s;

	if (!ext_conn[rx_chan]->mc) return -1;
	va_start(ap, msg);
	vasprintf(&s, msg, ap);
	va_end(ap);
	if (debug) printf("ext_send_msg: RX%d-%p <%s>\n", rx_chan, ext_conn[rx_chan], s);
	mg_websocket_write(ext_conn[rx_chan]->mc, WS_OPCODE_BINARY, s, strlen(s));
	free(s);
	return 0;
}

int ext_send_data_msg(int rx_chan, bool debug, u1_t cmd, u1_t *bytes, int nbytes)
{
	if (debug) printf("ext_send_data_msg: RX%d-%p cmd %d nbytes %d\n", rx_chan, ext_conn[rx_chan], cmd, nbytes);
	if (!ext_conn[rx_chan]->mc) return -1;
	send_data_msg(ext_conn[rx_chan], SM_NO_DEBUG, cmd, bytes, nbytes);
	return 0;
}

int ext_send_encoded_msg(int rx_chan, bool debug, const char *dst, const char *cmd, const char *fmt, ...)
{
	va_list ap;
	char *s;

	if (cmd == NULL || fmt == NULL) return 0;
	if (!ext_conn[rx_chan]->mc) return -1;
	
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
	int i;
	for (i=0; i < RX_CHANS; i++) {
		ext_receive_msgs[i] = NULL;
		ext_conn[i] = NULL;
		ext_receive_iq_samps[i] = NULL;
		ext_receive_real_samps[i] = NULL;
	}
	extint_init();
}

void extint_send_extlist(conn_t *conn)
{
	int i;
	char *elist = (char *) malloc(N_EXTS * 32);
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
	char *json = cfg_get_json(NULL);
	send_encoded_msg_mc(conn->mc, "EXT", "ext_cfg_json", "%s", json);
	send_msg_mc(conn->mc, false, "EXT ext_client_init");
	
	nbuf_t *nb = NULL;
	while (TRUE) {
		int rx_chan;
	
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

			printf("extint_w2a: CONN%d-%p RX%d-%p %d <%s>\n", conn->self_idx, conn, conn->ext_rx_chan, (conn->ext_rx_chan == -1)? 0:ext_conn[conn->ext_rx_chan], strlen(cmd), cmd);

			// answer from client ext about who they are
			// match against list of known extensions and register msg handler
			char ext_name[32];
			i = sscanf(cmd, "SET ext_client_reply=%s rx_chan=%d", ext_name, &rx_chan);
			if (i == 2) {
				for (i=0; i < n_exts; i++) {
					if (strcmp(ext_name, ext_list[i]->name) == 0) {
						printf("ext_client_reply: found func %p CONN%d-%p for ext %s RX%d\n", ext_list[i]->msgs, conn->self_idx, conn, ext_name, rx_chan);
						ext_receive_msgs[rx_chan] = ext_list[i]->msgs;
						ext_conn[rx_chan] = conn;
						conn->ext_rx_chan = rx_chan;
						break;
					}
				}
				if (i == n_exts) panic("ext_client_reply: unknown ext");

				// automatically let extension server-side know the connection has been established and
				// our stream thread is running
				ext_receive_msgs[rx_chan]((char *) "SET ext_server_init", rx_chan);
				continue;
			}
			
			char client[32];
			i = sscanf(cmd, "SET ext_switch_to_client=%s rx_chan=%d", client, &rx_chan);
			if (i == 2) {
				for (i=0; i < n_exts; i++) {
					if (strcmp(ext_name, ext_list[i]->name) == 0) {
						printf("ext_switch_to_client: found func %p CONN%d-%p for ext %s RX%d\n", ext_list[i]->msgs, conn->self_idx, conn, ext_name, rx_chan);
						ext_receive_msgs[rx_chan] = ext_list[i]->msgs;
						ext_conn[rx_chan] = conn;
						conn->ext_rx_chan = rx_chan;
						break;
					}
				}
				if (i == n_exts) panic("ext_client_reply: unknown ext");
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
			
			if (ext_receive_msgs[conn->ext_rx_chan]) {
				printf("extint_w2a: ext_receive_msgs() %p c %p RX%d-%p %d <%s>\n", ext_receive_msgs[conn->ext_rx_chan], conn, conn->ext_rx_chan, (conn->ext_rx_chan == -1)? 0:ext_conn[conn->ext_rx_chan], strlen(cmd), cmd);
				if (ext_receive_msgs[conn->ext_rx_chan](cmd, conn->ext_rx_chan))
					continue;
			}
			
			printf("extint_w2a: unknown command: <%s> ======================================================\n", cmd);
			continue;
		}
		
		conn->keep_alive = timer_sec() - ka_time;
		bool keepalive_expired = (conn->keep_alive > KEEPALIVE_SEC);
		if (keepalive_expired) {
			printf("EXT KEEP-ALIVE EXPIRED\n");
			rx_server_remove(conn);
			return;
		}

		TaskSleep(250000);
	}
}
