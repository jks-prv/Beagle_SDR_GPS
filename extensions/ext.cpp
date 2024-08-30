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

// Copyright (c) 2016 - 2022 John Seamons, ZL4VO/KF6VO

#include "types.h"
#include "kiwi.h"
#include "mode.h"
#include "clk.h"
#include "mem.h"
#include "misc.h"
#include "str.h"
#include "cfg.h"
#include "gps.h"
#include "rx.h"
#include "rx_util.h"
#include "ext_int.h"
#include "ant_switch.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>

ext_users_t ext_users[MAX_RX_CHANS];

double ext_update_get_sample_rateHz(int rx_chan)
{
    double adc_clk;

    if (rx_chan == ADC_CLK_SYS) {
        adc_clk = adc_clock_system();
    } else
    if (rx_chan == ADC_CLK_TYP) {
        adc_clk = ADC_CLOCK_TYP;
    } else {
        // FIXME XXX WRONG-WRONG-WRONG
        //conn_t *c = ext_users[rx_chan].conn_ext;
        //srate = c->adc_clock_corrected;
        //c->srate = srate;   // update stored sample rate since we're using a new clock value
        adc_clk = adc_clock_system();
    }

    double srate = adc_clk / rx_decim;
    //printf("EXT adc_clk=%.6f srate=%.6f\n", adc_clk, srate);
    return srate;
}

double ext_get_displayed_freq_kHz(int rx_chan)
{
    conn_t *conn = rx_channels[rx_chan].conn;
    if (conn == NULL) return 0;
    return ((double) conn->freqHz / kHz + freq.offset_kHz);
}

int ext_get_mode(int rx_chan)
{
    conn_t *conn = rx_channels[rx_chan].conn;
    if (conn == NULL) return MODE_AM;
    return conn->mode;
}

ext_auth_e ext_auth(int rx_chan)
{
    rx_chan_t *rx = &rx_channels[rx_chan];
    if (!rx->chan_enabled) return AUTH_USER;
    conn_t *conn = rx->conn;
    if (!conn) return AUTH_USER;

    if (conn->isLocal) return AUTH_LOCAL;
    if (conn->tlimit_exempt_by_pwd) return AUTH_PASSWORD;
    return AUTH_USER;
}

void ext_notify_connected(int rx_chan, u4_t seq, char *msg)
{
    extint.notify_chan = rx_chan;
    extint.notify_seq = seq;
    kiwi_strncpy(extint.notify_msg, msg, N_NOTIFY);
}

void ext_register_receive_iq_samps_raw(ext_receive_iq_samps_t func, int rx_chan)
{
	ext_users[rx_chan].receive_iq_pre_fir = func;
}

void ext_unregister_receive_iq_samps_raw(int rx_chan)
{
	ext_users[rx_chan].receive_iq_pre_fir = NULL;
}

void ext_register_receive_iq_samps(ext_receive_iq_samps_t func, int rx_chan, int flags)
{
    if (flags == PRE_AGC)
	    ext_users[rx_chan].receive_iq_pre_agc = func;
	else
	    ext_users[rx_chan].receive_iq_post_agc = func;
}

void ext_register_receive_iq_samps_task(tid_t tid, int rx_chan, int flags)
{
    if (flags == PRE_AGC)
	    ext_users[rx_chan].receive_iq_pre_agc_tid = tid;
	else
	    ext_users[rx_chan].receive_iq_post_agc_tid = tid;
}

void ext_unregister_receive_iq_samps(int rx_chan)
{
	ext_users[rx_chan].receive_iq_pre_agc = NULL;
	ext_users[rx_chan].receive_iq_post_agc = NULL;
}

