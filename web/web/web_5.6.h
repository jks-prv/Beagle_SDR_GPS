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

#pragma once

#include "embed.h"
#include "mongoose.h"

#ifdef USE_SDR
 #define N_EXT 32
#else
 #define N_EXT 0
#endif

#define WEB_PRINTF
#ifdef WEB_PRINTF

    // -ctrace 0xhh
    
	#define WEB_CACHING_DEBUG_SENT 0x01
	#define web_printf_sent(fmt, ...) \
		if (web_caching_debug & WEB_CACHING_DEBUG_SENT) lprintf(fmt, ## __VA_ARGS__)

	#define WEB_CACHING_DEBUG_CACHED 0x02
	#define web_printf_cached(fmt, ...) \
		if (web_caching_debug & WEB_CACHING_DEBUG_CACHED) lprintf(fmt, ## __VA_ARGS__)

	#define WEB_CACHING_DEBUG_ALL 0x04
	#define web_printf_all(fmt, ...) \
		if (web_caching_debug & WEB_CACHING_DEBUG_ALL) lprintf(fmt, ## __VA_ARGS__)

	#define WEB_CACHING_DEBUG_SERVED 0x08
	#define web_printf_served(fmt, ...) \
		if (web_caching_debug & WEB_CACHING_DEBUG_SERVED) lprintf(fmt, ## __VA_ARGS__)
#else
	#define web_printf_sent(fmt, ...)
	#define web_printf_cached(fmt, ...)
	#define web_printf_all(fmt, ...)
	#define web_printf_served(fmt, ...)
#endif

#define NREQ_BUF (16*1024)		// the dx list can easily get longer than 1K

#define N_CONN_SND_WF_EXT   (MAX_RX_CHANS * (3 + N_CAMP))
#define	N_CONN_EXTRA        8

#define	N_CONNS	(N_CONN_SND_WF_EXT + N_QUEUERS + N_CONN_ADMIN + N_CONN_EXTRA)


// Mongoose API
typedef struct stat file_stat_t;

typedef struct {        // cache info for in-memory stored data
    struct stat st;
    int cached;
    bool if_none_match;
        bool etag_match;
        #define N_ETAG 64
        char etag_server[N_ETAG], etag_client[N_ETAG];
    bool if_mod_since;
        bool not_mod_since;
        time_t server_mtime, client_mtime;
} cache_info_t;

#define mg_ws_send(mc, s, slen, op)         mg_websocket_write(mc, op, s, slen)
#define mg_http_write_chunk(mc, buf, len)   mg_send_data(mc, buf, len)
#define mg_response_complete(mc)
#define mg_free_header(header)
#define mg_http_send_header(mc, name, v)    mg_send_header(mc, name, v)
#define mg_http_send_standard_headers(mc, path, stat, msg) \
                                            mg_send_standard_headers(mc, path, stat, msg, NULL, NULL)

int web_ev_request(struct mg_connection *mc, int ev);
void mg_connection_close(struct mg_connection *mc);

// CAUTION: different from Mongoose 7 mg_http_reply() where "hdr_name, hdr_val" is really "*headers, content..."
#define mg_http_reply(mc, status, hdr_name, hdr_val) \
    mg_send_status(mc, status); \
    if (hdr_name != NULL) { \
        mg_send_header(mc, hdr_name, hdr_val); \
    }


extern embedded_files_t edata_embed[];
extern embedded_files_t edata_always[];
extern embedded_files_t edata_always2[];

extern char *web_server_hdr;
extern time_t mtime_obj_keep_edata_always_o;
extern time_t mtime_obj_keep_edata_always2_o;

void webserver_connection_cleanup(conn_t *c);

// client to server
int web_to_app(conn_t *c, nbuf_t **nbp, bool internal_connection = false);
void web_to_app_done(conn_t *c, nbuf_t *nb);

// server to client
void app_to_web(conn_t *c, char *s, int sl);
char *rx_server_ajax(struct mg_connection *mc, char *ip_forwarded, void *ev_data = NULL);
int websocket_request(struct mg_connection *mc, int ev);
int web_request(struct mg_connection *mc, int ev);

// UDP
void *udp_connect(char *url);
void udp_send(void *udp, const void *buf, size_t len);

typedef enum {WS_INIT_CREATE, WS_INIT_START} ws_init_t;
void web_server_init(ws_init_t type);
void services_start();
int web_served(conn_t *c);
void web_served_clear_cache(conn_t *c);
struct mg_connection *web_connect(const char *url);
void reload_index_params();
void iparams_add(const char *id, char *val);

