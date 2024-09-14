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

// Copyright (c) 2014-2023 John Seamons, ZL4VO/KF6VO

#include "types.h"
#include "config.h"
#include "kiwi.h"
#include "mode.h"
#include "rx.h"
#include "rx_cmd.h"
#include "rx_util.h"
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
#include "ext_int.h"
#include "shmem.h"      // shmem->status_str_large
#include "rx_noise.h"
#include "wdsp.h"
#include "security.h"
#include "options.h"
#include "services.h"
#include "stats.h"

#include "wspr.h"
#include "FT8.h"

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <sched.h>
#include <math.h>
#include <signal.h>

rx_util_t rx_util;

freq_t freq;

void rx_set_freq(double freq_with_offset_kHz, double foff_kHz)
{
    if (foff_kHz != -1) rx_set_freq_offset_kHz(foff_kHz);
    freq.f_kHz = freq_with_offset_kHz;
    freq.f_Hz = (u4_t) (freq.f_kHz * 1e3);
    freq.baseband_kHz = freq_with_offset_kHz - freq.offset_kHz;
    freq.baseband_Hz = (u4_t) (freq.baseband_kHz * 1e3);
}

void rx_set_freq_offset_kHz(double foff_kHz)
{
    freq.isOffset = (foff_kHz != 0);
    freq.offset_kHz = foff_kHz;
    freq.offmax_kHz = foff_kHz + ui_srate_kHz;
    freq.offset_Hz = (u64_t) (foff_kHz * 1e3);
    printf("FOFF foff_kHz=%.2f offset_Hz=%lld\n", foff_kHz, freq.offset_Hz);
}

bool rx_freq_inRange(double freq_kHz)
{
    return (freq_kHz >= freq.offset_kHz && freq_kHz <= freq.offmax_kHz);
}

conn_t *conn_other(conn_t *conn, int type)
{
    conn_t *cother = conn->other;
    if (cother && cother->type == type && cother->rx_channel == conn->rx_channel) {
        return cother;
    }
    return NULL;
}

// CAUTION: returned tstamp must be unique even if called in rapid succession!
u64_t rx_conn_tstamp()
{
    u64_t tstamp = timer_us64();
    
    // very unlikely given use of timer_us64(), but guarantee monotonic
    if (rx_util.last_conn_tstamp >= tstamp)
        tstamp = rx_util.last_conn_tstamp + 1;
    rx_util.last_conn_tstamp = tstamp;
    
    // Mark tstamp space that should skip IP address matching,
    // and also avoid collisions with older tstamp space based on msec TOD (e.g. kiwirecorder).
    tstamp |= NEW_TSTAMP_SPACE;
    return tstamp;
}

void rx_loguser(conn_t *c, logtype_e type)
{
    if (TaskFlags() & CTF_NO_LOG) return;
    
	u4_t now = timer_sec();
	
	if (!log_local_ip && c->isLocal_ip) {
	    if (type == LOG_ARRIVED) {
	        c->last_tune_time = now;
	        c->log_offset_kHz = freq.offset_kHz;
	    }
	    return;
	}
	
	char *s;
	u4_t t = now - c->arrival;
	u4_t sec = t % 60; t /= 60;
	u4_t min = t % 60; t /= 60;
	u4_t hr = t;

	if (type == LOG_ARRIVED) {
        #ifdef OPTION_DENY_APP_FINGERPRINT_CONN
            if (strcmp(c->ident_user, "kiwi_nc.py") == 0) {
                float f_kHz = (float) c->freqHz / kHz + freq.offset_kHz;
                int f = (int) floorf(f_kHz);
                bool freq_trig = (
                    (f >= (2941-10) && f < 3500) ||     // stay outside 80m ham band
                    (f >= (4654-10) && f <= (4687+10)) ||
                    (f >= (5451-10) && f <= (5720+10)) ||
                    (f >= (6529-10) && f <= (6712+10)) ||
                    (f >= (8825-10) && f <= (8977+10)) ||
                    (f >= (10027-10) && f <= (10093+10)) ||
                    (f >= (11184-10) && f <= (11387+10)) ||
                    (f >= (13264-10) && f <= (13351+10)) ||
                    (f >= (15025-10) && f <= (15025+10)) ||
                    (f >= (17901-10) && f <= (17985+10)) ||
                    (f >= (21928-10) && f <= (21997+10))
                );
                if (freq_trig) {
                    clprintf(c, "API: non-Kiwi app fingerprint-3 was denied connection\n");
                    c->kick = true;
                    return;
                }
            }
        #endif

		asprintf(&s, "(ARRIVED)");
		c->last_tune_time = now;
        c->log_offset_kHz = freq.offset_kHz;
		user_arrive(c);
	} else
	if (type == LOG_LEAVING) {
		asprintf(&s, "(%s after %d:%02d:%02d)", c->preempted? "PREEMPTED" : "LEAVING", hr, min, sec);
		user_leaving(c, now - c->arrival);
	} else {
		asprintf(&s, "%d:%02d:%02d%s", hr, min, sec, (type == LOG_UPDATE_NC)? " n/c":"");
	}
	
	const char *mode_s = "";
	if (c->type == STREAM_WATERFALL)
	    mode_s = "WF";      // for WF-only connections (i.e. c->isMaster set)
	else
	    mode_s = rx_enum2mode(c->mode);
	
	// c->log_offset_kHz so LOG_LEAVING freq is same as was shown in LOG_ARRIVED message
	double f_kHz = (double) c->freqHz / kHz + c->log_offset_kHz;
	
	if (type == LOG_ARRIVED || type == LOG_LEAVING) {
		clprintf(c, "%8.2f kHz %3s z%-2d %s%s\"%s\"%s%s%s%s %s\n",
			f_kHz, mode_s, c->zoom, c->ext? c->ext->name : "", c->ext? " ":"",
			c->ident_user? c->ident_user : "(no identity)", c->isUserIP? "":" ", c->isUserIP? "":c->remote_ip,
			c->geo? " ":"", c->geo? c->geo:"", s);
	}

    #ifdef OPTION_LOG_WF_ONLY_UPDATES
        if (c->type == STREAM_WATERFALL && type == LOG_UPDATE) {
            cprintf(c, "%8.2f kHz %3s z%-2d %s%s\"%s\"%s%s%s%s %s\n",
			    f_kHz, mode_s, c->zoom, c->ext? c->ext->name : "", c->ext? " ":"",
                c->ident_user? c->ident_user : "(no identity)", c->isUserIP? "":" ", c->isUserIP? "":c->remote_ip,
                c->geo? " ":"", c->geo? c->geo:"", s);
        }
    #endif

	// we don't do anything with LOG_UPDATE and LOG_UPDATE_NC at present
	kiwi_asfree(s);
}