void ext_unregister_receive_iq_samps_task(int rx_chan)
{
	ext_users[rx_chan].receive_iq_pre_agc_tid = (tid_t) NULL;
	ext_users[rx_chan].receive_iq_post_agc_tid = (tid_t) NULL;
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

void ext_register_receive_cmds(ext_receive_cmds_t func, int rx_chan)
{
    conn_t *conn = rx_channels[rx_chan].conn;
	conn->ext_cmd = func;
	if (conn->other != NULL) conn->other->ext_cmd = func;
}

void ext_unregister_receive_cmds(int rx_chan)
{
    conn_t *conn = rx_channels[rx_chan].conn;
    if (conn == NULL) return;
	conn->ext_cmd = NULL;
	if (conn->other != NULL) conn->other->ext_cmd = NULL;
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
	char *s;
	conn_t *conn = ext_users[rx_chan].conn_ext;
	if (!conn) return -1;

	va_list ap;
	va_start(ap, msg);
	vasprintf(&s, msg, ap);
	va_end(ap);

	if (debug) printf("ext_send_msg: RX%d(%p) <%s>\n", rx_chan, conn, s);
	send_msg_buf(conn, s, strlen(s));
	kiwi_asfree(s);
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
	char *s;
	if (cmd == NULL || fmt == NULL) return 0;
	conn_t *conn = ext_users[rx_chan].conn_ext;
	if (!conn) return -1;
	
	va_list ap;
	va_start(ap, fmt);
	vasprintf(&s, fmt, ap);
	va_end(ap);
	
	char *buf = kiwi_str_encode(s);
	kiwi_asfree(s);
	ext_send_msg(rx_chan, debug, "%s %s=%s", dst, cmd, buf);
	kiwi_ifree(buf, "ext_send_msg_encoded");
	return 0;
}

void ext_kick(int rx_chan)
{
    //printf("ext_kick rx_chan=%d\n", rx_chan);
	ext_users_t *extu = &ext_users[rx_chan];
	if (!extu) return;
	conn_t *conn_ext = extu->conn_ext;
	if (!conn_ext) return;
    //printf("ext_kick KICKING\n");
	conn_ext->kick = true;
}


////////////////////////////////
// private
////////////////////////////////

extint_t extint;

void extint_setup()
{
	extint_init();
	ant_switch_init();
}

void extint_send_extlist(conn_t *conn)
{
	int i;
	char *elist;

	if (n_exts == 0) {
		elist = (char *) "null";
	} else {
		elist = (char *) "[";
		for (i=0; i < n_exts; i++) {
			ext_t *ext = ext_list[i];
	
			// by now all the extensions have long since registered via ext_register()
			// send a list of all extensions to an object on the .js side
			elist = kstr_asprintf(elist, "\"%s\"%s", ext->name, (i < (n_exts-1))? ",":"]");
		}
	}
	//printf("elist = %s\n", kstr_sp(elist));
	send_msg_encoded(conn, "MSG", "extint_list_json", "%s", kstr_sp(elist));
	kstr_free(elist);
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
	send_msg(conn, false, "ADM ext_configs_done");
}

void extint_ext_users_init(int rx_chan)
{
    // so that rx_chan_free_count() doesn't count EXT_FLAGS_HEAVY when extension isn't running
    //printf("extint_ext_users_init rx_chan=%d\n", rx_chan);
    //c2s_waterfall_no_sync(rx_chan, false);      // NB: be certain to disable waterfall no_sync mode
    rx_channels[rx_chan].ext = NULL;
    memset(&ext_users[rx_chan], 0, sizeof(ext_users_t));
}

void extint_setup_c2s(void *param)
{
	conn_t *conn_ext = (conn_t *) param;

	// initialize extension for this connection
	// NB: has to be a 'MSG' and not an 'EXT' due to sequencing of recv_cb setup
    rcprintf(conn_ext->rx_channel, "EXT extint_setup_c2s SET: rx%d ext_client_init(is_locked)=%d\n", conn_ext->rx_channel, is_locked);
    send_msg(conn_ext, false, "MSG version_maj=%d version_min=%d debian_ver=%d", version_maj, version_min, debian_ver);
	send_msg(conn_ext, false, "MSG ext_client_init=%d", is_locked);
}

void extint_c2s(void *param)
{
	int n, i, j;
	conn_t *conn_ext = (conn_t *) param;    // STREAM_EXT conn_t
	rx_common_init(conn_ext);
	
	nbuf_t *nb = NULL;
	while (TRUE) {
		int rx_channel, ignored_rx_chan;
		ext_t *ext = NULL;
	
        if (nb) web_to_app_done(conn_ext, nb);
        n = web_to_app(conn_ext, &nb);
            
        if (n) {
            char *cmd = nb->buf;
            cmd[n] = 0;		// okay to do this -- see nbuf.c:nbuf_allocq()

            // SECURITY: this must be first for auth check
            if (rx_common_cmd(STREAM_EXT, conn_ext, cmd))
                continue;
        
            //printf("extint_c2s: %s CONN%d(%p) RX=%d(%p) %d <%s>\n", conn_ext->ext? conn_ext->ext->name:"?", conn_ext->self_idx,
            //    conn_ext, conn_ext->rx_channel, (conn_ext->rx_channel == -1)? 0:ext_users[conn_ext->rx_channel].conn_ext, strlen(cmd), cmd);

            // answer from client ext about who they are
            // match against list of known extensions and register msg handler
            char *client_m = NULL;
            int first_time;

            i = sscanf(cmd, "SET ext_switch_to_client=%32ms first_time=%d rx_chan=%d", &client_m, &first_time, &ignored_rx_chan);
            if (i == 3) {
                for (i=0; i < n_exts; i++) {
                    ext = ext_list[i];
                    if (strcmp(client_m, ext->name) == 0) {
                        //printf("ext_switch_to_client: found func %p CONN-%02d(%p) for ext %s RX%d\n",
                        //    ext->receive_msgs, conn_ext->self_idx, conn_ext, client_m, rx_channel);
                        rx_channel = conn_ext->rx_channel;
                        if (rx_channel < 0 || rx_channel >= rx_chans) {
                            printf("extint_c2s: FAIL rx_channel=%d rx_chans=%d\n", rx_channel, rx_chans);
                            panic("extint_c2s");
                        } else {
                            ext_users_t *eusr = &ext_users[rx_channel];
                            eusr->valid = TRUE;
                            eusr->ext = ext;
                            eusr->conn_ext = conn_ext;
                            conn_ext->ext = ext;
                            TaskNameS(ext->name);
                            u4_t flags = TaskFlags();
                            TaskSetFlags(flags | CTF_RX_CHANNEL | (conn_ext->rx_channel & CTF_CHANNEL));

                            // point STREAM_SOUND conn at ext_t so it has access to the ext->name after ext conn_t is gone
                            // point rx_channels[].ext at ext_t so it has access to ext->flags for EXT_FLAGS_HEAVY checking
                            conn_t *c_snd = rx_channels[rx_channel].conn;
                            if (c_snd && c_snd->valid && c_snd->type == STREAM_SOUND) {
                                c_snd->ext = ext;
                                rx_channels[rx_channel].ext = ext;
                            }
                        }
                        break;
                    }
                }
                if (i == n_exts) {
                    printf("EXT ext_switch_to_client UNKNOWN EXT: <%s>\n", client_m);
                    //panic("ext_switch_to_client: unknown ext");
                } else {
                    ext_send_msg(conn_ext->rx_channel, false, "MSG EXT-STOP-FLUSH-INPUT");

                    // Automatically let extension server-side know the connection has been established and
                    // our stream thread is running. Only called ONCE per client session.
                    if (first_time) {
                        SAN_NULL_PTR_CK(ext, ext->receive_msgs((char *) "SET ext_server_init", rx_channel));
                    }
                }
            
                kiwi_asfree(client_m);
                continue;
            }
            kiwi_asfree(client_m);
        
            i = sscanf(cmd, "SET ext_blur=%d", &ignored_rx_chan);
            if (i == 1) {
                extint_ext_users_init(conn_ext->rx_channel);
                continue;
            }
        
            i = strcmp(cmd, "SET init");
            if (i == 0) {
                continue;
            }

            i = strcmp(cmd, "SET ext_is_locked_status");
            if (i == 0) {
                printf("EXT ext_is_locked_status SET: rx%d ext_client_init=%d(is_locked)\n", conn_ext->rx_channel, is_locked);
                send_msg(conn_ext, false, "MSG version_maj=%d version_min=%d debian_ver=%d", version_maj, version_min, debian_ver);
                send_msg(conn_ext, false, "MSG ext_client_init=%d", is_locked);
                continue;
            }

            // ext client (e.g. kiwiclient) can't generate async keepalive messages
            // snd/wf will take care of it anyway
            i = strcmp(cmd, "SET ext_no_keepalive");
            if (i == 0) {
                conn_ext->ext_no_keepalive = true;
                continue;
            }

            rx_channel = conn_ext->rx_channel;
            if (rx_channel == -1) {
                printf("### extint_c2s: %s CONN%d(%p) rx_channel == -1?\n", conn_ext->ext? conn_ext->ext->name:"?", conn_ext->self_idx, conn_ext);
                continue;
            }
            ext = ext_users[rx_channel].ext;
            if (ext == NULL) {
                printf("### extint_c2s: %s CONN%d(%p) rx_channel=%d ext NULL?\n", conn_ext->ext? conn_ext->ext->name:"?", conn_ext->self_idx, conn_ext, rx_channel);
                continue;
            }
            if (ext->receive_msgs) {
                //printf("extint_c2s: %s ext->receive_msgs() %p CONN%d(%p) RX%d(%p) %d <%s>\n", conn_ext->ext? conn_ext->ext->name:"?", ext->receive_msgs, conn_ext->self_idx, conn_ext, rx_channel, (rx_channel == -1)? 0:ext_users[rx_channel].conn_ext, strlen(cmd), cmd);
                if (ext->receive_msgs(cmd, rx_channel))
                    continue;
            } else {
                printf("### extint_c2s: %s CONN%d(%p) RX%d(%p) ext->receive_msgs == NULL?\n", conn_ext->ext? conn_ext->ext->name:"?", conn_ext->self_idx, conn_ext, rx_channel, (rx_channel == -1)? 0:ext_users[rx_channel].conn_ext);
                continue;
            }
        
            printf("EXT extint_c2s: %s CONN%d(%p) unknown command: sl=%d %d|%d|%d [%s] ip=%s ==================================\n",
                conn_ext->ext? conn_ext->ext->name:"?", conn_ext->self_idx, conn_ext,
                strlen(cmd), cmd[0], cmd[1], cmd[2], cmd, conn_ext->remote_ip);

            continue;       // keep checking until no cmds in queue
        }
		
        rx_channel = conn_ext->rx_channel;
        ext = (rx_channel == -1)? NULL : ext_users[rx_channel].ext;

		conn_ext->keep_alive = (conn_ext->ext_no_keepalive || conn_ext->internal_connection)? 0 : (timer_sec() - conn_ext->keepalive_time);
		bool keepalive_expired = (conn_ext->keep_alive > KEEPALIVE_SEC);
		if (keepalive_expired || conn_ext->kick) {
			//printf("EXT %s RX%d %s\n", conn_ext->kick? "KICKED" : "KEEP-ALIVE EXPIRED", rx_channel, ext? ext->name : "(no ext)");
			if (ext != NULL && ext->close_conn != NULL) {
			    //printf("EXT CLOSE_CONN\n");
				ext->close_conn(rx_channel);
                //c2s_waterfall_no_sync(rx_channel, false);      // NB: be certain to disable waterfall no_sync mode
			}
			if (rx_channel != -1) {
				extint_ext_users_init(rx_channel);
			}
			rx_server_remove(conn_ext);
			panic("shouldn't return");
		}
		
		// call periodic callback if requested
        if (ext != NULL && ext->version == EXT_NEW_VERSION && ext->poll_cb != NULL)
            ext->poll_cb(rx_channel);

		TaskSleepReasonMsec("ext-cmd", 200);
	}
}

void extint_shutdown_c2s(void *param)
{
    conn_t *conn_ext = (conn_t*) param;
    //printf("EXT rx%d extint_shutdown_c2s mc=0x%x\n", conn_ext->rx_channel, conn_ext->mc);
    if (conn_ext && conn_ext->mc) {
        rx_server_websocket(WS_MODE_CLOSE, conn_ext->mc);
    }
}
