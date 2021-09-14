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

// Copyright (c) 2014-2021 John Seamons, ZL/KF6VO

#include "types.h"
#include "config.h"
#include "kiwi.h"
#include "rx.h"
#include "mem.h"
#include "misc.h"
#include "str.h"
#include "printf.h"
#include "timer.h"
#include "web.h"
#include "spi.h"
#include "gps.h"
#include "cfg.h"
#include "coroutines.h"
#include "net.h"
#include "clk.h"
#include "wspr.h"
#include "ext_int.h"
#include "shmem.h"      // shmem->status_str_large
#include "rx_noise.h"
#include "wdsp.h"
#include "security.h"

#ifdef DRM
 #include "DRM.h"
#else
 #define DRM_NREG_CHANS_DEFAULT 3
#endif

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <sched.h>
#include <math.h>
#include <signal.h>

// copy admin-related configuration from kiwi.json to new admin.json file
void cfg_adm_transition()
{
	int i;
	bool b;
	const char *s;

	s = cfg_string("user_password", NULL, CFG_REQUIRED);
	admcfg_set_string("user_password", s);
	cfg_string_free(s);
	b = cfg_bool("user_auto_login", NULL, CFG_REQUIRED);
	admcfg_set_bool("user_auto_login", b);
	s = cfg_string("admin_password", NULL, CFG_REQUIRED);
	admcfg_set_string("admin_password", s);
	cfg_string_free(s);
	b = cfg_bool("admin_auto_login", NULL, CFG_REQUIRED);
	admcfg_set_bool("admin_auto_login", b);
	
	i = cfg_int("port", NULL, CFG_REQUIRED);
	admcfg_set_int("port", i);
	
	b = cfg_bool("enable_gps", NULL, CFG_REQUIRED);
	admcfg_set_bool("enable_gps", b);

	b = cfg_bool("update_check", NULL, CFG_REQUIRED);
	admcfg_set_bool("update_check", b);
	b = cfg_bool("update_install", NULL, CFG_REQUIRED);
	admcfg_set_bool("update_install", b);
	
	b = cfg_bool("sdr_hu_register", NULL, CFG_REQUIRED);
	admcfg_set_bool("sdr_hu_register", b);
	s = cfg_string("api_key", NULL, CFG_REQUIRED);
	admcfg_set_string("api_key", s);
	cfg_string_free(s);


	// remove from kiwi.json file
	cfg_rem_string("user_password");
	cfg_rem_bool("user_auto_login");
	cfg_rem_string("admin_password");
	cfg_rem_bool("admin_auto_login");

	cfg_rem_int("port");

	cfg_rem_bool("enable_gps");

	cfg_rem_bool("update_check");
	cfg_rem_bool("update_install");

	cfg_rem_bool("sdr_hu_register");
	cfg_rem_string("api_key");


	// won't be present first time after upgrading from v1.2
	// first admin page connection will create
	if ((s = cfg_object("ip_address", NULL, CFG_OPTIONAL)) != NULL) {
		admcfg_set_object("ip_address", s);
		cfg_object_free(s);
		cfg_rem_object("ip_address");
	}


	// update JSON files
	admcfg_save_json(cfg_adm.json);
	cfg_save_json(cfg_cfg.json);
}

int inactivity_timeout_mins, ip_limit_mins;
int S_meter_cal, waterfall_cal;
double ui_srate, freq_offset;
int kiwi_reg_lo_kHz, kiwi_reg_hi_kHz;
float max_thr;
int n_camp;
bool log_local_ip, DRM_enable;

#define DC_OFFSET_DEFAULT -0.02F
#define DC_OFFSET_DEFAULT_PREV 0.05F
#define DC_OFFSET_DEFAULT_20kHz -0.034F
TYPEREAL DC_offset_I, DC_offset_Q;

#define WATERFALL_CALIBRATION_DEFAULT -13
#define SMETER_CALIBRATION_DEFAULT -13

#define N_MTU 3
static int mtu_v[N_MTU] = { 1500, 1440, 1400 };

static int snr_interval[] = { 0, 1, 4, 6, 24 };

