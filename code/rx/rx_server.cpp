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
	{ STREAM_USERS,		"USR" },
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

void loguser(conn_t *c, logtype_e type)
{
	char *s;
	if (type == LOG_ARRIVED) {
		asprintf(&s, "(ARRIVED)");
	} else
	if (type == LOG_LEAVING) {
		u4_t now = timer_sec();
		u4_t t = now - c->arrival;
		u4_t sec = t % 60; t /= 60;
		u4_t min = t % 60; t /= 60;
		u4_t hr = t;
		asprintf(&s, "(LEAVING after %d:%02d:%02d)", hr, min, sec);
	} else {
		asprintf(&s, " ");
	}
	clprintf(c, "%8.2f kHz %3s \"%s\" %s %s %s\n", (float) c->freqHz / KHz,
		enum2str(c->mode, mode_s, ARRAY_LEN(mode_s)),
		c->user, c->isUserIP? "":c->remote_ip, c->geo? c->geo:"", s);
	free(s);

	c->last_freqHz = c->freqHz;
	c->last_mode = c->mode;
}

void rx_server_remove(conn_t *c)
{
	c->stop_data = TRUE;
	c->mc->connection_param = NULL;
	c->mc = NULL;

	if (c->type == STREAM_SOUND) loguser(c, LOG_LEAVING);
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
	
	//assert(mc->remote_port != 0);
	
	c = (conn_t*) mc->connection_param;
	if (c) {	// existing connection
		
		if (c->magic != CN_MAGIC || mc != c->mc || mc->remote_port != c->remote_port) {
			lprintf("rx_server_websocket: BAD CONN MC PARAM\n");
			lprintf("rx_server_websocket: mc=%p == mc->c->mc=%p mc->c=%p mc->c->magic=0x%x CN_MAGIC=0x%x mc->c->rport=%d\n",
				mc, c->mc, c, c->magic, CN_MAGIC, c->remote_port);
			lprintf("rx_server_websocket: mc: %s:%d %s\n", mc->remote_ip, mc->remote_port, mc->uri);

			conn_t *cd;
			for (cd=conns; cd<&conns[RX_CHANS*2]; cd++) {
				lprintf("rx_server_websocket: CONN rx%d %s this=%p mc=%p magic=0x%x %s:%d\n",
					cd->rx_channel, streams[cd->type].uri, cd, cd->mc, cd->magic, cd->remote_ip, cd->remote_port);
			}
			return NULL;
		}
		
		return c;	// existing connection is valid
	}

	// new connection needed
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
		printf("%s conn %s rx%d %p remote_port=%d type=%d\n",
			(c->type == st->type)? "BUSY":"!TYP", streams[c->type].uri, c->rx_channel, c, c->remote_port, c->type);
	}
	
	if (c == &conns[RX_CHANS*2]) {
		printf("(too many rx channels open for %s)\n", st->uri);
		if (st->type == STREAM_WATERFALL) send_msg_mc(mc, "MSG too_busy=%d", RX_CHANS);
		return NULL;
	}

	printf("FREE conn %s rx%d %p remote_port=%d type=%d\n", streams[c->type].uri, c->rx_channel, c, c->remote_port, c->type);

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
	ndesc_init(&c->a2w);
	ndesc_init(&c->w2a);
	mc->connection_param = c;
	c->ui = find_ui(mc->local_port);
	assert(c->ui);
	c->arrival = timer_sec();
	//printf("NEW channel %d\n", c->rx_channel);

	if (st->f != NULL) {
		int id = _CreateTaskP(st->f, st->uri, st->priority, c);
		c->task = id;
	}
	
	return c;
}

#define	NSTATS_BUF	255
static char stats_buf[NSTATS_BUF+1];

volatile float audio_kbps, waterfall_kbps, waterfall_fps, http_kbps;
volatile int audio_bytes, waterfall_bytes, waterfall_frames, http_bytes;

