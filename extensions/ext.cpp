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
#include "clk.h"
#include "misc.h"
#include "str.h"
#include "cfg.h"
#include "gps.h"
#include "rx.h"
#include "ext_int.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>

ext_users_t ext_users[MAX_RX_CHANS];

double ext_update_get_sample_rateHz(int rx_chan)
{
    double adc_clk;

    if (rx_chan == -1) {
        adc_clk = adc_clock_system();
    } else
    if (rx_chan == -2) {
        adc_clk = ADC_CLOCK_TYP;
    } else {
        // jksx FIXME XXX WRONG-WRONG-WRONG
	    //conn_t *c = ext_users[rx_chan].conn_ext;
        //srate = c->adc_clock_corrected;
        //c->srate = srate;   // update stored sample rate since we're using a new clock value
        adc_clk = adc_clock_system();
    }
    
    double srate = adc_clk / rx_decim;
    //printf("EXT adc_clk=%.6f srate=%.6f\n", adc_clk, srate);
	return srate;
}

void ext_adjust_clock_offset(int rx_chan, double offset)
{
	if (offset > -1000.0 && offset < 1000.0)
	    return;
	
    /* jksx FIXME XXX WRONG-WRONG-WRONG
	conn_t *c = ext_users[rx_chan].conn_ext;
    c->adc_clock_corrected -= c->manual_offset;		// remove old offset first
    c->manual_offset = offset;
    c->adc_clock_corrected += c->manual_offset;
    clk.adc_clk_corrections++;
    c->srate = c->adc_clock_corrected / rx_decim;
    cprintf(c, "ext_adjust_clock_offset: clk.adc_clock %.6f offset %.2f\n", c->adc_clock_corrected/1e6, offset);
    */
}

void ext_register_receive_iq_samps(ext_receive_iq_samps_t func, int rx_chan)
{
	ext_users[rx_chan].receive_iq = func;
}

void ext_register_receive_iq_samps_task(tid_t tid, int rx_chan)
{
	ext_users[rx_chan].receive_iq_tid = tid;
}

void ext_unregister_receive_iq_samps(int rx_chan)
{
	ext_users[rx_chan].receive_iq = NULL;
}

void ext_unregister_receive_iq_samps_task(int rx_chan)
{
	ext_users[rx_chan].receive_iq_tid = (tid_t) NULL;
}

void ext_register_receive_real_samps(ext_receive_real_samps_t func, int rx_chan)
{
	ext_users[rx_chan].receive_real = func;
}

void ext_register_receive_real_samps_task(tid_t tid, int rx_chan)
{
	ext_users[rx_chan].receive_real_tid = tid;
}

void ext_unregister_receive_real_samps(int rx_chan)
{
	ext_users[rx_chan].receive_real = NULL;
}

void ext_unregister_receive_real_samps_task(int rx_chan)
{
	ext_users[rx_chan].receive_real_tid = (tid_t) NULL;
}

void ext_register_receive_FFT_samps(ext_receive_FFT_samps_t func, int rx_chan, int flags)
{
	ext_users[rx_chan].receive_FFT = func;
	ext_users[rx_chan].FFT_flags = flags;
}

void ext_unregister_receive_FFT_samps(int rx_chan)
{
	ext_users[rx_chan].receive_FFT = NULL;
}

void ext_register_receive_S_meter(ext_receive_S_meter_t func, int rx_chan)
{
	ext_users[rx_chan].receive_S_meter = func;
}

void ext_unregister_receive_S_meter(int rx_chan)
{
	ext_users[rx_chan].receive_S_meter = NULL;
}

static int n_exts;
static ext_t *ext_list[N_EXT];

void ext_register(ext_t *ext)
{
	check(n_exts < N_EXT);
	ext_list[n_exts] = ext;
	if (strcmp(ext->name, "ant_switch") == 0)
	    have_ant_switch_ext = true;
	printf("ext_register: #%d \"%s\"\n", n_exts, ext->name);
	n_exts++;
}

