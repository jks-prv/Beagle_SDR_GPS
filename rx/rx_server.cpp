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
#include "timer.h"
#include "web.h"
#include "peri.h"
#include "spi.h"
#include "gps.h"
#include "cfg.h"
#include "coroutines.h"
#include "data_pump.h"

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <sched.h>
#include <math.h>
#include <signal.h>
#include <fftw3.h>

conn_t conns[N_CONNS];

rx_chan_t rx_chan[RX_CHANS];

struct stream_t {
	int type;
	const char *uri;
	funcP_t f;
	u4_t priority;
};

static stream_t streams[] = {
	{ STREAM_SOUND,		"AUD",		&w2a_sound,		SND_PRIORITY },
	{ STREAM_WATERFALL,	"FFT",		&w2a_waterfall,	WF_PRIORITY },
	{ STREAM_ADMIN,		"ADM",		&w2a_admin,		ADMIN_PRIORITY },
	{ STREAM_MFG,		"MFG",		&w2a_mfg,		ADMIN_PRIORITY },
	{ STREAM_USERS,		"USR" },
	{ STREAM_DX,		"STA" },
	{ STREAM_DX_UPD,	"UPD" },
	{ STREAM_PWD,		"PWD" },
	{ STREAM_DISCOVERY,	"DIS" },
	{ STREAM_SDR_HU,	"status" },
	{ 0 }
};

static void conn_init(conn_t *c)
{
	memset(c, 0, sizeof(conn_t));
	c->magic = CN_MAGIC;
	c->self = c;
	c->self_idx = c - conns;
	c->rx_channel = -1;
}

static void dump_conn()
{
	int i;
	conn_t *cd;
	for (cd=conns, i=0; cd < &conns[N_CONNS]; cd++, i++) {
		lprintf("dump_conn: CONN-%d %p valid=%d type=%d KA=%d KC=%d mc=%p rx=%d %s magic=0x%x ip=%s:%d other=%s%d %s\n",
			i, cd, cd->valid, cd->type, cd->keep_alive, cd->keepalive_count, cd->mc, cd->rx_channel, streams[cd->type].uri, cd->magic,
			cd->remote_ip, cd->remote_port, cd->other? "CONN-":"", cd->other? cd->other-conns:0, cd->stop_data? "STOP":"");
	}
	rx_chan_t *rc;
	for (rc=rx_chan, i=0; rc < &rx_chan[RX_CHANS]; rc++, i++) {
		lprintf("dump_conn: RX_CHAN-%d en %d busy %d conn = %s%d %p\n",
			i, rc->enabled, rc->busy, rc->conn? "CONN-":"", rc->conn? rc->conn-conns:0, rc->conn);
	}
}

static void cfg_handler(int arg)
{
	lprintf("SIGUSR1: reloading configuration, dx list..\n");
	cfg_reload(NOT_CALLED_FROM_MAIN);

	struct sigaction act;
	act.sa_handler = cfg_handler;
	scall("SIGUSR1", sigaction(SIGUSR1, &act, NULL));
}

static void debug_handler(int arg)
{
	lprintf("SIGUSR2: debugging..\n");
	dump_conn();

	struct sigaction act;
	act.sa_handler = debug_handler;
	scall("SIGUSR2", sigaction(SIGUSR2, &act, NULL));
}

void rx_server_init()
{
	int i, j;
	
	conn_t *c = conns;
	for (i=0; i<N_CONNS; i++) {
		conn_init(c);
		c++;
	}
	
	struct sigaction act;
	act.sa_handler = cfg_handler;
	scall("SIGUSR1", sigaction(SIGUSR1, &act, NULL));
	act.sa_handler = debug_handler;
	scall("SIGUSR2", sigaction(SIGUSR2, &act, NULL));
	
	if (!down) {
		w2a_sound_init();
		w2a_waterfall_init();
	}
}