int rx_chan_free_count(rx_free_count_e flags, int *idx, int *heavy, int *preempt, int *busy)
{
	int i, free_cnt = 0, free_idx = -1, heavy_cnt = 0, preempt_cnt = 0, busy_cnt = 0;
	rx_chan_t *rx;

    // When configuration has a limited number of channels with waterfalls
    // allocate them to non-Kiwi UI users last.
    // Note that we correctly detect the WF-only use of kiwirecorder
    // (e.g. SNR-measuring applications)

    #define RX_CHAN_FREE_COUNT() { \
        rx = &rx_channels[i]; \
        if (rx->busy) { \
            /*printf("rx_chan_free_count rx%d: ext=%p flags=0x%x\n", i, rx->ext, rx->ext? rx->ext->flags : 0xffffffff);*/ \
            if (rx->ext && (rx->ext->flags & EXT_FLAGS_HEAVY)) \
                heavy_cnt++; \
            if (rx->conn && rx->conn->arun_preempt) \
                preempt_cnt++; \
            busy_cnt++; \
        } else { \
            free_cnt++; \
            if (free_idx == -1) free_idx = i; \
        } \
    }

    if (flags != RX_COUNT_ALL && wf_chans != 0 && wf_chans < rx_chans) {
        for (i = wf_chans; i < rx_chans; i++) RX_CHAN_FREE_COUNT();
        if (flags != RX_COUNT_NO_WF_AT_ALL) {
            for (i = 0; i < wf_chans; i++) RX_CHAN_FREE_COUNT();
        }
    } else {
        for (i = 0; i < rx_chans; i++) RX_CHAN_FREE_COUNT();
    }
	
	if (idx != NULL) *idx = free_idx;
	if (heavy != NULL) *heavy = heavy_cnt;
	if (preempt != NULL) *preempt = preempt_cnt;
	if (busy != NULL) *busy = busy_cnt;
	return free_cnt;
}

int rx_chan_no_pwd(pwd_check_e pwd_check)
{
    int chan_no_pwd = cfg_int("chan_no_pwd", NULL, CFG_REQUIRED);
    // adjust if number of rx channels changed due to mode change but chan_no_pwd wasn't adjusted
    if (chan_no_pwd >= rx_chans) chan_no_pwd = rx_chans - 1;

    if (pwd_check == PWD_CHECK_YES) {
        const char *pwd_s = admcfg_string("user_password", NULL, CFG_REQUIRED);
		if (pwd_s == NULL || *pwd_s == '\0') chan_no_pwd = 0;   // ignore setting if no password set
		cfg_string_free(pwd_s);
    }

    return chan_no_pwd;
}

int rx_count_server_conns(conn_count_e type, u4_t flags, conn_t *our_conn)
{
	int users=0, any=0;
	
	conn_t *c = conns;
	for (int i=0; i < N_CONNS; i++, c++) {
	    if (!c->valid)
	        continue;
	    
        // if type == EXTERNAL_ONLY don't count internal connections so e.g. WSPR autorun won't prevent updates
        bool sound = (c->type == STREAM_SOUND && ((type == EXTERNAL_ONLY)? !c->internal_connection : true));
        
        // don't count preemptable internal connections since they can be kicked
        if (sound && (type == INCLUDE_INTERNAL) && (flags & EXCEPT_PREEMPTABLE) && c->arun_preempt)
            sound = false;

	    if (type == TDOA_USERS) {
	        if (sound && kiwi_str_begins_with(c->ident_user, "TDoA_service"))
	            users++;
	    } else
	    if (type == EXT_API_USERS) {
	        if (c->ext_api)
	            users++;
	    } else
	    if (type == LOCAL_OR_PWD_PROTECTED_USERS) {
	        // don't count ourselves if e.g. SND has already connected but rx_count_server_conns() is being called during WF connection
	        if (our_conn && c->other && c->other == our_conn) continue;

	        if (sound && (c->isLocal || c->auth_prot)) {
                //show_conn("LOCAL_OR_PWD_PROTECTED_USERS ", PRINTF_LOG, c);
	            users++;
	        }
	    } else
	    if (type == ADMIN_USERS) {
	        // don't count admin connections awaiting password input (!c->auth)
	        if ((c->type == STREAM_ADMIN || c->type == STREAM_MFG) && c->auth)
                users++;
        } else
	    if (type == ADMIN_CONN) {
	        // don't count admin connections awaiting password input (!c->awaitingPassword)
	        if ((c->type == STREAM_ADMIN || c->type == STREAM_MFG) &&
	            c->auth && c->auth_admin && c->isMaster && !c->awaitingPassword)
                users++;
	    } else {
            if (sound) users++;
            // will return 1 if there are no sound connections but at least one waterfall connection
            if (sound || c->type == STREAM_WATERFALL) any = 1;
        }
	}
	
	return (users? users : any);
}

void rx_server_kick(kick_e kick, int chan)
{
	// kick users off (all or individual channel)
	printf("rx_server_kick %s rx=%d\n", kick_s[kick], chan);
	conn_t *c = conns;
	bool kick_chan_all = (kick == KICK_CHAN && chan == -1);
	int kicked = 1;
	const char *msg;
	
    if (kick == KICK_CHAN && chan != -1) {
        msg = "You were kicked!";
    } else
    if (kick == KICK_USERS) {
        msg = "User connections disabled.";
        kicked = 0;
    } else {
        msg = "Everyone was kicked!";
    }
	
	for (int i=0; i < N_CONNS; i++, c++) {
		if (!c->valid)
			continue;

        bool kick_chan  = (kick == KICK_CHAN && chan == c->rx_channel);
        bool kick_users = ((kick == KICK_USERS || kick_chan_all) && !c->internal_connection);
        bool kick_all   = (kick == KICK_ALL);
        bool kick_admin = (kick == KICK_ADMIN);
        //printf("rx_server_kick consider CONN-%02d rx=%d %s kick_chan=%d kick_users=%d kick_all=%d\n",
        //    i, c->rx_channel, rx_conn_type(c), kick_chan, kick_users, kick_all);
		if (c->type == STREAM_SOUND || c->type == STREAM_WATERFALL) {
		    if (kick_chan || kick_users || kick_all) {
		        send_msg_encoded(c, "MSG", "kiwi_kick", "%d,%s", kicked, msg);
                c->kick = true;
                printf("rx_server_kick KICKING rx=%d %s\n", c->rx_channel, rx_conn_type(c));
            }
		} else
		
		if (c->type == STREAM_EXT) {
		    if (kick_chan || kick_users || kick_all) {
		        send_msg_encoded(c, "MSG", "kiwi_kick", "%d,%s", kicked, msg);
                c->kick = true;
                printf("rx_server_kick KICKING rx=%d EXT %s\n", c->rx_channel, c->ext? c->ext->name : "?");
            }
		} else
		
		if (kick_admin && (c->type == STREAM_ADMIN || c->type == STREAM_MFG)) {
            c->kick = true;
            printf("rx_server_kick KICKING admin [%02d]\n", c->self_idx);
		}
	}
	//printf("rx_server_kick DONE\n");
}

