/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <string.h>             // memset, strcmp, strdup
#include <glib.h>               // g_async_queue_new
#include <libacars/dict.h>      // la_dict
#include "hfdl_config.h"             // WITH_*
#include "util.h"               // NEW, ASSERT
#include "options.h"            // describe_option
#include "metadata.h"           // struct metadata, metadata_copy, metadata_destroy
#include "output-common.h"
#include "kiwi-hfdl.h"

#include "fmtr-text.h"          // fmtr_DEF_text
#include "fmtr-basestation.h"   // fmtr_DEF_basestation

#include "output-file.h"        // out_DEF_file
#include "output-tcp.h"         // out_DEF_tcp
#include "output-udp.h"         // out_DEF_udp
#ifdef WITH_ZMQ
#include "output-zmq.h"         // out_DEF_zmq
#endif

static la_dict const fmtr_intype_names[] = {
	{
		.id = FMTR_INTYPE_DECODED_FRAME,
		.val = &(option_descr_t) {
			.name= "decoded",
			.description = "Output decoded frames"
		}
	},
	{
		.id = FMTR_INTYPE_RAW_FRAME,
		.val = &(option_descr_t) {
			.name= "raw",
			.description = "Output undecoded HFDL frames as raw bytes"
		}
	},
	{
		.id = FMTR_INTYPE_UNKNOWN,
		.val = NULL
	}
};

static la_dict const fmtr_descriptors[] = {
	{ .id = OFMT_TEXT,                  .val = &fmtr_DEF_text },
	{ .id = OFMT_BASESTATION,           .val = &fmtr_DEF_basestation },
	{ .id = OFMT_UNKNOWN,               .val = NULL }
};

static output_descriptor_t * output_descriptors[] = {
	&out_DEF_file,
	&out_DEF_tcp,
	&out_DEF_udp,
#ifdef WITH_ZMQ
	&out_DEF_zmq,
#endif
	NULL
};

fmtr_input_type_t fmtr_input_type_from_string(char const *str) {
	for (la_dict const *d = fmtr_intype_names; d->val != NULL; d++) {
		if (!strcmp(str, ((option_descr_t *)d->val)->name)) {
			return d->id;
		}
	}
	return FMTR_INTYPE_UNKNOWN;
}

fmtr_descriptor_t *fmtr_descriptor_get(output_format_t fmt) {
	return la_dict_search(fmtr_descriptors, fmt);
}

fmtr_instance_t *fmtr_instance_new(fmtr_descriptor_t *fmttd, fmtr_input_type_t intype) {
	ASSERT(fmttd != NULL);
	NEW(fmtr_instance_t, fmtr);
	fmtr->td = fmttd;
	fmtr->intype = intype;
	fmtr->outputs = NULL;
	return fmtr;
}

void fmtr_instance_destroy(fmtr_instance_t *fmtr) {
	if(fmtr != NULL) {
		la_list_free_full(fmtr->outputs, output_instance_destroy);
		XFREE(fmtr);
	}
}

output_format_t output_format_from_string(char const *str) {
	for (la_dict const *d = fmtr_descriptors; d->val != NULL; d++) {
		if (!strcmp(str, ((fmtr_descriptor_t *)d->val)->name)) {
			return d->id;
		}
	}
	return OFMT_UNKNOWN;
}

output_descriptor_t *output_descriptor_get(char const *output_name) {
	if(output_name == NULL) {
		return NULL;
	}
	for(output_descriptor_t **outd = output_descriptors; *outd != NULL; outd++) {
		if(!strcmp(output_name, (*outd)->name)) {
			return *outd;
		}
	}
	return NULL;
}

output_instance_t *output_instance_new(output_descriptor_t *outtd, output_format_t format, void *priv) {
	ASSERT(outtd != NULL);
	NEW(output_ctx_t, ctx);
	ctx->q = g_async_queue_new();
	ctx->format = format;
	ctx->priv = priv;
	ctx->active = true;
	NEW(output_instance_t, output);
	output->td = outtd;
	output->ctx = ctx;
	output->output_thread = XCALLOC(1, sizeof(pthr_t));
	return output;
}

void output_instance_destroy(output_instance_t *output) {
	if(output) {
		if(output->ctx) {
			g_async_queue_unref(output->ctx->q);
			if(output->td->ctx_destroy) {
				output->td->ctx_destroy(output->ctx->priv);
			} else {
				XFREE(output->ctx->priv);
			}
			XFREE(output->ctx);
		}
		XFREE(output->output_thread);
		XFREE(output);
	}
}

output_qentry_t *output_qentry_copy(output_qentry_t const *q) {
	ASSERT(q != NULL);

	NEW(output_qentry_t, copy);
	if(q->msg != NULL) {
		copy->msg = octet_string_copy(q->msg);
	}
	if(q->metadata != NULL) {
		copy->metadata = metadata_copy(q->metadata);
	}
	copy->format = q->format;
	copy->flags = q->flags;
	return copy;
}

void output_qentry_destroy(output_qentry_t *q) {
	if(q == NULL) {
		return;
	}
	octet_string_destroy(q->msg);
	metadata_destroy(q->metadata);
	XFREE(q);
}