void loguser(conn_t *c, logtype_e type)
{
	char *s;
	u4_t now = timer_sec();
	u4_t t = now - c->arrival;
	u4_t sec = t % 60; t /= 60;
	u4_t min = t % 60; t /= 60;
	u4_t hr = t;

	if (type == LOG_ARRIVED) {
		asprintf(&s, "(ARRIVED)");
		c->last_tune_time = now;
	} else
	if (type == LOG_LEAVING) {
		asprintf(&s, "(LEAVING after %d:%02d:%02d)", hr, min, sec);
	} else {
		asprintf(&s, "%d:%02d:%02d%s", hr, min, sec, (type == LOG_UPDATE_NC)? " n/c":"");
	}
	if (type == LOG_ARRIVED || type == LOG_LEAVING) {
		clprintf(c, "%8.2f kHz %3s z%-2d \"%s\"%s%s%s%s %s\n", (float) c->freqHz / KHz,
			enum2str(c->mode, mode_s, ARRAY_LEN(mode_s)), c->zoom, c->user,
			c->isUserIP? "":" ", c->isUserIP? "":c->remote_ip, c->geo? " ":"", c->geo? c->geo:"", s);
	}
	free(s);

	c->last_freqHz = c->freqHz;
	c->last_mode = c->mode;
	c->last_zoom = c->zoom;
	c->last_log_time = now;
}

void rx_server_remove(conn_t *c)
{
	c->stop_data = TRUE;
	c->mc = NULL;

	if (c->arrived) loguser(c, LOG_LEAVING);
	webserver_connection_cleanup(c);
	if (c->user) kiwi_free("user", c->user);
	if (c->geo) kiwi_free("geo", c->geo);
	
	int task = c->task;
	conn_init(c);
	check_for_update();
	TaskRemove(task);
}

int rx_server_users()
{
	int users=0, any=0;
	
	conn_t *c = conns;
	for (int i=0; i < N_CONNS; i++) {
		//if (c->valid && c->type == STREAM_SOUND && c->arrived) users++;
		if (c->valid && c->type == STREAM_SOUND) users++;
		if (c->valid && (c->type == STREAM_SOUND || c->type == STREAM_WATERFALL)) any = 1;
		c++;
	}
	return (users? users : any);
}

struct mg_connection *msgs_mc;