void update_vars_from_config(bool called_at_init)
{
    int n;
	bool update_cfg = false;
	bool update_admcfg = false;
	const char *s;
    bool err;

    // When called by "SET save_cfg/save_adm=":
	//  Makes C copies of vars that must be updated when configuration saved from js.
	//
	// When called by rx_server_init():
	//  Makes C copies of vars that must be updated when configuration loaded from cfg files.
	//  Creates configuration parameters with default values that must exist for client connections.

    inactivity_timeout_mins = cfg_default_int("inactivity_timeout_mins", 0, &update_cfg);
    ip_limit_mins = cfg_default_int("ip_limit_mins", 0, &update_cfg);

    int srate_idx = cfg_default_int("max_freq", 0, &update_cfg);
	ui_srate = srate_idx? 32*MHz : 30*MHz;


    // force DC offsets to the default value if not configured
    // also if set to the previous default value
    int firmware_sel = admcfg_default_int("firmware_sel", 0, &update_admcfg);   // needed below
    int mode_20kHz = (firmware_sel == RX3_WF3)? 1:0;
    TYPEREAL Ioff, Ioff_20kHz, Qoff, Qoff_20kHz;
    //printf("mode_20kHz=%d\n", mode_20kHz);

    Ioff = cfg_float("DC_offset_I", &err, CFG_OPTIONAL);
    if (err || Ioff == DC_OFFSET_DEFAULT_PREV) {
        Ioff = DC_OFFSET_DEFAULT;
        cfg_set_float("DC_offset_I", Ioff);
        lprintf("DC_offset_I: no cfg or prev default, setting to default value\n");
        update_cfg = true;
    }

    Qoff = cfg_float("DC_offset_Q", &err, CFG_OPTIONAL);
    if (err || Qoff == DC_OFFSET_DEFAULT_PREV) {
        Qoff = DC_OFFSET_DEFAULT;
        cfg_set_float("DC_offset_Q", Ioff);
        lprintf("DC_offset_Q: no cfg or prev default, setting to default value\n");
        update_cfg = true;
    }

    Ioff_20kHz = cfg_float("DC_offset_20kHz_I", &err, CFG_OPTIONAL);
    if (err) {
        Ioff_20kHz = DC_OFFSET_DEFAULT_20kHz;
        cfg_set_float("DC_offset_20kHz_I", Ioff_20kHz);
        lprintf("DC_offset_20kHz_I: no cfg or prev default, setting to default value\n");
        update_cfg = true;
    }

    Qoff_20kHz = cfg_float("DC_offset_20kHz_Q", &err, CFG_OPTIONAL);
    if (err) {
        Qoff_20kHz = DC_OFFSET_DEFAULT_20kHz;
        cfg_set_float("DC_offset_20kHz_Q", Qoff_20kHz);
        lprintf("DC_offset_20kHz_Q: no cfg or prev default, setting to default value\n");
        update_cfg = true;
    }

    DC_offset_I = mode_20kHz? Ioff_20kHz : Ioff;
    DC_offset_Q = mode_20kHz? Qoff_20kHz : Qoff;
    static bool dc_off_msg;
    if (!dc_off_msg) {
        lprintf("using DC_offsets: I %.6f Q %.6f\n", DC_offset_I, DC_offset_Q);
        dc_off_msg = true;
    }

    // DON'T use cfg_default_int() here because "DRM.nreg_chans" is a two-level we can't init from C code
    drm_nreg_chans = cfg_int("DRM.nreg_chans", &err, CFG_OPTIONAL);
    if (err) drm_nreg_chans = DRM_NREG_CHANS_DEFAULT;

    // fix any broken UTF-8 sequences via cfg_default_string()
    cfg_default_string("index_html_params.HTML_HEAD", "", &update_cfg);
    cfg_default_string("index_html_params.PAGE_TITLE", "", &update_cfg);
    cfg_default_string("index_html_params.RX_PHOTO_TITLE", "", &update_cfg);
    cfg_default_string("index_html_params.RX_PHOTO_DESC", "", &update_cfg);
    cfg_default_string("index_html_params.RX_TITLE", "", &update_cfg);
    cfg_default_string("index_html_params.RX_LOC", "", &update_cfg);
    cfg_default_string("status_msg", "", &update_cfg);
    cfg_default_string("rx_name", "", &update_cfg);
    cfg_default_string("rx_device", "", &update_cfg);
    cfg_default_string("rx_location", "", &update_cfg);
    cfg_default_string("rx_antenna", "", &update_cfg);
    cfg_default_string("owner_info", "", &update_cfg);
    cfg_default_string("reason_disabled", "", &update_cfg);

    S_meter_cal = cfg_default_int("S_meter_cal", SMETER_CALIBRATION_DEFAULT, &update_cfg);
    waterfall_cal = cfg_default_int("waterfall_cal", WATERFALL_CALIBRATION_DEFAULT, &update_cfg);
    cfg_default_bool("contact_admin", true, &update_cfg);
    cfg_default_int("chan_no_pwd", 0, &update_cfg);
    cfg_default_int("clk_adj", 0, &update_cfg);
    freq_offset = cfg_default_float("freq_offset", 0, &update_cfg);
    kiwi_reg_lo_kHz = cfg_default_int("sdr_hu_lo_kHz", 0, &update_cfg);
    kiwi_reg_hi_kHz = cfg_default_int("sdr_hu_hi_kHz", 30000, &update_cfg);
    cfg_default_bool("index_html_params.RX_PHOTO_LEFT_MARGIN", true, &update_cfg);
    cfg_default_bool("ext_ADC_clk", false, &update_cfg);
    cfg_default_int("ext_ADC_freq", (int) round(ADC_CLOCK_TYP), &update_cfg);
    cfg_default_bool("ADC_clk_corr", true, &update_cfg);
    cfg_default_string("tdoa_id", "", &update_cfg);
    cfg_default_int("tdoa_nchans", -1, &update_cfg);
    cfg_default_int("ext_api_nchans", -1, &update_cfg);
    cfg_default_bool("no_wf", false, &update_cfg);
    cfg_default_bool("test_webserver_prio", false, &update_cfg);
    cfg_default_bool("test_deadline_update", false, &update_cfg);
    cfg_default_bool("disable_recent_changes", false, &update_cfg);
    cfg_default_int("init.cw_offset", 500, &update_cfg);
    cfg_default_int("init.colormap", 0, &update_cfg);
    cfg_default_int("init.aperture", 1, &update_cfg);
    cfg_default_int("S_meter_OV_counts", 10, &update_cfg);
    cfg_default_bool("webserver_caching", true, &update_cfg);
    max_thr = (float) cfg_default_int("overload_mute", -15, &update_cfg);
    cfg_default_bool("agc_thresh_smeter", true, &update_cfg);
    n_camp = cfg_default_int("n_camp", N_CAMP, &update_cfg);
    snr_meas_interval_hrs = snr_interval[cfg_default_int("snr_meas_interval_hrs", 1, &update_cfg)];
    snr_local_time = cfg_default_bool("snr_local_time", true, &update_cfg);

    if (wspr_update_vars_from_config()) update_cfg = true;

    int espeed = cfg_default_int("ethernet_speed", 0, &update_cfg);
    static int current_espeed;
    if (espeed != current_espeed) {
        printf("ETH0 espeed %d\n", espeed? 10:100);
        non_blocking_cmd_system_child(
            "kiwi.ethtool", stprintf("ethtool -s eth0 speed %d duplex full", espeed? 10:100), NO_WAIT);
        current_espeed = espeed;
    }
    
    int mtu = cfg_default_int("ethernet_mtu", 0, &update_cfg);
    if (mtu < 0 || mtu >= N_MTU) mtu = 0;
    static int current_mtu;
    if (mtu != current_mtu) {
        int mtu_val = mtu_v[mtu];
        printf("ETH0 ifconfig eth0 mtu %d\n", mtu_val);
        non_blocking_cmd_system_child("kiwi.ifconfig", stprintf("ifconfig eth0 mtu %d", mtu_val), NO_WAIT);
        current_mtu = mtu;
    }
    
    // fix corruption left by v1.131 dotdot bug
    _cfg_int(&cfg_cfg, "WSPR.autorun", &err, CFG_OPTIONAL|CFG_NO_DOT);
    if (!err) {
        _cfg_set_int(&cfg_cfg, "WSPR.autorun", 0, CFG_REMOVE|CFG_NO_DOT, 0);
        _cfg_set_bool(&cfg_cfg, "index_html_params.RX_PHOTO_LEFT_MARGIN", 0, CFG_REMOVE|CFG_NO_DOT, 0);
        printf("removed v1.131 dotdot bug corruption\n");
        update_cfg = true;
    }
    
    // enforce waterfall min_dB < max_dB
    int min_dB = cfg_default_int("init.min_dB", -110, &update_cfg);
    int max_dB = cfg_default_int("init.max_dB", -10, &update_cfg);
    if (min_dB >= max_dB) {
        cfg_set_int("init.min_dB", -110);
        cfg_set_int("init.max_dB", -10);
        update_cfg = true;
    }

    int _dom_sel = cfg_default_int("sdr_hu_dom_sel", DOM_SEL_NAM, &update_cfg);

    #if 0
        // try and get this Kiwi working with the proxy
        //printf("serno=%d dom_sel=%d\n", serial_number, _dom_sel);
	    if (serial_number == 1006 && _dom_sel == DOM_SEL_NAM) {
            cfg_set_int("sdr_hu_dom_sel", DOM_SEL_REV);
            update_cfg = true;
            lprintf("######## FORCE DOM_SEL_REV serno=%d ########\n", serial_number);
	    }
    #endif
    
    // remove old kiwisdr.example.com default
    cfg_default_string("server_url", "", &update_cfg);
    const char *server_url = cfg_string("server_url", NULL, CFG_REQUIRED);
	if (strcmp(server_url, "kiwisdr.example.com") == 0) {
	    cfg_set_string("server_url", "");
	    update_cfg = true;
    }
    
    // not sure I want to do this yet..
    #if 0
        // Strange problem where cfg.sdr_hu_dom_sel seems to get changed occasionally between modes
        // DOM_SEL_NAM=0 and DOM_SEL_PUB=2. This can result in DOM_SEL_NAM selected but the corresponding
        // domain field blank which has bad consequences (e.g. TDoA host file corrupted).
        // So do some consistency checking here.
        if (dom_sel == DOM_SEL_NAM && (*server_url == '\0' || strcmp(server_url, "kiwisdr.example.com") == 0)) {
            lprintf("### DOM_SEL check: DOM_SEL_NAM but server_url=\"%s\"\n", server_url);
            lprintf("### DOM_SEL check: forcing change to DOM_SEL_PUB\n");
            cfg_set_int("sdr_hu_dom_sel", DOM_SEL_PUB);
            // FIXME: but then server_url needs to be set when pub ip is detected
            update_cfg = true;
        }
	#endif
    cfg_string_free(server_url); server_url = NULL;
    
    // move kiwi.json tlimit_exempt_pwd to admin.json
	if ((s = cfg_string("tlimit_exempt_pwd", NULL, CFG_OPTIONAL)) != NULL) {
		admcfg_set_string("tlimit_exempt_pwd", s);
	    cfg_string_free(s);
	    cfg_rem_string("tlimit_exempt_pwd");
	    update_cfg = update_admcfg = true;
	} else {
        admcfg_default_string("tlimit_exempt_pwd", "", &update_admcfg);
    }
    
    // sdr.hu => rx.kiwisdr.com in status msg
    char *status_msg = (char *) cfg_string("status_msg", NULL, CFG_REQUIRED);
    bool caller_must_free;
	char *nsm = kiwi_str_replace(status_msg, "sdr.hu", "rx.kiwisdr.com", &caller_must_free);
	if (nsm) {
	    nsm = kiwi_str_replace(nsm, "/?top=kiwi", "");  // shrinking, so nsm same memory space
	    cfg_set_string("status_msg", nsm);
	    if (caller_must_free) kiwi_ifree(nsm);
	    update_cfg = true;
    }
    cfg_string_free(status_msg); status_msg = NULL;

    char *rx_name = (char *) cfg_string("rx_name", NULL, CFG_REQUIRED);
    // shrinking, so same memory space
	nsm = kiwi_str_replace(rx_name, ", ZL/KF6VO, New Zealand", "");
	if (nsm) {
        cfg_set_string("rx_name", nsm);
        update_cfg = true;
    }
    cfg_string_free(rx_name); rx_name = NULL;

    char *rx_title = (char *) cfg_string("RX_TITLE", NULL, CFG_REQUIRED);
    // shrinking, so same memory space
	nsm = kiwi_str_replace(rx_title, " at <a href='http://kiwisdr.com' target='_blank' onclick='dont_toggle_rx_photo()'>ZL/KF6VO</a>", "");
	if (nsm) {
        cfg_set_string("RX_TITLE", nsm);
        update_cfg = true;
    }
    cfg_string_free(rx_title); rx_title = NULL;

    DRM_enable = cfg_bool("DRM.enable", &err, CFG_OPTIONAL);
    if (err) DRM_enable = true;

	if (update_cfg)
		cfg_save_json(cfg_cfg.json);


	// same, but for admin config
	// currently just default values that need to exist
	
    admcfg_default_bool("server_enabled", true, &update_admcfg);
    admcfg_default_bool("auto_add_nat", false, &update_admcfg);
    admcfg_default_bool("duc_enable", false, &update_admcfg);
    admcfg_default_string("duc_user", "", &update_admcfg);
    admcfg_default_string("duc_pass", "", &update_admcfg);
    admcfg_default_string("duc_host", "", &update_admcfg);
    admcfg_default_int("duc_update", 3, &update_admcfg);
    admcfg_default_bool("daily_restart", false, &update_admcfg);
    admcfg_default_int("restart_update", 0, &update_admcfg);
    admcfg_default_int("update_restart", 0, &update_admcfg);
    admcfg_default_string("ip_address.dns1", "1.1.1.1", &update_admcfg);
    admcfg_default_string("ip_address.dns2", "8.8.8.8", &update_admcfg);
    admcfg_default_string("url_redirect", "", &update_admcfg);
    admcfg_default_string("ip_blacklist", "47.88.219.24/24", &update_admcfg);
    admcfg_default_bool("no_dup_ip", false, &update_admcfg);
    admcfg_default_bool("my_kiwi", true, &update_admcfg);
    admcfg_default_bool("onetime_password_check", false, &update_admcfg);
    admcfg_default_string("proxy_server", "proxy.kiwisdr.com", &update_admcfg);
    admcfg_default_bool("console_local", true, &update_admcfg);
    log_local_ip = admcfg_default_bool("log_local_ip", true, &update_admcfg);

    // decouple rx.kiwisdr.com and sdr.hu registration
    bool sdr_hu_register = admcfg_bool("sdr_hu_register", NULL, CFG_REQUIRED);
	admcfg_bool("kiwisdr_com_register", &err, CFG_OPTIONAL);
    // never set or incorrectly set to false by v1.365,366
	if (err || (VERSION_MAJ == 1 && VERSION_MIN <= 369)) {
        admcfg_set_bool("kiwisdr_com_register", sdr_hu_register);
        update_admcfg = true;
    }

    // historical uses of options parameter:
    //int new_find_local = admcfg_int("options", NULL, CFG_REQUIRED) & 1;
    admcfg_default_int("options", 0, &update_admcfg);

    admcfg_default_bool("GPS_tstamp", true, &update_admcfg);
    admcfg_default_bool("use_kalman_position_solver", true, &update_admcfg);
    admcfg_default_int("rssi_azel_iq", 0, &update_admcfg);

    admcfg_default_bool("always_acq_gps", false, &update_admcfg);
    gps.set_date = admcfg_default_bool("gps_set_date", false, &update_admcfg);
    gps.include_alert_gps = admcfg_default_bool("include_alert_gps", false, &update_admcfg);
    //real_printf("gps.include_alert_gps=%d\n", gps.include_alert_gps);
    gps.include_E1B = admcfg_default_bool("include_E1B", true, &update_admcfg);
    //real_printf("gps.include_E1B=%d\n", gps.include_E1B);
    admcfg_default_int("E1B_offset", 4, &update_admcfg);

    gps.acq_Navstar = admcfg_default_bool("acq_Navstar", true, &update_admcfg);
    if (!gps.acq_Navstar) ChanRemove(Navstar);
    gps.acq_QZSS = admcfg_default_bool("acq_QZSS", true, &update_admcfg);
    if (!gps.acq_QZSS) ChanRemove(QZSS);
    gps.QZSS_prio = admcfg_default_bool("QZSS_prio", false, &update_admcfg);
    gps.acq_Galileo = admcfg_default_bool("acq_Galileo", true, &update_admcfg);
    if (!gps.acq_Galileo) ChanRemove(E1B);
    //real_printf("Navstar=%d QZSS=%d Galileo=%d\n", gps.acq_Navstar, gps.acq_QZSS, gps.acq_Galileo);

    // force plot_E1B true because there is no longer an option switch in the admin interface (to make room for new ones)
    bool plot_E1B = admcfg_default_bool("plot_E1B", true, &update_admcfg);
    if (!plot_E1B) {
	    admcfg_set_bool("plot_E1B", true);
        update_admcfg = true;
    }
    
    #ifdef CRYPT_PW
    
        // Either:
        // 1) Transitioning on startup from passwords stored in admin.json file from a prior version
        //      (.eup/.eap files will not exist).
        // or
        // 2) In response to a password change from the admin UI (sequence number will increment).
        //
        // Generates and saves a new salt/hash in either case.
        // The old {user,admin}_password fields are kept because other code needs to know when
        // the password is blank/empty. So it is either set to the empty string or "(encrypted)".
        // This does not preclude the user from using the string "(encrypted)" as an actual password.
        
        bool ok;
        const char *key;
        char *encrypted;
        static u4_t user_pwd_seq, admin_pwd_seq;
        
        if (called_at_init) {
            user_pwd_seq = admcfg_default_int("user_pwd_seq", 0, &update_admcfg);
            admin_pwd_seq = admcfg_default_int("admin_pwd_seq", 0, &update_admcfg);
        }
        u4_t updated_user_pwd_seq = admcfg_int("user_pwd_seq", NULL, CFG_REQUIRED);
        u4_t updated_admin_pwd_seq = admcfg_int("admin_pwd_seq", NULL, CFG_REQUIRED);

        bool eup_exists = kiwi_file_exists(DIR_CFG "/.eup");
        bool user_seq_diff = (user_pwd_seq != updated_user_pwd_seq);
        if (!eup_exists || user_seq_diff) {
            user_pwd_seq = updated_user_pwd_seq;
    	    key = admcfg_string("user_password", NULL, CFG_REQUIRED);
    	    
    	    if (!eup_exists) {
    	        // if hash file is missing, but key indicates encryption previously used, make key empty
    	        if (key && strcmp(key, "(encrypted)") == 0) {
                    cfg_string_free(key);
                    key = NULL;
                }
    	    }

            encrypted = kiwi_crypt_generate(key, user_pwd_seq);
            printf("### user crypt ok=%d seq=%d key=<%s> => %s\n", ok, updated_user_pwd_seq, key, encrypted);
            n = kiwi_file_write("eup", DIR_CFG "/.eup", encrypted, strlen(encrypted), /* add_nl */ true);
            free(encrypted);

            if (n) {
                admcfg_set_string("user_password", (key == NULL || *key == '\0')? "" : "(encrypted)");
                update_admcfg = true;
            }

            cfg_string_free(key);
        }

        bool eap_exists = kiwi_file_exists(DIR_CFG "/.eap");
        bool admin_seq_diff = (admin_pwd_seq != updated_admin_pwd_seq);
        if (!eap_exists || admin_seq_diff) {
            admin_pwd_seq = updated_admin_pwd_seq;
    	    key = admcfg_string("admin_password", NULL, CFG_REQUIRED);

    	    if (!eap_exists) {
    	        // if hash file is missing, but key indicates encryption previously used, make key empty
    	        if (key && strcmp(key, "(encrypted)") == 0) {
                    cfg_string_free(key);
                    key = NULL;
                }
    	    }

            encrypted = kiwi_crypt_generate(key, admin_pwd_seq);
            printf("### admin crypt ok=%d seq=%d key=<%s> => %s\n", ok, updated_admin_pwd_seq, key, encrypted);
            n = kiwi_file_write("eap", DIR_CFG "/.eap", encrypted, strlen(encrypted), /* add_nl */ true);
            free(encrypted);

            if (n) {
                admcfg_set_string("admin_password", (key == NULL || *key == '\0')? "" : "(encrypted)");
                update_admcfg = true;
            }

            cfg_string_free(key);
        }
    #endif
    
    // FIXME: resolve problem of ip_address.xxx vs ip_address:{xxx} in .json files
    //admcfg_default_bool("ip_address.use_static", false, &update_admcfg);

	if (update_admcfg)
		admcfg_save_json(cfg_adm.json);


    // one-time-per-run initializations
    
    static bool initial_clk_adj;
    if (!initial_clk_adj) {
        int clk_adj = cfg_int("clk_adj", &err, CFG_OPTIONAL);
        if (err == false) {
            printf("INITIAL clk_adj=%d\n", clk_adj);
            if (clk_adj != 0) {
                clock_manual_adj(clk_adj);
            }
        }
        initial_clk_adj = true;
    }
}

