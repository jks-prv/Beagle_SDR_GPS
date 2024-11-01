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

// Copyright (c) 2014-2016 John Seamons, ZL4VO/KF6VO

#include "types.h"
#include "config.h"
#include "options.h"
#include "kiwi.h"
#include "stats.h"
#include "rx.h"
#include "rx_util.h"
#include "mem.h"
#include "misc.h"
#include "web.h"
#include "spi.h"
#include "gps.h"
#include "coroutines.h"
#include "debug.h"
#include "printf.h"
#include "non_block.h"
#include "eeprom.h"
#include "shmem.h"
#include "ant_switch.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

static int last_hour = -1, last_min = -1;

// called periodically (currently every 10 seconds)
static void webserver_collect_print_stats(int print)
{
	int i, nusers=0;
	conn_t *c;
	
	// print / log connections
    rx_chan_t *rx;
    for (rx = rx_channels; rx < &rx_channels[rx_chans]; rx++) {
        if (!rx->busy) continue;
		c = rx->conn;
		if (c == NULL || !c->valid) continue;
        //assert(c->type == STREAM_SOUND || c->type == STREAM_WATERFALL);
		
		u4_t now = timer_sec();
		bool freqChange = (c->freqChangeLatch && (c->freqHz != c->last_freqHz));
		if (freqChange || c->mode != c->last_mode || c->zoom != c->last_zoom) {
			if (print) rx_loguser(c, LOG_UPDATE);
			
            #ifdef OPTION_LOG_WF_ONLY_UPDATES
			    if (c->type == STREAM_WATERFALL && c->last_log_time != 0) {
                    rx_loguser(c, LOG_UPDATE);
                }
            #endif
            
			c->last_tune_time = now;
            c->last_freqHz = c->freqHz;
            c->last_mode = c->mode;
            c->last_zoom = c->zoom;
            c->last_log_time = now;
		} else {
			u4_t diff = now - c->last_log_time;
			if (diff > MINUTES_TO_SEC(5)) {
				if (print) rx_loguser(c, LOG_UPDATE_NC);
			}
			
			//cprintf(c, "TO_MINS=%d exempt=%d\n", inactivity_timeout_mins, c->tlimit_exempt);
			if (inactivity_timeout_mins != 0 && !c->tlimit_exempt) {
			    if (c->last_tune_time == 0) c->last_tune_time = now;    // got here before first set in rx_loguser()
				diff = now - c->last_tune_time;
			    //cprintf(c, "diff=%d now=%d last=%d TO_SECS=%d\n", diff, now, c->last_tune_time,
			    //    MINUTES_TO_SEC(inactivity_timeout_mins));
				if (diff > MINUTES_TO_SEC(inactivity_timeout_mins)) {
                    cprintf(c, "TLIMIT-INACTIVE for %s\n", c->remote_ip);
					send_msg(c, false, "MSG inactivity_timeout=%d", inactivity_timeout_mins);
					c->inactivity_timeout = true;
				}
			}
		}
		
		if (ip_limit_mins && !c->tlimit_exempt) {
		    if (c->tlimit_zombie) {
                // After the browser displays the "time limit reached" error panel the connection
                // hangs open until the watchdog goes off. So have to flag as a zombie to keep the
                // database from getting incorrectly updated.
                //cprintf(c, "TLIMIT-IP zombie %s\n", c->remote_ip);
		    } else {
                int ipl_cur_secs = json_default_int(&cfg_ipl, c->remote_ip, 0, NULL);
                ipl_cur_secs += STATS_INTERVAL_SECS;
                //cprintf(c, "TLIMIT-IP setting database sec:%d for %s\n", ipl_cur_secs, c->remote_ip);
                json_set_int(&cfg_ipl, c->remote_ip, ipl_cur_secs);
                if (ipl_cur_secs >= MINUTES_TO_SEC(ip_limit_mins)) {
                    cprintf(c, "TLIMIT-IP connected 24hr LIMIT REACHED cur:%d >= lim:%d for %s\n",
                        SEC_TO_MINUTES(ipl_cur_secs), ip_limit_mins, c->remote_ip);
                    send_msg_encoded(c, "MSG", "ip_limit", "%d,%s", ip_limit_mins, c->remote_ip);
                    c->inactivity_timeout = true;
                    c->tlimit_zombie = true;
                }
            }
		}
		
		#ifdef OPTION_SERVER_GEOLOC
            if (!c->geo && !c->try_geoloc && (now - c->arrival) > 10) {
                //clprintf(c, "GEOLOC: %s sent no geoloc info, trying from here\n", c->remote_ip);
                CreateTask(geoloc_task, (void *) c, SERVICES_PRIORITY);
                c->try_geoloc = true;
            }
		#endif
		
		// SND and/or WF connections that have failed to follow API
		#define NO_API_TIME 20
		bool both_no_api = (!c->snd_cmd_recv_ok && !c->wf_cmd_recv_ok);
		if (c->auth && both_no_api && (now - c->arrival) >= NO_API_TIME) {
            clprintf(c, "\"%s\"%s%s%s%s incomplete connection kicked\n",
                kiwi_nonEmptyStr(c->ident_user)? c->ident_user : "(no identity)", c->isUserIP? "":" ", c->isUserIP? "":c->remote_ip,
                c->geo? " ":"", c->geo? c->geo:"");
            c->kick = true;
		}
		
		nusers++;
	}
	kiwi.current_nusers = nusers;

	// construct cpu stats response
	#define NCPU 4
	int usi[3][NCPU], del_usi[3][NCPU];
	static int last_usi[3][NCPU];

	u4_t now = timer_ms();
	static u4_t last_now;
	float secs = (float)(now - last_now) / 1000;
	last_now = now;

	char *reply = read_file_string_reply("/proc/stat");
	
	if (reply != NULL) {
		int ncpu;
		char *cpu_ptr = kstr_sp(reply);
		for (ncpu = 0; ncpu < NCPU; ncpu++) {
			char buf[10];
			kiwi_snprintf_buf(buf, "cpu%d", ncpu);
			cpu_ptr = strstr(cpu_ptr, buf);
			if (cpu_ptr == nullptr)
				break;
			sscanf(cpu_ptr + 4, " %d %*d %d %d", &usi[0][ncpu], &usi[1][ncpu], &usi[2][ncpu]);
		}

		for (i = 0; i < ncpu; i++) {
            del_usi[0][i] = lroundf((float)(usi[0][i] - last_usi[0][i]) / secs);
            del_usi[1][i] = lroundf((float)(usi[1][i] - last_usi[1][i]) / secs);
            del_usi[2][i] = lroundf((float)(usi[2][i] - last_usi[2][i]) / secs);
            //printf("CPU%d %.1fs u=%d%% s=%d%% i=%d%%\n", i, secs, del_usi[0][i], del_usi[1][i], del_usi[2][i]);
        }
		
		kstr_free(reply);
	    int cpufreq_kHz = 1000000, temp_deg_mC = 0;

        #if defined(CPU_AM5729) || defined(CPU_TDA4VM) || defined(CPU_BCM2837)
	        #ifdef CPU_TDA4VM
	            cpufreq_kHz = 2000000;  // FIXME: /sys/devices/system/cpu/cpufreq/ is empty currently
	        #else
                reply = read_file_string_reply("/sys/devices/system/cpu/cpufreq/policy0/cpuinfo_cur_freq");
                sscanf(kstr_sp(reply), "%d", &cpufreq_kHz);
                kstr_free(reply);
            #endif

            reply = read_file_string_reply("/sys/class/thermal/thermal_zone0/temp");
            sscanf(kstr_sp(reply), "%d", &temp_deg_mC);
            kstr_free(reply);
        #endif

		// ecpu_use() below can thread block, so cpu_stats_buf must be properly set NULL for reading thread
		kstr_t *ks;
		if (cpu_stats_buf) {
			ks = cpu_stats_buf;
			cpu_stats_buf = NULL;
			kstr_free(ks);
		}
		ks = kstr_asprintf(NULL, "\"ct\":%d,\"ce\":%.0f,\"cf\":%d,\"cc\":%.0f,",
			timer_sec(), ecpu_use(), cpufreq_kHz / 1000, (float) temp_deg_mC / 1000);

		ks = kstr_cat(ks, kstr_list_int("\"cu\":[", "%d", "],", &del_usi[0][0], ncpu));
		ks = kstr_cat(ks, kstr_list_int("\"cs\":[", "%d", "],", &del_usi[1][0], ncpu));
		ks = kstr_cat(ks, kstr_list_int("\"ci\":[", "%d", "]", &del_usi[2][0], ncpu));

		for (i = 0; i < ncpu; i++) {
            last_usi[0][i] = usi[0][i];
            last_usi[1][i] = usi[1][i];
            last_usi[2][i] = usi[2][i];
        }

		cpu_stats_buf = ks;
	}

	// collect network i/o stats
	static const float k = 1.0/1000.0/10.0;		// kbytes/sec every 10 secs
	
	for (i=0; i <= rx_chans; i++) {     // [rx_chans] is total of all channels
        audio_kbps[i] = audio_bytes[i]*k;
        waterfall_kbps[i] = waterfall_bytes[i]*k;
		waterfall_fps[i] = waterfall_frames[i]/10.0;
		audio_bytes[i] = waterfall_bytes[i] = waterfall_frames[i] = 0;
	}
	http_kbps = http_bytes*k;
	http_bytes = 0;

	// on the hour: report number of connected users & schedule updates
	int hour, min; utc_hour_min_sec(&hour, &min);
	
	if (hour != last_hour) {
		if (print) lprintf("(%d %s)\n", nusers, (nusers==1)? "user":"users");
		last_hour = hour;
	}

    // call every minute at the top of the minute
	if (min != last_min) {
		schedule_update(min);
		last_min = min;
	}
	
    rx_autorun_restart_victims(false);
	spi_stats();
}

