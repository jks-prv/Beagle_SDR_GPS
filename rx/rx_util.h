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

// Copyright (c) 2023 John Seamons, ZL4VO/KF6VO

#pragma once

#include "types.h"
#include "config.h"
#include "kiwi.h"
#include "conn.h"
#include "web.h"

// autorun
#define MAX_ARUN_INST   MAX_RX_CHANS
#define ARUN_REG_USE    0
#define ARUN_PREEMPT_NO   0
#define ARUN_PREEMPT_YES  1

typedef enum { ARUN_NONE = 0, ARUN_FT8 = 1, ARUN_WSPR = 2 } arun_e;

typedef struct {
    #define NEW_TSTAMP_SPACE (1LL << 62)    // i.e. 0x4...
	u64_t last_conn_tstamp;

	arun_e arun_which[MAX_RX_CHANS];
	int arun_band[MAX_RX_CHANS];
	u4_t arun_evictions[MAX_RX_CHANS];
} rx_util_t;

extern rx_util_t rx_util;

typedef struct {
    u64_t  f_Hz,  baseband_Hz,  offset_Hz;
    double f_kHz, baseband_kHz, offset_kHz, offmax_kHz;
    bool isOffset;
} freq_t;

extern freq_t freq;

#define SNR_MEAS_MAX    (24 * 7)

#define SNR_MEAS_ALL    0
#define SNR_MEAS_HF     1
#define SNR_MEAS_0_2    2
#define SNR_MEAS_2_10   3
#define SNR_MEAS_10_20  4
#define SNR_MEAS_20_MAX 5
#define SNR_MEAS_NDATA  6

typedef struct {
    int f_lo, f_hi, min, max, pct_50, pct_95, snr;
} SNR_data_t;

typedef struct {
	bool valid;
    u4_t seq;
    char tstamp[CTIME_R_NL + 1 + SPACE_FOR_NULL];
    bool is_local_time;
    SNR_data_t data[SNR_MEAS_NDATA];
} SNR_meas_t;

extern SNR_meas_t SNR_meas_data[SNR_MEAS_MAX];
extern int SNR_meas_tid;

int dB_wire_to_dBm(int db_value);
void SNR_meas(void *param);

typedef enum { LOG_ARRIVED, LOG_UPDATE, LOG_UPDATE_NC, LOG_LEAVING } logtype_e;
void rx_loguser(conn_t *c, logtype_e type);

typedef enum {
    RX_COUNT_ALL,           // check all channels in order, *idx will be first free
    RX_COUNT_NO_WF_FIRST,   // check non-wf channels first, then wf channels
    RX_COUNT_NO_WF_AT_ALL   // never consider wf channels for configs where wf_chans != 0 && wf_chans < rx_chans
} rx_free_count_e;
int rx_chan_free_count(rx_free_count_e flags, int *idx = NULL, int *heavy = NULL, int *preempt = NULL, int *busy = NULL);

typedef enum { PWD_CHECK_NO, PWD_CHECK_YES } pwd_check_e;
int rx_chan_no_pwd(pwd_check_e pwd_check = PWD_CHECK_NO);

enum conn_count_e { EXTERNAL_ONLY, INCLUDE_INTERNAL, TDOA_USERS, EXT_API_USERS, LOCAL_OR_PWD_PROTECTED_USERS, ADMIN_USERS, ADMIN_CONN };
#define EXCEPT_PREEMPTABLE  0x01
int rx_count_server_conns(conn_count_e type, u4_t flags = 0, conn_t *our_conn = NULL);

enum kick_e { KICK_CHAN, KICK_USERS, KICK_ALL, KICK_ADMIN };
const char * const kick_s[] = { "KICK_CHAN", "KICK_USERS", "KICK_ALL", "KICK_ADMIN" };
void rx_server_kick(kick_e kick, int chan = -1);

void rx_set_freq(double freq_with_offset_kHz, double foff_kHz = -1);
void rx_set_freq_offset_kHz(double foff_kHz);
bool rx_freq_inRange(double freq_kHz);

conn_t *conn_other(conn_t *conn, int type);
void show_conn(const char *prefix, u4_t printf_type, conn_t *cd);
u64_t rx_conn_tstamp();
void rx_autorun_clear();
int rx_autorun_find_victim();
void rx_autorun_restart_victims(bool initial);
void rx_server_send_config(conn_t *conn);
bool save_config(u2_t key, conn_t *conn, char *cmd);
#define IS_ADMIN true
char *rx_users(bool isAdmin = false);
void geoloc_task(void *param);
int rx_mode2enum(const char *mode);
const char * rx_enum2mode(int e);
void debug_init();
void dump_init();
void rx_send_config(int rx_chan);
void on_GPS_solution();
float dB_fast(float x);
