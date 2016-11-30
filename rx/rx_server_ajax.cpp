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
#include "kiwi.h"
#include "misc.h"
#include "printf.h"
#include "timer.h"
#include "web.h"
#include "peri.h"
#include "spi.h"
#include "gps.h"
#include "cfg.h"
#include "dx.h"
#include "coroutines.h"
#include "data_pump.h"
#include "ext_int.h"
#include "net.h"

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <sched.h>
#include <math.h>
#include <signal.h>
#include <fftw3.h>

#define	NSTATS_BUF	255
static char stats_buf[NSTATS_BUF+1];

volatile float audio_kbps, waterfall_kbps, waterfall_fps[RX_CHANS+1], http_kbps;
volatile int audio_bytes, waterfall_bytes, waterfall_frames[RX_CHANS+1], http_bytes;

static int current_nusers;

// process non-websocket connections
char *rx_server_ajax(struct mg_connection *mc, char *buf, size_t *size)
{
	int i, j, n;
	const char *s;
	stream_t *st;
	char *op = buf, *oc = buf, *lc;
	int rem = NREQ_BUF;
	
	for (st=streams; st->uri; st++) {
		if (strcmp(mc->uri, st->uri) == 0)
			break;
	}

	if (!st->uri) return NULL;

	if ((down || update_in_progress) &&
		(st->type != STREAM_PWD && st->type != STREAM_USERS && st->type != STREAM_SDR_HU))
		return NULL;

	//printf("rx_server_ajax: uri=<%s> qs=<%s>\n", mc->uri, mc->query_string);
	
	if (mc->query_string == NULL
		&& st->type != STREAM_SDR_HU
		&& st->type != STREAM_DISCOVERY
		&& st->type != STREAM_PHOTO
		) {
		lprintf("rx_server_ajax: missing query string! uri=<%s>\n", mc->uri);
		*size = snprintf(op, rem, "kiwi_server_error(\"missing query string\");");
		return buf;
	}
	
	*size = 0;
	buf[0]=0;
	
	switch (st->type) {
	
	case STREAM_PHOTO:
		char vname[64], fname[64];
		const char *data;
		int data_len, rc;
		rc = 0;
		printf("PHOTO UPLOAD REQUESTED from %s, %p len=%d\n",
			mc->remote_ip, mc->content, mc->content_len);
		mg_parse_multipart(mc->content, mc->content_len,
			vname, sizeof(vname), fname, sizeof(fname), &data, &data_len);
		
		if (data_len < 2*M) {
			FILE *fp;
			scallz("fopen photo", (fp = fopen(DIR_CFG "/photo.upload.tmp", "w")));
			scall("fwrite photo", (n = fwrite(data, 1, data_len, fp)));
			fclose(fp);
			
			// do some server-side checking
			char reply[256];
			int status;
			n = non_blocking_cmd("file " DIR_CFG "/photo.upload.tmp" , reply, sizeof(reply), &status);
			if (n > 0) {
				if (strstr(reply, "image data") == 0)
					rc = 1;
			} else {
				rc = 2;
			}
		} else {
			rc = 3;
		}
		
		// only clobber the old file if the checks pass
		if (rc == 0)
			system("mv " DIR_CFG "/photo.upload.tmp " DIR_CFG "/photo.upload");
		
		printf("%p %d \"%s\" rc=%d\n", data, data_len, fname, rc);
		n = snprintf(oc, rem, "webpage_photo_uploaded(%d);", rc);
		if (!rem || rem < n) { *oc = 0; } else { oc += n; rem -= n; }
		*size = oc-op;
		break;

	// used by kiwisdr.com/scan -- the KiwiSDR auto-discovery scanner
	case STREAM_DISCOVERY:
		n = snprintf(oc, rem, "%d %s %s %d %d %s",
			ddns.serno, ddns.ip_pub, ddns.ip_pvt, ddns.port, ddns.nm_bits, ddns.mac);
		if (!rem || rem < n) { *oc = 0; } else { oc += n; rem -= n; }
		*size = oc-op;
		printf("DISCOVERY REQUESTED from %s: <%s>\n", mc->remote_ip, buf);
		break;

	case STREAM_SDR_HU:
		//if (cfg_bool("sdr_hu_register", NULL, CFG_OPTIONAL) != true) return NULL;
		static time_t avatar_ctime;
		// the avatar file is in the in-memory store, so it's not going to be changing after server start
		if (avatar_ctime == 0) time(&avatar_ctime);
		const char *s1, *s2, *s3, *s4, *s5, *s6, *gps_loc;
		int gps_default;
		
		// if location hasn't been changed from the default, put us in Antarctica to be noticed
		s4 = cfg_string("rx_gps", NULL, CFG_OPTIONAL);
		gps_default= (strcmp(s4, "(-37.631120, 176.172210)") == 0);
		gps_loc = gps_default? "(-69.0, 90.0)" : s4;
		
		// append location to name if none of the keywords in location appear in name
		s1 = cfg_string("rx_name", NULL, CFG_OPTIONAL);
		char *name;
		name = strdup(s1);
		if (s1) cfg_string_free(s1);
		s5 = cfg_string("rx_location", NULL, CFG_OPTIONAL);
		if (name && s5) {
			#define NKWDS 8
			char *kwds[NKWDS], *loc;
			loc = strdup(s5);
			n = kiwi_split((char *) loc, ",;-:/()[]{}<>| \t\n", kwds, NKWDS);
			for (i=0; i < n; i++) {
				//printf("KW%d: <%s>\n", i, kwds[i]);
				if (strcasestr(name, kwds[i]))
					break;
			}
			free(loc);
			if (i == n) {
				char *name2;
				asprintf(&name2, "%s | %s", name, s5);
				free(name);
				name = name2;
				//printf("KW <%s>\n", name);
			}
		}

		n = snprintf(oc, rem, "status=active\nname=%s\nsdr_hw=%s v%d.%d%s\nop_email=%s\nbands=0-%.0f\nusers=%d\nusers_max=%d\navatar_ctime=%ld\ngps=%s\nasl=%d\nloc=%s\nsw_version=%s%d.%d\nantenna=%s\n",
			name,
			(s2 = cfg_string("rx_device", NULL, CFG_OPTIONAL)), VERSION_MAJ, VERSION_MIN,
			gps_default? " [default location set]" : "",
			(s3 = cfg_string("admin_email", NULL, CFG_OPTIONAL)),
			ui_srate, current_nusers, RX_CHANS, avatar_ctime, gps_loc,
			cfg_int("rx_asl", NULL, CFG_OPTIONAL),
			s5,
			"KiwiSDR_v", VERSION_MAJ, VERSION_MIN,
			(s6 = cfg_string("rx_antenna", NULL, CFG_OPTIONAL)));

		if (name) free(name);
		if (s2) cfg_string_free(s2);
		if (s3) cfg_string_free(s3);
		if (s4) cfg_string_free(s4);
		if (s5) cfg_string_free(s5);
		if (s6) cfg_string_free(s6);

		if (!rem || rem < n) { *oc = 0; } else { oc += n; rem -= n; }
		*size = oc-op;
		//printf("SDR.HU STATUS REQUESTED from %s: <%s>\n", mc->remote_ip, buf);
		break;

	case STREAM_USERS:
		rx_chan_t *rx;
		int underruns, seq_errors;
		underruns = seq_errors = 0;
		for (rx = rx_chan, i=0; rx < &rx_chan[RX_CHANS]; rx++, i++) {
			n = 0;
			if (rx->busy) {
				conn_t *c = rx->conn;
				if (c && c->valid && c->arrived && c->user != NULL) {
					assert(c->type == STREAM_SOUND);
					u4_t now = timer_sec();
					u4_t t = now - c->arrival;
					u4_t sec = t % 60; t /= 60;
					u4_t min = t % 60; t /= 60;
					u4_t hr = t;
					
					// SECURITY
					// Must construct the 'user' and 'geo' string arguments with double quotes since
					// single quotes are valid content.
					// FIXME: use mg_url_encode() instead
					n = snprintf(oc, rem, "user(%d,\"%s\",\"%s\",%d,\"%s\",%d,\"%d:%02d:%02d\",\"%s\");",
						// if no user name don't show IP address in user list (but continue to show in log file)
						i, c->isUserIP? "":c->user, c->geo, c->freqHz,
						enum2str(c->mode, mode_s, ARRAY_LEN(mode_s)), c->zoom,
						hr, min, sec, ext_users[i].ext? ext_users[i].ext->name : "");
					
					underruns += c->audio_underrun;
					seq_errors += c->sequence_errors;
				}
			}
			if (n == 0) n = snprintf(oc, rem, "user(%d);", i);
			if (!rem || rem < n) { *oc = 0; break; } else { oc += n; rem -= n; }
		}
		
		// statistics
		// changed e.g. "stats=" to "st=" so some browser ABP filters don't remove XHR!
		int stats, config, update, ch;
		stats = config = update = ch = 0;
		n = sscanf(mc->query_string, "st=%d&co=%d&up=%d&ch=%d", &stats, &config, &update, &ch);
		//printf("USR n=%d stats=%d config=%d update=%d ch=%d <%s>\n", n, stats, config, update, ch, mc->query_string);

		// FIXME: remove at some point
		// handle lingering connections using previous protocol!
		if (n != 4) {
			stats = config = update = ch = 0;
			n = sscanf(mc->query_string, "stats=%d&config=%d&update=%d&ch=%d", &stats, &config, &update, &ch);
		}
		
		lc = oc;	// skip entire response if we run out of room
		for (; stats || update; ) {	// hack so we can use 'break' statements below
			n = strlen(stats_buf);
			strcpy(oc, stats_buf);
			if (!rem || rem < n) { oc = lc; *oc = 0; break; } else { oc += n; rem -= n; }

			float sum_kbps = audio_kbps + waterfall_kbps + http_kbps;
			n = snprintf(oc, rem, "ajax_audio_stats(%.0f, %.0f, %.0f, %.0f, %.0f, %.0f);",
				audio_kbps, waterfall_kbps, waterfall_fps[ch], waterfall_fps[RX_CHANS], http_kbps, sum_kbps);
			if (!rem || rem < n) { *lc = 0; break; } else { oc += n; rem -= n; }

			n = snprintf(oc, rem, "ajax_msg_gps(%d, %d, %d, %d, %.6f, %d);",
				gps.acquiring, gps.tracking, gps.good, gps.fixes, adc_clock/1000000, gps.adc_clk_corr);
			if (!rem || rem < n) { *lc = 0; break; } else { oc += n; rem -= n; }

			if (config) {
				n = snprintf(oc, rem, "ajax_msg_config(%d, %d, %d, '%s', %d, '%s', %d, '%s', %d, %d);",
					RX_CHANS, GPS_CHANS, ddns.serno, ddns.ip_pub, ddns.port, ddns.ip_pvt, ddns.nm_bits, ddns.mac, VERSION_MAJ, VERSION_MIN);
				if (!rem || rem < n) { oc = lc; *oc = 0; break; } else { oc += n; rem -= n; }
			}
			
			if (update) {
				n = snprintf(oc, rem, "ajax_msg_update(%d, %d, %d, %d, %d, %d, %d, %d, '%s', '%s');",
					update_pending, update_in_progress, RX_CHANS, GPS_CHANS, VERSION_MAJ, VERSION_MIN, pending_maj, pending_min, __DATE__, __TIME__);
				if (!rem || rem < n) { oc = lc; *oc = 0; break; } else { oc += n; rem -= n; }
			}
			
			extern int audio_dropped;
			n = snprintf(oc, rem, "ajax_admin_stats(%d, %d, %d);",
				audio_dropped, underruns, seq_errors);
			if (!rem || rem < n) { *lc = 0; break; } else { oc += n; rem -= n; }

			stats = update = 0;	// only run loop once
		}
		
		*size = oc-op;
		assert(*size != 0);
		break;

#define DX_SPACING_ZOOM_THRESHOLD	5
#define DX_SPACING_THRESHOLD_PX		10
		dx_t *dp, *ldp, *upd;

	case STREAM_DX_UPD:
		float freq;
		int gid, mkr_off, flags, new_len;
		char text[256+1], notes[256+1];
		flags = 0;
		text[0] = notes[0] = '\0';
		n = sscanf(mc->query_string, "g=%d&f=%f&o=%d&m=%d&i=%256[^&]&n=%256[^&]", &gid, &freq, &mkr_off, &flags, text, notes);
		printf("DX_UPD #%d %8.2f 0x%x <%s> <%s>\n", gid, freq, flags, text, notes);

		// possible for text and/or notes to be empty, hence n==4 okay
		if (n != 2 && n != 4 && n != 5 && n != 6) {
			printf("STREAM_DX_UPD n=%d\n", n);
			break;
		}
		
		dx_t *dxp;
		if (gid >= -1 && gid < dx.len) {
			if (gid != -1 && freq == -1) {
				// delete entry by forcing to top of list, then decreasing size by one before save
				printf("DX_UPD delete entry #%d\n", gid);
				dxp = &dx.list[gid];
				dxp->freq = 999999;
				new_len = dx.len - 1;
			} else {
				if (gid == -1) {
					// new entry: add to end of list (in hidden slot), then sort will insert it properly
					printf("DX_UPD adding new entry\n");
					assert(dx.hidden_used == false);		// FIXME need better serialization
					dxp = &dx.list[dx.len];
					dx.hidden_used = true;
					dx.len++;
					new_len = dx.len;
				} else {
					// modify entry
					printf("DX_UPD modify entry #%d\n", gid);
					dxp = &dx.list[gid];
					new_len = dx.len;
				}
				dxp->freq = freq;
				dxp->offset = mkr_off;
				dxp->flags = flags;
				dxp->ident = strdup(text);		// can't use kiwi_strdup because free() must be used
				dxp->notes = strdup(notes);
			}
		} else {
			printf("STREAM_DX_UPD: gid %d >= dx.len %d ?\n", gid, dx.len);
		}
		
		qsort(dx.list, dx.len, sizeof(dx_t), qsort_floatcomp);
		printf("DX_UPD after qsort dx.len %d new_len %d top elem f=%.2f\n",
			dx.len, new_len, dx.list[dx.len-1].freq);
		dx.len = new_len;
		dx_save_as_json();		// FIXME need better serialization
		dx_reload();

		n = snprintf(oc, rem, "dx_update();");		// get client to request updated dx list
		if (!rem || rem < n) { *oc = 0; } else { oc += n; rem -= n; }
		*size = oc-op;
		break;

	case STREAM_DX:
		float min, max;
		int zoom, width;
		n = sscanf(mc->query_string, "min=%f&max=%f&zoom=%d&width=%d", &min, &max, &zoom, &width);
		if (n != 4) break;
		float bw;
		bw = max - min;
		static bool first = true;
		static int dx_lastx;
		dx_lastx = 0;
		time_t t; time(&t);
		
		n = snprintf(oc, rem, "dx(-1,%ld);", t);		// reset appending
		if (!rem || rem < n) { *oc = 0; } else { oc += n; rem -= n; }

		for (dp = dx.list, i=j=0; i < dx.len; dp++, i++) {
			float freq;
			freq = dp->freq;
			if (freq < min) continue;
			if (freq > max) break;
			
			// reduce dx label clutter
			if (zoom <= DX_SPACING_ZOOM_THRESHOLD) {
				int x = ((dp->freq - min) / bw) * width;
				int diff = x - dx_lastx;
				//printf("DX spacing %d %d %d %s\n", dx_lastx, x, diff, dp->text);
				if (!first && diff < DX_SPACING_THRESHOLD_PX) continue;
				dx_lastx = x;
				first = false;
			}
			
			float f = dp->freq + (dp->offset / 1000.0);
			n = snprintf(oc, rem, "dx(%d,%.3f,%.0f,%d,\'%s\'%s%s%s);", i, f, dp->offset, dp->flags, dp->ident,
				dp->notes? ",\'":"", dp->notes? dp->notes:"", dp->notes? "\'":"");
			//printf("dx(%d,%.3f,%.0f,%d,\'%s\'%s%s%s)\n", i, f, dp->offset, dp->flags, dp->ident,
			//	dp->notes? ",\'":"", dp->notes? dp->notes:"", dp->notes? "\'":"");
			if (!rem || rem < n) {
				*oc = 0;
				printf("STREAM_DX: buffer overflow %d min=%f max=%f z=%d w=%d\n", j, min, max, zoom, width);
				break;
			} else {
				oc += n; rem -= n; j++;
			}
		}
		
		//printf("STREAM_DX: %d <%s>\n", j, op);
		*size = oc-op;
		assert(*size != 0);
		break;

	case STREAM_PWD:
		char cb[32], type[32], pwd_buf[256], *pwd, *cp;
		const char *cfg_pwd;
		int cfg_auto_login;
		int badp;
		cb[0]=0; type[0]=0; pwd_buf[0]=0;
		cp = (char*) mc->query_string;
		
		//printf("STREAM_PWD: <%s>\n", mc->query_string);
		int i, sl;
		sl = strlen(cp);
		for (i=0; i < sl; i++) { if (*cp == '&') *cp = ' '; cp++; }
		i = sscanf(mc->query_string, "cb=%s type=%s pwd=%255s", cb, type, pwd_buf);
		if (i != 3) break;
		pwd = &pwd_buf[1];	// skip leading 'x'
		//printf("PWD %s pwd \"%s\" from %s\n", type, pwd, mc->remote_ip);
		
		bool is_local, allow, is_admin_mfg;
		is_local = allow = false;
		is_admin_mfg = (strcmp(type, "admin") == 0 || strcmp(type, "mfg") == 0);

		is_local = isLocal_IP(mc->remote_ip, is_admin_mfg);
		
		if (strcmp(type, "kiwi") == 0) {
			cfg_pwd = cfg_string("user_password", NULL, CFG_REQUIRED);
			cfg_auto_login = cfg_bool("user_auto_login", NULL, CFG_REQUIRED);

			// if no user password set allow unrestricted connection
			if ((!cfg_pwd || !*cfg_pwd)) {
				printf("PWD kiwi: no config pwd set, allow any\n");
				allow = true;
			} else
			
			// pwd set, but auto_login for local subnet is true
			if (cfg_auto_login && is_local) {
				printf("PWD kiwi: config pwd set, but is_local and auto-login set\n");
				allow = true;
			}
		} else
		if (is_admin_mfg) {
			cfg_pwd = cfg_string("admin_password", NULL, CFG_REQUIRED);
			cfg_auto_login = cfg_bool("admin_auto_login", NULL, CFG_REQUIRED);
			lprintf("PWD %s: config pwd set %s, auto-login %s\n", type,
				(!cfg_pwd || !*cfg_pwd)? "FALSE":"TRUE", cfg_auto_login? "TRUE":"FALSE");

			// no pwd set in config (e.g. initial setup) -- allow if connection is from local network
			if ((!cfg_pwd || !*cfg_pwd) && is_local) {
				lprintf("PWD %s: no config pwd set, but is_local\n", type);
				allow = true;
			} else
			
			// pwd set, but auto_login for local subnet is true
			if (cfg_auto_login && is_local) {
				lprintf("PWD %s: config pwd set, but is_local and auto-login set\n", type);
				allow = true;
			}
		} else {
			printf("PWD bad type=%s\n", type);
			cfg_pwd = NULL;
		}
		
		if (allow) {
			if (is_admin_mfg)
				lprintf("PWD %s allow override: sent from %s\n", type, mc->remote_ip);
			badp = 0;
		} else
		if ((!cfg_pwd || !*cfg_pwd)) {
			lprintf("PWD %s rejected: no config pwd set, sent from %s\n", type, mc->remote_ip);
			badp = 1;
		} else {
			badp = cfg_pwd? strcasecmp(pwd, cfg_pwd) : 1;
			lprintf("PWD %s %s: sent from %s\n", type, badp? "rejected":"accepted", mc->remote_ip);
		}
		
		if (cfg_pwd)
			cfg_string_free(cfg_pwd);
		
		// SECURITY: disallow double quotes in pwd
		kiwi_chrrep(pwd, '"', '\'');
		n = snprintf(oc, rem, "%s(%d);", cb, badp);
		
		if (!rem || rem < n) { *oc = 0; } else { oc += n; rem -= n; }
		*size = oc-op;
		break;
		
	default:
		break;
	}

	if (*size)
		return buf;
	else
		return NULL;
}

