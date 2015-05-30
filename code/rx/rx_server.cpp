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

// Copyright (c) 2014 John Seamons, ZL/KF6VO

#include "types.h"
#include "config.h"
#include "dx.h"
#include "wrx.h"
#include "misc.h"
#include "timer.h"
#include "web.h"
#include "peri.h"
#include "spi.h"
#include "gps.h"
#include "coroutines.h"

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <sched.h>
#include <math.h>
#include <fftw3.h>

conn_t conns[RX_CHANS*2];

struct stream_t {
	int type;
	const char *uri;
	funcP_t f;
	u4_t priority;
};

static stream_t streams[] = {
	{ STREAM_SOUND,		"AUD",		&w2a_sound,		HIGH_PRIORITY },
	{ STREAM_WATERFALL,	"FFT",		&w2a_waterfall,	LOW_PRIORITY },
	{ STREAM_OTHERS,	"USR" },
	{ STREAM_DX,		"STA" },
	{ STREAM_PWD,		"PWD" },
	{ 0 }
};

static void conn_init(conn_t *c, int type, int rx_channel)
{
	memset(c, 0, sizeof(conn_t));
	c->type = type;
	c->rx_channel = rx_channel;
}

// caution: returns a *static whose use must be short-lived
char *rx_clist(conn_t *c_rx)
{
	static char list[16];
	char *cp = list;
	conn_t *c = conns;
	int i;
	
	for (i=0; i<RX_CHANS; i++) {
		*cp++ = c->remote_port? '0'+i : ' ';
		c++; c++;
	}

	sprintf(cp, " rx%d ", c_rx->rx_channel);
	return list;
}

void rx_server_init()
{
	int i;
	conn_t *c = conns;
	
	// NB: implied ordering here is assumed in code elsewhere
	for (i=0; i<RX_CHANS; i++) {
		conn_init(c, STREAM_SOUND, i);
		c++;
		conn_init(c, STREAM_WATERFALL, i);
		c++;
	}
}

//void loguser(conn_t *c, logtype_e type)
void loguser(conn_t *c, const char *type)
{
//	fixme: WHY-WHY-WHY does the world break when these simple changes are enabled!?!?!?!?!?!?!
//	stack corruption? memory smash?
#if 0
	char *s;
	if (type == LOG_ARRIVED) {
		asprintf(&s, "(ARRIVED)");
	} else
	if (type == LOG_LEAVING) {
		u4_t t = timer_sec() - c->arrival;
		u4_t sec = t % 60; t /= 60;
		u4_t min = t % 60; t /= 60;
		u4_t hr = t;
		asprintf(&s, "(LEAVING after %d:%d:%d)", hr, min, sec);
	} else {
		asprintf(&s, " ");
	}
#endif
	lprintf("rx%d %8.2f kHz %3s \"%s\" %s %s %s\n", c->rx_channel, (float) c->freqHz / KHz,
		(c->mode == MODE_AM)? "AM " : "SSB",
		c->user, c->isUserIP? "":c->remote_ip, c->geo? c->geo:"", type? type:"");
	//free(s);

	c->last_freqHz = c->freqHz;
	c->last_mode = c->mode;
}

void rx_server_remove(conn_t *c)
{
	c->stop_data = TRUE;
	c->mc = NULL;
	//if (c->type == STREAM_SOUND) loguser(c, LOG_LEAVING);
	if (c->type == STREAM_SOUND) loguser(c, "(LEAVING)");
	webserver_connection_cleanup(c);
	if (c->user) wrx_free("user", c->user);
	if (c->geo) wrx_free("geo", c->geo);
	
	int task = c->task;
	conn_init(c, c->type, c->rx_channel);
	TaskRemove(task);
	panic("shouldn't return");
}

