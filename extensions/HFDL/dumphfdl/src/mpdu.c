/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdint.h>
#include <sys/time.h>                       // struct timeval
#include <libacars/libacars.h>              // la_type_descriptor, la_proto_node
#include <libacars/reassembly.h>            // la_reasm_ctx
#include <libacars/list.h>                  // la_list
#include "pdu.h"                            // struct hfdl_pdu_hdr_data, hfdl_pdu_fcs_check
#include "lpdu.h"                           // lpdu_parse
#include "statsd.h"                         // statsd_*
#include "util.h"                           // NEW, ASSERT, struct octet_string, {ac,gs}_id_format_text

struct hfdl_mpdu {
	struct octet_string *pdu;
	la_list *dst_aircraft;                  // List of destination aircraft (for multi-LPDU uplinks)
	struct hfdl_pdu_hdr_data header;
};

struct mpdu_dst {
	uint8_t dst_id;
	uint8_t lpdu_cnt;
};

// Forward declarations
la_type_descriptor const proto_DEF_hfdl_mpdu;
static int32_t parse_lpdu_list(uint8_t *lpdu_len_ptr, uint8_t *data_ptr,
		uint8_t *endptr, uint32_t lpdu_cnt, la_list **lpdu_list,
		struct hfdl_pdu_hdr_data mpdu_header, la_reasm_ctx *reasm_ctx,
		struct timeval rx_timestamp);

la_list *mpdu_parse(struct octet_string *pdu, la_reasm_ctx *reasm_ctx,
		struct timeval rx_timestamp, int32_t freq) {
	ASSERT(pdu);
	ASSERT(pdu->buf);
	ASSERT(pdu->len > 0);

	la_list *result = NULL;
	la_list *lpdu_list = NULL;
	la_proto_node *mpdu_node = NULL;
	struct hfdl_mpdu *mpdu = NULL;
	if(hfdl->Config.output_mpdus) {
		mpdu = XCALLOC(1, sizeof(struct hfdl_mpdu));
		mpdu->pdu = pdu;
		mpdu_node = la_proto_node_new();
		mpdu_node->data = mpdu;
		mpdu_node->td = &proto_DEF_hfdl_mpdu;
	}

	struct hfdl_pdu_hdr_data mpdu_header = {0};
	mpdu_header.freq = freq;
	uint32_t aircraft_cnt = 0;
	uint32_t lpdu_cnt = 0;
	uint32_t hdr_len = 0;
	uint8_t *buf = pdu->buf;
	uint32_t len = pdu->len;
	if(pdu->buf[0] & 0x2) {
		mpdu_header.direction = DOWNLINK_PDU;
		lpdu_cnt = (buf[0] >> 2) & 0xF;
		hdr_len = 6 + lpdu_cnt;             // header length, not incl. FCS
	} else {
		mpdu_header.direction = UPLINK_PDU;
		aircraft_cnt = ((buf[0] & 0x70) >> 4) + 1;
		debug_print(D_PROTO, "aircraft_cnt: %u\n", aircraft_cnt);
		hdr_len = 2;                        // P/NAC/T + UTC/GS ID
		for(uint32_t i = 0; i < aircraft_cnt; i++) {
			if(len < hdr_len + 2) {
				debug_print(D_PROTO, "uplink: too short: %u < %u\n", len, hdr_len + 2);
				statsd_increment_per_channel(freq, "frame.errors.too_short");
				goto end;
			}
			lpdu_cnt = buf[hdr_len+1] >> 4;
			hdr_len += 2 + lpdu_cnt;        // aircraft_id + NLP/DDR/P + LPDU size octets (one per LPDU)
			debug_print(D_PROTO, "uplink: ac %u lpdu_cnt: %u hdr_len: %u\n", i, lpdu_cnt, hdr_len);
		}
	}
	debug_print(D_PROTO, "hdr_len: %u\n", hdr_len);
	if(len < hdr_len + 2) {
		debug_print(D_PROTO, "Too short: %u < %u\n", len, hdr_len + lpdu_cnt + 2);
		statsd_increment_per_channel(freq, "frame.errors.too_short");
		goto end;
	}

	if(hfdl_pdu_fcs_check(buf, hdr_len)) {
		mpdu_header.crc_ok = true;
		statsd_increment_per_channel(freq, "frames.good");
	} else {
		statsd_increment_per_channel(freq, "frame.errors.bad_fcs");
		goto end;
	}

	uint8_t *dataptr = buf + hdr_len + 2;       // First data octet of the first LPDU
	if(mpdu_header.direction == DOWNLINK_PDU) {
		statsd_increment_per_channel(freq, "frame.dir.air2gnd");
		mpdu_header.src_id = buf[2];
		mpdu_header.dst_id = buf[1] & 0x7f;
		uint8_t *hdrptr = buf + 6;              // First LPDU size octet
		if(parse_lpdu_list(hdrptr, dataptr, buf + len, lpdu_cnt, &lpdu_list,
					mpdu_header, reasm_ctx, rx_timestamp) < 0) {
			goto end;
		}
	} else {                                    // UPLINK_PDU
		statsd_increment_per_channel(freq, "frame.dir.gnd2air");
		mpdu_header.src_id = buf[1] & 0x7f;
		mpdu_header.dst_id = 0;                 // See comment in mpdu_format_text()
		uint8_t *hdrptr = buf + 2;              // First AC ID octet
		int32_t consumed_octets = 0;
		for(uint32_t i = 0; i < aircraft_cnt; i++, hdrptr++, dataptr += consumed_octets) {
			mpdu_header.dst_id = *hdrptr++;
			lpdu_cnt = (*hdrptr++ >> 4) & 0xF;
			if(hfdl->Config.output_mpdus == true) {
				NEW(struct mpdu_dst, dst_ac);
				dst_ac->dst_id = mpdu_header.dst_id;
				dst_ac->lpdu_cnt = lpdu_cnt;
				mpdu->dst_aircraft = la_list_append(mpdu->dst_aircraft, dst_ac);
			}
			if((consumed_octets = parse_lpdu_list(hdrptr, dataptr, buf + len,
							lpdu_cnt, &lpdu_list, mpdu_header,
							reasm_ctx, rx_timestamp)) < 0) {
				goto end;
			}
		}
	}

end:
	if(hfdl->Config.output_mpdus && (mpdu_header.crc_ok || hfdl->Config.output_corrupted_pdus)) {
		mpdu->header = mpdu_header;
		result = la_list_append(result, mpdu_node);
		result->next = lpdu_list;
	} else {
		la_proto_tree_destroy(mpdu_node);
		result = lpdu_list;
	}
	return result;
}