int ext_send_msg(int rx_chan, bool debug, const char *msg, ...)
{
	va_list ap;
	char *s;

	conn_t *conn = ext_users[rx_chan].conn_ext;
	if (!conn) return -1;
	va_start(ap, msg);
	vasprintf(&s, msg, ap);
	va_end(ap);
	if (debug) printf("ext_send_msg: RX%d(%p) <%s>\n", rx_chan, conn, s);
	send_msg_buf(conn, s, strlen(s));
	free(s);
	return 0;
}

int ext_send_msg_data(int rx_chan, bool debug, u1_t cmd, u1_t *bytes, int nbytes)
{
	conn_t *conn = ext_users[rx_chan].conn_ext;
	if (debug) printf("ext_send_msg_data: RX%d(%p) cmd %d nbytes %d\n", rx_chan, conn, cmd, nbytes);
	if (!conn) return -1;
	send_msg_data(conn, SM_NO_DEBUG, cmd, bytes, nbytes);
	return 0;
}

int ext_send_msg_data2(int rx_chan, bool debug, u1_t cmd, u1_t data2, u1_t *bytes, int nbytes)
{
	conn_t *conn = ext_users[rx_chan].conn_ext;
	if (debug) printf("ext_send_msg_data2: RX%d(%p) cmd %d data2 %d nbytes %d\n", rx_chan, conn, cmd, data2, nbytes);
	if (!conn) return -1;
	send_msg_data2(conn, SM_NO_DEBUG, cmd, data2, bytes, nbytes);
	return 0;
}

int ext_send_msg_encoded(int rx_chan, bool debug, const char *dst, const char *cmd, const char *fmt, ...)
{
	va_list ap;
	char *s;

	if (cmd == NULL || fmt == NULL) return 0;
	conn_t *conn = ext_users[rx_chan].conn_ext;
	if (!conn) return -1;
	
	va_start(ap, fmt);
	vasprintf(&s, fmt, ap);
	va_end(ap);
	
	char *buf = kiwi_str_encode(s);
	free(s);
	ext_send_msg(rx_chan, debug, "%s %s=%s", dst, cmd, buf);
	free(buf);
	return 0;
}


////////////////////////////////
// private
////////////////////////////////

void extint_setup()
{
	extint_init();
}

void extint_send_extlist(conn_t *conn)
{
	int i;
	#define	MAX_EXT_NAME 32
	char *elist = (char *) malloc(N_EXT * MAX_EXT_NAME);

	if (n_exts == 0) {
		strcpy(elist, "null");
	} else {
		strcpy(elist, "[");
		for (i=0; i < n_exts; i++) {
			ext_t *ext = ext_list[i];
	
			// by now all the extensions have long since registered via ext_register()
			// send a list of all extensions to an object on the .js side
			sprintf(elist + strlen(elist), "\"%s\"%s", ext->name, (i < (n_exts-1))? ",":"]");
		}
	}
	//printf("elist = %s\n", elist);
	send_msg_encoded(conn, "MSG", "extint_list_json", "%s", elist);
	free(elist);
}

// create the <script> tags needed to load all the extension .js and .css files
char *extint_list_js()
{
	int i;
	char *sb;
	
	sb = NULL;
	for (i=0; i < n_exts; i++) {
		ext_t *ext = ext_list[i];
		//sb = kstr_asprintf(sb, "<script>alert('%s.js');</script>\n", ext->name);
		sb = kstr_asprintf(sb, "<script src=\"extensions/%s/%s.js\"></script>\n", ext->name, ext->name);
		sb = kstr_asprintf(sb, "<link rel=\"stylesheet\" type=\"text/css\" href=\"extensions/%s/%s.css\" />\n", ext->name, ext->name);
	}
    //sb = kstr_asprintf(sb, "<script>alert('exts done');</script>\n");

	return sb;
}

void extint_load_extension_configs(conn_t *conn)
{
	int i;
	for (i=0; i < n_exts; i++) {
		ext_t *ext = ext_list[i];
		send_msg_encoded(conn, "ADM", "ext_call", "%s_config_html", ext->name);
	}
}