void output_queue_drain(GAsyncQueue *q) {
	ASSERT(q != NULL);
	#ifdef KIWI
	    printf("HFDL DRAIN\n");
	    TaskSleepReasonMsec("HFDL-drain", 10000);
	#endif
	g_async_queue_lock(q);
	while(g_async_queue_length_unlocked(q) > 0) {
		output_qentry_t *qentry = g_async_queue_pop_unlocked(q);
		output_qentry_destroy(qentry);
	}
	g_async_queue_unlock(q);
}

void output_usage() {
	fprintf(stderr, "\n<output_specifier> is a parameter of the --output option. It has the following syntax:\n\n");
	fprintf(stderr, "%*s<what_to_output>:<output_format>:<output_type>:<output_parameters>\n\n", IND(1), "");
	fprintf(stderr, "where:\n");
	fprintf(stderr, "\n%*s<what_to_output> specifies what data should be sent to the output:\n\n", IND(1), "");
	for(la_dict const *p = fmtr_intype_names; p->val != NULL; p++) {
		option_descr_t *n = p->val;
		describe_option(n->name, n->description, 2);
	}
	fprintf(stderr, "\n%*s<output_format> specifies how the output should be formatted:\n\n", IND(1), "");
	for(la_dict const *p = fmtr_descriptors; p->val != NULL; p++) {
		fmtr_descriptor_t *n = p->val;
		describe_option(n->name, n->description, 2);
	}
	fprintf(stderr, "\n%*s<output_type> specifies the type of the output:\n\n", IND(1), "");
	for(output_descriptor_t **od = output_descriptors; *od != NULL; od++) {
		describe_option((*od)->name, (*od)->description, 2);
	}
	fprintf(stderr,
			"\n%*s<output_parameters> - specifies detailed output options with a syntax of: param1=value1,param2=value2,...\n",
			IND(1), ""
		   );
	for(output_descriptor_t **od = output_descriptors; *od != NULL; od++) {
		fprintf(stderr, "\nParameters for output type '%s':\n\n", (*od)->name);
		if((*od)->options != NULL) {
			for(option_descr_t const *opt = (*od)->options; opt->name != NULL; opt++) {
				describe_option(opt->name, opt->description, 2);
			}
		}
	}
	fprintf(stderr, "\n");
}

void *output_thread(void *arg) {
	ASSERT(arg != NULL);
	output_instance_t *oi = arg;
	ASSERT(oi->ctx != NULL);
	output_ctx_t *ctx = oi->ctx;

	if(oi->td->init != NULL) {
		if(oi->td->init(ctx->priv) < 0) {
			goto fail;
		}
	}

	while(1) {
		output_qentry_t *q = hfdl_g_async_queue_pop("HFDL-out", ctx->q);
		ASSERT(q != NULL);
		if(q->flags & OUT_FLAG_ORDERED_SHUTDOWN) {
			output_qentry_destroy(q);
			break;
		}
		int32_t result = oi->td->produce(ctx->priv, q->format, q->metadata, q->msg);
		output_qentry_destroy(q);
		if(result < 0) {
			break;
		}
	}

	if(oi->td->handle_shutdown != NULL) {
		oi->td->handle_shutdown(ctx->priv);
	}
	ctx->active = false;
	return NULL;

fail:
	ctx->active = false;
	if(oi->td->handle_failure != NULL) {
		oi->td->handle_failure(ctx->priv);
	}
	output_queue_drain(ctx->q);
	return NULL;
}

void output_queue_push(void *data, void *ctx) {
	ASSERT(data);
	ASSERT(ctx);
	output_instance_t *output = data;
	output_qentry_t *qentry = ctx;

	bool overflow = (hfdl->Config.output_queue_hwm != OUTPUT_QUEUE_HWM_NONE &&
			g_async_queue_length(output->ctx->q) >= hfdl->Config.output_queue_hwm);
	bool active = output->ctx->active;
	if(qentry->flags & OUT_FLAG_ORDERED_SHUTDOWN || (active && !overflow)) {
		output_qentry_t *copy = output_qentry_copy(qentry);
		g_async_queue_push(output->ctx->q, copy);
		debug_print(D_OUTPUT, "dispatched %s output %p\n", output->td->name, output);
	} else {
		if(overflow) {
			fprintf(stderr, "%s output queue overflow, throttling\n", output->td->name);
		} else if(!active) {
			debug_print(D_OUTPUT, "%s output %p is inactive, skipping\n", output->td->name, output);
		}
	}
}

void shutdown_outputs(la_list *fmtr_list) {
	fmtr_instance_t *fmtr = NULL;
	for(la_list *p = fmtr_list; p != NULL; p = la_list_next(p)) {
		fmtr = (fmtr_instance_t *)(p->data);
		output_qentry_t qentry = {
			.msg = NULL,
			.metadata = NULL,
			.format = OFMT_UNKNOWN,
			.flags = OUT_FLAG_ORDERED_SHUTDOWN
		};
		la_list_foreach(fmtr->outputs, output_queue_push, &qentry);
	}
}

bool output_thread_is_any_running(la_list *fmtr_list) {
	fmtr_instance_t *fmtr = NULL;
	for(la_list *fl = fmtr_list; fl != NULL; fl = la_list_next(fl)) {
		fmtr = (fmtr_instance_t *)(fl->data);
		output_instance_t *output = NULL;
		for(la_list *ol = fmtr->outputs; ol != NULL; ol = la_list_next(ol)) {
			output = (output_instance_t *)(ol->data);
			if(output->ctx->active) {
				return true;
			}
		}
	}
	return false;
}
