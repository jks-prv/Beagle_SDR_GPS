/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>                      // fprintf
#include <string.h>                     // strdup, strerror
#include <unistd.h>                     // close
#include <errno.h>                      // errno
#include <sys/types.h>                  // socket, connect
#include <sys/socket.h>                 // socket, connect
#include <netdb.h>                      // getaddrinfo
#include "output-common.h"              // output_descriptor_t, output_qentry_t, output_queue_drain
#include "kvargs.h"                     // kvargs, option_descr_t
#include "util.h"                       // ASSERT

typedef struct {
	char *address;
	char *port;
	int sockfd;
} out_udp_ctx_t;

static bool out_udp_supports_format(output_format_t format) {
	return(format == OFMT_TEXT || format == OFMT_BASESTATION);
}

static void *out_udp_configure(kvargs *kv) {
	ASSERT(kv != NULL);
	NEW(out_udp_ctx_t, cfg);
	if(kvargs_get(kv, "address") == NULL) {
		fprintf(stderr, "output_udp: IP address not specified\n");
		goto fail;
	}
	cfg->address = strdup(kvargs_get(kv, "address"));
	if(kvargs_get(kv, "port") == NULL) {
		fprintf(stderr, "output_udp: UDP port not specified\n");
		goto fail;
	}
	cfg->port = strdup(kvargs_get(kv, "port"));
	return cfg;
fail:
	XFREE(cfg);
	return NULL;
}

static int out_udp_init(void *selfptr) {
	ASSERT(selfptr != NULL);
	out_udp_ctx_t *self = selfptr;

	struct addrinfo hints, *result, *rptr;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = 0;
	hints.ai_protocol = 0;
	int ret = getaddrinfo(self->address, self->port, &hints, &result);
	if(ret != 0) {
		fprintf(stderr, "output_udp: could not resolve %s: %s\n", self->address, gai_strerror(ret));
		return -1;
	}
	for (rptr = result; rptr != NULL; rptr = rptr->ai_next) {
		self->sockfd = socket(rptr->ai_family, rptr->ai_socktype, rptr->ai_protocol);
		if(self->sockfd == -1) {
			continue;
		}
		if(connect(self->sockfd, rptr->ai_addr, rptr->ai_addrlen) != -1) {
			break;
		}
		close(self->sockfd);
		self->sockfd = 0;
	}
	if (rptr == NULL) {
		fprintf(stderr, "output_udp: Could not set up UDP socket to %s:%s: all addresses failed\n",
				self->address, self->port);
		self->sockfd = 0;
		return -1;
	}
	freeaddrinfo(result);
	return 0;
}

static void out_udp_produce_text(out_udp_ctx_t *self, struct metadata *metadata, struct octet_string *msg) {
	UNUSED(metadata);
	ASSERT(msg != NULL);
	ASSERT(self->sockfd != 0);
	if(msg->len < 2) {
		return;
	}
	if(write(self->sockfd, msg->buf, msg->len) < 0) {
		debug_print(D_OUTPUT, "output_udp: error while writing to the network socket: %s", strerror(errno));
	}
}

static int out_udp_produce(void *selfptr, output_format_t format, struct metadata *metadata, struct octet_string *msg) {
	ASSERT(selfptr != NULL);
	out_udp_ctx_t *self = selfptr;
	if(format == OFMT_TEXT || format == OFMT_BASESTATION) {
		out_udp_produce_text(self, metadata, msg);
	}
	return 0;
}

static void out_udp_handle_shutdown(void *selfptr) {
	ASSERT(selfptr != NULL);
	out_udp_ctx_t *self = selfptr;
	fprintf(stderr, "output_udp(%s:%s): shutting down\n", self->address, self->port);
	close(self->sockfd);
}

static void out_udp_handle_failure(void *selfptr) {
	ASSERT(selfptr != NULL);
	out_udp_ctx_t *self = selfptr;
	fprintf(stderr, "output_udp: can't connect to %s:%s, deactivating output\n",
			self->address, self->port);
	close(self->sockfd);
}

static const option_descr_t out_udp_options[] = {
	{
		.name = "address",
		.description = "Destination host name or IP address (required)"
	},
	{
		.name = "port",
		.description = "Destination UDP port (required)"
	},
	{
		.name = NULL,
		.description = NULL
	}
};

output_descriptor_t out_DEF_udp = {
	.name = "udp",
	.description = "Output to a remote host via UDP",
	.options = out_udp_options,
	.supports_format = out_udp_supports_format,
	.configure = out_udp_configure,
	.init = out_udp_init,
	.produce = out_udp_produce,
	.handle_shutdown = out_udp_handle_shutdown,
	.handle_failure = out_udp_handle_failure
};
