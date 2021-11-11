/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>                      // FILE, fprintf, fwrite
#include <string.h>                     // strcmp, strdup, strerror
#include <time.h>                       // gmtime_r, localtime_r, strftime
#include <errno.h>                      // errno
#include <arpa/inet.h>                  // htons
#include "output-common.h"              // output_descriptor_t, output_qentry_t, output_queue_drain
#include "output-file.h"                // OUT_BINARY_FRAME_LEN_OCTETS, OUT_BINARY_FRAME_LEN_MAX
#include "kvargs.h"                     // kvargs
#include "options.h"                    // option_descr_t
#include "util.h"                       // ASSERT, NEW
#include "kiwi-hfdl.h"

typedef enum {
	ROT_NONE,
	ROT_HOURLY,
	ROT_DAILY
} out_file_rotation_mode;

typedef struct {
	FILE *fh;
	char *filename_prefix;
	char *extension;
	size_t prefix_len;
	struct tm current_tm;
	out_file_rotation_mode rotate;
} out_file_ctx_t;

static bool out_file_supports_format(output_format_t format) {
	return(format == OFMT_TEXT || format == OFMT_BASESTATION);
}

static void *out_file_configure(kvargs *kv) {
	ASSERT(kv != NULL);
	NEW(out_file_ctx_t, cfg);
	if(kvargs_get(kv, "path") == NULL) {
		fprintf(stderr, "output_file: path not specified\n");
		goto fail;
	}
	cfg->filename_prefix = strdup(kvargs_get(kv, "path"));
	debug_print(D_OUTPUT, "filename_prefix: %s\n", cfg->filename_prefix);
	char *rotate = kvargs_get(kv, "rotate");
	if(rotate != NULL) {
		if(!strcmp(rotate, "hourly")) {
			cfg->rotate = ROT_HOURLY;
		} else if(!strcmp(rotate, "daily")) {
			cfg->rotate = ROT_DAILY;
		} else {
			fprintf(stderr, "output_file: invalid rotation mode: %s\n", rotate);
			goto fail;
		}
	} else {
		cfg->rotate = ROT_NONE;
	}
	return cfg;
fail:
	XFREE(cfg);
	return NULL;
}

static void out_file_ctx_destroy(void *ctxptr) {
	if(ctxptr != NULL) {
		out_file_ctx_t *ctx = ctxptr;
		XFREE(ctx->filename_prefix);
		XFREE(ctx);
	}
}

static int32_t out_file_open(out_file_ctx_t *self) {
	char *filename = NULL;
	char *fmt = NULL;
	size_t tlen = 0;

	if(self->rotate != ROT_NONE) {
		time_t t = time(NULL);
		if(hfdl->Config.utc == true) {
			gmtime_r(&t, &self->current_tm);
		} else {
			localtime_r(&t, &self->current_tm);
		}
		char suffix[16];
		if(self->rotate == ROT_HOURLY) {
			fmt = "_%Y%m%d_%H";
		} else if(self->rotate == ROT_DAILY) {
			fmt = "_%Y%m%d";
		}
		ASSERT(fmt != NULL);
		tlen = strftime(suffix, sizeof(suffix), fmt, &self->current_tm);
		if(tlen == 0) {
			fprintf(stderr, "open_outfile(): strfime returned 0\n");
			return -1;
		}
		filename = XCALLOC(self->prefix_len + tlen + 2, sizeof(uint8_t));
		sprintf(filename, "%s%s%s", self->filename_prefix, suffix, self->extension);
	} else {
		filename = strdup(self->filename_prefix);
	}

	if((self->fh = fopen(filename, "a+")) == NULL) {
		fprintf(stderr, "Could not open output file %s: %s\n", filename, strerror(errno));
		XFREE(filename);
		return -1;
	}
	XFREE(filename);
	return 0;
}

