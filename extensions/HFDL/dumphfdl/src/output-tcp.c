/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>                      // fprintf
#include <string.h>                     // strdup, strerror
#include <unistd.h>                     // close
#include <errno.h>                      // errno
#include <sys/time.h>                   // struct timeval
#include <time.h>                       // time_t, time
#include <sys/types.h>                  // socket, connect
#include <sys/socket.h>                 // socket, connect
#include <netdb.h>                      // getaddrinfo
#include "output-common.h"              // output_descriptor_t, output_qentry_t, output_queue_drain
#include "kvargs.h"                     // kvargs, option_descr_t
#include "util.h"                       // ASSERT

// Number of seconds to wait before re-establishing the connection
// after the last error.
#define MIN_RECONNECT_INTERVAL 10
// Send socket operations timeout
#define SOCKET_SEND_TIMEOUT 5

// Forward declarations
static void out_tcp_handle_shutdown(void *selfptr);

typedef struct {
	char *address;
	char *port;
	time_t next_reconnect_time;
	int32_t sockfd;
} out_tcp_ctx_t;

static bool out_tcp_supports_format(output_format_t format) {
	return(format == OFMT_TEXT || format == OFMT_BASESTATION);
}

static void *out_tcp_configure(kvargs *kv) {
	ASSERT(kv != NULL);
	NEW(out_tcp_ctx_t, cfg);
	if(kvargs_get(kv, "address") == NULL) {
		fprintf(stderr, "output_tcp: address not specified\n");
		goto fail;
	}
	cfg->address = strdup(kvargs_get(kv, "address"));
	if(kvargs_get(kv, "port") == NULL) {
		fprintf(stderr, "output_tcp: port not specified\n");
		goto fail;
	}
	cfg->port = strdup(kvargs_get(kv, "port"));
	return cfg;
fail:
	XFREE(cfg);
	return NULL;
}

static void out_tcp_ctx_destroy(void *ctxptr) {
	if(ctxptr != NULL) {
		out_tcp_ctx_t *ctx = ctxptr;
		XFREE(ctx->address);
		XFREE(ctx->port);
		XFREE(ctx);
	}
}

static int32_t out_tcp_reconnect(void *selfptr) {
	ASSERT(selfptr != NULL);
	out_tcp_ctx_t *self = selfptr;

	if(self->next_reconnect_time > time(NULL)) {
		return -1;
	}
	static struct timeval const timeout = { .tv_sec = SOCKET_SEND_TIMEOUT, .tv_usec = 0 };
	struct addrinfo hints, *result, *rptr;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = 0;
	hints.ai_protocol = 0;
	fprintf(stderr, "Connecting to %s:%s...\n", self->address, self->port);
	int32_t ret = getaddrinfo(self->address, self->port, &hints, &result);
	if(ret != 0) {
		fprintf(stderr, "Could not resolve address %s:%s: %s\n", self->address, self->port,
				gai_strerror(ret));
		goto fail;
	}
	for (rptr = result; rptr != NULL; rptr = rptr->ai_next) {
		self->sockfd = socket(rptr->ai_family, rptr->ai_socktype, rptr->ai_protocol);
		if(self->sockfd == -1) {
			continue;
		}
		if(setsockopt(self->sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) {
			fprintf(stderr, "Could not set timeout on socket: %s\n", strerror(errno));
		}

		if(connect(self->sockfd, rptr->ai_addr, rptr->ai_addrlen) != -1) {
			fprintf(stderr, "Connection to %s:%s established\n", self->address, self->port);
			break;
		}
		close(self->sockfd);
		self->sockfd = 0;
	}
	if (rptr == NULL) {
		fprintf(stderr, "Could not connect to %s:%s: all addresses failed\n",
				self->address, self->port);
		self->sockfd = 0;
		goto fail;
	}
	freeaddrinfo(result);
	self->next_reconnect_time = 0;
	return 0;
fail:
	freeaddrinfo(result);
	self->next_reconnect_time = time(NULL) + MIN_RECONNECT_INTERVAL;
	return -1;
}

static int32_t out_tcp_init(void *selfptr) {
	ASSERT(selfptr != NULL);
	out_tcp_ctx_t *self = selfptr;
	self->next_reconnect_time = 0;      // Force reconnection now
	out_tcp_reconnect(selfptr);
	// Always return success, even when connection failed - otherwise the output thread would
	// declare the error as fatal and disable the output without giving us a change to reestablish
	// the connection.
	return 0;
}

static int32_t out_tcp_produce_text(out_tcp_ctx_t *self, struct metadata *metadata, struct octet_string *msg) {
	UNUSED(metadata);
	ASSERT(msg != NULL);
	ASSERT(self->sockfd != 0);
	if(msg->len < 1) {
		return 0;
	}
	if(write(self->sockfd, msg->buf, msg->len) < 0) {
		return -1;
	}
	return 0;
}

static int32_t out_tcp_produce(void *selfptr, output_format_t format, struct metadata *metadata, struct octet_string *msg) {
	ASSERT(selfptr != NULL);
	out_tcp_ctx_t *self = selfptr;
	int32_t result = 0;
	if(self->sockfd == 0) {         // No connection?
		if(out_tcp_reconnect(selfptr) < 0) {
			return 0;               // If unable to reconnect, then discard the message silently
		}
	}
	if(format == OFMT_TEXT || format == OFMT_BASESTATION) {
		result = out_tcp_produce_text(self, metadata, msg);
	}
	if(result < 0) {
		// Possible connection failure. Close it and reschedule an immediate reconnection.
		fprintf(stderr, "Error while sending to %s:%s: %s\n", self->address, self->port, strerror(errno));
		out_tcp_handle_shutdown(selfptr);
		self->next_reconnect_time = 0;
	}
	// Always return success, even on send error - otherwise the output thread would declare
	// the error as fatal and disable the output without giving us a change to reestablish
	// the connection.
	return 0;
}

static void out_tcp_handle_shutdown(void *selfptr) {
	ASSERT(selfptr != NULL);
	out_tcp_ctx_t *self = selfptr;
	close(self->sockfd);
	self->sockfd = 0;
	fprintf(stderr, "Connection to %s:%s closed\n", self->address, self->port);
}

static void out_tcp_handle_failure(void *selfptr) {
	ASSERT(selfptr != NULL);
	out_tcp_ctx_t *self = selfptr;
	fprintf(stderr, "Could not connect to %s:%s, deactivating output\n",
			self->address, self->port);
	close(self->sockfd);
	self->sockfd = 0;
}

static const option_descr_t out_tcp_options[] = {
	{
		.name = "address",
		.description = "Destination host name or IP address (required)"
	},
	{
		.name = "port",
		.description = "Destination TCP port (required)"
	},
	{
		.name = NULL,
		.description = NULL
	}
};

output_descriptor_t out_DEF_tcp = {
	.name = "tcp",
	.description = "Output to a remote host via TCP",
	.options = out_tcp_options,
	.supports_format = out_tcp_supports_format,
	.configure = out_tcp_configure,
	.ctx_destroy = out_tcp_ctx_destroy,
	.init = out_tcp_init,
	.produce = out_tcp_produce,
	.handle_shutdown = out_tcp_handle_shutdown,
	.handle_failure = out_tcp_handle_failure
};
