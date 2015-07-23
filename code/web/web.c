#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>

#define _WRX_CONFIG_

#include "wrx.h"
#include "types.h"
#include "config.h"
#include "wrx.h"
#include "misc.h"
#include "timer.h"
#include "web.h"
#include "coroutines.h"
#include "mongoose.h"
#include "nbuf.h"

user_iface_t *find_ui(int port)
{
	user_iface_t *ui = user_iface;
	while (ui->port) {
		if (ui->port == port)
			return ui;
		ui++;
	}
	return NULL;
}

void webserver_connection_cleanup(conn_t *c)
{
	nbuf_cleanup(&c->w2a);
	nbuf_cleanup(&c->a2w);
}


// w2a
// web to app communication (client to server)
//		e.g. SET commands (no return data), and GET/POST for file transfer and commands with return data
// 1) browser => (websocket) => webserver => (mutex memcpy) => app  [half-duplex]
// 2) browser => (GET/POST) => webserver => (file, rx_server_request) => app


extern const char *edata_embed(const char *, size_t *);
extern const char *edata_always(const char *, size_t *);

static const char* edata(const char *uri, size_t *size, char **free_buf) {
	const char* data = NULL;
	
#ifdef EDATA_EMBED
	// the normal background daemon loads files from in-memory embedded data
	data = edata_embed(uri, size);
#endif

	// some large, seldom-changed files are always loaded from memory
	if (!data) data = edata_always(uri, size);

#ifdef EDATA_DEVEL
	// to speed edit-copy-compile-debug development, load the files from the local filesystem
	static bool init;
	if (!init) {
		scall("chdir web", chdir("web"));
		init = true;
	}

	// try as a local file
	if (!data) {
		int fd = open(uri, O_RDONLY);
		if (fd >= 0) {
			struct stat st;
			fstat(fd, &st);
			*size = st.st_size;
			data = (char *) wrx_malloc("file", *size);
			*free_buf = (char *) data;
			ssize_t rsize = read(fd, (void *) data, *size);
			assert(rsize == *size);
			close(fd);
		}
	}
#endif

	return data;
}

