/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdbool.h>
#include <stdint.h>
#include <string.h>                 // memcpy
#include <glib.h>                   // GAsyncQueue, g_async_queue_*
#include <libacars/libacars.h>      // la_proto_tree_destroy()
#include <libacars/list.h>          // la_list_*
#include <libacars/reassembly.h>    // la_reasm_ctx, la_reasm_ctx_new()
#include "util.h"                   // NEW, ASSERT, struct octet_string
#include "output-common.h"          // output_queue_push, shutdown_outputs
#include "crc.h"                    // crc16_ccitt
#include "mpdu.h"                   // mpdu_parse
#include "spdu.h"                   // spdu_parse
#include "statsd.h"                 // statsd_*
#include "pdu.h"                    // struct hfdl_pdu_metadata
#include "kiwi-hfdl.h"

struct hfdl_pdu_qentry {
	struct metadata *metadata;
	struct octet_string *pdu;
	uint32_t flags;
};

/******************************
 * Forward declarations
 ******************************/

static void *pdu_decoder_thread(void *ctx);
static struct metadata_vtable hfdl_pdu_metadata_vtable;

/******************************
 * Public methods
 ******************************/

void pdu_decoder_queue_push(struct metadata *metadata, struct octet_string *pdu, uint32_t flags) {
	NEW(struct hfdl_pdu_qentry, qentry);
	qentry->metadata = metadata;
	qentry->pdu = pdu;
	qentry->flags = flags;
	g_async_queue_push(hfdl->pdu_decoder_queue, qentry);
}

void hfdl_pdu_decoder_init(void) {
	hfdl->pdu_decoder_queue = g_async_queue_new();
}

int32_t hfdl_pdu_decoder_start(void *ctx) {
	pthr_t pdu_th;
	int32_t ret = start_thread("HFDL-pdu", &pdu_th, pdu_decoder_thread, ctx);
	if(ret == 0) {
		hfdl->pdu_decoder_thread_active = true;
	}
	return ret;
}

void hfdl_pdu_decoder_stop(void) {
	pdu_decoder_queue_push(NULL, NULL, OUT_FLAG_ORDERED_SHUTDOWN);
}

bool hfdl_pdu_decoder_is_running(void) {
	return hfdl->pdu_decoder_thread_active;
}

// Compute FCS over the first hdr_len octets of buf.
// The received FCS value is expected to be stored just after the buffer.
bool hfdl_pdu_fcs_check(uint8_t *buf, uint32_t hdr_len) {
	uint16_t fcs_check = buf[hdr_len] | (buf[hdr_len + 1] << 8);
	uint16_t fcs_computed = crc16_ccitt(buf, hdr_len, 0xFFFFu) ^ 0xFFFFu;
	debug_print(D_PROTO, "FCS: computed: 0x%04x check: 0x%04x\n",
			fcs_computed, fcs_check);
	if(fcs_check != fcs_computed) {
		debug_print(D_PROTO, "FCS check failed\n");
		return false;
	}
	debug_print(D_PROTO, "FCS check OK\n");
	return true;
}

struct metadata *hfdl_pdu_metadata_create() {
	NEW(struct hfdl_pdu_metadata, m);
	m->metadata.vtable = &hfdl_pdu_metadata_vtable;
	return &m->metadata;
}

/****************************************
 * Private variables and methods
 ****************************************/