// pass result json back to main process via shmem->status_str_large
static int _geo_task(void *param)
{
	nbcmd_args_t *args = (nbcmd_args_t *) param;
	char *sp = kstr_sp(args->kstr);
    kiwi_strncpy(shmem->status_str_large, sp, N_SHMEM_STATUS_STR_LARGE);
    return 0;
}

static bool geoloc_json(conn_t *conn, const char *geo_host_ip_s, const char *country_s, const char *region_s)
{
	char *cmd_p;
	
    asprintf(&cmd_p, "curl -s --ipv4 \"https://%s\" 2>&1", geo_host_ip_s);
    //cprintf(conn, "GEOLOC: <%s>\n", cmd_p);
    
    // NB: don't use non_blocking_cmd() here to prevent audio gliches
    int status = non_blocking_cmd_func_forall("kiwi.geolocate", cmd_p, _geo_task, 0, POLL_MSEC(1000));
    kiwi_ifree(cmd_p);
    int exit_status;
    if (WIFEXITED(status) && (exit_status = WEXITSTATUS(status))) {
        clprintf(conn, "GEOLOC: failed for %s\n", geo_host_ip_s);
        return false;
    }
    //cprintf(conn, "GEOLOC: returned <%s>\n", shmem->status_str_large);

	cfg_t cfg_geo;
    if (json_init(&cfg_geo, shmem->status_str_large) == false) {
        clprintf(conn, "GEOLOC: JSON parse failed for %s\n", geo_host_ip_s);
        return false;
    }
    
    char *country_name = (char *) json_string(&cfg_geo, country_s, NULL, CFG_OPTIONAL);
    char *region_name = (char *) json_string(&cfg_geo, region_s, NULL, CFG_OPTIONAL);
    char *city = (char *) json_string(&cfg_geo, "city", NULL, CFG_OPTIONAL);
    
    char *country;
	if (country_name && strcmp(country_name, "United States") == 0 && region_name && *region_name) {
		country = kstr_cat(region_name, ", USA");
	} else {
		country = kstr_cat(country_name, NULL);     // possible that country_name == NULL
	}

	char *geo = NULL;
	if (city && *city) {
		geo = kstr_cat(geo, city);
		geo = kstr_cat(geo, ", ");
	}
    geo = kstr_cat(geo, country);   // NB: country freed here

    clprintf(conn, "GEOLOC: %s <%s>\n", geo_host_ip_s, kstr_sp(geo));
	kiwi_ifree(conn->geo);
    conn->geo = strdup(kstr_sp(geo));
    kstr_free(geo);

    json_string_free(&cfg_geo, country_name);
    json_string_free(&cfg_geo, region_name);
    json_string_free(&cfg_geo, city);
    
	json_release(&cfg_geo);
    return true;
}

