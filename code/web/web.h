#ifndef _WEB_H_
#define _WEB_H_

#include "nbuf.h"
#include "mongoose.h"

#define	WS_OPCODE_TEXT		1
#define	WS_OPCODE_BINARY	2

#define NREQ_BUF (16*1024)		// the dx list can easily get longer than 1K

user_iface_t *find_ui(int port);

struct conn_t;

struct rx_chan_t {
	bool enabled;
	bool busy;
	conn_t *conn;
};

#define	N_ADMIN	1
#define	N_CONNS	(RX_CHANS*2 + N_ADMIN)

struct conn_t {
	#define CN_MAGIC 0xcafecafe
	u4_t magic;
	conn_t *self;
	int self_idx;
	bool valid;
	int type;
	conn_t *other;
	int rx_channel;
	struct mg_connection *mc;

	#define NRIP 48
	char remote_ip[NRIP];         // Max IPv6 string length is 45 characters
	int remote_port;
	u64_t tstamp;
	ndesc_t a2w, w2a;

	// set in both STREAM_SOUND & STREAM_WATERFALL
	int task;
	bool stop_data;
	user_iface_t *ui;
	u4_t arrival;

	// set only in STREAM_SOUND
	bool arrived;
	int freqHz, last_freqHz;
	int mode, last_mode;
	int zoom, last_zoom;	// zoom set in both
	int last_tune_time;

	int nloop;
	char *user;
	bool isUserIP;
	char *geo;
	
	// debug
	int wf_frames;
	u4_t wf_loop, wf_lock, wf_get;
	bool first_slow;
};

// conn_t.type
#define STREAM_SOUND		0
#define STREAM_WATERFALL	1
#define STREAM_ADMIN		2
#define STREAM_USERS		3
#define STREAM_DX			4
#define STREAM_DX_UPD		5
#define STREAM_PWD			6
#define STREAM_STATUS		7

void app_to_web(conn_t *c, char *s, int sl);

char *rx_server_request(struct mg_connection *mc, char *buf, size_t *size);
int web_to_app(conn_t *c, char *s, int sl);

void webserver_connection_cleanup(conn_t *c);

typedef enum {WS_INIT_CREATE, WS_INIT_START} ws_init_t;
void web_server_init(ws_init_t type);

#endif