static void *pdu_decoder_thread(void *ctx) {
	ASSERT(ctx != NULL);
	la_list *fmtr_list = ctx;
	struct hfdl_pdu_qentry *q = NULL;
	la_list *lpdu_list = NULL;
	la_reasm_ctx *reasm_ctx = la_reasm_ctx_new();
	enum {
		DECODING_NOT_DONE,
		DECODING_SUCCESS,
		DECODING_FAILURE
	} decoding_status;
	#define IS_MPDU(buf) ((buf)[0] & 1)

	while(true) {
		q = hfdl_g_async_queue_pop("HFDL-pdu", hfdl->pdu_decoder_queue);
		if(q->flags & OUT_FLAG_ORDERED_SHUTDOWN) {
			fprintf(stderr, "Shutting down decoder thread\n");
			shutdown_outputs(fmtr_list);
			XFREE(q);
			break;
		}
		ASSERT(q->metadata != NULL);

		fmtr_instance_t *fmtr = NULL;
		decoding_status = DECODING_NOT_DONE;
		for(la_list *p = fmtr_list; p != NULL; p = la_list_next(p)) {
			fmtr = p->data;
			if(fmtr->intype == FMTR_INTYPE_DECODED_FRAME) {
				// Decode the pdu unless we've done it before
				if(decoding_status == DECODING_NOT_DONE) {
					struct hfdl_pdu_metadata *hm = container_of(q->metadata,
							struct hfdl_pdu_metadata, metadata);
					statsd_increment_per_channel(hm->freq, "frames.processed");
					if(IS_MPDU(q->pdu->buf)) {
						lpdu_list = mpdu_parse(q->pdu, reasm_ctx, q->metadata->rx_timestamp, hm->freq);
					} else {
						lpdu_list = spdu_parse(q->pdu, hm->freq);
					}
					if(lpdu_list != NULL) {
						decoding_status = DECODING_SUCCESS;
					} else {
						decoding_status = DECODING_FAILURE;
					}
				}
				if(decoding_status == DECODING_SUCCESS) {
					for(la_list *lpdu = lpdu_list; lpdu != NULL; lpdu = la_list_next(lpdu)) {
						ASSERT(lpdu->data != NULL);
						struct octet_string *serialized_msg = fmtr->td->format_decoded_msg(q->metadata, lpdu->data);
						// First check if the formatter actually returned something.
						// A formatter might be suitable only for a particular message type. If this is the case.
						// it will return NULL for all messages it cannot handle.
						// An example is pp_acars which only deals with ACARS messages.
						if(serialized_msg != NULL) {
							output_qentry_t qentry = {
								.msg = serialized_msg,
								.metadata = q->metadata,
								.format = fmtr->td->output_format
							};
							la_list_foreach(fmtr->outputs, output_queue_push, &qentry);
							// output_queue_push makes a copy of serialized_msg, so it's safe to free it now
							octet_string_destroy(serialized_msg);
						}
					}
				}
			} else if(fmtr->intype == FMTR_INTYPE_RAW_FRAME) {
				struct octet_string *serialized_msg = fmtr->td->format_raw_msg(q->metadata, q->pdu);
				if(serialized_msg != NULL) {
					output_qentry_t qentry = {
						.msg = serialized_msg,
						.metadata = q->metadata,
						.format = fmtr->td->output_format
					};
					la_list_foreach(fmtr->outputs, output_queue_push, &qentry);
					// output_queue_push makes a copy of serialized_msg, so it's safe to free it now
					octet_string_destroy(serialized_msg);
				}
			}
		}
		la_list_free_full(lpdu_list, la_proto_tree_destroy);
		lpdu_list = NULL;
		octet_string_destroy(q->pdu);
		XFREE(q->metadata);
		XFREE(q);
	}
	la_reasm_ctx_destroy(reasm_ctx);
	hfdl->pdu_decoder_thread_active = false;
	return NULL;
}

static struct metadata *hfdl_pdu_metadata_copy(struct metadata const *m) {
	ASSERT(m != NULL);
	struct hfdl_pdu_metadata *hm = container_of(m, struct hfdl_pdu_metadata, metadata);
	NEW(struct hfdl_pdu_metadata, copy);
	memcpy(copy, hm, sizeof(struct hfdl_pdu_metadata));
	if(hm->station_id != NULL) {
		copy->station_id = strdup(hm->station_id);
	}
	return &copy->metadata;
}

static void hfdl_pdu_metadata_destroy(struct metadata *m) {
	if(m == NULL) {
		return;
	}
	struct hfdl_pdu_metadata *hm = container_of(m, struct hfdl_pdu_metadata, metadata);
	XFREE(hm->station_id);
	XFREE(hm);
}

static struct metadata_vtable hfdl_pdu_metadata_vtable = {
	.copy = hfdl_pdu_metadata_copy,
	.destroy = hfdl_pdu_metadata_destroy
};