// if this connection is new spawn new receiver channel with sound/waterfall tasks
conn_t *rx_server_websocket(struct mg_connection *mc)
{
	int i;
	conn_t *c;
	stream_t *st;
	
	assert(mc->remote_port != 0);
	
	c = (conn_t*) mc->connection_param;
	if (c) {
		// existing connection
		if (c->magic != CN_MAGIC) {
			lprintf("mc: %s:%d %s\n", mc->remote_ip, mc->remote_port, mc->uri);
			lprintf("mc: this=0x%x mc->c=0x%x mc->c->magic=0x%x CN_MAGIC=0x%x\n",
				mc, c, c->magic, CN_MAGIC);
			conn_t *cd;
			for (cd=conns; cd<&conns[RX_CHANS*2]; cd++) {
				lprintf("CONN%d: %s this=0x%x mc=0x%x magic=0x%x %s:%d\n",
					cd->rx_channel, streams[cd->type].uri, cd, cd->mc, cd->magic, cd->remote_ip, cd->remote_port);
			}
		}
// fixme: workaround for c->magic != CN_MAGIC bug
if (c->magic != 0) {
		assert(c->magic == CN_MAGIC);
		assert(c->mc == mc);
		assert(c->remote_port == mc->remote_port);
		return c;
}
	}

	const char *uri = mc->uri;
	if (uri[0] == '/') uri++;
	//printf("#### new connection: %s:%d %s ", mc->remote_ip, mc->remote_port, uri);
	
	for (i=0; streams[i].uri; i++) {
		st = &streams[i];
		
		if (strcmp(uri, st->uri) == 0)
			break;
	}
	
	if (!streams[i].uri) {
		lprintf("**** unknown stream type <%s>\n", uri);
		return NULL;
	}

	printf("LOOKING for free conn for type=%d (%s)\n", st->type, st->uri);
	for (c=conns; c<&conns[RX_CHANS*2]; c++) {
		if ((c->remote_port == 0) && (c->type == st->type))
			break;
		printf("%s conn rx=%d, remote_port=%d, type=%d (%s)\n",
			(c->type == st->type)? "BUSY":"!TYPE", c->rx_channel, c->remote_port, c->type, streams[c->type].uri);
	}
	
	if (c == &conns[RX_CHANS*2]) {
		printf("(too many rx channels open for %s)\n", st->uri);
		if (st->type == STREAM_WATERFALL) send_msg_mc(mc, "MSG too_busy=%d", RX_CHANS);
		return NULL;
	}

	printf("FREE conn rx=%d, remote_port=%d, type=%d (%s)\n", c->rx_channel, c->remote_port, c->type, streams[c->type].uri);

	if (down) {
		//printf("(down for devl)\n");
		if (st->type == STREAM_WATERFALL) send_msg_mc(mc, "MSG down");
		return NULL;
	}
	
	if (!do_wrx) {
		if (do_gps && (st->type == STREAM_WATERFALL)) {
			// display GPS data in waterfall
			send_msg_mc(mc, "MSG gps");
		} else
		if (do_fft && (st->type == STREAM_WATERFALL)) {
			send_msg_mc(mc, "MSG fft");
		} else
		if (do_fft && (st->type == STREAM_SOUND)) {
			;	// start sound task to process sound UI controls
		} else
		{
			//printf("(no wrx)\n");
			return NULL;
		}
	}

	// NB: c->type & c->rx_channel are preset
	c->magic = CN_MAGIC;
	c->mc = mc;
	c->remote_port = mc->remote_port;
	memcpy(c->remote_ip, mc->remote_ip, NRIP);
	c->a2w.mc = c->w2a.mc = mc;
	lock_init(&c->a2w.lock);
	lock_init(&c->w2a.lock);
	mc->connection_param = c;
	c->ui = find_ui(mc->local_port);
	assert(c->ui);
	//printf("NEW channel %d\n", c->rx_channel);

	if (st->f != NULL) {
		int id = _CreateTaskP(st->f, st->uri, st->priority, c);
		c->task = id;
	}
	
	return c;
}

volatile float audio_kbps, waterfall_kbps, waterfall_fps, http_kbps;
volatile int audio_bytes, waterfall_bytes, waterfall_frames, http_bytes;

