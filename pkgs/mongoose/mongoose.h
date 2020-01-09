// Copyright (c) 2004-2013 Sergey Lyubka <valenok@gmail.com>
// Copyright (c) 2013-2014 Cesanta Software Limited
// All rights reserved
//
// This library is dual-licensed: you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation. For the terms of this
// license, see <http://www.gnu.org/licenses/>.
//
// You are free to use this library under the terms of the GNU General
// Public License, but WITHOUT ANY WARRANTY; without even the implied
// warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// See the GNU General Public License for more details.
//
// Alternatively, you can license this library under a commercial
// license, as set out in <http://cesanta.com/>.
//
// NOTE: Detailed API documentation is at http://cesanta.com/#docs

#ifndef MONGOOSE_HEADER_INCLUDED
#define  MONGOOSE_HEADER_INCLUDED

#define MONGOOSE_VERSION "5.3"

#include <stdio.h>      // required for FILE
#include <stddef.h>     // required for size_t
#include <sys/stat.h>   // required for struct stat

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

// This structure contains information about HTTP request.
struct mg_connection {
  const char *request_method; // "GET", "POST", etc
  const char *uri;            // URL-decoded URI
  const char *http_version;   // E.g. "1.0", "1.1"
  const char *query_string;   // URL part after '?', not including '?', or NULL

  char remote_ip[48];         // Max IPv6 string length is 45 characters
  const char *local_ip;       // Local IP address
  unsigned short remote_port; // Client's port
  unsigned short local_port;  // Local port number

  int num_headers;            // Number of HTTP headers
  struct mg_header {
    const char *name;         // HTTP header name
    const char *value;        // HTTP header value
  } http_headers[30];

  char *content;              // POST (or websocket message) data, or NULL
  size_t content_len;		  // content length

  int is_websocket;           // Connection is a websocket connection
  int status_code;            // HTTP status code for HTTP error handler
  int wsbits;                 // First byte of the websocket frame
  void *server_param;         // Parameter passed to mg_add_uri_handler()

  struct mg_cache {			  // cache info for non-filesystem stored data
    struct stat st;
    int cached;
    bool if_none_match;
      bool etag_match;
      #define N_ETAG 64
      char etag_server[N_ETAG], etag_client[N_ETAG];
    bool if_mod_since;
      bool not_mod_since;
      time_t server_mtime, client_mtime;
  } cache_info;

  void *connection_param;     // Placeholder for connection-specific data
};

struct mg_server; // Opaque structure describing server instance
enum mg_result { MG_FALSE, MG_TRUE };
enum mg_event {
  MG_POLL = 100,	// Callback return value is ignored
  MG_CONNECT,		// If callback returns MG_FALSE, connect fails
  MG_AUTH,			// If callback returns MG_FALSE, authentication fails
  MG_REQUEST,		// If callback returns MG_FALSE, Mongoose continues with req
  MG_REPLY,			// If callback returns MG_FALSE, Mongoose closes connection
  MG_CLOSE,			// Connection is closed
  MG_CACHE_INFO,	// Ask callback to return caching info
  MG_CACHE_RESULT,	// Report caching decision result
  MG_HTTP_ERROR		// If callback returns MG_FALSE, Mongoose continues with err
};
typedef int (*mg_handler_t)(struct mg_connection *, enum mg_event);

// Server management functions
struct mg_server *mg_create_server(void *server_param, mg_handler_t handler);
void mg_destroy_server(struct mg_server **);
const char *mg_set_option(struct mg_server *, const char *opt, const char *val);
int mg_poll_server(struct mg_server *, int milliseconds);
const char **mg_get_valid_option_names(void);
const char *mg_get_option(const struct mg_server *server, const char *name);
void mg_set_listening_socket(struct mg_server *, int sock);
int mg_get_listening_socket(struct mg_server *);
void mg_iterate_over_connections(struct mg_server *, mg_handler_t);
void mg_wakeup_server(struct mg_server *);
struct mg_connection *mg_connect(struct mg_server *, const char *, int, int);

// Connection management functions
void mg_send_status(struct mg_connection *, int status_code);
void mg_send_header(struct mg_connection *, const char *name, const char *val);
void mg_send_standard_headers(struct mg_connection *, const char *path, struct stat *,
	const char *msg, char *range, bool more_headers_to_follow);
void mg_send_data(struct mg_connection *, const void *data, int data_len);
void mg_printf_data(struct mg_connection *, const char *format, ...);

int mg_websocket_write(struct mg_connection *, int opcode,
                       const char *data, size_t data_len);

// Deprecated in favor of mg_send_* interface
int mg_write(struct mg_connection *, const void *buf, int len);
int mg_printf(struct mg_connection *conn, const char *fmt, ...);

const char *mg_get_header(const struct mg_connection *, const char *name);
const char *mg_get_mime_type(const char *name, const char *default_mime_type);
int mg_get_var(const struct mg_connection *conn, const char *var_name,
               char *buf, size_t buf_len);
int mg_parse_header(const char *hdr, const char *var_name, char *buf, size_t);
int mg_parse_multipart(const char *buf, int buf_len,
                       char *var_name, int var_name_len,
                       char *file_name, int file_name_len,
                       const char **data, int *data_len);

// Utility functions
void mg_url_encode(const char *src, char *dst, size_t dst_len);
int mg_url_decode(const char *src, int src_len, char *dst,
                  int dst_len, int is_form_url_encoded);
void *mg_start_thread(void *(*func)(void *), void *param);
char *mg_md5(char buf[33], ...);
int mg_authorize_digest(struct mg_connection *c, FILE *fp);
void mg_remove_double_dots_and_double_slashes(char *s);
void mg_bin2str(char *to, const unsigned char *p, size_t len);
void mg_str2bin(unsigned char *to, size_t len, const char *p);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // MONGOOSE_HEADER_INCLUDED
