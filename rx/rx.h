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

// Copyright (c) 2015 John Seamons, ZL4VO/KF6VO

#pragma once

#include "types.h"
#include "kiwi.h"
#include "conn.h"
#include "web.h"

typedef struct {
	bool chan_enabled;
	bool data_enabled;
	bool busy;
	conn_t *conn;       // the STREAM_SOUND conn or STREAM_WATERFALL for WF-only connections
	ext_t *ext;
	int cfg_update_seq;
	u4_t wr, rd;
    tid_t wb_task;

	int n_camp;
	conn_t *camp_conn[N_CAMP];
	u4_t camp_id[N_CAMP];
} rx_chan_t;

extern rx_chan_t rx_channels[];

extern volatile float audio_kbps[], waterfall_kbps[], waterfall_fps[], http_kbps;
extern volatile u4_t audio_bytes[], waterfall_bytes[], waterfall_frames[], http_bytes;

void rx_server_init();
void rx_server_remove(conn_t *c);
void rx_common_init(conn_t *conn);
bool rx_common_cmd(int stream_type, conn_t *conn, char *cmd, bool *keep_alive = NULL);
const char *rx_conn_type(conn_t *c);

typedef enum { WS_MODE_ALLOC, WS_MODE_LOOKUP, WS_MODE_CLOSE, WS_INTERNAL_CONN } websocket_mode_e;
const char * const ws_mode_s[] = { "alloc", "lookup", "close", "internal" };
#define WS_FL_NONE              0x00
#define WS_FL_PREEMPT_AUTORUN   0x01    // should preempt an autorun
#define WS_FL_IS_AUTORUN        0x02    // is an autorun
#define WS_FL_INITIAL           0x04    // initial start (not victim restart)
#define WS_FL_NO_LOG            0x08    // don't log arrived/leaving msgs
conn_t *rx_server_websocket(websocket_mode_e mode, struct mg_connection *mc, u4_t ws_flags = 0);

typedef enum { RX_CHAN_ENABLE, RX_CHAN_DISABLE, RX_DATA_ENABLE, RX_CHAN_FREE } rx_chan_action_e;
void rx_enable(int chan, rx_chan_action_e action);
