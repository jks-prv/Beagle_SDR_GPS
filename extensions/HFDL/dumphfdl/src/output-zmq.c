/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifdef WITH_ZMQ
#include <stdio.h>                      // fprintf
#include <string.h>                     // strdup, strerror
#include <errno.h>                      // errno
#include <zmq.h>                        // zmq_*
#include "hfdl_config.h"                     // LIBZMQ_VER_*
#include "output-common.h"              // output_descriptor_t, output_qentry_t, output_queue_drain
#include "kvargs.h"                     // kvargs
#include "options.h"                    // option_descr_t
#include "util.h"                       // ASSERT, NEW

typedef enum {
	ZMQ_MODE_SERVER,
	ZMQ_MODE_CLIENT
} out_zmq_mode_t;

typedef struct {
	char *endpoint;
	void *zmq_ctx;
	void *zmq_sock;
	out_zmq_mode_t mode;
} out_zmq_ctx_t;

static bool out_zmq_supports_format(output_format_t format) {
	return(format == OFMT_TEXT || format == OFMT_BASESTATION);
}

static void *out_zmq_configure(kvargs *kv) {
	ASSERT(kv != NULL);
	NEW(out_zmq_ctx_t, cfg);

	int major, minor, patch;
	zmq_version(&major, &minor, &patch);
	if((major * 1000000 + minor * 1000 + patch) < (LIBZMQ_VER_MAJOR_MIN * 1000000 + LIBZMQ_VER_MINOR_MIN * 1000 + LIBZMQ_VER_PATCH_MIN)) {
		fprintf(stderr, "output_zmq: error: libzmq library version %d.%d.%d is too old; at least %d.%d.%d is required\n",
				major, minor, patch,
				LIBZMQ_VER_MAJOR_MIN, LIBZMQ_VER_MINOR_MIN, LIBZMQ_VER_PATCH_MIN);
		goto fail;
	}
	if(kvargs_get(kv, "endpoint") == NULL) {
		fprintf(stderr, "output_zmq: endpoint not specified\n");
		goto fail;
	}
	cfg->endpoint = strdup(kvargs_get(kv, "endpoint"));
	char *mode = NULL;
	if((mode = kvargs_get(kv, "mode")) == NULL) {
		fprintf(stderr, "output_zmq: mode not specified\n");
		goto fail;
	}
	if(!strcmp(mode, "server")) {
		cfg->mode = ZMQ_MODE_SERVER;
	} else if(!strcmp(mode, "client")) {
		cfg->mode = ZMQ_MODE_CLIENT;
	} else {
		fprintf(stderr, "output_zmq: mode '%s' is invalid; must be either 'client' or 'server'\n", mode);
		goto fail;
	}
	return cfg;
fail:
	XFREE(cfg);
	return NULL;
}

static int out_zmq_init(void *selfptr) {
	ASSERT(selfptr != NULL);
	out_zmq_ctx_t *self = selfptr;

	self->zmq_ctx = zmq_ctx_new();
	if(self->zmq_ctx == NULL) {
		fprintf(stderr, "output_zmq(%s): failed to set up ZMQ context\n", self->endpoint);
		return -1;
	}
	self->zmq_sock = zmq_socket(self->zmq_ctx, ZMQ_PUB);
	int rc = 0;
	if(self->mode == ZMQ_MODE_SERVER) {
		rc = zmq_bind(self->zmq_sock, self->endpoint);
	} else {    // ZMQ_MODE_CLIENT
		rc = zmq_connect(self->zmq_sock, self->endpoint);
	}
	if(rc < 0) {
		fprintf(stderr, "output_zmq(%s): %s failed: %s\n",
				self->endpoint, self->mode == ZMQ_MODE_SERVER ? "bind" : "connect",
				zmq_strerror(errno));
		return -1;
	}
	rc = zmq_setsockopt(self->zmq_sock, ZMQ_SNDHWM, &hfdl->Config.output_queue_hwm,
			sizeof(hfdl->Config.output_queue_hwm));
	if(rc < 0) {
		fprintf(stderr, "output_zmq(%s): could not set ZMQ_SNDHWM option for socket: %s\n",
				self->endpoint, zmq_strerror(errno));
		return -1;
	}
	return 0;
}

static void out_zmq_produce_text(out_zmq_ctx_t *self, struct metadata *metadata, struct octet_string *msg) {
	UNUSED(metadata);
	ASSERT(msg != NULL);
	ASSERT(self->zmq_sock != 0);
	if(msg->len < 2) {
		return;
	}
	if(zmq_send(self->zmq_sock, msg->buf, msg->len, 0) < 0) {
		fprintf(stderr, "output_zmq(%s): zmq_send error: %s", self->endpoint, zmq_strerror(errno));
	}
}

static int out_zmq_produce(void *selfptr, output_format_t format, struct metadata *metadata, struct octet_string *msg) {
	ASSERT(selfptr != NULL);
	out_zmq_ctx_t *self = selfptr;
	if(format == OFMT_TEXT || format == OFMT_BASESTATION) {
		out_zmq_produce_text(self, metadata, msg);
	}
	return 0;
}

static void out_zmq_handle_shutdown(void *selfptr) {
	ASSERT(selfptr != NULL);
	out_zmq_ctx_t *self = selfptr;
	fprintf(stderr, "output_zmq(%s): shutting down\n", self->endpoint);
	zmq_close(self->zmq_sock);
	zmq_ctx_destroy(self->zmq_ctx);
}

static void out_zmq_handle_failure(void *selfptr) {
	ASSERT(selfptr != NULL);
	out_zmq_ctx_t *self = selfptr;
	fprintf(stderr, "output_zmq(%s): could not %s, deactivating output\n",
			self->endpoint,	self->mode == ZMQ_MODE_SERVER ? "bind" : "connect");
}

static const option_descr_t out_zmq_options[] = {
	{
		.name = "mode",
		.description = "Socket mode: client or server (required)"
	},
	{
		.name= "endpoint",
		.description = "Socket endpoint: tcp://address:port (required)"
	},
	{
		.name = NULL,
		.description = NULL
	}
};

output_descriptor_t out_DEF_zmq = {
	.name = "zmq",
	.description = "Output to a ZeroMQ publisher socket (as a server or a client)",
	.options = out_zmq_options,
	.supports_format = out_zmq_supports_format,
	.configure = out_zmq_configure,
	.init = out_zmq_init,
	.produce = out_zmq_produce,
	.handle_shutdown = out_zmq_handle_shutdown,
	.handle_failure = out_zmq_handle_failure
};
#endif