static void geoloc_task(void *param)
{
	conn_t *conn = (conn_t *) param;
	char *ip = (isLocal_ip(conn->remote_ip) && net.pub_valid)? net.ip_pub : conn->remote_ip;

    u4_t i = timer_sec();   // mix it up a bit
    int retry = 0;
    bool okay = false;
    do {
        i = (i+1) % 3;
        if (i == 0) okay = geoloc_json(conn, stprintf("ipapi.co/%s/json", ip), "country_name", "region");
        else
        if (i == 1) okay = geoloc_json(conn, stprintf("extreme-ip-lookup.com/json/%s", ip), "country", "region");
        else
        if (i == 2) okay = geoloc_json(conn, stprintf("get.geojs.io/v1/ip/geo/%s.json", ip), "country", "region");
        retry++;
    } while (!okay && retry < 10);
    if (!okay) clprintf(conn, "GEOLOC: for %s FAILED for all geo servers\n", ip);
}

char *rx_users(bool include_ip)
{
    int i, n;
    rx_chan_t *rx;
    bool need_comma = false;
    char *sb = (char *) "[", *sb2;
    
    for (rx = rx_channels, i=0; rx < &rx_channels[rx_chans]; rx++, i++) {
        n = 0;
        if (rx->busy) {
            conn_t *c = rx->conn;
            if (c && c->valid && c->arrived) {
                assert(c->type == STREAM_SOUND || c->type == STREAM_WATERFALL);

                // connected time
                u4_t now = timer_sec();
                u4_t t = now - c->arrival;
                u4_t sec = t % 60; t /= 60;
                u4_t min = t % 60; t /= 60;
                u4_t hr = t;

                // 24hr ip TLIMIT time left (if applicable)
                int rem_24hr = 0;
                if (ip_limit_mins && !c->tlimit_exempt) {
                    rem_24hr = MINUTES_TO_SEC(ip_limit_mins) - json_default_int(&cfg_ipl, c->remote_ip, 0, NULL);
                    if (rem_24hr < 0 ) rem_24hr = 0;
                }

                // conn inactivity TLIMIT time left (if applicable)
                int rem_inact = 0;
                if (inactivity_timeout_mins && !c->tlimit_exempt) {
                    if (c->last_tune_time == 0) c->last_tune_time = now;    // got here before first set in rx_loguser()
                    rem_inact = MINUTES_TO_SEC(inactivity_timeout_mins) - (now - c->last_tune_time);
                    if (rem_inact < 0 ) rem_inact = 0;
                }

                int rtype = 0;
                u4_t rn = 0;
                if (rem_24hr || rem_inact) {
                    if (rem_24hr && rem_inact) {
                        if (rem_24hr < rem_inact) {
                            rn = rem_24hr;
                            rtype = 2;
                        } else {
                            rn = rem_inact;
                            rtype = 1;
                        }
                    } else
                    if (rem_24hr) {
                        rn = rem_24hr;
                        rtype = 2;
                    } else
                    if (rem_inact) {
                        rn = rem_inact;
                        rtype = 1;
                    }
                }
                
                t = rn;
                u4_t r_sec = t % 60; t /= 60;
                u4_t r_min = t % 60; t /= 60;
                u4_t r_hr = t;

                char *user = (c->isUserIP || !c->user)? NULL : kiwi_str_encode(c->user);
                char *geo = c->geo? kiwi_str_encode(c->geo) : NULL;
                char *ext = ext_users[i].ext? kiwi_str_encode((char *) ext_users[i].ext->name) : NULL;
                const char *ip = include_ip? c->remote_ip : "";
                asprintf(&sb2, "%s{\"i\":%d,\"n\":\"%s\",\"g\":\"%s\",\"f\":%d,\"m\":\"%s\",\"z\":%d,\"t\":\"%d:%02d:%02d\","
                    "\"rt\":%d,\"rn\":%d,\"rs\":\"%d:%02d:%02d\",\"e\":\"%s\",\"a\":\"%s\",\"c\":%.1f,\"fo\":%.0f,\"ca\":%d,"
                    "\"nc\":%d,\"ns\":%d}",
                    need_comma? ",":"", i, user? user:"", geo? geo:"", c->freqHz,
                    kiwi_enum2str(c->mode, mode_s, ARRAY_LEN(mode_s)), c->zoom, hr, min, sec,
                    rtype, rn, r_hr, r_min, r_sec, ext? ext:"", ip, wdsp_SAM_carrier(i), freq_offset, rx->n_camp,
                    extint.notify_chan, extint.notify_seq);
                if (user) kiwi_ifree(user);
                if (geo) kiwi_ifree(geo);
                if (ext) kiwi_ifree(ext);
                n = 1;
            }
        }
        if (n == 0) {
            asprintf(&sb2, "%s{\"i\":%d}", need_comma? ",":"", i);
        }
        sb = kstr_cat(sb, kstr_wrap(sb2));
        need_comma = true;
    }

    sb = kstr_cat(sb, "]\n");
    return sb;
}

