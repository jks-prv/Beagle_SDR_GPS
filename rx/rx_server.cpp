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

stream_t streams[] = {
	{ STREAM_SOUND,		"AUD",		&w2a_sound,		SND_PRIORITY },
	{ STREAM_WATERFALL,	"FFT",		&w2a_waterfall,	WF_PRIORITY },
	{ STREAM_ADMIN,		"ADM",		&w2a_admin,		ADMIN_PRIORITY },
	{ STREAM_MFG,		"MFG",		&w2a_mfg,		ADMIN_PRIORITY },
	{ STREAM_EXT,		"EXT",		&extint_w2a,	EXT_PRIORITY },
	{ STREAM_USERS,		"USR" },
	{ STREAM_DX,		"MKR" },
	{ STREAM_DX_UPD,	"UPD" },
	{ STREAM_PWD,		"PWD" },
	{ STREAM_DISCOVERY,	"DIS" },
	{ STREAM_PHOTO,		"PIX" },
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
	c->ext_rx_chan = -1;
}

void dump()
{
	int i;
	
	for (i=0; i < RX_CHANS; i++) {
		rx_chan_t *rx = &rx_chan[i];
		lprintf("RX%d en%d busy%d conn%d-%p\n", i, rx->enabled, rx->busy,
			rx->conn? rx->conn->self_idx : 9999, rx->conn? rx->conn : 0);
	}

	conn_t *cd;
	for (cd=conns, i=0; cd < &conns[N_CONNS]; cd++, i++) {
		if (cd->valid)
			lprintf("CONN%02d-%p %s rx=%d KA=%06d KC=%06d mc=%9p magic=0x%x ip=%s:%d other=%s%d %s%s\n",
				i, cd, streams[cd->type].uri, (cd->type == STREAM_EXT)? cd->ext_rx_chan : cd->rx_channel,
				cd->keep_alive, cd->keepalive_count, cd->mc, cd->magic,
				cd->remote_ip, cd->remote_port, cd->other? "CONN":"", cd->other? cd->other-conns:0,
				(cd->type == STREAM_EXT)? cd->ext->name : "", cd->stop_data? " STOP_DATA":"");
	}
	
	TaskDump(PRINTF_LOG);
	lock_dump();
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
	act.sa_flags = 0;
	sigemptyset(&act.sa_mask);
	act.sa_handler = cfg_handler;
	scall("SIGUSR1", sigaction(SIGUSR1, &act, NULL));
}

static void debug_handler(int arg)
{
	lprintf("SIGUSR1: debugging..\n");
	dump();

	struct sigaction act;
	act.sa_flags = 0;
	sigemptyset(&act.sa_mask);
	act.sa_handler = debug_handler;
	scall("SIGUSR1", sigaction(SIGUSR1, &act, NULL));
}

int inactivity_timeout_mins;
double ui_srate;

double DC_offset_I, DC_offset_Q;

void update_IQ_offsets()
{
	double offset;
	offset = cfg_float("DC_offset_I", NULL, CFG_OPTIONAL);
	if (offset != 0 ) {
		//printf("new DC_offset_I %e\n", offset);
		DC_offset_I = offset;
	}
	offset = cfg_float("DC_offset_Q", NULL, CFG_OPTIONAL);
	if (offset != 0 ) {
		//printf("new DC_offset_Q %e\n", offset);
		DC_offset_Q = offset;
	}
}

void rx_server_init()
{
	int i, j;
	
	conn_t *c = conns;
	for (i=0; i<N_CONNS; i++) {
		conn_init(c);
		ndesc_register(&c->w2a);
		ndesc_register(&c->a2w);
		c++;
	}
	
	// SIGUSR2 is now used exclusively by TaskCollect()
	struct sigaction act;
	memset(&act, 0, sizeof(act));
	
	#if 0
	sigemptyset(&act.sa_mask);
	act.sa_handler = cfg_handler;
	scall("SIGUSR1", sigaction(SIGUSR1, &act, NULL));
	#endif

	#if 1
	act.sa_handler = debug_handler;
	scall("SIGUSR1", sigaction(SIGUSR1, &act, NULL));
	#endif
	
	inactivity_timeout_mins = cfg_int("inactivity_timeout_mins", NULL, CFG_REQUIRED);
	
	i = cfg_int("max_freq", NULL, CFG_OPTIONAL);
	ui_srate = i? 32*MHz : 30*MHz;
	
	update_IQ_offsets();
	
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
		int ext_chan = c->rx_channel;
		clprintf(c, "%8.2f kHz %3s z%-2d %s%s\"%s\"%s%s%s%s %s\n", (float) c->freqHz / kHz,
			enum2str(c->mode, mode_s, ARRAY_LEN(mode_s)), c->zoom,
			ext_users[ext_chan].ext? ext_users[ext_chan].ext->name : "", ext_users[ext_chan].ext? " ":"",
			c->user, c->isUserIP? "":" ", c->isUserIP? "":c->remote_ip, c->geo? " ":"", c->geo? c->geo:"", s);
	}
	
	// we don't do anything with LOG_UPDATE and LOG_UPDATE_NC at present
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
	check_for_update(WAIT_UNTIL_NO_USERS);
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

