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

// Copyright (c) 2014-2024 John Seamons, ZL4VO/KF6VO

#include "kiwi.h"
#include "types.h"
#include "config.h"
#include "mem.h"
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
#include "ext_int.h"
#include "printf.h"
#include "debug.h"
#include "rx_util.h"

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
			mg_ws_send()
	
	Called by (or on behalf of) mongoose web server:
		// event requests _from_ web server:
		// (prompted by data coming into web server)
		mg_create_server()
			ev_handler()
				MG_EV_HTTP_MSG:
					web_ev_request()
						is_websocket:
							copy mc data to nbufs:
								mc data => nbuf_allocq(c2s) [=> web_to_app()]
						file:
							return file/AJAX data to server:
								mg_send_header
								(file data, AJAX) => mg_http_write_chunk()
				MG_EV_CLOSE:
					rx_server_websocket(WS_MODE_CLOSE)
				MG_EV_AUTH:
					MG_TRUE
		
		// polled push of data _to_ web server
		TASK:
		web_server()
			mg_poll_server()	// also forces mongoose internal buffering to write to sockets
			mg_iterate_over_connections()
				iterate_callback()
					is_websocket:
						[app_to_web() =>] nbuf_dequeue(s2c) => mg_ws_send()
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

int web_to_app(conn_t *c, nbuf_t **nbp, bool internal_connection)
{
	nbuf_t *nb;
	
    *nbp = NULL;
	if (c->stop_data) return 0;
    ndesc_t *ndesc = internal_connection? &c->s2c : &c->c2s;
    if (ndesc->mc == NULL) return -1;
	nb = nbuf_dequeue(ndesc);
	if (!nb) return 0;
	assert(!nb->done && !nb->expecting_done && nb->buf && nb->len);
	nb->expecting_done = TRUE;
	*nbp = nb;

    if (cmd_debug) {
        char *cmd = nb->buf;
        cmd[nb->len] = '\0';    // okay to do this -- see nbuf.c:nbuf_allocq()
        cmd_debug_print(c, cmd, nb->len, false);
    }
	
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
// 1) websocket: {SND, W/F} data streams, sent by app_to_web(), received by .js via (ws).onmessage()
// 2) websocket: {MSG, ADM, MFG, EXT, DAT} messages sent by send_msg*()/mg_ws_send(), received via open_websocket() msg_cb/recv_cb routines
// 3) 

void app_to_web(conn_t *c, char *s, int sl)
{
	if (c->stop_data) return;
	if (c->internal_connection) {
	    if (!((c->type == STREAM_SOUND && c->internal_want_snd) || (c->type == STREAM_WATERFALL && c->internal_want_wf))) {
	        //printf("SKIP app_to_webinternal_connection %s sl=%d\n", rx_conn_type(c), sl);
	        return;
	    }
	}
    //printf("app_to_webinternal_connection %s sl=%d\n", rx_conn_type(c), sl);
	nbuf_allocq(&c->s2c, s, sl);
	//NextTask("s2c");
}


// polled send of data _to_ web server
static int iterate_callback(struct mg_connection *mc, int ev)
{
	int ret;
	nbuf_t *nb;
	
    if (ev == MG_EV_POLL && mc->is_websocket) {
        conn_t *c = rx_server_websocket(WS_MODE_LOOKUP, mc);
        if (c == NULL) return MG_FALSE;

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
							c->rx_channel, c->audio_sequence, out->h.seq, c->ident_user);
						c->audio_sequence = out->h.seq;
					}
					c->audio_sequence++;
					c->audio_pkts_sent++;
				}
				#endif

				//printf("s2c %d WEBSOCKET: %d %p\n", mc->remote_port, nb->len, nb->buf);
				ret = mg_ws_send(mc, nb->buf, nb->len, WEBSOCKET_OP_BINARY);
				if (ret <= 0) printf("$$$$$$$$ socket write ret %d\n", ret);
				nb->done = TRUE;
			} else {
				break;
			}
		}
        evWS(EC_EVENT, EV_WS, 0, "WEB_SERVER", "..iterate_callback");
	} else {
		if (ev != MG_EV_POLL)
		    printf("$$$$$$$$ s2c %d OTHER: %d len %d\n", mc->remote_port, (int) ev, (int) mc->content_len);
	}
	
	//NextTask("web callback");
    return MG_TRUE;
}