static int32_t out_file_init(void *selfptr) {
	ASSERT(selfptr != NULL);
	out_file_ctx_t *self = selfptr;
	if(!strcmp(self->filename_prefix, "-")) {
		self->fh = stdout;
		self->rotate = ROT_NONE;
	} else {
		self->prefix_len = strlen(self->filename_prefix);
		if(self->rotate != ROT_NONE) {
			char *basename = strrchr(self->filename_prefix, '/');
			if(basename != NULL) {
				basename++;
			} else {
				basename = self->filename_prefix;
			}
			char *ext = strrchr(self->filename_prefix, '.');
			if(ext != NULL && (ext <= basename || ext[1] == '\0')) {
				ext = NULL;
			}
			if(ext) {
				self->extension = strdup(ext);
				*ext = '\0';
			} else {
				self->extension = strdup("");
			}
		}
		return out_file_open(self);
	}
	return 0;
}

static int32_t out_file_rotate(out_file_ctx_t *self) {
	// FIXME: rotation should be driven by message timestamp, not the current timestamp
	struct tm new_tm;
	time_t t = time(NULL);
	if(hfdl->Config.utc == true) {
		gmtime_r(&t, &new_tm);
	} else {
		localtime_r(&t, &new_tm);
	}
	if((self->rotate == ROT_HOURLY && new_tm.tm_hour != self->current_tm.tm_hour) ||
			(self->rotate == ROT_DAILY && new_tm.tm_mday != self->current_tm.tm_mday)) {
		if(self->fh != NULL) {
		    if(self->fh != stdout)
			    fclose(self->fh);
			self->fh = NULL;
		}
		return out_file_open(self);
	}
	return 0;
}

static void out_file_produce_text(out_file_ctx_t *self, struct metadata *metadata, struct octet_string *msg) {
	ASSERT(msg != NULL);
	ASSERT(self->fh != NULL);
	UNUSED(metadata);
	//fwrite(msg->buf, sizeof(uint8_t), msg->len, self->fh);
    ext_send_msg_encoded(hfdl->rx_chan, false, "EXT", "chars", "%.*s", msg->len, msg->buf);
	fflush(self->fh);
}

static int32_t out_file_produce(void *selfptr, output_format_t format, struct metadata *metadata, struct octet_string *msg) {
	ASSERT(selfptr != NULL);
	out_file_ctx_t *self = selfptr;
	if(self->rotate != ROT_NONE && out_file_rotate(self) < 0) {
		return -1;
	}
	if(format == OFMT_TEXT) {
		out_file_produce_text(self, metadata, msg);
	}
	return 0;
}

static void out_file_handle_shutdown(void *selfptr) {
	ASSERT(selfptr != NULL);
	out_file_ctx_t *self = selfptr;
	fprintf(stderr, "output_file(%s): shutting down\n", self->filename_prefix);
	if(self->fh != NULL) {
	    if(self->fh != stdout)
		    fclose(self->fh);
		self->fh = NULL;
	}
}

static void out_file_handle_failure(void *selfptr) {
	ASSERT(selfptr != NULL);
	out_file_ctx_t *self = selfptr;
	fprintf(stderr, "output_file: could not write to '%s', deactivating output\n",
			self->filename_prefix);
	if(self->fh != NULL) {
        if(self->fh != stdout)
		    fclose(self->fh);
		self->fh = NULL;
	}
}

static option_descr_t const out_file_options[] = {
	{
		.name = "path",
		.description = "Path to the output file (required)"
	},
	{
		.name = "rotate",
		.description = "How often to start a new file: Accepted values: daily, hourly"
	},
	{
		.name = NULL,
		.description = NULL
	}
};

output_descriptor_t out_DEF_file = {
	.name = "file",
	.description = "Output to a file",
	.options = out_file_options,
	.supports_format = out_file_supports_format,
	.configure = out_file_configure,
	.ctx_destroy = out_file_ctx_destroy,
	.init = out_file_init,
	.produce = out_file_produce,
	.handle_shutdown = out_file_handle_shutdown,
	.handle_failure = out_file_handle_failure
};