void stream_tramp(void *param)
{
	conn_t *conn = (conn_t *) param;
	char *json = cfg_get_json(NULL);
	if (json != NULL) {
		send_msg(conn, SM_NO_DEBUG, "MSG version_maj=%d version_min=%d", VERSION_MAJ, VERSION_MIN);
		send_encoded_msg_mc(conn->mc, "MSG", "load_cfg", "%s", json);
	}
	(conn->task_func)(param);
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
	
	// if we're doing anything other than allocating (e.g. lookup, close) we should have matched above
	if (mode != WS_MODE_ALLOC)
		return NULL;
	
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
		//printf("down=%d UIP=%d stream=%s\n", down, update_in_progress, st->uri);
		if (st->type == STREAM_WATERFALL) {
			bool update = !down && update_in_progress;
			int comp_ctr = 0;
			if (update) {
				FILE *fp;
				fp = fopen("/root/" REPO_NAME "/.comp_ctr", "r");
				if (fp != NULL) {
					int n = fscanf(fp, "%d\n", &comp_ctr);
					//printf(".comp_ctr %d\n", comp_ctr);
					fclose(fp);
				}
			}
			//printf("send_msg_mc MSG down=%d comp_ctr=%d", update? 1:0, comp_ctr);
			send_msg_mc(mc, SM_NO_DEBUG, "MSG down=%d comp_ctr=%d", update? 1:0, comp_ctr);
		}

		// allow admin connections during update
		if (! (update_in_progress && st->type == STREAM_ADMIN))
			return NULL;
		//printf("down/update, but allowing connection..\n");
	}
	
	#if 0
	if (!do_sdr) {
		if (do_gps && (st->type == STREAM_WATERFALL)) {
			// display GPS data in waterfall
			;
		} else
		if (do_fft && (st->type == STREAM_WATERFALL)) {
			send_msg_mc(mc, SM_NO_DEBUG, "MSG fft");
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
	bool snd_or_wf = (st->type == STREAM_SOUND || st->type == STREAM_WATERFALL);
	
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
			if (snd_or_wf && c->type == st->type) {
				//printf("CONN-%d DUPLICATE!\n", cn);
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
			if (st->type != STREAM_SOUND) send_msg_mc(mc, SM_NO_DEBUG, "MSG too_busy=%d", RX_CHANS);
			return NULL;
		}
	}

	mc->connection_param = c;
	conn_init(c);
	c->type = st->type;
	c->other = cother;

	if (snd_or_wf) {
		int rx = -1;
		if (!cother) {
			for (i=0; i < RX_CHANS; i++) {
				//printf("RX_CHAN-%d busy %d\n", i, rx_chan[i].busy);
				if (!rx_chan[i].busy && rx == -1) rx = i;
			}
			if (rx == -1) {
				//printf("(too many rx channels open for %s)\n", st->uri);
				if (st->type == STREAM_WATERFALL) send_msg_mc(mc, SM_NO_DEBUG, "MSG too_busy=%d", RX_CHANS);
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
		c->task_func = st->f;
		int id = CreateTaskSP(stream_tramp, st->uri, c, st->priority);
		c->task = id;
	}
	
	//printf("CONN-%d <=== USE THIS ONE\n", cn);
	c->valid = true;
	return c;
}

bool rx_common_cmd(const char *name, conn_t *conn, char *cmd)
{
	int n;
	
	if (strcmp(cmd, "SET keepalive") == 0) {
		conn->keepalive_count++;
		return true;
	}

	n = strncmp(cmd, "SET save=", 9);
	if (n == 0) {
		char *json = cfg_realloc_json(strlen(cmd));		// a little bigger than necessary
		n = sscanf(cmd, "SET save=%s", json);
		assert(n == 1);
		//printf("SET save=...\n");
		int slen = strlen(json);
		mg_url_decode(json, slen, json, slen+1, 0);		// dst=src is okay because length dst always <= src
		cfg_save_json(json);
		
		// variables for C code that should be updated when configuration saved
		update_IQ_offsets();
		S_meter_cal = cfg_int("S_meter_cal", NULL, CFG_REQUIRED);
		
		return true;
	}

	int wf_comp;
	n = sscanf(cmd, "SET wf_comp=%d", &wf_comp);
	if (n == 1) {
		w2a_waterfall_compression(conn->rx_channel, wf_comp? true:false);
		printf("### SET wf_comp=%d\n", wf_comp);
		return true;
	}

	if (strncmp(cmd, "SERVER DE CLIENT", 16) == 0) return true;
	
	// we see these sometimes; not part of our protocol
	if (strcmp(cmd, "PING") == 0) return true;

	// we see these at the close of a connection; not part of our protocol
	if (strcmp(cmd, "?") == 0) return true;

	return false;
}
