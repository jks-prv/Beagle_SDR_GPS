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
#include "cuteSDR.h"

typedef struct {
	bool enabled;
	bool busy;
	conn_t *conn_snd;       // the STREAM_SOUND conn
} rx_chan_t;

extern rx_chan_t rx_channels[];

// sound
typedef struct {
	u4_t seq;
    #ifdef SND_SEQ_CHECK
        bool snd_seq_init;
	    u4_t snd_seq;
    #endif
} snd_t;

extern snd_t snd_inst[MAX_RX_CHANS];

typedef struct {
	struct {
		char id[3];
		u1_t flags;
		u4_t seq;           // waterfall syncs to this sequence number on the client-side
		char smeter[2];
	} __attribute__((packed)) h;
	u1_t buf[FASTFIR_OUTBUF_SIZE * sizeof(u2_t)];
} __attribute__((packed)) snd_pkt_real_t;

typedef struct {
	struct {
		char id[3];
		u1_t flags;
		u4_t seq;                // waterfall syncs to this sequence number on the client-side
		char smeter[2];
		u1_t last_gps_solution; // time difference to last gps solution in seconds
		u1_t dummy;
		u4_t gpssec;            // GPS time stamp (GPS seconds)
		u4_t gpsnsec;           // GPS time stamp (fractional seconds in units of ns)
	} __attribute__((packed)) h;
	u1_t buf[FASTFIR_OUTBUF_SIZE * 2 * sizeof(u2_t)];
} __attribute__((packed)) snd_pkt_iq_t;


// waterfall

// Use odd values so periodic signals like radars running at even-Hz rates don't
// beat against update rate and produce artifacts or blanking.

#define	WF_SPEED_MAX		23

#define WF_SPEED_OFF        0
#define	WF_SPEED_1FPS		1
#define	WF_SPEED_SLOW		5
#define	WF_SPEED_MED		13
#define	WF_SPEED_FAST		WF_SPEED_MAX

#define	WEB_SERVER_POLL_US	(1000000 / WF_SPEED_MAX / 2)


void rx_server_init();
void rx_server_remove(conn_t *c);
void rx_server_user_kick(int chan);
void rx_server_send_config(conn_t *conn);
bool rx_common_cmd(const char *stream_name, conn_t *conn, char *cmd);

enum conn_count_e { EXTERNAL_ONLY, INCLUDE_INTERNAL, TDOA_USERS };
int rx_count_server_conns(conn_count_e type);

typedef enum { WS_MODE_ALLOC, WS_MODE_LOOKUP, WS_MODE_CLOSE, WS_INTERNAL_CONN } websocket_mode_e;
conn_t *rx_server_websocket(websocket_mode_e mode, struct mg_connection *mc);

typedef enum { RX_CHAN_ENABLE, RX_CHAN_DISABLE, RX_CHAN_FREE } rx_chan_action_e;
void rx_enable(int chan, rx_chan_action_e action);
int rx_chan_free(bool isWF_conn, int *idx);

typedef enum { LOG_ARRIVED, LOG_UPDATE, LOG_UPDATE_NC, LOG_LEAVING } logtype_e;
void rx_loguser(conn_t *c, logtype_e type);
