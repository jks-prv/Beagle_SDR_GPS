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
#include "ext.h"

#define	WS_OPCODE_TEXT		1
#define	WS_OPCODE_BINARY	2

#define NREQ_BUF (16*1024)		// the dx list can easily get longer than 1K

user_iface_t *find_ui(int port);

struct conn_t;

struct rx_chan_t {
	bool enabled;
	bool busy;
	conn_t *conn;
};

struct stream_t {
	int type;
	const char *uri;
	funcP_t f;
	funcP_t setup;
	u4_t priority;
};

extern stream_t streams[];

#define	N_ADMIN			8
#define N_CONN_SND_WF	2

// N_EXT below because it's possible that a user could have loaded, and idled all possible extensions
#define	N_CONNS	(RX_CHANS * (N_CONN_SND_WF + N_EXT) + N_ADMIN)

struct ext_t;

struct conn_t {
	#define CN_MAGIC 0xcafecafe
	u4_t magic;
	conn_t *self;
	int self_idx;
	bool valid, auth;
	int type;
	conn_t *other;
	int rx_channel;
	struct mg_connection *mc;

	#define NRIP 48
	char remote_ip[NRIP];         // Max IPv6 string length is 45 characters
	int remote_port;
	u64_t tstamp;
	ndesc_t s2c, c2s;
	funcP_t task_func;

	// set in both STREAM_SOUND & STREAM_WATERFALL
	int task;
	u4_t keep_alive, keepalive_count;
	bool stop_data;
	user_iface_t *ui;

	// set only in STREAM_SOUND
	bool arrived, inactivity_timeout, inactivity_msg_sent, inactivity_timeout_override;
	int freqHz, last_freqHz;
	int mode, last_mode;
	int zoom, last_zoom;	// zoom set in both
	int last_tune_time, last_log_time;
	float half_bw;
	TYPECPX last_sample;
	
	// set only in STREAM_EXT
	int ext_rx_chan;
	ext_t *ext;
	
	// set only in STREAM_ADMIN
	int log_last_sent, log_last_not_shown;
	
	u4_t arrival;
	int nloop;
	char *user;
	bool isUserIP;
	char *geo;
	
	// debug
	int wf_frames;
	u4_t wf_loop, wf_lock, wf_get;
	bool first_slow;
	u4_t audio_underrun, sequence_errors;

	#ifdef SND_SEQ_CHECK
		bool audio_check;
		u4_t audio_pkts_sent, audio_epoch, audio_last_time;
		u2_t audio_sequence;
		s4_t sum2;
	#endif
};

// conn_t.type
#define STREAM_SOUND		0
#define STREAM_WATERFALL	1
#define STREAM_ADMIN		2
#define STREAM_MFG			3
#define STREAM_EXT			4
#define AJAX_DISCOVERY		5
#define AJAX_PHOTO			6
#define AJAX_VERSION		7
#define AJAX_SDR_HU			8
#define AJAX_DUMP			9

void app_to_web(conn_t *c, char *s, int sl);

char *rx_server_ajax(struct mg_connection *mc);
int web_to_app(conn_t *c, nbuf_t **nbp);
void web_to_app_done(conn_t *c, nbuf_t *nb);

void webserver_connection_cleanup(conn_t *c);

typedef enum {WS_INIT_CREATE, WS_INIT_START} ws_init_t;
void web_server_init(ws_init_t type);

#define SVCS_RESTART_TRUE	true
#define SVCS_RESTART_FALSE	false
void services_start(bool restart);

void reload_index_params();