//#ifdef CAT_TASK_CHILD
static int CAT_task_tid;

static void called_every_second()
{
	int i;
	conn_t *c;
	u4_t now = timer_sec();
	int ch;
    rx_chan_t *rx;

    for (rx = rx_channels, ch = 0; rx < &rx_channels[rx_chans]; rx++, ch++) {


        // CAT tuned frequency reporting

        if (!rx->busy) {
            if (kiwi.CAT_ch == ch) { kiwi.CAT_ch = -1; return; }
            continue;
        }
		c = rx->conn;
		if (c == NULL || !c->valid) {
            if (kiwi.CAT_ch == ch) { kiwi.CAT_ch = -1; return; }
            continue;
        }
		
        if (kiwi.CAT_fd > 0 && !c->internal_connection && (kiwi.CAT_ch < 0 || kiwi.CAT_ch == ch)) {
            //printf("CAT_CH=%d ch=%d\n", kiwi.CAT_ch, ch);
            if (kiwi.CAT_ch < 0) kiwi.CAT_ch = ch;      // lock to the first channel encountered

            if (c->freqHz != shmem->CAT_last_freqHz) {
                shmem->CAT_last_freqHz = c->freqHz;
                #ifdef CAT_TASK_CHILD
                #else
                    TaskWakeup(CAT_task_tid);
                #endif
            }
        }


        // External API detection and limiting
        
		if (c->ext_api_determined) continue;
		int served = web_served(c);
		
		#ifdef OPTION_HONEY_POT
            cprintf(c, "API: rx%d arrival=%d served=%d type=%s ext_api%d isLocal%d internal%d\n",
                ch, now - c->arrival, served, rx_conn_type(c),
                c->ext_api, c->isLocal, c->internal_connection);
        #endif
        
		if (c->isLocal || c->internal_connection || c->awaitingPassword ||
		    (c->type != STREAM_SOUND && c->type != STREAM_WATERFALL)) {
            c->ext_api = false;
            c->ext_api_determined = true;
		    web_served_clear_cache(c);
		    //cprintf(c, "API: connection is exempt\n");
            continue;
		}

        // can't be too short because of slow network connections delaying the served counter
		#define EXT_API_DECISION_SECS 10
		if ((now - c->arrival) < EXT_API_DECISION_SECS) continue;

        // can't be too many due to browser caching (lowest limit currently seen with iOS caching)
		#define EXT_API_DECISION_SERVED 3
		if (served >= EXT_API_DECISION_SERVED) {
            c->ext_api = false;
            c->ext_api_determined = true;
		    web_served_clear_cache(c);
		    cprintf(c, "API: decided connection is OKAY (served=%d) %s\n",
		        served, c->browser? c->browser : "");
		    continue;
		}
		
		c->ext_api = c->ext_api_determined = true;
		web_served_clear_cache(c);
		cprintf(c, "API: decided connection is non-Kiwi app (served=%d) %s\n",
		    served, c->browser? c->browser : "");
		//dump();

        // ext_api_nchans, if exceeded, overrides tdoa_nchans
        if (!c->kick && !c->awaitingPassword) {
            int ext_api_ch = cfg_int("ext_api_nchans", NULL, CFG_REQUIRED);
            if (ext_api_ch == -1) ext_api_ch = rx_chans;      // has never been set
            int ext_api_users = rx_count_server_conns(EXT_API_USERS);
            cprintf(c, "API: ext_api_users=%d >? ext_api_ch=%d %s\n", ext_api_users, ext_api_ch,
                (ext_api_users > ext_api_ch)? "T(DENY)":"F(OKAY)");
            bool kick = false;
            if (ext_api_users > ext_api_ch) {
                clprintf(c, "API: non-Kiwi app was denied connection: %d/%d %s \"%s\"\n",
                    ext_api_users, ext_api_ch, c->remote_ip, kiwi_nonEmptyStr(c->ident_user)? c->ident_user : "(no identity)");
                kick = true;
            } else {
                #ifdef OPTION_DENY_APP_FINGERPRINT_CONN
                    float f_kHz = (float) c->freqHz / kHz + freq.offset_kHz;
                    // 58.59 175.781
                    float floor_kHz = floorf(f_kHz);
                    bool freq_trig = (floor_kHz == 58.0f || floor_kHz == 175.0f);
                    bool hasDelimiter = (c->ident_user != NULL && strpbrk(c->ident_user, "-_.`,/+~") != NULL);
                    //bool trig = (c->type == STREAM_WATERFALL && freq_trig && hasDelimiter && c->zoom == 8);
                    bool trig = (c->type == STREAM_WATERFALL && freq_trig && c->zoom == 8);
                    clprintf(c, "API: TRIG=%s %s(T%d) f_kHz=%.3f freq_trig=%d hasDelimiter=%d z=%d\n",
                        trig? "T":"F", rx_conn_type(c), c->type, f_kHz, freq_trig, hasDelimiter, c->zoom);
                    if (trig) {
                        clprintf(c, "API: non-Kiwi app fingerprint was denied connection\n");
                        kick = true;
                    } else {
                        if (kiwi_str_begins_with(c->ident_user, "TDoA_service")) {
                            int f = (int) floor_kHz;
                            freq_trig = (
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
                                clprintf(c, "API: non-Kiwi app fingerprint-2 was denied connection\n");
                                c->kick = true;
                            }
                        }
                    }
                #endif
            }
            
            if (kick) {
                send_msg(c, SM_NO_DEBUG, "MSG too_busy=%d", ext_api_ch);
                c->kick = true;
            }
        }


        // TDoA connection detection and limiting
        //
        // Can only distinguish the TDoA service at the time the kiwirecorder identifies itself.
        // If a match and the limit is exceeded then kick the connection off immediately.
        // This identification is typically sent right after initial connection is made.

        if (!c->kick && kiwi_str_begins_with(c->ident_user, "TDoA_service")) {
            int tdoa_ch = cfg_int("tdoa_nchans", NULL, CFG_REQUIRED);
            if (tdoa_ch == -1) tdoa_ch = rx_chans;      // has never been set
            int tdoa_users = rx_count_server_conns(TDOA_USERS);
            cprintf(c, "TDoA_service tdoa_users=%d >? tdoa_ch=%d %s\n", tdoa_users, tdoa_ch, (tdoa_users > tdoa_ch)? "T(DENY)":"F(OKAY)");
            if (tdoa_users > tdoa_ch) {
                send_msg(c, SM_NO_DEBUG, "MSG too_busy=%d", tdoa_ch);
                c->kick = true;
            }
        }
	}
}