void rx_autorun_clear()
{
    // when one group (wspr, ft8) does a restart reset _all_ eviction counters
    // so future evictions rate will be equal
    memset(rx_util.arun_evictions, 0, sizeof(rx_util.arun_evictions));
}

int rx_autorun_find_victim()
{
    conn_t *c, *lowest_eviction_conn;
    u4_t lowest_eviction_count = (u4_t) -1;
    int lowest_eviction_ch = -1, highest_eviction_ch = -1;
    
    for (int ch = 0; ch < rx_chans; ch++) {
        rx_chan_t *rx = &rx_channels[ch];
        if (!rx->busy) continue;
        c = rx->conn;
        if (!c->arun_preempt) continue;
        int evictions = rx_util.arun_evictions[ch];
        //rcprintf(ch, "AUTORUN: rx_autorun_find_victim %s rx_chan=%d evictions=%d\n",
        //    c->ident_user, ch, evictions);
        if (evictions < lowest_eviction_count) {
            lowest_eviction_count = evictions;
            lowest_eviction_ch = ch;
            lowest_eviction_conn = c;
        }
        if (evictions > highest_eviction_ch) {
            highest_eviction_ch = evictions;
        }
    }
    if (lowest_eviction_ch != -1) {
        //rcprintf(lowest_eviction_ch, "AUTORUN: evicting %s rx_chan=%d evictions=%d|%d\n",
        //    lowest_eviction_conn->ident_user, lowest_eviction_ch, lowest_eviction_count, highest_eviction_ch);
        rx_util.arun_which[lowest_eviction_ch] = ARUN_NONE;
        rx_util.arun_evictions[lowest_eviction_ch]++;
        return lowest_eviction_ch;
    }
    return -1;
}

void rx_autorun_restart_victims(bool initial)
{
    // if any autorun is in the process of a shutdown then skip this victim restart cycle
    if (wspr_c.arun_suspend_restart_victims || ft8_conf.arun_suspend_restart_victims ||
        update_in_progress || backup_in_progress || sd_copy_in_progress) {
        //printf(YELLOW "rx_autorun_restart_victims SUSPENDED" NONL);
        return;
    }
    //printf(GREEN "rx_autorun_restart_victims GO" NONL);
    
    // if autorun on configurations with limited wf chans (e.g. rx8_wf2) never use the wf chans at all
    int free_chans = rx_chan_free_count(RX_COUNT_NO_WF_AT_ALL);
    //printf("rx_autorun_restart_victims: initial=%d free_chans=%d rx_chans=%d wf_chans=%d\n", initial, free_chans, rx_chans, wf_chans);
    if (free_chans == 0) {
        //printf("rx_autorun_restart_victims: no free chans\n");
        return;
    }
    //printf("rx_autorun_restart_victims: free_chans=%d\n", free_chans);

    ft8_autorun_start(initial);
    wspr_autorun_start(initial);
}

void rx_server_send_config(conn_t *conn)
{
	// SECURITY: only send configuration after auth has validated
	assert(conn->auth == true);

	char *json = cfg_get_json(NULL);
	if (json != NULL) {
		send_msg_encoded(conn, "MSG", "load_cfg", "%s", json);

        json = dxcfg_get_json(NULL);
        if (json != NULL) {
            send_msg_encoded(conn, "MSG", "load_dxcfg", "%s", json);
        }

        json = dxcomm_cfg_get_json(NULL);
        if (json != NULL) {
            send_msg_encoded(conn, "MSG", "load_dxcomm_cfg", "%s", json);
        }

		// send admin config ONLY if this is an authenticated connection from the admin/mfg page
		if ((conn->type == STREAM_ADMIN || conn->type == STREAM_MFG) && conn->auth_admin) {
			json = admcfg_get_json(NULL);
			if (json != NULL) {
				send_msg_encoded(conn, "MSG", "load_adm", "%s", json);
			}
		}

        send_msg(conn, false, "MSG cfg_loaded");
	}
}

// Because of the false hash match problem with rx_common_cmd()
// must only do auth after the command string compares.
static bool rx_auth_okay(conn_t *conn)
{
    if (conn->auth_admin == FALSE) {
        lprintf("** attempt to save kiwi config with auth_admin == FALSE! IP %s\n", conn->remote_ip);
        return false;
    }
    
    // To prevent cfg database multi-writer data loss, enforce no admin connections (a source of writes)
    // on any non-admin/mfg (i.e. user) connection cfg save.
    int n;
    if (conn->type != STREAM_ADMIN && conn->type != STREAM_MFG && (n = rx_count_server_conns(ADMIN_USERS)) != 0) {
        //cprintf(conn, "CMD_SAVE_CFG: abort because admin_users=%d\n", n);
        send_msg(conn, false, "MSG no_admin_conns=0");    // tell user their cfg save was rejected
        //rx_server_send_config(conn);    // and reload last saved config to flush bad values
        return false;
    }

    return true;
}

static bool admin_auth_okay(conn_t *conn)
{
    if (conn->type != STREAM_ADMIN && conn->type != STREAM_MFG) {
        lprintf("** attempt to save admin config from non-STREAM_ADMIN! IP %s\n", conn->remote_ip);
        return false;
    }

    if (conn->auth_admin == FALSE) {
        lprintf("** attempt to save admin config with auth_admin == FALSE! IP %s\n", conn->remote_ip);
        return false;
    }
    
    return true;
}

typedef struct {
    const char name[8];
    bool init;
    conn_t *conn;
	char remote_ip[NET_ADDRSTRLEN];
	int remote_port;
	u64_t tstamp;       // msec since 1970
    char *frag_prefix_s, *end_prefix_s;
    int seq;
    char *json;
    int json_slen;
} save_cfg_t;

