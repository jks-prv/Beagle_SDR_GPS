/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdint.h>
#include <stdio.h>              // fprintf
#include "hfdl_config.h"
#include "util.h"               // ASSERT, XCALLOC, NEW, container_of
#include "input-common.h"
#include "input-helpers.h"      // get_sample_converter
#include "input-file.h"         // file_input_vtable
#include "input-buf.h"          // buf_input_vtable
#ifdef WITH_SOAPYSDR
#include "input-soapysdr.h"     // soapysdr_input_vtable
#endif

static struct input_vtable *input_vtables[] = {
	[INPUT_TYPE_FILE] = &file_input_vtable,
	[INPUT_TYPE_BUF] = &buf_input_vtable,
#ifdef WITH_SOAPYSDR
	[INPUT_TYPE_SOAPYSDR] = &soapysdr_input_vtable,
#endif
	[INPUT_TYPE_UNDEF] = NULL
};

static struct input_vtable *input_vtable_get(input_type type) {
	if(type < INPUT_TYPE_MAX) {
		return input_vtables[type];
	} else {
		return NULL;
	}
}

struct input_cfg *input_cfg_create() {
	NEW(struct input_cfg, cfg);
	cfg->centerfreq= -1;
	cfg->sample_rate = -1;
	cfg->sfmt = SFMT_UNDEF;
	cfg->gain = AUTO_GAIN;
	return cfg;
}

void input_cfg_destroy(struct input_cfg *cfg) {
	XFREE(cfg);
}

struct block *input_create(struct input_cfg *cfg) {
	if(cfg == NULL) {
		return NULL;
	}
	struct input_vtable *vtable = input_vtable_get(cfg->type);
	if(vtable == NULL) {
		return NULL;
	}
	struct input *input = vtable->create(cfg);
	if(input == NULL) {
		return NULL;
	}
	struct producer producer = { .type = PRODUCER_SINGLE, .max_tu = 0 };
	struct consumer consumer = { .type = CONSUMER_NONE };
	input->block.producer = producer;
	input->block.consumer = consumer;
	input->block.thread_routine = vtable->rx_thread_routine;
	input->config = cfg;
	input->vtable = vtable;
	return &input->block;
}

int32_t input_init(struct block *block) {
	ASSERT(block != NULL);
	struct input *input = container_of(block, struct input, block);
	int32_t ret = input->vtable->init(input);
	if(ret < 0) {
		goto end;
	}
	ASSERT(input->bytes_per_sample > 0);
	ASSERT(input->full_scale > 0.f);
	ASSERT(block->producer.max_tu > 0);

	// Provide sample converter from the native format to complex float
	input->convert_sample_buffer = get_sample_converter(input->config->sfmt);
	if(input->convert_sample_buffer == NULL) {
		fprintf(stderr, "No sample conversion routine found for sample format %d\n",
				input->config->sfmt);
		ret = -1;
		goto end;
	}
	// TODO: Lookup converters of other, non-native formats supported by the device

end:
	return ret;
}

void input_destroy(struct block *block) {
	if(block != NULL) {
		struct input *input = container_of(block, struct input, block);
		ASSERT(input != NULL);
		if(input->vtable != NULL && input->vtable->destroy != NULL) {
			input->vtable->destroy(input);
		}
	}
}
