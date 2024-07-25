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


// mongoose API
const char * const mg_ev_names[] = {
    "EV_ERROR",     "EV_OPEN",          "EV_POLL",          "EV_RESOLVE",
    "EV_CONNECT",   "EV_ACCEPT",        "EV_TLS_HS",        "EV_READ",
    "EV_WRITE",     "EV_CLOSE",         "EV_HTTP_HDRS",     "EV_HTTP_MSG",
    "EV_WS_OPEN",   "EV_WS_MSG",        "EV_WS_CTL",        "EV_MQTT_CMD",
    "EV_MQTT_MSG",  "EV_MQTT_OPEN",     "EV_SNTP_TIME",     "EV_WAKEUP",
    "EV_USER",      "MG_EV_CACHE_INFO", "MG_EV_CACHE_DONE"
};

#define MG_API_NEW

typedef struct {        // cache info for in-memory stored data
    size_t size;
    time_t mtime;
    int cached;
    bool if_none_match;
        bool etag_match;
        #define N_ETAG 64
        char etag_server[N_ETAG], etag_client[N_ETAG];
    bool if_mod_since;
        bool not_mod_since;
        time_t server_mtime, client_mtime;
} cache_info_t;

int web_ev_request(struct mg_connection *mc, int ev, void *ev_data);
void mg_ev_setup_api_compat(struct mg_connection *mc);
char *mg_str_to_cstr(mg_str *mgs);
const char *mg_get_mime_type(const char *path, const char *default_mime_type);
const char *mg_get_header(struct mg_connection *mc, const char *name);
void mg_free_header(const char *header);
void mg_http_send_standard_headers(struct mg_connection *mc, const char *path, cache_info_t *cache, const char *msg);
void mg_response_complete(struct mg_connection *mc);
void mg_connection_close(struct mg_connection *mc);
void mg_send_flush();
void mg_remove_double_dots_and_double_slashes(char *s);
void mg_bin2str(char *to, const unsigned char *p, size_t len);

enum { MG_FIRST_HEADER, MG_MIDDLE_HEADER, MG_LAST_HEADER };
void mg_http_send_header(struct mg_connection *mc, const char *name, const char *v, int which = MG_MIDDLE_HEADER, size_t len = 0);


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
char *rx_server_ajax(struct mg_connection *mc, char *ip_forwarded, void *ev_data);
int websocket_request(struct mg_connection *mc, int ev, void *ev_data);
int web_request(struct mg_connection *mc, int ev, void *ev_data);

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