// if this connection is new, spawn new receiver channel with sound/waterfall tasks
conn_t *rx_server_websocket(struct mg_connection *mc, websocket_mode_e mode)
{
	int i;
	conn_t *c;
	stream_t *st;

	c = (conn_t*) mc->connection_param;
	if (c) {	// existing connection
		
		if (c->magic != CN_MAGIC || !c->valid || mc != c->mc || mc->remote_port != c->remote_port) {
			if (mode != WS_MODE_ALLOC) return NULL;
		#if 0
			lprintf("rx_server_websocket(%s): BAD CONN MC PARAM\n", (mode == WS_MODE_LOOKUP)? "lookup" : "alloc");
			lprintf("rx_server_websocket: (mc=%p == mc->c->mc=%p)? mc->c=%p mc->c->valid %d mc->c->magic=0x%x CN_MAGIC=0x%x mc->c->rport=%d\n",
				mc, c->mc, c, c->valid, c->magic, CN_MAGIC, c->remote_port);
			lprintf("rx_server_websocket: mc: %s:%d %s\n", mc->remote_ip, mc->remote_port, mc->uri);
			dump_conn();
			lprintf("rx_server_websocket: returning NULL\n");
		#endif
			return NULL;
		}
		
		if (mode == WS_MODE_CLOSE) {
			c->mc = NULL;
			return NULL;
		}
	
		return c;	// existing connection is valid
	}
	
	if (mode != WS_MODE_ALLOC) return NULL;
	
	// new connection needed
	const char *uri_ts = mc->uri;
	char uri[64];
	if (uri_ts[0] == '/') uri_ts++;
	//printf("#### new connection: %s:%d %s\n", mc->remote_ip, mc->remote_port, uri_ts);
	
	u64_t tstamp;
	if (sscanf(uri_ts, "%lld/%s", &tstamp, uri) != 2) {
		printf("bad URI_TS format\n");
		return NULL;
	}
	
	for (i=0; streams[i].uri; i++) {
		st = &streams[i];
		
		if (strcmp(uri, st->uri) == 0)
			break;
	}
	
	if (!streams[i].uri) {
		lprintf("**** unknown stream type <%s>\n", uri);
		return NULL;
	}

	if (down || update_in_progress) {
		if (st->type == STREAM_WATERFALL) send_msg_mc(mc, SM_DEBUG, "MSG down=%d", (!down && update_in_progress)? 1:0);
		return NULL;
	}
	
	#if 0
	if (!do_sdr) {
		if (do_gps && (st->type == STREAM_WATERFALL)) {
			// display GPS data in waterfall
			;
		} else
		if (do_fft && (st->type == STREAM_WATERFALL)) {
			send_msg_mc(mc, SM_DEBUG, "MSG fft");
		} else
		if (do_fft && (st->type == STREAM_SOUND)) {
			;	// start sound task to process sound UI controls
		} else
		{
			//printf("(no kiwi)\n");
			return NULL;
		}
	}
	#endif
	
	//printf("CONN LOOKING for free conn for type=%d (%s) mc %p\n", st->type, uri, mc);
	bool multiple = false;
	int cn, cnfree;
	conn_t *cfree = NULL, *cother = NULL;
	for (c=conns, cn=0; c<&conns[N_CONNS]; c++, cn++) {
		assert(c->magic == CN_MAGIC);
		if (!c->valid) {
			if (!cfree) { cfree = c; cnfree = cn; }
			//printf("CONN-%d !VALID\n", cn);
			continue;
		}
		//printf("CONN-%d IS %p type=%d tstamp=%lld ip=%s:%d rx=%d other%s%ld mc=%p\n", cn, c, c->type, c->tstamp, c->remote_ip,
		//	c->remote_port, c->rx_channel, c->other? "=CONN-":"=", c->other? c->other-conns:0, c->mc);
		if (c->tstamp == tstamp && (strcmp(mc->remote_ip, c->remote_ip) == 0)) {
			if (c->type == st->type) {
				printf("CONN-%d DUPLICATE!\n", cn);
				return NULL;
			}
			if (st->type == STREAM_SOUND && c->type == STREAM_WATERFALL) {
				if (!multiple) {
					cother = c;
					multiple = true;
					//printf("CONN-%d OTHER WF @ CONN-%ld\n", cn, c-conns);
				} else {
					printf("CONN-%d MULTIPLE OTHER!\n", cn);
					return NULL;
				}
			}
			if (st->type == STREAM_WATERFALL && c->type == STREAM_SOUND) {
				if (!multiple) {
					cother = c;
					multiple = true;
					//printf("CONN-%d OTHER SND @ CONN-%ld\n", cn, c-conns);
				} else {
					printf("CONN-%d MULTIPLE OTHER!\n", cn);
					return NULL;
				}
			}
		}
	}
	
	if (c == &conns[N_CONNS]) {
		if (cfree) {
			c = cfree;
			cn = cnfree;
		} else {
			//printf("(too many network connections open for %s)\n", st->uri);
			if (st->type != STREAM_SOUND) send_msg_mc(mc, SM_DEBUG, "MSG too_busy=%d", RX_CHANS);
			return NULL;
		}
	}

	mc->connection_param = c;
	conn_init(c);
	c->type = st->type;
	c->other = cother;

	if (st->type == STREAM_SOUND || st->type == STREAM_WATERFALL) {
		int rx = -1;
		if (!cother) {
			for (i=0; i < RX_CHANS; i++) {
				//printf("RX_CHAN-%d busy %d\n", i, rx_chan[i].busy);
				if (!rx_chan[i].busy && rx == -1) rx = i;
			}
			if (rx == -1) {
				//printf("(too many rx channels open for %s)\n", st->uri);
				if (st->type == STREAM_WATERFALL) send_msg_mc(mc, SM_DEBUG, "MSG too_busy=%d", RX_CHANS);
				mc->connection_param = NULL;
				conn_init(c);
				return NULL;
			}
			//printf("CONN-%d no other, new alloc rx%d\n", cn, rx);
			rx_chan[rx].busy = true;
		} else {
			cother->other = c;
		}
		c->rx_channel = cother? cother->rx_channel : rx;
		if (st->type == STREAM_SOUND) rx_chan[c->rx_channel].conn = c;
	}
	
	memcpy(c->remote_ip, mc->remote_ip, NRIP);

	c->mc = mc;
	c->remote_port = mc->remote_port;
	c->tstamp = tstamp;
	ndesc_init(&c->a2w, mc);
	ndesc_init(&c->w2a, mc);
	c->ui = find_ui(mc->local_port);
	assert(c->ui);
	c->arrival = timer_sec();
	//printf("NEW channel %d\n", c->rx_channel);
	
	// remember channel for recording error messages remotely
	if (c->type == STREAM_ADMIN || c->type == STREAM_MFG) {
		msgs_mc = mc;
	}

	if (st->f != NULL) {
		int id = _CreateTaskP(st->f, st->uri, st->priority, c);
		c->task = id;
	}
	
	//printf("CONN-%d <=== USE THIS ONE\n", cn);
	c->valid = true;
	return c;
}