char *rx_server_request(struct mg_connection *mc, char *buf, size_t *size)
{
	int i, j, n;
	const char *s;
	stream_t *st;
	char *op = buf, *oc = buf, *lc;
	int badp;
	int rem = NREQ_BUF;
	
	for (st=streams; st->uri; st++) {
		if (strcmp(mc->uri, st->uri) == 0)
			break;
	}
	
	if (!st->uri) return NULL;
	if (down && (st->type != STREAM_PWD)) return NULL;
	//printf("rx_server_request: uri=<%s> qs=<%s>\n", mc->uri, mc->query_string);
	
	if (mc->query_string == NULL) {
		lprintf("rx_server_request: missing query string! uri=<%s>\n", mc->uri);
		*size = snprintf(op, rem, "wrx_server_error(\"missing query string\");");
		return buf;
	}
	
	buf[0]=0;

	switch (st->type) {

	case STREAM_USERS:
		conn_t *c;
		
		for (c=conns; c<&conns[RX_CHANS*2]; c++) {
			if (c->type == STREAM_SOUND) {
				if (c->arrived && c->user != NULL) {
					n = snprintf(oc, rem, "user(%d,\"%s\",\"%s\",%d,\"%s\");",
						c->rx_channel, c->user, c->geo, c->freqHz,
						enum2str(c->mode, mode_s, ARRAY_LEN(mode_s)));
				} else {
					n = snprintf(oc, rem, "user(%d,\"\",\"\",0,\"\");", c->rx_channel);
				}
				if (!rem || rem < n) { *oc = 0; break; } else { oc += n; rem -= n; }
			}
		}
		
		// statistics
		int stats, config;
		stats=0; config=0;
		n = sscanf(mc->query_string, "stats=%d&config=%d", &stats, &config);
		//printf("USR n=%d stats=%d config=%d <%s>\n", n, stats, config, mc->query_string);
		
		lc = oc;	// skip entire response if we run out of room
		for (; stats; ) {	// hack so we can use 'break' statements below
			n = strlen(stats_buf);
			strcpy(oc, stats_buf);
			if (!rem || rem < n) { oc = lc; *oc = 0; break; } else { oc += n; rem -= n; }

			float sum_kbps = audio_kbps + waterfall_kbps + http_kbps;
			n = snprintf(oc, rem,
				"wrx_audio_stats(\"audio %.1f kB/s, waterfall %.1f kB/s (%.1f fps), http %.1f kB/s, total %.1f kB/s (%.1f kb/s)\");",
				audio_kbps, waterfall_kbps, waterfall_fps, http_kbps, sum_kbps, sum_kbps*8);
			if (!rem || rem < n) { *lc = 0; break; } else { oc += n; rem -= n; }

			if (config) {
				n = snprintf(oc, rem, "wrx_config(\"%d receiver channels, %d GPS channels\");", RX_CHANS, GPS_CHANS);
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
#define DX_SPACING_THRESHOLD_PX		5

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
			
			float f = dp->freq + (dp->offset / 1000.0);
			n = snprintf(oc, rem, "dx(%.3f,%.0f,%d,\'%s\'%s%s%s);", f, dp->offset, dp->flags, dp->text,
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
		cp = (char*) mc->query_string;
		
		//printf("STREAM_PWD: <%s>\n", mc->query_string);
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

// called periodically
void webserver_collect_print_stats()
{
	int nusers=0;
	conn_t *c;
	
	// print / log connections
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
			if ((c->freqHz != c->last_freqHz) || (c->mode != c->last_mode)) loguser(c, LOG_UPDATE);
			nusers++;
		}
	}

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
	
	n = snprintf(oc, rem, "wrx_cpu_stats("); oc += n; rem -= n;

	float del_user = 0;
	float del_sys = 0;
	float del_idle = 0;
	FILE *pf = popen("cat /proc/stat", "r");
	if (pf) {
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
		oc += n; rem -= n;
		last_user = user;
		last_sys = sys;
		last_idle = idle;
	}
	n = snprintf(oc, rem, "\"FPGA eCPU %.1f%%\");", ecpu_use()); oc += n; rem -= n;
	*oc = '\0';

	// collect network i/o stats
	static const float k = 1.0/1000.0/10.0;		// kbytes/sec every 10 secs
	audio_kbps = audio_bytes*k;
	waterfall_kbps = waterfall_bytes*k;
	waterfall_fps = waterfall_frames/10.0;
	http_kbps = http_bytes*k;
	audio_bytes = waterfall_bytes = waterfall_frames = http_bytes = 0;

	// report number of connected users on the hour
	time_t t;
	time(&t);
	struct tm tm;
	localtime_r(&t, &tm);
	
	if (tm.tm_hour != last_hour) {
		lprintf("(%d %s)\n", nusers, (nusers==1)? "user":"users");
		last_hour = tm.tm_hour;
	}
}