static int request(struct mg_connection *mc) {
	size_t edata_size=0;
	const char *edata_data;
	char *free_buf = NULL;

	if (mc->is_websocket) {
		// This handler is called for each incoming websocket frame, one or more
		// times for connection lifetime.
		char *s = mc->content;
		int sl = mc->content_len;
		//printf("WEBSOCKET: len %d uri <%s>\n", sl, mc->uri);
		if (sl == 0) {
			//printf("----KA %d\n", mc->remote_port);
			return MG_TRUE;	// keepalive?
		}
		
		conn_t *c = rx_server_websocket(mc);
		if (c == NULL) return MG_FALSE;
		if (c->stop_data) return MG_FALSE;
		
		s[sl]=0;
		//printf("WEBSOCKET: %d <%s> ", sl, s);
		nbuf_allocq(&c->w2a, s, sl);
		
		if (mc->content_len == 4 && !memcmp(mc->content, "exit", 4)) {
			//printf("----EXIT %d\n", mc->remote_port);
			return MG_FALSE;
		} else {
			return MG_TRUE;
		}
	} else {
		if (strcmp(mc->uri, "/") == 0) mc->uri = "index.html"; else
		if (mc->uri[0] == '/') mc->uri++;
		
		char *ouri = (char *) mc->uri;
		char *uri = ouri;
		bool free_uri = FALSE;
		
		if (strncmp(ouri, "wrx/", 4) == 0) {
			uri = (char *) &mc->uri[4];
		} else {
			user_iface_t *ui = find_ui(mc->local_port);
			// should never not find match since we only listen to ports in ui table
			assert(ui);
			asprintf(&uri, "%s/%s", ui->name, ouri);
			free_uri = TRUE;
		}
		//printf("---- HTTP: uri %s (%s)\n", ouri, uri);

		// try as file from in-memory embedded data
		edata_data = edata(uri, &edata_size, &free_buf);
		
		// try as request from browser
		if (!edata_data) {
			free_buf = (char*) wrx_malloc("req", NREQ_BUF);
			edata_data = rx_server_request(mc, free_buf, &edata_size);	// mc->uri is ouri without ui->name prefix
			if (!edata_data) { wrx_free("req", free_buf); free_buf = NULL; }
		}

		if (!edata_data) {
			printf("unknown URL: %s (%s) %s\n", ouri, uri, mc->query_string);
			return MG_FALSE;
		}
		
		// for index.html process %[substitution]
		if (strcmp(ouri, "index.html") == 0) {
			static bool index_init;
			static char *index_html, *index_buf;
			static size_t index_size;

#ifdef EDATA_EMBED
			if (!index_init) {		// only have to do once
#else
			if (true) {		// file might change anytime during development
#endif
				if (!index_buf) index_buf = (char*) wrx_malloc("index_buf", edata_size*3/2);
				char *cp = (char*) edata_data, *np = index_buf, *pp;
				int i, cl, sl, nl=0, pl;

				for (cl=0; cl < edata_size;) {
					if (*cp == '%' && *(cp+1) == '[') {
						cp += 2; cl += 2; pp = cp; pl = 0;
						while (*cp != ']' && cl < edata_size) { cp++; cl++; pl++; }
						cp++; cl++;
						for (i=0; i < ARRAY_LEN(index_html_params); i++) {
							index_html_params_t *ip = &index_html_params[i];
							if (strncmp(pp, ip->param, pl) == 0) {
								sl = strlen(ip->value);
								strcpy(np, ip->value); np += sl;
								break;
							}
						}
						if (i == ARRAY_LEN(index_html_params)) {
							// not found, put back original
							strcpy(np, "%["); np += 2;
							strncpy(np, pp, pl); np += pl;
							*np++ = ']';
						}
					} else {
						*np++ = *cp++; cl++;
					}
				}
				
				index_html = index_buf;
				index_size = np - index_buf;
				index_init = true;
			}
			edata_data = index_html;
			edata_size = index_size;
		}

		//printf("DATA: %s %d ", mc->uri, (int) edata_size);
		mg_send_header(mc, "Content-Type", mg_get_mime_type(mc->uri, "text/plain"));
		mg_send_data(mc, edata_data, edata_size);
		
		if (free_uri) free(uri);
		if (free_buf) wrx_free("req", free_buf);
		
		http_bytes += edata_size;
		return MG_TRUE;
	}
}

static int ev_handler(struct mg_connection *mc, enum mg_event ev) {
  int r;
  
  //printf("ev_handler %d:%d len %d ", mc->local_port, mc->remote_port, (int) mc->content_len);
  if (ev == MG_REQUEST) {
  	//printf("MG_REQUEST: URI:%s query:%s\n", mc->uri, mc->query_string);
    r = request(mc);
    //printf("\n");
    return r;
  } else
  if (ev == MG_AUTH) {
  	//printf("MG_AUTH\n");
    return MG_TRUE;
  } else {
  	//printf("MG_OTHER\n");
    return MG_FALSE;
  }
}

int web_to_app(conn_t *c, char *s, int sl)
{
	nbuf_t *nb;
	
	if (c->stop_data) return 0;
	nb = nbuf_dequeue(&c->w2a);
	if (!nb) return 0;
	assert(!nb->done && nb->buf && nb->len);
	memcpy(s, nb->buf, MIN(nb->len, sl));
	nb->done = TRUE;
	NextTaskL("w2a");
	
	return nb->len;
}


// a2w
// app to web communication (server to client) e.g. audio, waterfall and MSG data
// app => (mutex memcpy) => webserver => (websocket) => browser  [half-duplex]

void app_to_web(conn_t *c, char *s, int sl)
{
	if (c->stop_data) return;
	nbuf_allocq(&c->a2w, s, sl);
	NextTaskL("a2w");
}

static int iterate_callback(struct mg_connection *mc, enum mg_event ev)
{
	int ret;
	nbuf_t *nb;
	
	if (ev == MG_POLL && mc->is_websocket) {
		conn_t *c = (conn_t*) mc->connection_param;
		if (c == NULL)  return MG_FALSE;

		// fixme: not reliable
		if (c->magic != CN_MAGIC) {
			printf("iterate_callback: bad CN_MAGIC 0x%x: c=%p mc=%s:%d\n", c->magic, c, mc->remote_ip, mc->remote_port);
			mc->connection_param = NULL;
			return MG_FALSE;
		}

		while (TRUE) {
			if (c->stop_data) break;
			nb = nbuf_dequeue(&c->a2w);
			//printf("a2w CHK port %d nb %p\n", mc->remote_port, nb);
			
			if (nb) {
				assert(!nb->done && nb->buf && nb->len);
//if (c->type == STREAM_SOUND) printf("s%d ", mc->remote_port);
//if (c->type == STREAM_WATERFALL) printf("w%d ", mc->remote_port);
//fflush(stdout);
				//printf("a2w %d WEBSOCKET: %d %p\n", mc->remote_port, nb->len, nb->buf);
				ret = mg_websocket_write(mc, WS_OPCODE_BINARY, nb->buf, nb->len);
				if (ret<=0) printf("$$$$$$$$ socket write ret %d\n", ret);
				nb->done = TRUE;
			} else {
				break;
			}
		}
	} else {
		if (ev != MG_POLL) printf("$$$$$$$$ a2w %d OTHER: %d len %d\n", mc->remote_port, (int) ev, (int) mc->content_len);
	}
	
	NextTaskL("web callback");
	
	return MG_TRUE;
}

void web_server(void *param)
{
	user_iface_t *ui = (user_iface_t *) param;
	struct mg_server *server = ui->server;
	const char *err;
	u4_t current_timer = 0, last_timer = timer_ms();
	
	while (1) {
		mg_poll_server(server, 0);		// passing 0 effects a poll
		current_timer = timer_ms();
		
		if (time_diff(current_timer, last_timer) > WEB_SERVER_POLL_MS) {
			last_timer = current_timer;
			mg_iterate_over_connections(server, iterate_callback);
		}
		
		NextTaskL("web server");
	}
	
	//mg_destroy_server(&server);
}

void web_server_init(ws_init_t type)
{
	user_iface_t *ui = user_iface;
	static bool init;
	
	if (!init) {
		nbuf_init();
		init = TRUE;
	}
	
	if (type == WS_INIT_START) {
		// send private/public ip addrs to registry
		FILE *pf = popen("hostname -i", "r");
		char ip_pvt[64];
		fscanf(pf, "%16s", ip_pvt);
		pclose(pf);
		
		char ip_pub[64];
		pf = popen("curl -s ident.me", "r");
		fscanf(pf, "%16s", ip_pub);
		pclose(pf);
	
		char *bp;
		asprintf(&bp, "curl -s -o /dev/null http://%s/php/register.php?reg=%d.%s.%d.%s",
			LOGGING_HOST, SERIAL_NUMBER, ip_pvt, ui->port, ip_pub);
		lprintf("private ip: %s public ip: %s\n", ip_pvt, ip_pub);
		system(bp);
		free(bp);
	}

	if (type == WS_INIT_CREATE) {
		// if specified, override the port number of the first UI
		if (port_override) {
			lprintf("overriding port from %d -> %d for \"%s\"\n",
				user_iface[0].port, port_override, user_iface[0].name);
			user_iface[0].port = port_override;
		}
	}

	// create webserver port(s)
	while (ui->port) {
		if (type == WS_INIT_CREATE) {
			ui->server = mg_create_server(NULL, ev_handler);
			char *s_port;
			asprintf(&s_port, "%d", ui->port);
			if (mg_set_option(ui->server, "listening_port", s_port) != NULL) {
				lprintf("network port %d for \"%s\" in use\n", ui->port, ui->name);
				lprintf("app already running in background?\ntry \"make stop\" (or \"m stop\") first\n");
				xit(-1);
			}
			lprintf("webserver for \"%s\" on port %s\n", ui->name, mg_get_option(ui->server, "listening_port"));
			free(s_port);

		} else {	// WS_INIT_START
			CreateTaskP(&web_server, LOW_PRIORITY, ui);
		}
		ui++;
	}
}