#define	NSTATS_BUF	255
static char stats_buf[NSTATS_BUF+1];

volatile float audio_kbps, waterfall_kbps, waterfall_fps[RX_CHANS+1], http_kbps;
volatile int audio_bytes, waterfall_bytes, waterfall_frames[RX_CHANS+1], http_bytes;

static int current_nusers;

// process non-websocket connections
char *rx_server_request(struct mg_connection *mc, char *buf, size_t *size)
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
	if ((down || update_in_progress) && (st->type != STREAM_PWD)) return NULL;
	//printf("rx_server_request: uri=<%s> qs=<%s>\n", mc->uri, mc->query_string);
	
	if (st->type != STREAM_SDR_HU && st->type != STREAM_DISCOVERY && mc->query_string == NULL) {
		lprintf("rx_server_request: missing query string! uri=<%s>\n", mc->uri);
		*size = snprintf(op, rem, "kiwi_server_error(\"missing query string\");");
		return buf;
	}
	
	buf[0]=0;

	switch (st->type) {
	
	case STREAM_DISCOVERY:
		n = snprintf(oc, rem, "%d %s %s %d %d %s",
			ddns.serno, ddns.ip_pub, ddns.ip_pvt, ddns.port, ddns.netmask, ddns.mac);
		if (!rem || rem < n) { *oc = 0; } else { oc += n; rem -= n; }
		*size = oc-op;
		printf("DISCOVERY REQUESTED from %s: <%s>\n", mc->remote_ip, buf);
		return buf;
		break;

	case STREAM_SDR_HU:
		if (!cfg_bool("sdr_hu_register", NULL, CFG_OPTIONAL)) return NULL;
		static time_t avatar_ctime;
		// the avatar file is in the in-memory store, so it's not going to be changing after server start
		if (avatar_ctime == 0) time(&avatar_ctime);		
		n = snprintf(oc, rem, "status=active\nname=%s\nsdr_hw=%s\nop_email=%s\nbands=0-%.0f\nusers=%d\navatar_ctime=%ld\ngps=%s\nasl=%d\nloc=%s\nsw_version=%s%d.%d\nantenna=%s\n",
			cfg_string("rx_name", NULL, CFG_OPTIONAL),
			cfg_string("rx_device", NULL, CFG_OPTIONAL),
			cfg_string("admin_email", NULL, CFG_OPTIONAL),
			user_iface[0].ui_srate, current_nusers, avatar_ctime,
			cfg_string("rx_gps", NULL, CFG_OPTIONAL),
			cfg_int("rx_asl", NULL, CFG_OPTIONAL),
			cfg_string("rx_location", NULL, CFG_OPTIONAL),
			"KiwiSDR_v", VERSION_MAJ, VERSION_MIN,
			cfg_string("rx_antenna", NULL, CFG_OPTIONAL));
		if (!rem || rem < n) { *oc = 0; } else { oc += n; rem -= n; }
		*size = oc-op;
		//printf("SDR.HU STATUS REQUESTED from %s: <%s>\n", mc->remote_ip, buf);
		return buf;
		break;

	case STREAM_USERS:
		rx_chan_t *rx;
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
					n = snprintf(oc, rem, "user(%d,\"%s\",\"%s\",%d,\"%s\",%d,\"%d:%02d:%02d\");",
						i, c->user, c->geo, c->freqHz,
						enum2str(c->mode, mode_s, ARRAY_LEN(mode_s)), c->zoom,
						hr, min, sec);
				}
			}
			if (n == 0) n = snprintf(oc, rem, "user(%d,\"\",\"\",0,\"\");", i);
			if (!rem || rem < n) { *oc = 0; break; } else { oc += n; rem -= n; }
		}
		
		// statistics
		int stats, config, ch;
		stats = config = 0;
		n = sscanf(mc->query_string, "stats=%d&config=%d&ch=%d", &stats, &config, &ch);
		//printf("USR n=%d stats=%d config=%d <%s>\n", n, stats, config, mc->query_string);
		
		lc = oc;	// skip entire response if we run out of room
		for (; stats; ) {	// hack so we can use 'break' statements below
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
				n = snprintf(oc, rem, "ajax_msg_config(%d, %d);", RX_CHANS, GPS_CHANS);
				if (!rem || rem < n) { oc = lc; *oc = 0; break; } else { oc += n; rem -= n; }
			}
			
			stats = 0;	// only run loop once
		}
		
		*size = oc-op;
		if (*size == 0) {
			buf[0] = ';';
			*size = 1;
		}
		return buf;
		break;