void extint_ext_users_init(int rx_chan)
{
    // so that rx_chan_free_count() doesn't count EXT_FLAGS_HEAVY when extension isn't running
    //printf("extint_ext_users_init rx_chan=%d\n", rx_chan);
    rx_channels[rx_chan].ext = NULL;
    memset(&ext_users[rx_chan], 0, sizeof(ext_users_t));
}

void extint_setup_c2s(void *param)
{
	conn_t *conn_ext = (conn_t *) param;

	// initialize extension for this connection
	// NB: has to be a 'MSG' and not an 'EXT' due to sequencing of recv_cb setup
    printf("rx%d SET extint_setup_c2s: ext_client_init=%d(is_locked)\n", conn_ext->ext_rx_chan, is_locked);
	send_msg(conn_ext, false, "MSG ext_client_init=%d", is_locked);
}

void extint_c2s(void *param)
{
	int n, i, j;
	conn_t *conn_ext = (conn_t *) param;    // STREAM_EXT conn_t
	rx_common_init(conn_ext);
	
	nbuf_t *nb = NULL;
	while (TRUE) {
		int rx_chan, ext_rx_chan;
		ext_t *ext = NULL;
	
		if (nb) web_to_app_done(conn_ext, nb);
		n = web_to_app(conn_ext, &nb);
				
		if (n) {
			char *cmd = nb->buf;
			cmd[n] = 0;		// okay to do this -- see nbuf.c:nbuf_allocq()

			// SECURITY: this must be first for auth check (except for keepalive check above)
			if (rx_common_cmd("EXT", conn_ext, cmd))
				continue;
			
			//printf("extint_c2s: %s CONN%d(%p) RX=%d(%p) %d <%s>\n", conn_ext->ext? conn_ext->ext->name:"?", conn_ext->self_idx,
			//    conn_ext, conn_ext->ext_rx_chan, (conn_ext->ext_rx_chan == -1)? 0:ext_users[conn_ext->ext_rx_chan].conn_ext, strlen(cmd), cmd);

			// answer from client ext about who they are
			// match against list of known extensions and register msg handler
			char *client_m = NULL;
			int first_time;

			i = sscanf(cmd, "SET ext_switch_to_client=%32ms first_time=%d rx_chan=%d", &client_m, &first_time, &rx_chan);
			if (i == 3) {
				for (i=0; i < n_exts; i++) {
					ext = ext_list[i];
					if (strcmp(client_m, ext->name) == 0) {
						//printf("ext_switch_to_client: found func %p CONN%d(%p) for ext %s RX%d\n", ext->receive_msgs, conn_ext->self_idx, conn_ext, client_m, rx_chan);
                        ext_users_t *eusr = &ext_users[rx_chan];
                        eusr->valid = TRUE;
						eusr->ext = ext;
						eusr->conn_ext = conn_ext;
						conn_ext->ext_rx_chan = rx_chan;
						conn_ext->ext = ext;
						TaskNameS(ext->name);
                        u4_t flags = TaskFlags();
                        TaskSetFlags(flags | CTF_RX_CHANNEL | (conn_ext->ext_rx_chan & CTF_CHANNEL));

                        // point STREAM_SOUND conn at ext_t so it has access to the ext->name after ext conn_t is gone
                        // point rx_channel at ext_t so it has access to ext->flags for EXT_FLAGS_HEAVY checking
                        conn_t *c = rx_channels[rx_chan].conn;
                        if (c && c->valid && c->type == STREAM_SOUND) {
                            c->ext = ext;
                            rx_channels[rx_chan].ext = ext;
                        }

						break;
					}
				}
				if (i == n_exts) {
				    printf("ext_switch_to_client: <%s>\n", client_m);
				    panic("ext_switch_to_client: unknown ext");
				}

				ext_send_msg(conn_ext->ext_rx_chan, false, "MSG EXT-STOP-FLUSH-INPUT");

				// Automatically let extension server-side know the connection has been established and
				// our stream thread is running. Only called ONCE per client session.
				if (first_time) {
					SAN_NULL_PTR_CK(ext, ext->receive_msgs((char *) "SET ext_server_init", rx_chan));
				}
				
			    free(client_m);
				continue;
			}
			free(client_m);
			
			i = sscanf(cmd, "SET ext_blur=%d", &rx_chan);
			if (i == 1) {
				extint_ext_users_init(rx_chan);
				continue;
			}
			
			i = strcmp(cmd, "SET init");
			if (i == 0) {
				continue;
			}

			i = strcmp(cmd, "SET ext_is_locked_status");
			if (i == 0) {
			    printf("rx%d SET ext_is_locked_status: ext_client_init=%d(is_locked)\n", conn_ext->ext_rx_chan, is_locked);
                send_msg(conn_ext, false, "MSG ext_client_init=%d", is_locked);
				continue;
			}

			ext_rx_chan = conn_ext->ext_rx_chan;
			if (ext_rx_chan == -1) {
				printf("### extint_c2s: %s CONN%d(%p) ext_rx_chan == -1?\n", conn_ext->ext? conn_ext->ext->name:"?", conn_ext->self_idx, conn_ext);
				continue;
			}
			ext = ext_users[ext_rx_chan].ext;
			if (ext == NULL) {
				printf("### extint_c2s: %s CONN%d(%p) ext_rx_chan %d ext NULL?\n", conn_ext->ext? conn_ext->ext->name:"?", conn_ext->self_idx, conn_ext, ext_rx_chan);
				continue;
			}
			if (ext->receive_msgs) {
				//printf("extint_c2s: %s ext->receive_msgs() %p CONN%d(%p) RX%d(%p) %d <%s>\n", conn_ext->ext? conn_ext->ext->name:"?", ext->receive_msgs, conn_ext->self_idx, conn_ext, ext_rx_chan, (ext_rx_chan == -1)? 0:ext_users[ext_rx_chan].conn_ext, strlen(cmd), cmd);
				if (ext->receive_msgs(cmd, ext_rx_chan))
					continue;
			} else {
				printf("### extint_c2s: %s CONN%d(%p) RX%d(%p) ext->receive_msgs == NULL?\n", conn_ext->ext? conn_ext->ext->name:"?", conn_ext->self_idx, conn_ext, ext_rx_chan, (ext_rx_chan == -1)? 0:ext_users[ext_rx_chan].conn_ext);
				continue;
			}
			
			printf("extint_c2s: %s CONN%d(%p) unknown command: sl=%d %d|%d|%d [%s] ip=%s ==================================\n",
			    conn_ext->ext? conn_ext->ext->name:"?", conn_ext->self_idx, conn_ext,
			    strlen(cmd), cmd[0], cmd[1], cmd[2], cmd, conn_ext->remote_ip);

			continue;
		}
		
        ext_rx_chan = conn_ext->ext_rx_chan;
        ext = (ext_rx_chan == -1)? NULL : ext_users[ext_rx_chan].ext;

		conn_ext->keep_alive = timer_sec() - conn_ext->keepalive_time;
		bool keepalive_expired = (!conn_ext->internal_connection && conn_ext->keep_alive > KEEPALIVE_SEC);
		if (keepalive_expired || conn_ext->kick) {
			//printf("EXT %s RX%d %s\n", conn_ext->kick? "KICKED" : "KEEP-ALIVE EXPIRED", ext_rx_chan, ext? ext->name : "(no ext)");
			if (ext != NULL && ext->close_conn != NULL)
				ext->close_conn(ext_rx_chan);
			if (ext_rx_chan != -1)
				extint_ext_users_init(ext_rx_chan);
			rx_server_remove(conn_ext);
			panic("shouldn't return");
		}
		
		// call periodic callback if requested
        if (ext != NULL && ext->version == EXT_NEW_VERSION && ext->poll_cb != NULL)
            ext->poll_cb(ext_rx_chan);

		TaskSleepReasonMsec("ext-cmd", 250);
	}
}