static int32_t parse_lpdu_list(uint8_t *lpdu_len_ptr, uint8_t *data_ptr,
		uint8_t *endptr, uint32_t lpdu_cnt, la_list **lpdu_list,
		struct hfdl_pdu_hdr_data mpdu_header, la_reasm_ctx *reasm_ctx,
		struct timeval rx_timestamp) {
	int32_t consumed_octets = 0;
	for(uint32_t j = 0; j < lpdu_cnt; j++) {
		uint32_t lpdu_len = *lpdu_len_ptr + 1;
		if(data_ptr + lpdu_len <= endptr) {
			debug_print(D_PROTO, "lpdu %u/%u: lpdu_len=%u\n", j + 1, lpdu_cnt, lpdu_len);
			la_proto_node *node = lpdu_parse(data_ptr, lpdu_len, mpdu_header, reasm_ctx, rx_timestamp);
			if(node != NULL) {
				*lpdu_list = la_list_append(*lpdu_list, node);
			}
			data_ptr += lpdu_len;              // Move to the next LPDU
			consumed_octets += lpdu_len;
			lpdu_len_ptr++;
		} else {
			debug_print(D_PROTO, "lpdu %u/%u truncated: end is %td octets past buffer\n",
					j + 1, lpdu_cnt, data_ptr + lpdu_len - endptr);
			return -1;
		}
	}
	return consumed_octets;
}

static void mpdu_format_text(la_vstring *vstr, void const *data, int32_t indent) {
	ASSERT(vstr != NULL);
	ASSERT(data);
	ASSERT(indent >= 0);

	struct hfdl_mpdu const *mpdu = data;
	if(hfdl->Config.output_raw_frames == true) {
		append_hexdump_with_indent(vstr, mpdu->pdu->buf, mpdu->pdu->len, indent+1);
	}
	if(!mpdu->header.crc_ok) {
		// We say "PDU" rather than "MPDU", because if the CRC check failed the we are
		// not even sure whether it's a MPDU or SPDU...
		LA_ISPRINTF(vstr, indent, "-- Unparseable PDU (CRC check failed)\n");
		return;
	}
	if(mpdu->header.direction == UPLINK_PDU) {
		LA_ISPRINTF(vstr, indent, "Uplink MPDU:\n");
			indent++;
			gs_id_format_text(vstr, indent, "Src GS", mpdu->header.src_id);
			for(la_list *ac = mpdu->dst_aircraft; ac != NULL; ac = la_list_next(ac)) {
				struct mpdu_dst *dst = ac->data;
				ac_id_format_text(vstr, indent, "Dst AC", mpdu->header.freq, dst->dst_id);
				LA_ISPRINTF(vstr, indent+1, "LPDU count: %hhu\n", dst->lpdu_cnt);
			}
	} else {
		LA_ISPRINTF(vstr, indent, "Downlink MPDU:\n");
			indent++;
			ac_id_format_text(vstr, indent, "Src AC", mpdu->header.freq, mpdu->header.src_id);
			gs_id_format_text(vstr, indent, "Dst GS", mpdu->header.dst_id);
	}
}

static void mpdu_destroy(void *data) {
	if(data == NULL) {
		return;
	}
	struct hfdl_mpdu *mpdu = data;
	la_list_free(mpdu->dst_aircraft);
	XFREE(mpdu);
}

la_type_descriptor const proto_DEF_hfdl_mpdu = {
	.format_text = mpdu_format_text,
	.destroy = mpdu_destroy
};