int dB_wire_to_dBm(int db_value)
{
    // (u1_t) 255..55 => 0 .. -200 dBm
    if (db_value < 0) db_value = 0;
	if (db_value > 255) db_value = 255;
	int dBm = -(255 - db_value);
	return (dBm + waterfall_cal);
}


// schedule SNR measurements

int snr_meas_interval_hrs, snr_all, snr_HF;
bool snr_local_time;
SNR_meas_t SNR_meas_data[SNR_MEAS_MAX];
static int dB_raw[WF_WIDTH];

int SNR_calc(SNR_meas_t *meas, int meas_type, int f_lo, int f_hi)
{
    static int dB[WF_WIDTH];
    int i, rv = 0, len = 0, masked = 0;
    double ui_srate_kHz = round(ui_srate/1e3);
    int b_lo = f_lo - freq_offset, b_hi = f_hi - freq_offset;
    int start = (float) WF_WIDTH * b_lo / ui_srate_kHz;
    int stop  = (float) WF_WIDTH * b_hi / ui_srate_kHz;
    //printf("b_lo=%d b_hi=%d fo=%d s/s=%d/%d\n", b_lo, b_hi, (int) freq_offset, start, stop);

    for (i = (int) start; i < stop; i++) {
        if (dB_raw[i] <= -190) {
            masked++;
            continue;   // disregard masked areas
        }
        dB[len] = dB_raw[i];
        len++;
    }
    //printf("SNR_calc-%d base=%d:%d freq=%d:%d start/stop=%d/%d len=%d masked=%d\n",
    //    meas_type, b_lo, b_hi, f_lo, f_hi, start, stop, len, masked);

    if (len) {
        qsort(dB, len, sizeof(int), qsort_intcomp);
        SNR_data_t *data = &meas->data[meas_type];
        data->f_lo = f_lo; data->f_hi = f_hi;
        data->min = dB[0]; data->max = dB[len-1];
        data->pct_95 = 95;
        data->pct_50 = median_i(dB, len, &data->pct_95);
        data->snr = data->pct_95 - data->pct_50;
        rv = data->snr;
        //printf("SNR_calc-%d: [%d,%d] noise(50%)=%d signal(95%)=%d snr=%d\n",
        //    meas_type, data->min, data->max, data->pct_50, data->pct_95, data->snr);
    }
    
    return rv;
}