// event requests _from_ web server:
// (prompted by data coming into web server)
static int ev_handler(struct mg_connection *mc, int ev)
{
    const char *names[] = {
        "POLL",        "CONNECT",      "AUTH",         "REQUEST",
        "REPLY",       "RECV",         "CLOSE",        "WS_HANDSHAKE",
        "WS_CONNECT",  "CACHE_INFO",   "CACHE_RESULT", "HTTP_ERROR"
    };
    //printf("ev_handler ev%d(%s) %s:%d => %d\n", ev, names[ev], mc->remote_ip, mc->remote_port, mc->loc.port);

    switch (ev) {
        case MG_EV_HTTP_MSG:
                if (mc->is_websocket) {
                    //printf("ev_handler ev%d(%s) WEBSOCKET len=%d\n", ev, names[ev], mc->content_len);
                    return websocket_request(mc, ev);
                } else {
                    //printf("ev_handler ev%d(%s) %s %s\n", ev, names[ev], mc->uri, mc->query);
                    if (mc->cache_info == NULL) {
                        mc->cache_info = malloc(sizeof(cache_info_t));
                        memset(mc->cache_info, 0, sizeof(cache_info_t));
                    }
                    return web_ev_request(mc, ev);
                }
            
        case MG_EV_CLOSE:
            rx_server_websocket(WS_MODE_CLOSE, mc);
            kiwi_asfree_set_null(mc->cache_info);
            mc->connection_param = NULL;
            return MG_TRUE;
            
        case MG_EV_AUTH:
            return MG_TRUE;
            
        default:
            return MG_FALSE;
    }

    return MG_FALSE;
}

void web_server(void *param)
{
	struct mg_server *server = (struct mg_server *) param;
	const char *err;
	
	while (1) {
        mg_poll_server(server, 0);		// passing 0 effects a poll
        struct mg_connection *mc;
        for (mc = mg_next(server, NULL); mc != NULL; mc = mg_next(server, mc)) {
            iterate_callback(mc, MG_EV_POLL);
        }
		
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

static struct mg_server *server;

// UDP
void *udp_connect(char *url)
{
    struct mg_connection *mg = mg_connect(server, url);
    return mg;
}

void udp_send(void *udp, const void *buf, size_t len)
{
    mg_write((struct mg_connection *) udp, buf, len);
}

char *web_server_hdr;

void web_server_init(ws_init_t type)
{
	int i;
	static bool init;
	
	if (!init) {
		nbuf_init();

		// add the new "port_ext" config param if needed
		// done here because web_server_init(WS_INIT_CREATE) called earlier than rx_server_init() in main.c
        bool update_admcfg = false;
		net.port = admcfg_int("port", NULL, CFG_REQUIRED);
        net.port_ext = admcfg_default_int("port_ext", net.port, &update_admcfg);

        #ifdef USE_SSL
            net.use_ssl = admcfg_default_bool("use_ssl", false, &update_admcfg);
            net.port_http_local = admcfg_default_int("port_http_local", net.port + 100, &update_admcfg);
        #endif

        if (update_admcfg) admcfg_save_json(cfg_adm.json);      // during init doesn't conflict with admin cfg

        if (!background_mode) {
            struct stat st;
            scall("stat edata_always", stat(BUILD_DIR "/obj_keep/edata_always.o", &st));
            mtime_obj_keep_edata_always_o = st.st_mtime;
            scall("stat edata_always2", stat(BUILD_DIR "/obj_keep/edata_always2.o", &st));
            mtime_obj_keep_edata_always2_o = st.st_mtime;
        }

        asprintf(&web_server_hdr, "KiwiSDR_%d.%d/Mongoose_%s", version_maj, version_min, MG_VERSION);
		init = TRUE;
	}
	
	if (type == WS_INIT_CREATE) {
		lprintf("webserver: listening on port %d/%d for HTTP%s connections\n",
		    net.port, net.port_ext, net.use_ssl? "S (SSL)" : "");

        #ifdef USE_SSL
            if (net.use_ssl) {
                lprintf("webserver: listening on port %d for local HTTP connections\n", net.port_http_local);
                lprintf("webserver: listening on port 80 for ACME challenge requests\n");
            }
        #endif
	} else

	if (type == WS_INIT_START) {
		reload_index_params();
		services_start();
	}

	// create webserver port(s)
    if (type == WS_INIT_CREATE) {
        server = mg_create_server(NULL, ev_handler);
        //mg_set_option(server, "document_root", "./");		// if serving from file system

        #define SSL_CERT DIR_CFG "/cert.pem"

        char *s_port;
        if (net.use_ssl)
            asprintf(&s_port, "ssl://[::]:%d:%s,[::]:%d,[::]:80", net.port, SSL_CERT, net.port_http_local);
        else
            asprintf(&s_port, "[::]:%d", net.port);

        if (mg_set_option(server, "listening_port", s_port) != NULL) {
            lprintf("network port(s) %s in use\n", s_port);
            lprintf("app already running in background?\ntry \"make stop\" (or \"m stop\") first\n");
            kiwi_exit(-1);
        }

        kiwi_asfree(s_port);
        
    } else {	// WS_INIT_START
        bool err;
        bool test_webserver_prio = cfg_bool("test_webserver_prio", &err, CFG_OPTIONAL);
        printf("test_webserver_prio=%d err=%d\n", test_webserver_prio, err);
        if (err) test_webserver_prio = false;
        CreateTask(web_server, server, test_webserver_prio? SND_PRIORITY : WEBSERVER_PRIORITY);
    }
}