static save_cfg_t save_cfg = { "cfg" }, save_dxcfg = { "dxcfg" }, save_adm = { "adm" };

//#define SAVE_DBG
#ifdef SAVE_DBG
	#define save_prf(conn, fmt, ...) \
	    clprintf(conn, fmt,  ## __VA_ARGS__);
#else
	#define save_prf(conn, fmt, ...)
#endif

bool save_config(u2_t key, conn_t *conn, char *cmd)
{
    int n, seq;
    char *sb;
    const bool dbug = true;
    save_cfg_t *cfg;
    
    switch (key) {
        case CMD_SAVE_CFG:   cfg = &save_cfg; break;
        case CMD_SAVE_DXCFG: cfg = &save_dxcfg; break;
        case CMD_SAVE_ADM:   cfg = &save_adm; break;
        default: panic("save_config"); break;
    }
    
    if (!cfg->init) {
        asprintf(&cfg->frag_prefix_s, "SET save_%s_frag=", cfg->name);
        asprintf(&cfg->end_prefix_s, "SET save_%s=", cfg->name);
        cfg->init = true;
    }
    
    // CAUTION: Remember that key could be a false hash match for non rx_common_cmd() commands.
    // So as with other rx_common_cmd() code, must do cmd prefix matching before continuing.
    // Does no harm that we essentially have an incorrect cfg based on a bogus key.
    // It just causes the cfg->init code above to be done earlier than it might otherwise.
    bool isFrag = (kiwi_str_begins_with(cmd, cfg->frag_prefix_s));
    bool isEnd  = (kiwi_str_begins_with(cmd, cfg->end_prefix_s));
    if (!isFrag && !isEnd) {
        //clprintf(conn, "save_config: bogus hash match, key=%d cmd=\"%.32s\"\n", key, cmd);
        return false;   // indicate cmd matching failed
    }
    
    if (cfg->conn != conn || strncmp(cfg->remote_ip, conn->remote_ip, NET_ADDRSTRLEN) != 0 ||
        cfg->remote_port != conn->remote_port || cfg->tstamp != conn->tstamp) {
        // persists: init, frag_prefix_s, end_prefix_s
        cfg->seq = 0;
        save_prf(conn, "save_config: %s RESET cfg|conn %d|%d %p|%p IP:%s|%s PORT:%d|%d TSTAMP:0x%llx|0x%llx json %d|%d<%s>\n",
            cfg->name, cfg->conn? cfg->conn->self_idx : -1, conn? conn->self_idx : -1, cfg->conn, conn,
            cfg->remote_ip, conn->remote_ip, cfg->remote_port, conn->remote_port, cfg->tstamp, conn->tstamp,
            cfg->json_slen, kstr_len(cfg->json), kstr_sp(cfg->json));
        kstr_free(cfg->json); cfg->json = NULL; cfg->json_slen = 0;
        cfg->conn = conn;
        kiwi_strncpy(cfg->remote_ip, conn->remote_ip, NET_ADDRSTRLEN);
        cfg->remote_port = conn->remote_port;
        cfg->tstamp = conn->tstamp;
    }
    
    #if 0
        if (key == CMD_SAVE_CFG) {
            clprintf(conn, "CMD_SAVE_CFG key=%d %s \"%s\" \"%s\" \"%.32s\"\n", key, cfg->name, cfg->frag_prefix_s, cfg->end_prefix_s, cmd);
            show_conn("save_config ", PRINTF_REG, conn);
        }
    #endif
    
    // Handle web socket fragmentation by sending in parts which can be reassembled on server side.
    // Due to data getting large after double encoding or web socket buffering limitations on browser.

    if (isFrag) {
        if (key == CMD_SAVE_ADM) {
            if (!admin_auth_okay(conn)) { printf("save_config: BAD AUTH\n"); return true; }
        } else {
            if (!rx_auth_okay(conn)) { printf("save_config: BAD AUTH\n"); return true; }
        }
        
        if ((n = sscanf(cmd, "%*s %*[^=]=%d %ms", &seq, &sb)) != 2) {
            printf("save_config: BAD DECODE n=%d \"%.32s\"\n", n, cmd);
            return true;
        }
        save_prf(conn, "save_config: %s FRAG conn=%p seq=%d sl=%d\n", cfg->name, conn, seq, strlen(sb));
        if (seq < cfg->seq) {
            kiwi_asfree(sb);
            save_prf(conn, "save_config: %s FRAG conn=%p seq=%d < cfg.seq=%d IGNORE DELAYED, OUT-OF-SEQ FRAG\n", cfg->name, conn, seq, cfg->seq);
            return true;
        }
        if (cfg->json != NULL && seq > cfg->seq) {
            kstr_free(cfg->json); cfg->json = NULL; cfg->json_slen = 0;
            save_prf(conn, "save_config: %s FRAG conn=%p seq=%d > cfg.seq=%d NEW SEQ, DISCARD ACCUMULATED FRAGS\n", cfg->name, conn, seq, cfg->seq);
        }
        cfg->seq = seq;     // lock to new seq
        cfg->json = kstr_cat(cfg->json, kstr_wrap(sb), &cfg->json_slen);
        return true;
    }
    
    if (isEnd) {
        if (key == CMD_SAVE_ADM) {
            if (!admin_auth_okay(conn)) return true;
        } else {
            if (!rx_auth_okay(conn)) return true;
        }

        if ((n = sscanf(cmd, "%*s %*[^=]=%d %ms", &seq, &sb)) != 2) {
            printf("save_config: BAD DECODE n=%d \"%.32s\"\n", n, cmd);
            return true;
        }
        save_prf(conn, "save_config: %s END conn=%p seq=%d sl=%d\n", cfg->name, conn, seq, strlen(sb));
        if (seq < cfg->seq) {
            kiwi_asfree(sb);
            save_prf(conn, "save_config: %s END conn=%p seq=%d < cfg.seq=%d IGNORE DELAYED, OUT-OF-SEQ FRAG\n", cfg->name, conn, seq, cfg->seq);
            return true;
        }
        
        // this should only happen in the case of seq N being fragmented, but seq N+1 having no fragments (due to reduced size presumably)
        if (cfg->json != NULL && seq > cfg->seq) {
            kstr_free(cfg->json); cfg->json = NULL; cfg->json_slen = 0;
            save_prf(conn, "save_config: %s END conn=%p seq=%d > cfg.seq=%d NEW SEQ, DISCARD ACCUMULATED FRAGS\n", cfg->name, conn, seq, cfg->seq);
        }
        cfg->seq = seq;     // lock to new seq
        cfg->json = kstr_cat(cfg->json, kstr_wrap(sb), &cfg->json_slen);
         
        // The following comment is no longer true for cfg string params since they are now JSON.stringify() based.
        // This is because the first encoding mentioned is not performed since the JSON.stringify() handles all escaping properly.
        // Doing the double decodeURI below causes no harm in that case.

        // For cfg strings double URI encoding is effectively used since they are stored encoded and
        // another encoding is done for transmission.
        // So decode the transmission encoding with kiwi_str_decode_inplace()
        // and then decode the cfg string encoding with kiwi_str_decode_selective_inplace().
        char *sp = kstr_sp(cfg->json);
        kiwi_str_decode_inplace(sp);
        kiwi_str_decode_selective_inplace(sp);

        switch (key) {
            case CMD_SAVE_CFG:   /* printf("_cfg_save_json save_config:CMD_SAVE_CFG\n"); */ cfg_save_json(sp); break;
            case CMD_SAVE_DXCFG: dxcfg_save_json(sp); break;
            case CMD_SAVE_ADM:   admcfg_save_json(sp); break;
            default: panic("save_config"); break;
        }

        //#define TEST_COW
        #ifdef TEST_COW
            if (key == CMD_SAVE_ADM) {
                cfg_t cow_json = {0};
                cow_json.filename = DIR_CFG "/test.json";
                real_printf(">>> %s\n", cow_json.filename);
                //if (json_init(&cow_json, (char *) "{\"foo\":1138}", "cow_json") == true) {
                if (json_init_file(&cow_json) == true) {
                    real_printf("SAVE %d\n", strlen(sp));
                    sp = strdup(sp);
                    json_save(&cow_json, sp);
                    kiwi_asfree(sp);
                }
                json_release(&cow_json);
            }
        #endif

        // NB: cfg->json = NULL here is extremely important
        kstr_free(cfg->json); cfg->json = NULL; cfg->json_slen = 0;
        
        // get all active user connections to refresh dx list (and updated cfg)
        // when a non dx list change is made by an admin connection (e.g. dx type menu)
        if (key == CMD_SAVE_DXCFG) {
            //printf("CMD_SAVE_DXCFG {cfg,dx}.update_seq++\n");
            cfg_cfg.update_seq++;
            dx.update_seq++;
        }

        update_vars_from_config();      // update C copies of vars

        save_prf(conn, "save_config: %s " RED "COMMIT" NORM " conn=%p seq=%d\n", cfg->name, conn, cfg->seq);
        return true;
    }

    return false;   // always (and only) return false if the cmd matching failed
}

void show_conn(const char *prefix, u4_t printf_type, conn_t *cd)
{
    if (!cd->valid) {
        lprintf("%sCONN not valid\n", prefix);
        return;
    }
    
    char *type_s = (cd->type == STREAM_ADMIN)? (char *) "ADM" : stnprintf(0, "%s", rx_conn_type(cd));
    char *rx_s = (cd->rx_channel == -1)? (char *) "" : stnprintf(1, "rx%d", cd->rx_channel);
    char *other_s = cd->other? stnprintf(2, " +CONN-%02d", cd->other-conns) : (char *) "";
    char *ext_s = (cd->type == STREAM_EXT)? (cd->ext? stnprintf(3, " %s", cd->ext->name) : (char *) " (ext name?)") : (char *) "";
    
    lfprintf(printf_type,
        "%sCONN-%02d %p %3s %-3s %s%s%s%s%s%s%s%s%s isPwd%d tle%d%d sv=%02d KA=%02d/60 KC=%05d mc=%9p %s%s:%d:%016llx%s%s%s%s\n",
        prefix, cd->self_idx, cd, type_s, rx_s,
        cd->auth? "*":"_", cd->auth_kiwi? "K":"_", cd->auth_admin? "A":"_",
        cd->isMaster? "M":"_", cd->arun_preempt? "P":(cd->internal_connection? "I":(cd->ext_api? "E":"_")), cd->ext_api_determined? "D":"_",
        cd->isLocal? "L":(cd->force_notLocal? "F":"_"), cd->auth_prot? "P":"_", cd->awaitingPassword? "A":(cd->kick? "K":"_"),
        cd->isPassword, cd->tlimit_exempt, cd->tlimit_exempt_by_pwd, cd->served,
        cd->keep_alive, cd->keepalive_count, cd->mc, cd->debug? stprintf("dbg=%d ", cd->debug):"",
        cd->remote_ip, cd->remote_port, cd->tstamp,
        other_s, ext_s, cd->stop_data? " STOP_DATA":"", cd->is_locked? " LOCKED":"");

    if (cd->isMaster && cd->arrived)
        lprintf("        user=<%s> isUserIP=%d geo=<%s>\n", cd->ident_user, cd->isUserIP, cd->geo);
}

static void dump_task(void *param)
{
	int i;

    while (1) {
        TaskSleep();
    
        lprintf("\n");
        lprintf("dump --------\n");
        lprintf("rf_attn_dB=%.1f\n", kiwi.rf_attn_dB);
        lprintf("\n");
        
        for (i=0; i < rx_chans; i++) {
            rx_chan_t *rx = &rx_channels[i];
            lprintf("RX%d chan_en=%d data_en=%d busy=%d conn-%2s %2.0f %2.0f %p %d|w %d|r\n",
                i, rx->chan_enabled, rx->data_enabled, rx->busy,
                rx->conn? stprintf("%02d", rx->conn->self_idx) : "",
                //toUnits(audio_bytes[i], 0), toUnits(waterfall_bytes[i], 1),   // %6s
                audio_kbps[i], waterfall_kbps[i],
                rx->conn? rx->conn : 0, rx->wr, rx->rd);
        }
    
        conn_t *cd;
        int nconn = 0;
        for (cd = conns, i=0; cd < &conns[N_CONNS]; cd++, i++) {
            if (cd->valid) nconn++;
        }
        lprintf("\n");
        lprintf("CONNS: used %d/%d is_locked=%d  ______ => *auth, Kiwi, Admin, Master, Internal/Preempt/ExtAPI, DetAPI, Local/ForceNotLocal, ProtAuth, Kicked/AwaitPwd\n",
            nconn, N_CONNS, is_locked);
    
        for (cd = conns, i=0; cd < &conns[N_CONNS]; cd++, i++) {
            if (!cd->valid) continue;
            show_conn("", PRINTF_LOG, cd);
        }
        
        TaskDump(TDUMP_LOG | TDUMP_HIST | PRINTF_LOG);
        lock_dump();
        ip_blacklist_dump(false);
        mt_dump();
        data_pump_dump();
    }
}

static tid_t dump_tid;

void dump()
{
	TaskWakeup(dump_tid);
}

void dump_init()
{
	dump_tid = CreateTask(dump_task, NULL, MAIN_PRIORITY);
}

// can optionally configure SIG_DEBUG to call this debug handler
static void debug_dump_handler(int arg)
{
	lprintf("\n");
	lprintf("SIG_DEBUG: debugging..\n");
	dump();
	sig_arm(SIG_DEBUG, debug_dump_handler);
}

static void dump_info_handler(int arg)
{
    printf("SIGHUP: info.json requested\n");
    char *sb;
    sb = kstr_asprintf(NULL, "echo '{ \"utc\": \"%s\"", utc_ctime_static());
    
    #ifdef USE_GPS
        sb = kstr_asprintf(sb, ", \"gps\": { \"lat\": %.6f, \"lon\": %.6f", gps.sgnLat, gps.sgnLon);

        latLon_t loc;
        loc.lat = gps.sgnLat;
        loc.lon = gps.sgnLon;
        char grid6[LEN_GRID];
        if (latLon_to_grid6(&loc, grid6) == 0) {
            sb = kstr_asprintf(sb, ", \"grid\": \"%.6s\"", grid6);
        }

        sb = kstr_asprintf(sb, ", \"fixes\": %d, \"fixes_min\": %d }", gps.fixes, gps.fixes_min);
    #endif

    sb = kstr_cat(sb, " }' > /root/kiwi.config/info.json");
    non_blocking_cmd_system_child("kiwi.info", kstr_sp(sb), NO_WAIT);
    kstr_free(sb);
	sig_arm(SIGHUP, dump_info_handler);
}

static void debug_exit_backtrace_handler(int arg)
{
    panic("debug_exit_backtrace_handler");
}

void debug_init()
{
    sig_arm(SIG_DEBUG, debug_dump_handler);
    sig_arm(SIGHUP, dump_info_handler);
}

int rx_mode2enum(const char *mode)
{
	for (int i = 0; i < ARRAY_LEN(mode_lc); i++) {
		if (strcasecmp(mode, mode_lc[i]) == 0) return i;
	}
	return NOT_FOUND;
}

const char *rx_enum2mode(int e)
{
	if (e < 0 || e >= ARRAY_LEN(mode_lc)) return NULL;
	return (mode_lc[e]);
}

// Pass result json back to main process via shmem->status_str_large
// since _geo_task runs in context of child_task()'s child process.
// This presumes the returned JSON size is < N_SHMEM_STATUS_STR_LARGE.
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
	
    asprintf(&cmd_p, "curl -Ls --ipv4 \"%s\" 2>&1", geo_host_ip_s);
    //cprintf(conn, "GEOLOC: <%s>\n", cmd_p);
    
    // NB: don't use non_blocking_cmd() here to prevent audio gliches
    int status = non_blocking_cmd_func_forall("kiwi.geo", cmd_p, _geo_task, 0, POLL_MSEC(1000));
    kiwi_asfree(cmd_p);
    int exit_status;
    if (WIFEXITED(status) && (exit_status = WEXITSTATUS(status))) {
        clprintf(conn, "GEOLOC: curl(%d) failed for %s\n", exit_status, geo_host_ip_s);
        return false;
    }
    //cprintf(conn, "GEOLOC: returned <%s>\n", shmem->status_str_large);

	cfg_t cfg_geo;
    if (json_init(&cfg_geo, shmem->status_str_large, "cfg_geo") == false) {
        clprintf(conn, "GEOLOC: JSON parse failed for %s\n", geo_host_ip_s);
	    json_release(&cfg_geo);
        return false;
    }
    
	char *geo = NULL;
    char *country_name = (char *) json_string(&cfg_geo, country_s, NULL, CFG_OPTIONAL);
    char *region_name = (char *) json_string(&cfg_geo, region_s, NULL, CFG_OPTIONAL);
    char *city = (char *) json_string(&cfg_geo, "city", NULL, CFG_OPTIONAL);
    //cprintf(conn, "GEOLOC: %s=<%s> %s=<%s> city=<%s> \n", country_s, country_name, region_s, region_name, city);
    
    char *country;
	if (country_name && strcmp(country_name, "United States") == 0 && region_name && *region_name) {
		country = kstr_cat(region_name, ", USA");
	} else {
		country = kstr_cat(country_name, NULL);     // possible that country_name == NULL
	}

    if (cfg_true("show_geo_city")) {
        if (city && *city) {
            geo = kstr_cat(geo, city);
            geo = kstr_cat(geo, ", ");
        }
        geo = kstr_cat(geo, country);   // NB: country freed here
    } else {
        geo = kstr_cat(country, NULL);  // NB: country freed here
    }

    //clprintf(conn, "GEOLOC: %s <%s>\n", geo_host_ip_s, kstr_sp(geo));
	kiwi_asfree(conn->geo);
    conn->geo = strdup(kstr_sp(geo));
    kstr_free(geo);

    json_string_free(&cfg_geo, country_name);
    json_string_free(&cfg_geo, region_name);
    json_string_free(&cfg_geo, city);
    
	json_release(&cfg_geo);
    return true;
}

