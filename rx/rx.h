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

// Copyright (c) 2015 John Seamons, ZL/KF6VO

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

	int n_camp;
	conn_t *camp_conn[N_CAMP];
	u4_t camp_id[N_CAMP];
} rx_chan_t;

extern rx_chan_t rx_channels[];

extern volatile float audio_kbps[], waterfall_kbps[], waterfall_fps[], http_kbps;
extern volatile u4_t audio_bytes[], waterfall_bytes[], waterfall_frames[], http_bytes;

void rx_server_init();
void rx_server_remove(conn_t *c);
void rx_server_user_kick(int chan);
void rx_server_send_config(conn_t *conn);
void rx_common_init(conn_t *conn);
bool rx_common_cmd(const char *stream_name, conn_t *conn, char *cmd);
char *rx_users(bool include_ip);
void show_conn(const char *prefix, conn_t *cd);

#define SNR_MEAS_INT_HOURS  6
#define SNR_MEAS_PER_DAY    (24 / SNR_MEAS_INT_HOURS)

#define SNR_MEAS_ALL    0
#define SNR_MEAS_HF     1
#define SNR_MEAS_NDATA  2

typedef struct {
    int f_lo, f_hi, min, max, pct_50, pct_95, snr;
} SNR_data_t;

typedef struct {
	bool valid;
    char tstamp[CTIME_R_NL + 1 + SPACE_FOR_NULL];
    bool is_local_time;
    SNR_data_t data[SNR_MEAS_NDATA];
} SNR_meas_t;

extern SNR_meas_t SNR_meas_data[SNR_MEAS_PER_DAY];

int dB_wire_to_dBm(int db_value);
void SNR_meas(void *param);

enum conn_count_e { EXTERNAL_ONLY, INCLUDE_INTERNAL, TDOA_USERS, EXT_API_USERS, LOCAL_OR_PWD_PROTECTED_USERS };
int rx_count_server_conns(conn_count_e type, conn_t *our_conn = NULL);

typedef enum { WS_MODE_ALLOC, WS_MODE_LOOKUP, WS_MODE_CLOSE, WS_INTERNAL_CONN } websocket_mode_e;
conn_t *rx_server_websocket(websocket_mode_e mode, struct mg_connection *mc);

typedef enum { RX_CHAN_ENABLE, RX_CHAN_DISABLE, RX_DATA_ENABLE, RX_CHAN_FREE } rx_chan_action_e;
void rx_enable(int chan, rx_chan_action_e action);

typedef enum { RX_COUNT_ALL, RX_COUNT_NO_WF_FIRST } rx_free_count_e;
int rx_chan_free_count(rx_free_count_e flags, int *idx = NULL, int *heavy = NULL);

typedef enum { LOG_ARRIVED, LOG_UPDATE, LOG_UPDATE_NC, LOG_LEAVING } logtype_e;
void rx_loguser(conn_t *c, logtype_e type);

typedef enum { PWD_CHECK_NO, PWD_CHECK_YES } pwd_check_e;
int rx_chan_no_pwd(pwd_check_e pwd_check = PWD_CHECK_NO);