char *rx_server_request(struct mg_connection *mc, char *buf, size_t *size)
{
	int i, j, n;
	const char *s;
	stream_t *st;
	char *op = NULL, *oc, *lc;
	int badp;
	int rem = NREQ_BUF;
	
	for (st=streams; st->uri; st++) {
		if (strcmp(mc->uri, st->uri) == 0)
			break;
	}
	
	if (!st->uri) return NULL;
	if (down && (st->type != STREAM_PWD)) return NULL;
	
	buf[0]=0;

	switch (st->type) {

	case STREAM_OTHERS:
		conn_t *c;
		op = buf;
		oc = op;
		
		for (c=conns; c<&conns[RX_CHANS*2]; c++) {
			if (c->type == STREAM_SOUND) {
				if (c->arrived && c->user != NULL) {
					n = snprintf(oc, rem, "user(%d,\"%s\",\"%s\",%d);",
						c->rx_channel, c->user, c->geo, c->freqHz);
				} else {
					n = snprintf(oc, rem, "user(%d,\"\",\"\",0);", c->rx_channel);
				}
				if (!rem || rem < n) { *oc = 0; break; } else { oc += n; rem -= n; }
			}
		}
		
		// statistics
		int stats;
		stats=0;
		n = sscanf(mc->query_string, "stats=%d", &stats);
		//printf("%d %d <%s>\n", n, stats, mc->query_string);
		
		lc = oc;	// skip entire response if we run out of room
		for (; stats; ) {	// hack so we can use 'break' statements below
			n = snprintf(oc, rem, "wrx_cpu_stats("); 
			if (!rem || rem < n) { oc = lc; *oc = 0; break; } else { oc += n; rem -= n; }

			int user, sys, idle;
			static int last_user, last_sys, last_idle;
			user = sys = 0;
			u4_t now = timer_ms();
			static u4_t last_now;
			float secs = (float)(now - last_now) / 1000;
			last_now = now;
			
			float del_user = 0;
			float del_sys = 0;
			float del_idle = 0;
			FILE *pf = popen("cat /proc/stat", "r");
			if (pf) {
				char pfbuf[128];
				pfbuf[0]=0;
				n = fscanf(pf, "cpu %d %*d %d %d", &user, &sys, &idle);
				//long clk_tick = sysconf(_SC_CLK_TCK);
				del_user = (float)(user - last_user) / secs;
				del_sys = (float)(sys - last_sys) / secs;
				del_idle = (float)(idle - last_idle) / secs;
				//printf("CPU %.1fs u=%.1f%% s=%.1f%% i=%.1f%%\n", secs, del_user, del_sys, del_idle);
				pclose(pf);
				n = snprintf(oc, rem,
					"\"Beagle CPU %.0f%% usr / %.0f%% sys / %.0f%% idle, \"+",
					del_user, del_sys, del_idle);
				if (!rem || rem < n) { oc = lc; *oc = 0; break; } else { oc += n; rem -= n; }
				last_user = user;
				last_sys = sys;
				last_idle = idle;
			}
			n = snprintf(oc, rem, "\"FPGA eCPU %.1f%%\");", ecpu_use());
			if (!rem || rem < n) { oc = lc; *oc = 0; break; } else { oc += n; rem -= n; }
			
			float sum_kbps = audio_kbps + waterfall_kbps + http_kbps;
			n = snprintf(oc, rem,
				"wrx_audio_stats(\"audio %.1f kB/s, waterfall %.1f kB/s (%.1f fps), http %.1f kB/s, total %.1f kB/s (%.1f kb/s)\");",
				audio_kbps, waterfall_kbps, waterfall_fps, http_kbps, sum_kbps, sum_kbps*8);
			if (!rem || rem < n) { *lc = 0; break; } else { oc += n; rem -= n; }

			// fixme: only really need to send once
			n = snprintf(oc, rem, "wrx_config(\"%d receiver channels, %d GPS channels\");", RX_CHANS, GPS_CHANS);
			if (!rem || rem < n) { oc = lc; *oc = 0; break; } else { oc += n; rem -= n; }
			
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
#define DX_SPACING_THRESHOLD_PX		5

	case STREAM_DX:
		float min, max;
		int zoom, width;
		n = sscanf(mc->query_string, "min=%f&max=%f&zoom=%d&width=%d", &min, &max, &zoom, &width);
		if (n != 4) break;
		float bw;
		bw = max - min;
		op = buf;
		oc = op;
		static bool first = true;
		static int dx_lastx;
		dx_lastx = 0;
		
		for (i=0, j=0; i < ARRAY_LEN(dx); i++) {
			dx_t *dp;
			dp = &dx[i];
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
			
			n = snprintf(oc, rem, "dx(%.3f,%d,\'%s\'%s%s%s);", dp->freq, dp->flags, dp->text,
				dp->notes? ",\'":"", dp->notes? dp->notes:"", dp->notes? "\'":"");
			if (!rem || rem < n) {
				*oc = 0;
				printf("STREAM_DX: buffer overflow %d/%d min=%f max=%f z=%d w=%d\n", i, ARRAY_LEN(dx), min, max, zoom, width);
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
		const char *match;
		type[0]=0; pwd[0]=0;
		//printf("STREAM_PWD: <%s>\n", mc->query_string);
		 cp = (char*) mc->query_string;
		int i, sl;
		sl = strlen(cp);
		for (i=0; i < sl; i++) { if (*cp == '&') *cp = ' '; cp++; }
		sscanf(mc->query_string, "type=%s pwd=%31s", type, pwd);
		//lprintf("PWD %s pwd %s from %s\n", type, pwd, mc->remote_ip);
		if (strcmp(type, "demop") == 0) {
			match = "kiwi";		// fixme: get from files
		} else
		if (strcmp(type, "admin") == 0) {
			match = "kaikoura";
		} else {
			printf("bad type=%s\n", type);
			match = NULL;
		}
		badp = match? strcasecmp(pwd, match) : 1;
		if (badp) lprintf("bad %s pwd %s from %s\n", type, pwd, mc->remote_ip);
		
		op = buf;
		oc = op;
		n = snprintf(oc, rem, "wrx_setpwd(\"%s\",\"%s\");", badp? "bad":pwd, badp? "bad":WRX_KEY);
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

void webserver_print_stats()
{
	int nusers=0;
	conn_t *c;
	
	for (c=conns; c<&conns[RX_CHANS*2]; c++) {
		if (!c->arrived) continue;
		
		#if 0
		if (c->type == STREAM_WATERFALL) {
			printf("WF%d %2d fps %7.3f %7.3f %7.3f\n", c->rx_channel, c->wf_frames,
				(float) c->wf_get / 1000, (float) c->wf_lock / 1000, (float) c->wf_loop / 1000);
			c->wf_frames = 0;
		}
		#endif

		if (c->type == STREAM_SOUND) {
			//if ((c->freqHz != c->last_freqHz) || (c->mode != c->last_mode)) loguser(c, LOG_UPDATE);
			if ((c->freqHz != c->last_freqHz) || (c->mode != c->last_mode)) loguser(c, NULL);
			nusers++;
		}
	}

	static const float k = 1.0/1000.0/10.0;		// kbytes/sec every 10 secs
	audio_kbps = audio_bytes*k;
	waterfall_kbps = waterfall_bytes*k;
	waterfall_fps = waterfall_frames/10.0;
	http_kbps = http_bytes*k;
	audio_bytes = waterfall_bytes = waterfall_frames = http_bytes = 0;

	time_t t;
	time(&t);
	struct tm tm;
	localtime_r(&t, &tm);
	
	if (tm.tm_hour != last_hour) {
		lprintf("(%d %s)\n", nusers, (nusers==1)? "user":"users");
		last_hour = tm.tm_hour;
	}
}
