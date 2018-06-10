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

// mongoose.h struct mg_connection *
// types.h funcP_t
// config.h user_iface_t
#include "nbuf.h"           // ndesc_t
#include "ext.h"            // ext_t
#include "non_block.h"      // non_blocking_cmd_t
#include "update.h"         // update_check_e
#include "datatypes.h"      // TYPECPX

typedef struct conn_st {
	#define CN_MAGIC 0xcafecafe
	u4_t magic;
	struct conn_st *self;
	int self_idx;
	bool valid, auth, auth_kiwi, auth_admin, isLocal;
	int type;
	struct conn_st *other;
	int rx_channel;
	struct mg_connection *mc;
	bool internal_connection;

	char remote_ip[NET_ADDRSTRLEN];
	int remote_port;
	u64_t tstamp;
	ndesc_t s2c, c2s;
	funcP_t task_func;
	char *tname;

	// set in both STREAM_SOUND & STREAM_WATERFALL
	int task;
	u4_t keep_alive, keepalive_count;
	bool stop_data, kick;
	user_iface_t *ui;

	// set only in STREAM_SOUND
	bool arrived, inactivity_timeout, inactivity_timeout_override;
	int freqHz, last_freqHz;
	int mode, last_mode;
	int zoom, last_zoom;	// zoom set in both
	int last_tune_time, last_log_time;
	int ipl_cur_secs;
	bool tlimit_exempt;
	float half_bw;
	TYPECPX last_sample;
	char *pref_id, *pref;
	
	// set in STREAM_EXT, STREAM_SOUND
	int ext_rx_chan;
	ext_t *ext;
	
	// set only in STREAM_ADMIN
	int log_last_sent, log_last_not_shown;
	non_blocking_cmd_t console_nbc;
	int master_pty_fd, child_pid;
	bool send_ctrl_c, send_ctrl_d, send_ctrl_backslash;
	
	bool adjust_clock;      // should this connections clock be adjusted?
	double adc_clock_corrected, manual_offset, srate;
	u4_t arrival;
	update_check_e update_check;
	int nloop;
	char *user;
	bool isUserIP;
	char *geo;
	bool try_geoloc;
	
	// debug
	int wf_frames;
	u4_t wf_loop, wf_lock, wf_get;
	u4_t audio_underrun, sequence_errors;

	#ifdef SND_TIMING_CK
		bool audio_check;
		u4_t audio_pkts_sent, audio_epoch, audio_last_time;
		u2_t audio_sequence;
		s4_t sum2;
	#endif
} conn_t;

// conn_t.type
#define AJAX_VERSION		0
#define STREAM_ADMIN		1
#define STREAM_SOUND		2
#define STREAM_WATERFALL	3
#define STREAM_MFG			4
#define STREAM_EXT			5
#define AJAX_DISCOVERY		6
#define AJAX_PHOTO			7
#define AJAX_STATUS			8

extern conn_t conns[];
