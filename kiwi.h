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
#include "kiwi.gen.h"
#include "str.h"
#include "printf.h"
#include "datatypes.h"
#include "coroutines.h"
#include "cfg.h"
#include "non_block.h"

#define	I	0
#define	Q	1
#define	NIQ	2

// for backward compatibility with old versions of the antenna switch extension
#define RX_CHANS 32

// The hardware returns RXO_BITS (typically 24-bits) and scaling down by RXOUT_SCALE
// will convert this to a +/- 1.0 float.
// But the CuteSDR code assumes a scaling of +/- 32.0k, so we scale up by CUTESDR_SCALE.
#define	RXOUT_SCALE	(RXO_BITS-1)	// s24 -> +/- 1.0
#define	CUTESDR_SCALE	15			// +/- 1.0 -> +/- 32.0k (s16 equivalent)
#define CUTESDR_MAX_VAL ((float) ((1 << CUTESDR_SCALE) - 1))
#define CUTESDR_MAX_PWR (CUTESDR_MAX_VAL * CUTESDR_MAX_VAL)

extern int version_maj, version_min;

extern bool background_mode, need_hardware, is_multi_core, kiwi_restart,
	DUC_enable_start, rev_enable_start, web_nocache, kiwi_reg_debug, cmd_debug,
	have_ant_switch_ext, gps_e1b_only, disable_led_task, debug_printfs, force_camp,
	snr_local_time, log_local_ip, DRM_enable, have_snd_users;

extern int wf_sim, wf_real, wf_time, ev_dump, wf_flip, wf_exit, wf_start, tone, down, navg,
	rx_cordic, rx_cic, rx_cic2, rx_dump, wf_cordic, wf_cic, wf_mult, wf_mult_gen, meas, monitors_max,
	rx_yield, gps_chans, spi_clkg, spi_speed, wf_max, rx_num, wf_num, do_slice, do_gps, do_sdr, wf_olap,
	spi_delay, do_fft, noisePwr, unwrap, rev_iq, ineg, qneg, fft_file, fftsize, fftuse, bg, dx_print,
	port, print_stats, ecpu_cmds, ecpu_tcmds, serial_number, ip_limit_mins, is_locked, test_flag, n_camp,
	use_spidev, inactivity_timeout_mins, S_meter_cal, waterfall_cal, current_nusers, debug_v, debian_ver,
	utc_offset, dst_offset, reg_kiwisdr_com_status, reg_kiwisdr_com_tid, kiwi_reg_lo_kHz, kiwi_reg_hi_kHz,
	debian_maj, debian_min, gps_debug, gps_var, gps_lo_gain, gps_cg_gain, use_foptim, web_caching_debug,
	drm_nreg_chans, snr_meas_interval_hrs, snr_all, snr_HF;

extern char **main_argv;

extern u4_t ov_mask, snd_intr_usec;
extern float g_genfreq, g_genampl, g_mixfreq, max_thr;
extern double ui_srate, freq_offset;
extern TYPEREAL DC_offset_I, DC_offset_Q;
extern kstr_t *cpu_stats_buf;
extern char *tzone_id, *tzone_name;
extern cfg_t cfg_ipl;
extern char *fpga_file;
extern lock_t spi_lock;

extern int p0, p1, p2;
extern float p0_f, p1_f, p2_f, p3_f, p4_f, p5_f, p6_f, p7_f;
extern int p0_i, p1_i, p2_i, p3_i, p4_i, p5_i, p6_i, p7_i;


// values defined in rx_cmd.cpp
// CAUTION: order in mode_s/modu_s must match mode_e, mode_hbw, mode_offset
// CAUTION: order in mode_s/modu_s must match kiwi.js:kiwi.modes_l
// CAUTION: add new entries at the end
#define N_MODE 16
// = { "am", "amn", "usb", "lsb", "cw", "cwn", "nbfm", "iq", "drm", "usn", "lsn", "sam", "sau", "sal", "sas", "qam" };
extern const char *mode_s[N_MODE], *modu_s[N_MODE];
extern const int mode_hbw[N_MODE], mode_offset[N_MODE];
typedef enum {
    MODE_AM, MODE_AMN, MODE_USB, MODE_LSB, MODE_CW, MODE_CWN, MODE_NBFM, MODE_IQ, MODE_DRM, MODE_USN, MODE_LSN, MODE_SAM, MODE_SAU, MODE_SAL, MODE_SAS, MODE_QAM
} mode_e;


typedef enum { DOM_SEL_NAM=0, DOM_SEL_DUC=1, DOM_SEL_PUB=2, DOM_SEL_SIP=3, DOM_SEL_REV=4 } dom_sel_e;

typedef enum { RX4_WF4=0, RX8_WF2=1, RX3_WF3=2, RX14_WF0=3 } firmware_e;

#define	KEEPALIVE_SEC		    60
#define KEEPALIVE_SEC_NO_AUTH   20      // don't hang the rx channel as long if waiting for password entry

// print_stats
#define STATS_GPS       0x01
#define STATS_GPS_SOLN  0x02
#define STATS_TASK      0x04

#define IDENT_LEN_MIN   16      // e.g. "wsprdaemon_v3.0a" is 16 chars

void fpga_init();

void update_vars_from_config(bool called_at_init = false);
void cfg_adm_transition();
void dump();

void c2s_sound_init();
void c2s_sound_setup(void *param);
void c2s_sound(void *param);
void c2s_sound_shutdown(void *param);

void c2s_waterfall_init();
void c2s_waterfall_compression(int rx_chan, bool compression);
void c2s_waterfall_no_sync(int rx_chan, bool no_sync);
void c2s_waterfall_setup(void *param);
void c2s_waterfall(void *param);
void c2s_waterfall_shutdown(void *param);

void c2s_mon_setup(void *param);
void c2s_mon(void *param);

void c2s_admin_setup(void *param);
void c2s_admin_shutdown(void *param);
void c2s_admin(void *param);

void c2s_mfg_setup(void *param);
void c2s_mfg(void *param);

extern bool update_pending, update_in_progress, backup_in_progress;
extern int pending_maj, pending_min;

extern bool sd_copy_in_progress;

void stat_task(void *param);