void SNR_meas(void *param)
{
    int i, j, n, len;
    static internal_conn_t iconn;
    TaskSleepSec(20);

    do {
        static int meas_idx;
	
        for (int loop = 0; loop == 0 && snr_meas_interval_hrs; loop++) {   // so break can be used below
            if (internal_conn_setup(ICONN_WS_WF, &iconn, 0, PORT_INTERNAL_SNR,
                NULL, 0, 0, 0,
                "SNR-measure", "internal%20task", "SNR",
                WF_ZOOM_MIN, 15000, -110, -10, WF_SELECT_FAST, WF_COMP_OFF) == false) {
                printf("SNR_meas: all channels busy\n");
                break;
            };
    
            SNR_meas_t *meas = &SNR_meas_data[meas_idx];
            memset(meas, 0, sizeof(*meas));
            meas_idx = ((meas_idx + 1) % SNR_MEAS_MAX);

	        // relative to local time if timezone has been determined, utc otherwise
	        if (snr_local_time) {
                time_t local = local_time(&meas->is_local_time);
                var_ctime_r(&local, meas->tstamp);
            } else {
                utc_ctime_r(meas->tstamp);
            }
            static u4_t snr_seq;
            meas->seq = snr_seq++;
            
            #define SNR_NSAMPS 32
            memset(dB_raw, 0, sizeof(dB_raw));
    
            nbuf_t *nb = NULL;
            for (i = 0; i < SNR_NSAMPS;) {
                do {
                    if (nb) web_to_app_done(iconn.cwf, nb);
                    n = web_to_app(iconn.cwf, &nb, /* internal_connection */ true);
                    if (n == 0) continue;
                    wf_pkt_t *wf = (wf_pkt_t *) nb->buf;
                    //printf("SNR_meas i=%d rcv=%d <%.3s>\n", i, n, wf->id4);
                    if (strncmp(wf->id4, "W/F", 3) != 0) continue;
                    for (j = 0; j < WF_WIDTH; j++) {
                        dB_raw[j] += dB_wire_to_dBm(wf->un.buf[j]);
                    }
                    i++;
                } while (n);
                TaskSleepMsec(1000 / WF_SPEED_FAST);
            }
            for (i = 0; i < WF_WIDTH; i++) {
                dB_raw[i] /= SNR_NSAMPS;
            }
            
            double ui_srate_kHz = round(ui_srate/1e3);
            int f_lo = freq_offset, f_hi = freq_offset + ui_srate_kHz;
            
            snr_all = SNR_calc(meas, SNR_MEAS_ALL, f_lo, f_hi);

            // only measure HF if no transverter frequency offset has been set
            snr_HF = 0;
            if (f_lo == 0) {
                snr_HF = SNR_calc(meas, SNR_MEAS_HF, 1800, f_hi);
                SNR_calc(meas, SNR_MEAS_0_2, 0, 1800);
                SNR_calc(meas, SNR_MEAS_2_10, 1800, 10000);
                SNR_calc(meas, SNR_MEAS_10_20, 10000, 20000);
                SNR_calc(meas, SNR_MEAS_20_MAX, 20000, f_hi);
            }

            meas->valid = true;
            rx_server_websocket(WS_MODE_CLOSE, &iconn.wf_mc);
        }
        
        //TaskSleepSec(60);
        // if disabled check again in an hour to see if re-enabled
        u64_t hrs = snr_meas_interval_hrs? snr_meas_interval_hrs : 1;
        TaskSleepSec(hrs * 60 * 60);
    } while (1);
}
