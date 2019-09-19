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

// Copyright (c) 2014-2016 John Seamons, ZL/KF6VO

#include "kiwi.h"
#include "types.h"
#include "config.h"
#include "misc.h"
#include "timer.h"
#include "web.h"
#include "net.h"
#include "coroutines.h"
#include "mongoose.h"
#include "nbuf.h"
#include "cfg.h"
#include "str.h"
#include "rx.h"
#include "clk.h"
#include "spi.h"
#include "ext_int.h"
#include "debug.h"

#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>

void webserver_connection_cleanup(conn_t *c)
{
	nbuf_cleanup(&c->c2s);
	nbuf_cleanup(&c->s2c);
}


/*
	Architecture of web server:
		c2s = client-to-server
		s2c = server-to-client
	
	NB: The only "push" s2c data is server websocket output (stream data and messages).
	Other s2c data are responses to c2s requests.
	
	Called by Kiwi server code:
		// server polled check of websocket SET messages
		web_to_app()
			return nbuf_dequeue(c2s nbuf)
		
		// server demand push of websocket stream data
		app_to_web(buf)
			buf => nbuf_allocq(s2c)

		// server demand push of websocket message data (no need to use nbufs)
		send_msg*()
			mg_websocket_write()
	
	Called by (or on behalf of) mongoose web server:
		// event requests _from_ web server:
		// (prompted by data coming into web server)
		mg_create_server()
			ev_handler()
				MG_REQUEST:
					web_request()
						is_websocket:
							copy mc data to nbufs:
								mc data => nbuf_allocq(c2s) [=> web_to_app()]
						file:
							return file/AJAX data to server:
								mg_send_header
								(file data, AJAX) => mg_send_data()
				MG_CLOSE:
					rx_server_websocket(WS_MODE_CLOSE)
				MG_AUTH:
					MG_TRUE
		
		// polled push of data _to_ web server
		TASK:
		web_server()
			mg_poll_server()	// also forces mongoose internal buffering to write to sockets
			mg_iterate_over_connections()
				iterate_callback()
					is_websocket:
						[app_to_web() =>] nbuf_dequeue(s2c) => mg_websocket_write()
					other:
						ERROR
			LOOP
					
*/


// c2s
// client to server
// 1) websocket: {SET, SND, W/F, EXT, ADM, MFG} messages sent from .js via {msg,snd,wf,ext}_send(), received via websocket connection threads
//		no response returned (unless s2c send_msg*() done)
// 2) HTTP GET: normal browser file downloads, response returned (e.g. .html, .css, .js, images files)
// 3) HTTP GET: AJAX requests, response returned (e.g. "GET /status")
//		eliminating most of these in favor of websocket messages so connection auth can be performed
// 4) HTTP PUT: e.g. kiwi_ajax_send() upload photo file, response returned

int web_to_app(conn_t *c, nbuf_t **nbp)
{
	nbuf_t *nb;
	
    *nbp = NULL;
	if (c->stop_data) return 0;
	nb = nbuf_dequeue(&c->c2s);
	if (!nb) return 0;
	assert(!nb->done && !nb->expecting_done && nb->buf && nb->len);
	nb->expecting_done = TRUE;
	*nbp = nb;
	
	return nb->len;
}

void web_to_app_done(conn_t *c, nbuf_t *nb)
{
	assert(nb->expecting_done && !nb->done);
	nb->expecting_done = FALSE;
	nb->done = TRUE;
}


// s2c
// server to client
// 1) websocket: {SND, W/F} data streams received by .js via (ws).onmessage()
// 2) websocket: {MSG, ADM, MFG, EXT, DAT} messages sent by send_msg*(), received via open_websocket() msg_cb/recv_cb routines
// 3) 

void app_to_web(conn_t *c, char *s, int sl)
{
	if (c->stop_data) return;
	if (c->internal_connection) {
	    //printf("app_to_webinternal_connection sl=%d\n", sl);
	    return;
	}
	nbuf_allocq(&c->s2c, s, sl);
	//NextTask("s2c");
}