void geoloc_task(void *param)
{
	conn_t *conn = (conn_t *) param;
	char *ip = (isLocal_ip(conn->remote_ip) && net.pub_valid)? net.ip_pub : conn->remote_ip;

    char *geo_host_ip_s;
    u4_t i = timer_sec();   // mix it up a bit
    int retry = 0;
    bool okay = false;
    do {
        i = (i+1) % 3;
        if (i == 0) {
            asprintf(&geo_host_ip_s, "https://ipapi.co/%s/json", ip);
            okay = geoloc_json(conn, geo_host_ip_s, "country_name", "region");
        } else
        if (i == 1) {
            asprintf(&geo_host_ip_s, "https://get.geojs.io/v1/ip/geo/%s.json", ip);
            okay = geoloc_json(conn, geo_host_ip_s, "country", "region");
        } else
        if (i == 2) {
            asprintf(&geo_host_ip_s, "http://ip-api.com/json/%s?fields=49177", ip);
            okay = geoloc_json(conn, geo_host_ip_s, "country", "regionName");
        }
        kiwi_asfree(geo_host_ip_s);
        retry++;
    } while (!okay && retry < 10);
    if (okay)
        clprintf(conn, "GEOLOC: %s sent no geoloc info, we got \"%s\" from geo host #%d\n",
            conn->remote_ip, conn->geo, i);
    else
        clprintf(conn, "GEOLOC: for %s FAILED for all geo servers\n", ip);
}