////////////////////////////////
// user list
////////////////////////////////

//#define USER_LIST_DBG
#ifdef USER_LIST_DBG
	#define ul_prf(fmt, ...) \
		printf(fmt, ## __VA_ARGS__)
#else
	#define ul_prf(fmt, ...)
#endif
    
//#define USER_LIST_DBG2
#ifdef USER_LIST_DBG2
	#define ul_prf2(fmt, ...) \
		printf(fmt, ## __VA_ARGS__)
#else
	#define ul_prf2(fmt, ...)
#endif
    
typedef struct {
    u4_t idx;
    #define UL_FL 1
    u2_t flags;
    u2_t connected;
	char *ident;
	list_t *entry_base;
	u4_t total_time;
} user_log_t;

#define N_USER_LOG_ALLOC 8
static list_t *user_base;
u4_t user_tx_idx;

typedef struct {
    u2_t flags;
    u2_t connected;
    //u4_t ip4;
	char *ip;
	char *geoloc;
	u4_t connect_time;
} user_entry_t;

#define N_USER_ENTRY_ALLOC 1

bool user_ident_cmp(const void *elem1, void *elem2)
{
	const char *s1 = (const char *) elem1;
	user_log_t *ul = (user_log_t *) FROM_VOID_PARAM(elem2);
    ul_prf2("user_strcmp <%s> <%s> ", s1, ul->ident);
	return (strcmp(s1, ul->ident) == 0);
}

/*
bool user_ip4_cmp(const void *elem1, void *elem2)
{
	u4_t ip4_1 = (u4_t) FROM_VOID_PARAM(elem1);
	user_entry_t *entry = (user_entry_t *) FROM_VOID_PARAM(elem2);
    ul_prf2("user_ip4_cmp %s %s ", inet4_h2s(ip4_1, 0), inet4_h2s(entry->ip4, 1));
	return (ip4_1 == entry->ip4);
}
*/

bool user_ip_cmp(const void *elem1, void *elem2)
{
	const char *ip_1 = (const char *) elem1;
	user_entry_t *entry = (user_entry_t *) FROM_VOID_PARAM(elem2);
    ul_prf2("user_ip_cmp %s %s ", ip_1, entry->ip);
	return (strcmp(ip_1, entry->ip) == 0);
}

void user_list_clear()
{
    int i, j;
    list_t *ub = user_base;
    if (!ub) return;
    user_base = NULL;
    
    user_log_t *ul;
    for (i = 0, ul = (user_log_t *) ub->items; i < ub->n_items; ul++, i++) {
        kiwi_ifree(ul->ident, "user_list_clear");
        list_t *entry_base = ul->entry_base;
        for (j = 0; j < entry_base->n_items; j++) {
            user_entry_t *entry = (user_entry_t *) FROM_VOID_PARAM(item_ptr(entry_base, j));
            kiwi_ifree(entry->ip, "user_list_clear");
            kiwi_ifree(entry->geoloc, "user_list_clear");
        }
        list_free(entry_base);
    }
    list_free(ub);

    user_base = list_init("user_base", sizeof(user_log_t), N_USER_LOG_ALLOC);
}

void user_arrive(conn_t *c)
{
    int i, j;
    static bool init;
    if (c->internal_connection || (init && !user_base)) return;
    
    if (!init) {
        #ifdef USER_LIST_DBG
            printf_highlight(0, "user");
        #endif
        user_base = list_init("user_base", sizeof(user_log_t), N_USER_LOG_ALLOC);
        init = true;
        #if 0
            conn_t cx;
            memset(&cx, 0, sizeof(conn_t));
            for (i = 0; i < 128; i++) {
                asprintf(&(cx.ident_user), "ident_%d", i);
                kiwi_snprintf_buf(cx.remote_ip, "%d.%d.%d.%d", i, i+1, i+2, i>>1);
                asprintf(&(cx.geo), "geo_%d", i);
                user_arrive(&cx);
                kiwi_asfree(cx.ident_user);
                kiwi_asfree(cx.geo);
            }
        #endif
    }
    
    const char *ident = (!c->isUserIP && kiwi_nonEmptyStr(c->ident_user))? c->ident_user : "(no identity)";
    bool isNew;
    
    i = item_find_grow(user_base, TO_VOID_PARAM(ident), user_ident_cmp, &isNew);

    user_log_t *ul;
    if (isNew) {
        // new ident
        ul = (user_log_t *) FROM_VOID_PARAM(item_ptr(user_base, i));
        ul->idx = i;
        ul->ident = strdup(ident);
        ul->entry_base = list_init("entry_base", sizeof(user_entry_t), N_USER_ENTRY_ALLOC);
    } else {
        // existing ident
        ul = (user_log_t *) FROM_VOID_PARAM(item_ptr(user_base, i));
    }
    ul->connected++;
    ul_prf("user %s #%d %s <%s>\n", isNew? "NEW" : "EXISTING", i, c->remote_ip, ul->ident);
    list_t *entry_base = ul->entry_base;

    #if 0
        bool err;
        u4_t ip4 = inet4_d2h(c->remote_ip, &err);
        if (err) {
            printf("user_arrive NOT IPv4 <%s>\n", c->remote_ip);
            ip4 = 0;
        }
    
        j = item_find_grow(entry_base, TO_VOID_PARAM(ip4), user_ip4_cmp, &isNew);
    #else
        j = item_find_grow(entry_base, TO_VOID_PARAM(c->remote_ip), user_ip_cmp, &isNew);
    #endif
    
    user_entry_t *entry;
    if (isNew) {
        // new entry
        entry = (user_entry_t *) FROM_VOID_PARAM(item_ptr(entry_base, j));
        //entry->ip4 = ip4;
        entry->ip = strdup(c->remote_ip);
        entry->geoloc = strdup(c->geo? c->geo : "");
    } else {
        // existing entry
        //entry = (user_entry_t *) FROM_VOID_PARAM(item_ptr(entry_base, j));
    }
    
    #ifdef USER_LIST_DBG
        user_dump();
    #endif
}

void user_leaving(conn_t *c, u4_t connected_secs)
{
    const char *ident = (!c->isUserIP && kiwi_nonEmptyStr(c->ident_user))? c->ident_user : "(no identity)";
    bool found;
    if (c->internal_connection || !user_base) return;
    
    int i = item_find(user_base, TO_VOID_PARAM(ident), user_ident_cmp, &found);
    if (!found) return;
    user_log_t *ul = (user_log_t *) FROM_VOID_PARAM(item_ptr(user_base, i));
    ul->connected--;

    list_t *entry_base = ul->entry_base;
    ul->total_time += connected_secs;

    #if 0
        bool err;
        u4_t ip4 = inet4_d2h(c->remote_ip, &err);
        if (err) {
            printf("user_leaving NOT IPv4 <%s>\n", c->remote_ip);
            ip4 = 0;
        }
        int j = item_find(entry_base, TO_VOID_PARAM(ip4), user_ip4_cmp, &found);
    #else
        int j = item_find(entry_base, TO_VOID_PARAM(c->remote_ip), user_ip_cmp, &found);
    #endif
    
    if (found) {
        user_entry_t *entry = (user_entry_t *) FROM_VOID_PARAM(item_ptr(entry_base, j));
        entry->connect_time += connected_secs;
    }

    ul_prf("user LEAVING #%d %d|%d secs %s\n", i, connected_secs, ul->total_time, ul->ident);
}

kstr_t *user_list()
{
    int idx, j;

    kstr_t *sb = NULL;
    user_log_t *ul;
    bool comma = false;

    ul_prf("START idx=%d\n", user_tx_idx);
    sb = (kstr_t *) "[";
    for (idx = user_tx_idx; kstr_len(sb) <= 1024; idx++) {
        ul = user_base? ((user_log_t *) FROM_VOID_PARAM(item_ptr(user_base, idx))) : NULL;
        if (ul == NULL)
            break;
        sb = kstr_asprintf(sb, "%s{\"s\":%d,\"i\":\"%s\",\"t\":%d,\"a\":[", comma? ",":"",
            idx, ul->ident, ul->total_time);
            bool comma2 = false;
            list_t *entry_base = ul->entry_base;
            for (j = 0; j < entry_base->n_items; j++) {
                user_entry_t *entry = (user_entry_t *) FROM_VOID_PARAM(item_ptr(entry_base, j));
                sb = kstr_asprintf(sb, "%s{\"ip\":\"%s\",\"g\":\"%s\",\"t\":%d}", comma2? ",":"",
                    //inet4_h2s(entry->ip4), entry->geoloc? entry->geoloc : "(no location)", entry->connect_time);
                    entry->ip, entry->geoloc? entry->geoloc : "(no location)", entry->connect_time);
                //printf("CHECK IDENT: %s <%s>\n", entry->ip, kiwi_str_ASCII_static(ul->ident));
                comma2 = true;
            }
        sb = kstr_cat(sb, "]}");
        comma = true;
    }
    if (ul == NULL) {
        sb = kstr_asprintf(sb, "%s{\"end\":1}", comma? ",":"");
        user_tx_idx = 0;
        ul_prf2("DONE idx=%d\n", idx);
    } else {
        user_tx_idx = idx;
        ul_prf2("BUFFER FULL idx=%d\n", idx);
    }
    sb = kstr_cat(sb, "]");
    ul_prf("%s\n", kstr_sp(sb));
    return sb;
}

void user_dump()
{
    int i, j;
    if (!user_base) return;
    real_printf("user LIST n=%d cur_nel=%d\n", user_base->n_items, user_base->cur_nel);

    user_log_t *ul;
    for (i = 0, ul = (user_log_t *) user_base->items; i < user_base->n_items; ul++, i++) {
        real_printf("user LIST #%d %d Tsecs <%s>\n", i, ul->total_time, ul->ident);
        list_t *entry_base = ul->entry_base;
        for (j = 0; j < entry_base->n_items; j++) {
            user_entry_t *entry = (user_entry_t *) FROM_VOID_PARAM(item_ptr(entry_base, j));
            real_printf("   %s %s %d secs\n",
            //inet4_h2s(entry->ip4), entry->geoloc? entry->geoloc : "(no location)", entry->connect_time);
            entry->ip, entry->geoloc? entry->geoloc : "(no location)", entry->connect_time);
        }
    }
}


////////////////////////////////
// CAT interface
////////////////////////////////

static void CAT_write_tty(void *param)
{
    int CAT_fd = (int) FROM_VOID_PARAM(param);
    bool loop = false;
    
    #ifdef CAT_TASK_CHILD
        set_cpu_affinity(1);
        loop = true;
    #endif
    
    do {
        static int last_freqHz;
        if (last_freqHz != shmem->CAT_last_freqHz) {
            char *s;
            //asprintf(&s, "IF%011d     +000000 000%1d%1d00001 ;\n", shmem->CAT_last_freqHz, /* mode */ 0, /* vfo */ 0);
            asprintf(&s, "FA%011d;", shmem->CAT_last_freqHz);
            //real_printf("CAT %s", s); fflush(stdout);
            write(CAT_fd, s, strlen(s));
            fsync(CAT_fd);
            free(s);
            last_freqHz = shmem->CAT_last_freqHz;
        }
        
        if (loop) kiwi_msleep(900);
    } while (loop);
}

void CAT_task(void *param)
{
    int CAT_fd = (int) FROM_VOID_PARAM(param);

    #ifdef CAT_TASK_CHILD
        child_task("kiwi.CAT", CAT_write_tty, NO_WAIT, param);
    #else
        while (1) {
            TaskSleep();
            CAT_write_tty(TO_VOID_PARAM(CAT_fd));
        }
    #endif
}

void stat_task(void *param)
{
	u64_t stats_deadline = timer_us64() + SEC_TO_USEC(1);
	u4_t secs = 0;      // 136 year overflow at 1 Hz update

    bool err;
    int baud = cfg_int("CAT_baud", &err, CFG_OPTIONAL);
    if (!err && baud > 0) {
        bool isUSB = true;
        kiwi.CAT_fd = open("/dev/ttyUSB0", O_RDWR | O_NONBLOCK);
        if (kiwi.CAT_fd < 0) {
            isUSB = false;
            kiwi.CAT_fd = open("/dev/ttyACM0", O_RDWR | O_NONBLOCK);
        }

        if (kiwi.CAT_fd >= 0) {
            kiwi.CAT_ch = -1;
            char *s;
            const int baud_i[] = { 0, 115200 };
            asprintf(&s, "stty -F /dev/tty%s0 %d", isUSB? "USB" : "ACM", baud_i[baud]);
            system(s);
            free(s);
            printf("CAT /dev/tty%s0 %d baud\n", isUSB? "USB" : "ACM", baud_i[baud]);
            //if (isUSB) system("stty -a -F /dev/ttyUSB0"); else system("stty -a -F /dev/ttyACM0");
            CAT_task_tid = CreateTask(CAT_task, TO_VOID_PARAM(kiwi.CAT_fd), CAT_PRIORITY);
        }
    }

	while (TRUE) {
	    called_every_second();

		if ((secs % STATS_INTERVAL_SECS) == 0) {
			if (do_sdr) {
				webserver_collect_print_stats(print_stats & STATS_TASK);
				if (!do_gps) nbuf_stat();
	            ant_switch_poll_10s();
			}

            cull_zombies();
		}

		NextTask("stat task");
		eeprom_test();

		if ((print_stats & STATS_TASK) && !(print_stats & STATS_GPS)) {
			if (!background_mode) {
				if (do_sdr) {
					printf("ECPU %4.1f%%, cmds %d/%d, malloc #%d|hi:%d|%s",
						ecpu_use(), ecpu_cmds, ecpu_tcmds, mem.nmt, mem.hiwat, toUnits(mem.size));
					ecpu_cmds = ecpu_tcmds = 0;
				}
				//printf(", "); TaskDump(PRINTF_REG);
				printf("\n");
			}
		}

		// update on a regular interval
		u64_t now_us = timer_us64();
		s64_t diff = stats_deadline - now_us;
		if (diff > 0) TaskSleepUsec(diff);
		stats_deadline += SEC_TO_USEC(1);
		secs++;
	}
}