// event requests _from_ web server:
// (prompted by data coming into web server)
static int ev_handler(struct mg_connection *mc, enum mg_event evt) {
  
	//printf("ev_handler %d:%d len %d\n", mc->local_port, mc->remote_port, (int) mc->content_len);
	//printf("MG_REQUEST: ip:%s URI:%s query:%s\n", mc->remote_ip, mc->uri, mc->query_string);
	
    switch (evt) {
        case MG_REQUEST:
        case MG_CACHE_INFO:
        case MG_CACHE_RESULT:
            return web_request(mc, evt);
        case MG_CLOSE:
            //printf("MG_CLOSE\n");
            rx_server_websocket(WS_MODE_CLOSE, mc);
            mc->connection_param = NULL;
            return MG_TRUE;
        case MG_AUTH:
            //printf("MG_AUTH\n");
            return MG_TRUE;
        default:
            //printf("MG_OTHER evt=%d\n", evt);
            return MG_FALSE;
    }
}

// polled send of data _to_ web server
static int iterate_callback(struct mg_connection *mc, enum mg_event evt)
{
	int ret;
	nbuf_t *nb;
	
	if (evt == MG_POLL && mc->is_websocket) {
		conn_t *c = rx_server_websocket(WS_MODE_LOOKUP, mc);
		if (c == NULL)  return MG_FALSE;

        evWS(EC_EVENT, EV_WS, 0, "WEB_SERVER", "iterate_callback..");
		while (TRUE) {
			if (c->stop_data) break;
			nb = nbuf_dequeue(&c->s2c);
			//printf("s2c CHK port %d nb %p\n", mc->remote_port, nb);
			
			if (nb) {
				assert(!nb->done && nb->buf && nb->len);

				#ifdef SND_TIMING_CK
				// check timing of audio output (assumes non-IQ mode always selected)
				snd_pkt_real_t *out = (snd_pkt_real_t *) nb->buf;
				if (c->type == STREAM_SOUND && strncmp(out->h.id, "SND", 3) == 0) {
					u4_t now = timer_ms();
					if (!c->audio_check) {
						c->audio_epoch = now;
						c->audio_sequence = c->audio_pkts_sent = c->audio_last_time = c->sum2 = 0;
						c->audio_check = true;
					}
					double audio_rate = ext_update_get_sample_rateHz(c->rx_channel);
					u4_t expected1 = c->audio_epoch + (u4_t)((1.0/audio_rate * (512*4) * c->audio_pkts_sent)*1000.0);
					s4_t diff1 = (s4_t)(now - expected1);
					u4_t expected2 = (u4_t)((1.0/audio_rate * (512*4))*1000.0);
					s4_t diff2 = c->audio_last_time? (s4_t)((now - c->audio_last_time) - expected2) : 0;
					c->audio_last_time = now;
					#define DIFF1 30
					#define DIFF2 1
					if (diff1 < -DIFF1 || diff1 > DIFF1 || diff2 < -DIFF2 || diff2 > DIFF2) {
						printf("SND%d %4d Q%d d1=%6.3f d2=%6.3f/%6.3f %.6f %f\n",
							c->rx_channel, c->audio_sequence, nbuf_queued(&c->s2c)+1,
							(float)diff1/1e4, (float)diff2/1e4, (float)c->sum2/1e4,
							adc_clock_system()/1e6, audio_rate);
					}
					c->sum2 += diff2;
					if (out->h.seq != c->audio_sequence) {
						printf("SND%d SEQ expecting %d got %d, %s -------------------------\n",
							c->rx_channel, c->audio_sequence, out->h.seq, c->user);
						c->audio_sequence = out->h.seq;
					}
					c->audio_sequence++;
					c->audio_pkts_sent++;
				}
				#endif

				//printf("s2c %d WEBSOCKET: %d %p\n", mc->remote_port, nb->len, nb->buf);
				ret = mg_websocket_write(mc, WS_OPCODE_BINARY, nb->buf, nb->len);
				if (ret<=0) printf("$$$$$$$$ socket write ret %d\n", ret);
				nb->done = TRUE;
			} else {
				break;
			}
		}
        evWS(EC_EVENT, EV_WS, 0, "WEB_SERVER", "..iterate_callback");
	} else {
		if (evt != MG_POLL) printf("$$$$$$$$ s2c %d OTHER: %d len %d\n", mc->remote_port, (int) evt, (int) mc->content_len);
	}
	
	//NextTask("web callback");
	
	return MG_TRUE;
}