char *rx_users(bool isAdmin)
{
    int i, n;
    rx_chan_t *rx;
    bool show_geo = isAdmin || cfg_true("show_geo");
    bool show_user = isAdmin || cfg_true("show_user");
    bool need_comma = false;
    char *sb = (char *) "[", *sb2;
    
    for (rx = rx_channels, i=0; rx < &rx_channels[rx_chans]; rx++, i++) {
        n = 0;
        if (rx->busy) {
            // rx->conn is always STREAM_SOUND for regular SND+WF connections and
            // always STREAM_WATERFALL for WF-only connections
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

                //printf("rx%d ext_api=%d ident_user=<%s>\n", i, c->ext_api, c->ident_user);
                char *user;
                if (c->isUserIP || !c->ident_user)
                    user = NULL;
                else
                if (!show_user && !c->ext_api)
                    user = strdup("(private)");
                else
                    user = kiwi_str_encode(c->ident_user);
                
                bool show = show_geo || c->internal_connection;
                char *geo = show? (c->geo? kiwi_str_encode(c->geo) : NULL) : NULL;
                char *ext = ext_users[i].ext? kiwi_str_encode((char *) ext_users[i].ext->name) : NULL;
                const char *ip = isAdmin? c->remote_ip : "";
                asprintf(&sb2, "%s{\"i\":%d,\"n\":\"%s\",\"g\":\"%s\",\"f\":%d,"
                    "\"m\":\"%s\",\"z\":%d,"
                    "\"wf\":%d,"
                    "\"t\":\"%d:%02d:%02d\",\"rt\":%d,\"rn\":%d,\"rs\":\"%d:%02d:%02d\","
                    "\"e\":\"%s\",\"a\":\"%s\",\"c\":%.1f,\"fo\":%.3f,\"ca\":%d,"
                    "\"nc\":%d,\"ns\":%d}",
                    need_comma? ",":"", i, user? user:"", geo? geo:"", c->freqHz,
                    rx_enum2mode(c->mode), c->zoom,
                    (c->type == STREAM_WATERFALL)? 1:0,
                    hr, min, sec, rtype, rn, r_hr, r_min, r_sec,
                    ext? ext:"", ip,
                    #ifdef USE_SDR
                        wdsp_SAM_carrier(i),
                    #else
                        0.,
                    #endif
                    freq.offset_kHz, rx->n_camp,
                    extint.notify_chan, extint.notify_seq);
                kiwi_ifree(user, "rx_users user");
                kiwi_ifree(geo, "rx_users geo");
                kiwi_ifree(ext, "rx_users ext");
                n = 1;
            }
        }
        if (n == 0) {
            asprintf(&sb2, "%s{\"i\":%d}", need_comma? ",":"", i);
        }
        sb = kstr_cat(sb, kstr_wrap(sb2));
        need_comma = true;
    }

    //printf("rx_users: foff %.3f\n", freq.offset_kHz);
    sb = kstr_cat(sb, "]\n");
    return sb;
}