#define DX_SPACING_ZOOM_THRESHOLD	5
#define DX_SPACING_THRESHOLD_PX		10
		dx_t *dp, *ldp, *upd;

	case STREAM_DX_UPD:
		#if 0
		float freq;
		int flags;
		char text[256+1], notes[256+1];
		n = sscanf(mc->query_string, "f=%f&flags=%d&text=%256[^&]&notes=%256[^&]", &freq, &flags, text, notes);
		mg_url_decode(text, 256, text, 256, 0);		// dst=src is okay because length dst always <= src
		mg_url_decode(notes, 256, notes, 256, 0);		// dst=src is okay because length dst always <= src
		printf("DX_UPD %8.2f %d text=<%s> notes=<%s>\n", freq, flags, text, notes);
		if (n != 4) break;
		upd = (dx_t *) kiwi_malloc("dx_t", sizeof(dx_t));
		upd->freq = freq;
		upd->flags = flags;
		upd->ident = kiwi_strdup("dx_ident", text);
		upd->notes = kiwi_strdup("dx_notes", notes);

		for (dp = dx_list, j=0; dp; dp = dp->next) {
			ldp = dp;
			if (dp->freq < freq) continue;
			if (dp == dx_list) {
				upd->next = dx_list;
				dx_list = upd;
				//printf("insert first, next %.2f\n", upd->next->freq);
			} else {
				upd->next = dp;
				(dp-1)->next = upd;
				//printf("insert middle, next %.2f\n", upd->next->freq);
			}
			break;
		}
		if (dp == NULL) {
			ldp->next = upd;
			upd->next = NULL;
			//printf("insert last, prev %.2f\n", ldp->freq);
		}
		#endif
		
		buf[0] = ';';
		*size = 1;
		return buf;
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
		
		for (dp = dx_list, j=0; dp; dp = dp->next) {
			float freq;
			freq = dp->freq;
			if (freq < min || freq > max) continue;
			
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
			n = snprintf(oc, rem, "dx(%.3f,%.0f,%d,\'%s\'%s%s%s);", f, dp->offset, dp->flags, dp->ident,
				dp->notes? ",\'":"", dp->notes? dp->notes:"", dp->notes? "\'":"");
			if (!rem || rem < n) {
				*oc = 0;
				printf("STREAM_DX: buffer overflow %d min=%f max=%f z=%d w=%d\n", i, min, max, zoom, width);
				break;
			} else {
				oc += n; rem -= n; j++;
			}
		}
		
		//printf("STREAM_DX: %d <%s>\n", j, op);
		*size = oc-op;
		if (*size == 0) {
			buf[0] = ';';
			*size = 1;
		}
		return buf;
		break;

	case STREAM_PWD:
		char type[32], pwd[32], *cp;
		const char *cfg_pwd;
		int badp;
		type[0]=0; pwd[0]=0;
		cp = (char*) mc->query_string;
		
		//printf("STREAM_PWD: <%s>\n", mc->query_string);
		int i, sl, firstCheck;
		sl = strlen(cp);
		for (i=0; i < sl; i++) { if (*cp == '&') *cp = ' '; cp++; }
		sscanf(mc->query_string, "first=%d type=%s pwd=%31s", &firstCheck, type, pwd);
		printf("PWD %s pwd \"%s\" first=%d from %s\n", type, pwd, firstCheck, mc->remote_ip);
		
		bool is_local, allow;
		allow = is_local = false;
		u4_t ip_rmt, ip_pvt, nm;
		
		if (ddns.valid) {
			ip_rmt = kiwi_n2h_32(mc->remote_ip);
			ip_pvt = kiwi_n2h_32(ddns.ip_pvt);
			nm = ~((1 << (32 - ddns.netmask)) - 1);
			is_local = ((ip_rmt & nm) == (ip_pvt & nm));
			printf("PWD ip_rmt 0x%08x ip_pvt 0x%08x nm 0x%08x is_local %d\n", ip_rmt, ip_pvt, nm, is_local);
		}
		
		if (strcmp(type, "demop") == 0) {
			cfg_pwd = cfg_string("user_password", NULL, CFG_OPTIONAL);
			// if no user password set allow connection
			if (!cfg_pwd) allow = true;
		} else
		if (strcmp(type, "admin") == 0) {
			cfg_pwd = cfg_string("admin_password", NULL, CFG_OPTIONAL);
			
			// no pwd set in config (e.g. initial setup) -- see if connection is from local network
			//if (firstCheck && !cfg_pwd) {
			if (!cfg_pwd && is_local) {
				printf("ADMIN is local: rmt %s pvt %s nm /%d\n", mc->remote_ip, ddns.ip_pvt, ddns.netmask);
				allow = true;
			}
		} else
		if (strcmp(type, "mfg") == 0) {
			cfg_pwd = cfg_string("mfg_password", NULL, CFG_OPTIONAL);
			
			// no pwd set in config -- allow if connection is from local network
			if (!cfg_pwd && is_local) {
				printf("MFG is local: rmt %s pvt %s nm /%d\n", mc->remote_ip, ddns.ip_pvt, ddns.netmask);
				allow = true;
			}
		} else {
			printf("bad type=%s\n", type);
			cfg_pwd = NULL;
		}
		
		if (allow) {
			printf("allowing %s, prev pwd \"%s\" sent from %s\n", type, pwd, mc->remote_ip);
			pwd[0] = '\0';
			badp = 0;
		} else
		if (!cfg_pwd) {
			lprintf("no %s pwd set, \"%s\" sent from %s\n", type, pwd, mc->remote_ip);
			badp = 1;
		} else {
			badp = cfg_pwd? strcasecmp(pwd, cfg_pwd) : 1;
			if (badp) lprintf("bad %s pwd \"%s\" sent from %s\n", type, pwd, mc->remote_ip);
		}
		
		// SECURITY: disallow double quotes in pwd
		kiwi_chrrep(pwd, '"', '\'');
		n = snprintf(oc, rem, "kiwi_setpwd(\"%s\",\"%s\");", badp? "bad":pwd, badp? "bad":KIWI_KEY);
		
		if (!rem || rem < n) { *oc = 0; } else { oc += n; rem -= n; }
		*size = oc-op;
		return buf;
		break;
		
	default:
		break;
	}
	
	return NULL;
}