void web_server(void *param)
{
	user_iface_t *ui = (user_iface_t *) param;
	struct mg_server *server = ui->server;
	const char *err;
	
	while (1) {
		mg_poll_server(server, 0);		// passing 0 effects a poll
		mg_iterate_over_connections(server, iterate_callback);
		
		//#define MEAS_WEB_SERVER
		#ifdef MEAS_WEB_SERVER
		    u4_t quanta = FROM_VOID_PARAM(TaskSleepUsec(WEB_SERVER_POLL_US));
            static u4_t last, cps, max_quanta, sum_quanta;
            u4_t now = timer_sec();
            if (last != now) {
                for (; last < now; last++) {
                    if (last < (now-1))
                        real_printf("w- ");
                    else
                        real_printf("w%d|%d/%d ", cps, sum_quanta/(cps? cps:1), max_quanta);
                    fflush(stdout);
                }
                max_quanta = sum_quanta = 0;
                cps = 0;
            } else {
                if (quanta > max_quanta) max_quanta = quanta;
                sum_quanta += quanta;
                cps++;
            }
        #else
            TaskSleepUsec(WEB_SERVER_POLL_US);
        #endif
		
		//#define CHECK_ECPU_STACK
		#ifdef CHECK_ECPU_STACK
            static int every_1sec;
            every_1sec += WEB_SERVER_POLL_US;
            if (every_1sec >= 1000000) {
                static SPI_MISO sprp;
                spi_get_noduplex(CmdGetSPRP, &sprp, 4);
                printf("e_cpu: SP=%04x RP=%04x\n", sprp.word[0], sprp.word[1]);
                every_1sec = 0;
            }
		#endif
	}
}

char *web_server_hdr;

void web_server_init(ws_init_t type)
{
	int i;
	user_iface_t *ui = user_iface;
	static bool init;
	
	if (!init) {
		nbuf_init();

		// add the new "port_ext" config param if needed
		// done here because web_server_init(WS_INIT_CREATE) called earlier than rx_server_init() in main.c
		int port = admcfg_int("port", NULL, CFG_REQUIRED);
		bool error;
		admcfg_int("port_ext", &error, CFG_OPTIONAL);
		if (error) {
			admcfg_set_int("port_ext", port);
			admcfg_save_json(cfg_adm.json);
		}
		
        if (!background_mode) {
            struct stat st;
            scall("stat edata_always", stat(BUILD_DIR "/obj_keep/edata_always.o", &st));
            mtime_obj_keep_edata_always_o = st.st_mtime;
            scall("stat edata_always2", stat(BUILD_DIR "/obj_keep/edata_always2.o", &st));
            mtime_obj_keep_edata_always2_o = st.st_mtime;
        }

        asprintf(&web_server_hdr, "KiwiSDR_Mongoose/%d.%d", version_maj, version_min);
		init = TRUE;
	}
	
	if (type == WS_INIT_CREATE) {
		// if specified, override the default port number
		if (alt_port) {
			ddns.port = ddns.port_ext = alt_port;
		} else {
			ddns.port = admcfg_int("port", NULL, CFG_REQUIRED);
			ddns.port_ext = admcfg_int("port_ext", NULL, CFG_REQUIRED);
		}
		lprintf("listening on %s port %d/%d for \"%s\"\n", alt_port? "alt":"default",
			ddns.port, ddns.port_ext, ui->name);
		ui->port = ddns.port;
		ui->port_ext = ddns.port_ext;
	} else

	if (type == WS_INIT_START) {
		reload_index_params();
		services_start(SVCS_RESTART_FALSE);
	}

	// create webserver port(s)
	for (i = 0; ui->port; i++) {
	
		if (type == WS_INIT_CREATE) {
			// FIXME: stopgap until admin page supports config of multiple UIs
			if (i != 0) {
				ui->port = ddns.port + i;
				ui->port_ext = ddns.port_ext + i;
			}
			
			ui->server = mg_create_server(NULL, ev_handler);
			//mg_set_option(ui->server, "document_root", "./");		// if serving from file system
			char *s_port;
			asprintf(&s_port, "[::]:%d", ui->port);
			if (mg_set_option(ui->server, "listening_port", s_port) != NULL) {
				lprintf("network port %s for \"%s\" in use\n", s_port, ui->name);
				lprintf("app already running in background?\ntry \"make stop\" (or \"m stop\") first\n");
				xit(-1);
			}
			lprintf("webserver for \"%s\" on port %s\n", ui->name, mg_get_option(ui->server, "listening_port"));
			free(s_port);
		} else {	// WS_INIT_START
            bool err;
            bool test_webserver_prio = cfg_bool("test_webserver_prio", &err, CFG_OPTIONAL);
            printf("test_webserver_prio=%d err=%d\n", test_webserver_prio, err);
            if (err) test_webserver_prio = false;
			CreateTask(web_server, ui, test_webserver_prio? SND_PRIORITY : WEBSERVER_PRIORITY);
		}
		
		ui++;
		if (down) break;
	}
}