void rx_send_config(int rx_chan)
{
    char *sb;
    asprintf(&sb, "{\"r\":%d,\"g\":%d,\"s\":%d,\"pu\":\"%s\",\"pe\":%d,\"pv\":\"%s\",\"pi\":%d,\"n\":%d,\"m\":\"%s\",\"v1\":%d,\"v2\":%d,\"d1\":%d,\"d2\":%d}",
        rx_chans, gps_chans, net.serno, net.ip_pub, net.port_ext, net.ip_pvt, net.port, net.nm_bits, net.mac, version_maj, version_min, debian_maj, debian_min);
    snd_send_msg(rx_chan, SM_NO_DEBUG, "MSG config_cb=%s", sb);
    kiwi_asfree(sb);
}

int dB_wire_to_dBm(int db_value)
{
    // (u1_t) 255..55 => 0 .. -200 dBm
    if (db_value < 0) db_value = 0;
	if (db_value > 255) db_value = 255;
	int dBm = -(255 - db_value);
	return (dBm + waterfall_cal);
}

// fast, but approximate, dB = 10 * log10(x)
//
// log10f is exactly log2(x)/log2(10.0f)
// This is a fast approximation to log2():
// log2f_approx = C[0]*F*F*F + C[1]*F*F + C[2]*F + C[3] + E
// see: openaudio.blogspot.com/2017/02/faster-log10-and-pow.html
float dB_fast(float x)
{
    float log2f_approx, F;
    if (x == 0) return -300;    // clamped to -300 dB
    int E;
    F = frexpf(fabsf(x), &E);
    log2f_approx = 1.23149591368684f;
    log2f_approx *= F;
    log2f_approx += -4.11852516267426f;
    log2f_approx *= F;
    log2f_approx += 6.02197014179219f;
    log2f_approx *= F;
    log2f_approx += -3.13396450166353f;
    log2f_approx += E;
    #define LOG2OF10_TIMES10 3.010299956639812f
    return (log2f_approx * LOG2OF10_TIMES10);
}


// schedule SNR measurements

int snr_meas_interval_hrs, snr_all, snr_HF;
int ant_connected = 1;
bool snr_local_time;
SNR_meas_t SNR_meas_data[SNR_MEAS_MAX];
static int dB_raw[WF_WIDTH];