static int last_hour = -1;

// called periodically
void webserver_collect_print_stats(int print)
{
	int i, nusers=0;
	conn_t *c;
	
	// print / log connections
	if (print) for (c=conns; c < &conns[N_CONNS]; c++) {
		if (!(c->valid && c->type == STREAM_SOUND && c->arrived)) continue;
		
		u4_t now = timer_sec();
		if (c->freqHz != c->last_freqHz || c->mode != c->last_mode || c->zoom != c->last_zoom) {
			loguser(c, LOG_UPDATE);
			c->last_tune_time = now;
		} else {
			u4_t diff = now - c->last_log_time;
			if (diff > MINUTES_TO_SECS(5)) {
				loguser(c, LOG_UPDATE_NC);
			}
			
			if (!c->inactivity_timeout_override) {
				diff = now - c->last_tune_time;
				if (diff > MINUTES_TO_SECS(inactivity_timeout_mins) && !c->inactivity_msg_sent) {
					send_msg(c, SM_DEBUG, "MSG status_msg=INACTIVITY%20TIMEOUT");
					c->inactivity_msg_sent = true;
				}
				if (diff > (MINUTES_TO_SECS(inactivity_timeout_mins) + STATS_INTERVAL_SECS)) {
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
	n = non_blocking_popen("cat /proc/stat", buf, sizeof(buf));
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

	// report number of connected users on the hour
	if (print) {
		time_t t;
		time(&t);
		struct tm tm;
		localtime_r(&t, &tm);
		
		if (tm.tm_hour != last_hour) {
			lprintf("(%d %s)\n", nusers, (nusers==1)? "user":"users");
			last_hour = tm.tm_hour;
		}
	}
}
