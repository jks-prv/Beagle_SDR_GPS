/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>         // usleep
#include <errno.h>          // errno
#include <liquid/liquid.h>  // cbuffercf_*
#include "block.h"          // block_*
#include "input-common.h"   // input, sample_format, input_vtable
#include "input-helpers.h"  // get_sample_full_scale_value, get_sample_size
#include "util.h"	        // debug_print, ASSERT, XCALLOC
#include "globals.h"        // do_exit
#include <fcntl.h>
#include "kiwi-hfdl.h"

#define buf_BUFSIZE (hfdl->outputBlockSize * sizeof(float))
//#define buf_BUFSIZE (hfdl->outputBlockSize * sizeof(uint16_t))

struct buf_input {
	struct input input;
};

struct input *buf_input_create(struct input_cfg *cfg) {
	UNUSED(cfg);
	NEW(struct buf_input, buf_input);
	return &buf_input->input;
}

void buf_input_destroy(struct input *input) {
	if(input != NULL) {
		struct buf_input *fi = container_of(input, struct buf_input, input);
		XFREE(fi);
	}
}

void *buf_input_thread(void *ctx) {
	ASSERT(ctx);
	struct block *block = ctx;
	struct input *input = container_of(block, struct input, block);
	//struct buf_input *buf_input = container_of(input, struct buf_input, input);
	struct circ_buffer *circ_buffer = &block->producer.out->circ_buffer;

    //printf("buf_input_thread tid=%d\n", TaskID());
    char *s;
    asprintf(&s, "SET input_tid=%d rx_chan=%d", TaskID(), hfdl->rx_chan);
	hfdl_msgs(s, hfdl->rx_chan);
	free(s);

	float complex *outbuf = XCALLOC(buf_BUFSIZE / input->bytes_per_sample,
			sizeof(float complex));
	size_t space_available, len, samples_read;

	do {
	    void *in_buf = TaskSleepReason("in-buf sleep");
	    if (in_buf == 0) {
	        //printf("END"); fflush(stdout);
	        break;
	    }
	    //static int wakes;
	    //printf("w%d %p ", wakes++, in_buf); fflush(stdout);
	    len = buf_BUFSIZE;

		samples_read = len / input->bytes_per_sample;
		while(true) {
			pthr_mutex_lock(circ_buffer->mutex);
			space_available = cbuffercf_space_available(circ_buffer->buf);
			pthr_mutex_unlock(circ_buffer->mutex);
			if(space_available * input->bytes_per_sample >= len) {
				break;
			}
            #ifdef KIWI
                TaskSleepReasonMsec("in-buf delay", 1000);
            #else
			    usleep(100000);
			#endif
		}
		input->convert_sample_buffer(input, in_buf, len, outbuf);
		complex_samples_produce(circ_buffer, outbuf, samples_read);
	} while(hfdl->do_exit == 0);

	if (hfdl->do_exit) printf("buf_input_thread do_exit=%d\n", hfdl->do_exit);
    TaskSleepReasonMsec("HFDL-in-buf", 1000);
	debug_print(D_MISC, "Shutdown ordered, signaling consumer shutdown\n");
	block_connection_one2one_shutdown(block->producer.out);
	hfdl->do_exit = 1;
	block->running = false;
	XFREE(outbuf);
	return NULL;
}

int32_t buf_input_init(struct input *input) {
    //printf("buf_input_init\n");
	ASSERT(input != NULL);
	//struct buf_input *buf_input = container_of(input, struct buf_input, input);

	if(input->config->sfmt == SFMT_UNDEF) {
		fprintf(stderr, "Sample format must be specified for buf inputs\n");
		return -1;
	}
	
	input->full_scale = get_sample_full_scale_value(input->config->sfmt);
	input->bytes_per_sample = get_sample_size(input->config->sfmt);
	ASSERT(input->bytes_per_sample > 0);
	input->block.producer.max_tu = buf_BUFSIZE / input->bytes_per_sample;
	debug_print(D_SDR, "input-buf: max_tu=%zu\n", input->block.producer.max_tu);
	return 0;
}

struct input_vtable const buf_input_vtable = {
	.create = buf_input_create,
	.init = buf_input_init,
	.destroy = buf_input_destroy,
	.rx_thread_routine = buf_input_thread
};

