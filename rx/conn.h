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

#include <sys/types.h>
#include <regex.h>
#include <fnmatch.h>

typedef struct conn_st {
	#define CN_MAGIC 0xcafecafe
	u4_t magic;
	int self_idx;
	bool valid, auth, auth_kiwi, auth_prot, auth_admin;
	bool isLocal, force_isLocal, isPassword;
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

	// set in STREAM_{SOUND, WATERFALL, EXT, ADMIN}
	u4_t keepalive_time, keep_alive, keepalive_count;

	// set in both STREAM_SOUND & STREAM_WATERFALL
	int task;
	bool stop_data, kick;
	user_iface_t *ui;

	// set in STREAM_SOUND or STREAM_WATERFALL (WF-only connections)
	bool ident, arrived;
	char *user;
	bool isUserIP;

	// set only in STREAM_SOUND
	bool snd_cmd_recv_ok;
	bool inactivity_timeout;
	int freqHz, last_freqHz;
	int mode, last_mode;
	int zoom, last_zoom;	// zoom set in both
	int last_tune_time, last_log_time;
	bool tlimit_exempt, tlimit_exempt_by_pwd, tlimit_zombie;
	float half_bw;
	TYPECPX last_sample;
	char *pref_id, *pref;
	bool is_locked;
	bool ext_api;
	
	// set only in STREAM_WATERFALL
	bool wf_cmd_recv_ok;
	char *dx_filter_ident, *dx_filter_notes;
	bool dx_has_preg_ident, dx_has_preg_notes;
	int dx_err_preg_ident, dx_err_preg_notes;
	regex_t dx_preg_ident, dx_preg_notes;
	int dx_filter_case, dx_filter_wild, dx_filter_grep;
	bool isWF_conn;

	// set in STREAM_EXT, STREAM_SOUND
	int ext_rx_chan;
	ext_t *ext;
	
	// set only in STREAM_MONITOR
	bool queued;
	bool camp_init, camp_passband;

	// set only in STREAM_ADMIN
	int log_last_sent, log_last_not_shown;
	int master_pty_fd, console_child_pid;
	bool send_ctrl;
	char send_ctrl_char;
	
	bool adjust_clock;      // should this connections clock be adjusted?
	double adc_clock_corrected, manual_offset, srate;
	u4_t arrival;
	update_check_e update_check;
	int nloop;
	char *geo;
	bool try_geoloc;
	
	// debug
	int wf_frames;
	u4_t wf_loop, wf_lock, wf_get;
	u4_t audio_underrun, sequence_errors;
	u4_t spurious_timestamps_recvd, unknown_cmd_recvd;

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
#define STREAM_MONITOR      6
#define AJAX_DISCOVERY		7
#define AJAX_PHOTO			8
#define AJAX_STATUS			9
#define AJAX_USERS			10
#define AJAX_SNR			11

extern conn_t conns[];
