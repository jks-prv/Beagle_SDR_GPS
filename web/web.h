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

#pragma once

#include "config.h"
#include "nbuf.h"
#include "mongoose.h"
#include "non_block.h"
#include "update.h"
#include "conn.h"

#ifdef CFG_GPS_ONLY
 #define N_EXT 0
#endif

#define WEB_PRINTF
#ifdef WEB_PRINTF
	#define web_printf(fmt, ...) \
		if (web_caching_debug) printf(fmt, ## __VA_ARGS__)
#else
	#define web_printf(fmt, ...)
#endif

#define	WS_OPCODE_TEXT		1
#define	WS_OPCODE_BINARY	2
#define	WS_OPCODE_CLOSE     8

#define NREQ_BUF (16*1024)		// the dx list can easily get longer than 1K

user_iface_t *find_ui(int port);

typedef struct {
	int type;
	const char *uri;
	funcP_t f;
	funcP_t setup;
	funcP_t shutdown;
	u4_t priority;
} rx_stream_t;

extern rx_stream_t streams[];

#define	N_CONN_ADMIN        8
#define N_CONN_SND_WF_EXT   3
#define	N_CONN_EXTRA        16

#define	N_CONNS	(MAX_RX_CHANS * (N_CONN_SND_WF_EXT) + N_CONN_ADMIN + N_CONN_EXTRA)

extern char *web_server_hdr;
extern u4_t mtime_obj_keep_edata_always_o;

void webserver_connection_cleanup(conn_t *c);

// client to server
int web_to_app(conn_t *c, nbuf_t **nbp);
void web_to_app_done(conn_t *c, nbuf_t *nb);

// server to client
void app_to_web(conn_t *c, char *s, int sl);
char *rx_server_ajax(struct mg_connection *mc);
int web_request(struct mg_connection *mc, enum mg_event ev);
void reload_index_params();
void iparams_add(const char *id, char *val);

typedef enum {WS_INIT_CREATE, WS_INIT_START} ws_init_t;
void web_server_init(ws_init_t type);

#define SVCS_RESTART_TRUE	true
#define SVCS_RESTART_FALSE	false
void services_start(bool restart);
