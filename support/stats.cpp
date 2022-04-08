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

#include "types.h"
#include "config.h"
#include "options.h"
#include "kiwi.h"
#include "rx.h"
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


int current_nusers;
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
		if (c->freqHz != c->last_freqHz || c->mode != c->last_mode || c->zoom != c->last_zoom) {
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
                c->ident_user? c->ident_user : "(no identity)", c->isUserIP? "":" ", c->isUserIP? "":c->remote_ip,
                c->geo? " ":"", c->geo? c->geo:"");
            c->kick = true;
		}
		
		nusers++;
	}
	current_nusers = nusers;

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
			sprintf(buf, "cpu%d", ncpu);
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

#if defined(CPU_AM5729) || defined(CPU_BCM2837)
	    reply = read_file_string_reply("/sys/devices/system/cpu/cpufreq/policy0/cpuinfo_cur_freq");
		sscanf(kstr_sp(reply), "%d", &cpufreq_kHz);
		kstr_free(reply);

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

	if (min != last_min) {
		schedule_update(min);
		last_min = min;
	}
	
	spi_stats();
}

static void called_every_second()
{
	int i;
	conn_t *c;
	u4_t now = timer_sec();
	int ch;
    rx_chan_t *rx;

    for (rx = rx_channels, ch = 0; rx < &rx_channels[rx_chans]; rx++, ch++) {
        if (!rx->busy) continue;
		c = rx->conn;
		if (c == NULL || !c->valid || c->ext_api_determined) continue;
		int served = web_served(c);
		
		#if 0
            cprintf(c, "API: rx%d arrival=%d served=%d type=%s ext_api%d isLocal%d internal%d\n",
                ch, now - c->arrival, served, rx_streams[c->type].uri,
                c->ext_api, c->isLocal, c->internal_connection);
        #endif
        
		if (c->isLocal || c->internal_connection || c->awaitingPassword ||
		    (c->type != STREAM_SOUND && c->type != STREAM_WATERFALL)) {
            c->ext_api = false;
            c->ext_api_determined = true;
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
		    cprintf(c, "API: decided connection is OKAY (%d)\n", served);
		    continue;
		}
		
		c->ext_api = c->ext_api_determined = true;
		web_served_clear_cache(c);
		cprintf(c, "API: decided connection is non-Kiwi app (%d)\n", served);
		//dump();

        // ext_api_nchans, if exceeded, overrides tdoa_nchans
        if (!c->kick && !c->awaitingPassword) {
            int ext_api_ch = cfg_int("ext_api_nchans", NULL, CFG_REQUIRED);
            if (ext_api_ch == -1) ext_api_ch = rx_chans;      // has never been set
            int ext_api_users = rx_count_server_conns(EXT_API_USERS);
            cprintf(c, "API: ext_api_users=%d >? ext_api_ch=%d %s\n", ext_api_users, ext_api_ch,
                (ext_api_users > ext_api_ch)? "T(DENY)":"F(OKAY)");
            if (ext_api_users > ext_api_ch) {
                clprintf(c, "API: non-Kiwi app was denied connection: %d/%d %s \"%s\"\n",
                    ext_api_users, ext_api_ch, c->remote_ip, c->ident_user);
                send_msg(c, SM_NO_DEBUG, "MSG too_busy=%d", ext_api_ch);
                c->kick = true;
            }
        }

        // Can only distinguish the TDoA service at the time the kiwirecorder identifies itself.
        // If a match and the limit is exceeded then kick the connection off immediately.
        // This identification is typically sent right after initial connection is made.
        if (!c->kick && c->ident_user && kiwi_str_begins_with(c->ident_user, "TDoA_service")) {
            int tdoa_ch = cfg_int("tdoa_nchans", NULL, CFG_REQUIRED);
            if (tdoa_ch == -1) tdoa_ch = rx_chans;      // has never been set
            int tdoa_users = rx_count_server_conns(TDOA_USERS);
            cprintf(c, "TDoA_service tdoa_users=%d >? tdoa_ch=%d %s\n", tdoa_users, tdoa_ch, (tdoa_users > tdoa_ch)? "T":"F");
            if (tdoa_users > tdoa_ch) {
                send_msg(c, SM_NO_DEBUG, "MSG too_busy=%d", tdoa_ch);
                c->kick = true;
            }
        }
	}
}

void stat_task(void *param)
{
	u64_t stats_deadline = timer_us64() + SEC_TO_USEC(1);
	u64_t secs = 0;
	
	while (TRUE) {
	    called_every_second();

		if ((secs % STATS_INTERVAL_SECS) == 0) {
			if (do_sdr) {
				webserver_collect_print_stats(print_stats & STATS_TASK);
				if (!do_gps) nbuf_stat();
			}

            cull_zombies();
		}

		NextTask("stat task");
		eeprom_test();

		if ((print_stats & STATS_TASK) && !(print_stats & STATS_GPS)) {
			if (!background_mode) {
				if (do_sdr) {
					lprintf("ECPU %4.1f%%, cmds %d/%d, malloc %d, ",
						ecpu_use(), ecpu_cmds, ecpu_tcmds, kiwi_malloc_stat());
					ecpu_cmds = ecpu_tcmds = 0;
				}
				//TaskDump(PRINTF_REG);
				TaskDump(PRINTF_LOG);
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
