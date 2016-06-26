#ifndef _WEB_H_
#define _WEB_H_

#include "config.h"
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

#define	N_ADMIN	4
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
	u4_t keep_alive, keepalive_count;
	bool stop_data;
	user_iface_t *ui;

	// set only in STREAM_SOUND
	bool arrived, inactivity_timeout, inactivity_msg_sent, inactivity_timeout_override;
	int freqHz, last_freqHz;
	int mode, last_mode;
	int zoom, last_zoom;	// zoom set in both
	int last_tune_time, last_log_time;
	
	u4_t arrival;
	int nloop;
	char *user;
	bool isUserIP;
	char *geo;
	
	// debug
	int wf_frames;
	u4_t wf_loop, wf_lock, wf_get;
	bool first_slow;
	u4_t audio_underrun, sequence_errors;

	#ifdef SND_SEQ_CHECK
		bool audio_check;
		u4_t audio_pkts_sent, audio_epoch, audio_last_time;
		u2_t audio_sequence;
		s4_t sum2;
	#endif
};

// conn_t.type
#define STREAM_SOUND		0
#define STREAM_WATERFALL	1
#define STREAM_ADMIN		2
#define STREAM_MFG			3
#define STREAM_USERS		4
#define STREAM_DX			5
#define STREAM_DX_UPD		6
#define STREAM_PWD			7
#define STREAM_DISCOVERY	8
#define STREAM_SDR_HU		9

struct ddns_t {
	bool valid, pvt_valid, pub_valid;
	u4_t serno;
	char ip_pub[64], ip_pvt[64];
	int port;
	u4_t netmask;
	char mac[64];
};

extern ddns_t ddns;

void app_to_web(conn_t *c, char *s, int sl);

char *rx_server_request(struct mg_connection *mc, char *buf, size_t *size);
int web_to_app(conn_t *c, nbuf_t **nbp);
void web_to_app_done(conn_t *c, nbuf_t *nb);

void webserver_connection_cleanup(conn_t *c);

typedef enum {WS_INIT_CREATE, WS_INIT_START} ws_init_t;
void web_server_init(ws_init_t type);

#define SVCS_RESTART_TRUE	true
#define SVCS_RESTART_FALSE	false
void services_start(bool restart);

void dynamic_DNS(void *param);
bool isLocal_IP(u4_t ip);

void reload_index_params();

#endif