static int last_hour = -1, last_min = -1;

// called periodically
void webserver_collect_print_stats(int print)
{
	int i, nusers=0;
	conn_t *c;
	
	// print / log connections
	for (c=conns; c < &conns[N_CONNS]; c++) {
		if (!(c->valid && c->type == STREAM_SOUND && c->arrived)) continue;
		
		u4_t now = timer_sec();
		if (c->freqHz != c->last_freqHz || c->mode != c->last_mode || c->zoom != c->last_zoom) {
			if (print) loguser(c, LOG_UPDATE);
			c->last_tune_time = now;
		} else {
			u4_t diff = now - c->last_log_time;
			if (diff > MINUTES_TO_SEC(5)) {
				if (print) loguser(c, LOG_UPDATE_NC);
			}
			
			if (!c->inactivity_timeout_override && (inactivity_timeout_mins != 0)) {
				diff = now - c->last_tune_time;
				if (diff > MINUTES_TO_SEC(inactivity_timeout_mins) && !c->inactivity_msg_sent) {
					//send_msg(c, SM_NO_DEBUG, "MSG status_msg=INACTIVITY%20TIMEOUT");
					send_msg(c, SM_NO_DEBUG, "MSG inactivity_timeout_msg=%d", inactivity_timeout_mins);
					c->inactivity_msg_sent = true;
				}
				if (diff > (MINUTES_TO_SEC(inactivity_timeout_mins) + INACTIVITY_WARNING_SECS)) {
					c->inactivity_timeout = true;
				}
			}
		}
		nusers++;
	}
	current_nusers = nusers;

	// construct cpu stats response
	char *oc = stats_buf;
	int rem = NSTATS_BUF;
	int n, user, sys, idle;
	static int last_user, last_sys, last_idle;
	user = sys = 0;
	u4_t now = timer_ms();
	static u4_t last_now;
	float secs = (float)(now - last_now) / 1000;
	last_now = now;
	
	float del_user = 0;
	float del_sys = 0;
	float del_idle = 0;
	
	char buf[256];
	n = non_blocking_cmd("cat /proc/stat", buf, sizeof(buf), NULL);
	if (n > 0) {
		n = sscanf(buf, "cpu %d %*d %d %d", &user, &sys, &idle);
		//long clk_tick = sysconf(_SC_CLK_TCK);
		del_user = (float)(user - last_user) / secs;
		del_sys = (float)(sys - last_sys) / secs;
		del_idle = (float)(idle - last_idle) / secs;
		//printf("CPU %.1fs u=%.1f%% s=%.1f%% i=%.1f%%\n", secs, del_user, del_sys, del_idle);
		n = snprintf(oc, rem, "ajax_cpu_stats(%d, %.0f, %.0f, %.0f, %.0f);",
			timer_sec(), del_user, del_sys, del_idle, ecpu_use());
		oc += n; rem -= n;
		last_user = user;
		last_sys = sys;
		last_idle = idle;
	}
	*oc = '\0';

	// collect network i/o stats
	static const float k = 1.0/1000.0/10.0;		// kbytes/sec every 10 secs
	audio_kbps = audio_bytes*k;
	waterfall_kbps = waterfall_bytes*k;
	
	for (i=0; i <= RX_CHANS; i++) {
		waterfall_fps[i] = waterfall_frames[i]/10.0;
		waterfall_frames[i] = 0;
	}
	http_kbps = http_bytes*k;
	audio_bytes = waterfall_bytes = http_bytes = 0;

	// on the hour: report number of connected users & schedule updates
	time_t t;
	time(&t);
	struct tm tm;
	localtime_r(&t, &tm);
	
	if (tm.tm_hour != last_hour) {
		if (print) lprintf("(%d %s)\n", nusers, (nusers==1)? "user":"users");
		last_hour = tm.tm_hour;
	}

	if (tm.tm_min != last_min) {
		schedule_update(tm.tm_hour, tm.tm_min);
		last_min = tm.tm_min;
	}
}
