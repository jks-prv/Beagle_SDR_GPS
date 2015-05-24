#ifndef _WEB_H_
#define _WEB_H_

#include "nbuf.h"
#include "mongoose.h"

#define	WS_OPCODE_TEXT		1
#define	WS_OPCODE_BINARY	2

#define NREQ_BUF 8192		// the dx list can easily get longer than 1K

user_iface_t *find_ui(int port);

typedef struct {
	#define CN_MAGIC 0xcafecafe
	u4_t magic;
	int type;
	struct mg_connection *mc;
	int remote_port;
	int rx_channel;
	#define NRIP 48
  	char remote_ip[NRIP];         // Max IPv6 string length is 45 characters
	int task;
	int nloop;
	char *user;
	bool isUserIP;
	char *geo;
	int freqHz, last_freqHz;
	int mode, last_mode;
	bool arrived, stop_data;
	ndesc_t a2w, w2a;
	user_iface_t *ui;
	
	// debug
	int wf_frames;
	u4_t wf_loop, wf_lock, wf_get;
	bool first_slow;
} conn_t;

#define STREAM_SOUND		0
#define STREAM_WATERFALL	1
#define STREAM_OTHERS		2
#define STREAM_DX			3
#define STREAM_PWD			4

void app_to_web(conn_t *c, char *s, int sl);

char *rx_server_request(struct mg_connection *mc, char *buf, size_t *size);
int web_to_app(conn_t *c, char *s, int sl);

void webserver_connection_cleanup(conn_t *c);

typedef enum {WS_INIT_CREATE, WS_INIT_START} ws_init_t;
void web_server_init(ws_init_t type);

#endif