int SNR_calc(SNR_meas_t *meas, int meas_type, int f_lo, int f_hi)
{
    static int dB[WF_WIDTH];
    int i, rv = 0, len = 0, masked = 0;
    int b_lo = f_lo - freq.offset_kHz, b_hi = f_hi - freq.offset_kHz;
    int start = (float) WF_WIDTH * b_lo / ui_srate_kHz;
    int stop  = (float) WF_WIDTH * b_hi / ui_srate_kHz;
    //printf("b_lo=%d b_hi=%d fo=%d s/s=%d/%d\n", b_lo, b_hi, (int) freq.offset_kHz, start, stop);

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
        SNR_data_t *data = &meas->data[meas_type];
        data->f_lo = f_lo; data->f_hi = f_hi;
        data->pct_95 = 95;
        data->pct_50 = median_i(dB, len, &data->pct_95);    // does int qsort on dB array
        data->min = dB[0]; data->max = dB[len-1];
        data->snr = data->pct_95 - data->pct_50;
        rv = data->snr;
        
        // Antenna disconnect detector
        // Rules based on observed cases. Avoiding false-positives is very important.
        // Disconnected if:
        // 1) freq.offset_kHz == 0 (via meas_type == SNR_MEAS_HF),
        //      i.e. no V/UHF transverter setups that might be inherently quiet when no signals.
        // 2) HF snr <= 3
        // 3) Not greater than 2 peaks stronger than -100 dBm
        if (meas_type == SNR_MEAS_HF) {
            int peaks = 0;
            for (i = 0; i < len; i++) {
                if (dB[i] >= -100) peaks++;
            }
            int _ant_connected = (data->snr <= 3 && peaks <= 2)? 0:1;
            if (ant_connected != _ant_connected) {
            
                // wakeup the registration process (if any) so that the change in
                // antenna disconnection shows up on rx.kiwisdr.com as soon as possible
                bool woke = wakeup_reg_kiwisdr_com(WAKEUP_REG);
                ant_connected = _ant_connected;
                printf("SNR_calc-%d:%s ant_connected=%d\n",
                    meas_type, woke? " WAKEUP_REGISTRATION" : "", ant_connected);
            }
        }
        
        printf("SNR_calc-%d: [%d,%d] noise(50%%)=%d signal(95%%)=%d snr=%d range=%d|%d\n",
            meas_type, data->min, data->max, data->pct_50, data->pct_95,
            data->snr, data->pct_95 - data->min, data->max - data->min);
    }
    
    return rv;
}

int SNR_meas_tid;

void SNR_meas(void *param)
{
    int i, j, n, len;
    static internal_conn_t iconn;
	bool regular_wakeup = false;    // run once after restart
    TaskSleepSec(20);               // long enough for autoruns to start first
    
    //#define SNR_MEAS_SELECT WF_SELECT_1FPS
    //#define SNR_MEAS_SPEED WF_SPEED_1FPS
    //#define SNR_MEAS_NAVGS 64
    #define SNR_MEAS_SELECT WF_SELECT_FAST
    #define SNR_MEAS_SPEED WF_SPEED_FAST
    #define SNR_MEAS_NAVGS 32
    //printf("SNR_meas SNR_MEAS_SELECT/SPEED=%d/%d\n", SNR_MEAS_SELECT, SNR_MEAS_SPEED);

    do {
        static int meas_idx;
	
        for (int loop = 0; loop == 0 && (snr_meas_interval_hrs || !regular_wakeup); loop++) {   // so break can be used below
            if (internal_conn_setup(ICONN_WS_WF, &iconn, 0, PORT_BASE_INTERNAL_SNR, WS_FL_PREEMPT_AUTORUN | WS_FL_NO_LOG,
                NULL, 0, 0, 0,
                "SNR-measure", "internal%20task", "SNR",
                WF_ZOOM_MIN, 15000, -110, -10, SNR_MEAS_SELECT, WF_COMP_OFF) == false) {
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
            
            memset(dB_raw, 0, sizeof(dB_raw));
    
            nbuf_t *nb = NULL;
            bool early_exit = false;
            int nsamps;
            for (nsamps = 0; nsamps < SNR_MEAS_NAVGS && !early_exit;) {
                do {
                    if (nb) web_to_app_done(iconn.cwf, nb);
                    n = web_to_app(iconn.cwf, &nb, /* internal_connection */ true);
                    if (n == 0) continue;
                    if (n == -1) {
                        early_exit = true;
                        break;
                    }
                    wf_pkt_t *wf = (wf_pkt_t *) nb->buf;
                    //printf("SNR_meas nsamps=%d rcv=%d <%.3s>\n", nsamps, n, wf->id4);
                    if (strncmp(wf->id4, "W/F", 3) != 0) continue;
                    for (j = 0; j < WF_WIDTH; j++) {
                        dB_raw[j] += dB_wire_to_dBm(wf->un.buf[j]);
                    }
                    nsamps++;
                } while (n);
                TaskSleepMsec(900 / SNR_MEAS_SPEED);
            }
            //printf("SNR_meas DONE nsamps=%d\n", nsamps);
            if (nsamps) {
                for (i = 0; i < WF_WIDTH; i++) {
                        dB_raw[i] /= nsamps;
                }
            }
            
            int f_lo = freq.offset_kHz, f_hi = freq.offmax_kHz;
            
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
            kiwi.snr_initial_meas_done = true;  // CAUTION: this must always get set else ant switch hangs
            internal_conn_shutdown(&iconn);
            
            #ifdef OPTION_HONEY_POT
                snr_all = snr_HF = 55;
            #endif

            if (!regular_wakeup) {
                printf("SNR_meas: admin measure now %d:%d\n", snr_all, freq.offset_kHz? -1 : snr_HF);
                snd_send_msg(SM_SND_ADM_ALL, SM_NO_DEBUG,
                    "MSG snr_stats=%d,%d", snr_all, freq.offset_kHz? -1 : snr_HF);
            }
        }
        
        //TaskSleepSec(60);
        // if disabled check again in an hour to see if re-enabled
        u64_t hrs = snr_meas_interval_hrs? snr_meas_interval_hrs : 1;
        
        // a !regular_wakeup occurs where there is an on-demand request, such as the admin control tab
        // "measure SNR now" button or the /snr?meas control URL
        regular_wakeup = (bool) FROM_VOID_PARAM(TaskSleepSec(hrs * 60 * 60));
        //TaskSleepSec(60);
    } while (1);
}

void on_GPS_solution()
{
    //printf("on_GPS_solution: WSPR_auto=%d FT8_auto=%d haveLat=%d\n", wspr_c.GPS_update_grid, ft8_conf.GPS_update_grid, (gps.StatLat != 0));
    if (gps.StatLat) {
        if (wspr_c.GPS_update_grid || ft8_conf.GPS_update_grid) {
            latLon_t loc;
            loc.lat = gps.sgnLat;
            loc.lon = gps.sgnLon;
            if (latLon_to_grid6(&loc, kiwi.grid6) == 0) {   // see also gps/stat.cpp::TEST_MM
                if (wspr_c.GPS_update_grid)
                    wspr_update_rgrid(kiwi.grid6);
                if (ft8_conf.GPS_update_grid)
                    ft8_update_rgrid(kiwi.grid6);
            }
        }
        
        int res= cfg_true("GPS_update_web_hires")? 6:2;
        kiwi_snprintf_buf(kiwi.latlon_s, "(%.*f, %.*f)", res, gps.sgnLat, res, gps.sgnLon);
        //printf("on_GPS_solution: %s %s\n", kiwi.grid6, kiwi.latlon_s);
    }
}
