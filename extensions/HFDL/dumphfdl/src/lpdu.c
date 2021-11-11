/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdint.h>
#include <sys/time.h>               // struct timeval
#include <libacars/libacars.h>      // la_type_descriptor, la_proto_node
#include <libacars/reassembly.h>    // la_reasm_ctx
#include <libacars/dict.h>          // la_dict
#include "pdu.h"                    // struct hfdl_pdu_hdr_data, hfdl_pdu_fcs_check
#include "hfnpdu.h"                 // hfnpdu_parse
#include "ac_cache.h"               // ac_cache_entry_create, ac_cache_entry_delete
#include "globals.h"                // AC_cache, AC_cache_lock
#include "lpdu.h"
#include "statsd.h"                 // statsd_*
#include "util.h"                   // ASSERT, NEW, XCALLOC, XFREE, gs_id_format_text,
                                    // ac_id_format_text

// LPDU types
#define UNNUMBERED_DATA             0x0D
#define UNNUMBERED_ACKED_DATA       0x1D
#define LOGON_DENIED                0x2F
#define LOGOFF_REQUEST              0x3F
#define LOGON_RESUME_CONFIRM        0x5F
#define LOGON_RESUME                0x4F
#define LOGON_REQUEST_NORMAL        0x8F
#define LOGON_CONFIRM               0x9F
#define LOGON_REQUEST_DLS           0xBF

struct lpdu_logon_request {
	uint32_t icao_address;
};

struct lpdu_logon_confirm {
	uint32_t icao_address;
	uint8_t ac_id;
};

struct lpdu_logoff_request {
	uint32_t icao_address;
	uint8_t reason_code;
};

struct hfdl_lpdu {
	struct octet_string *pdu;
	struct hfdl_pdu_hdr_data mpdu_header;
	int32_t type;
	bool crc_ok;
	bool err;
	union {
		struct lpdu_logon_request logon_request;
		struct lpdu_logon_confirm logon_confirm;
		struct lpdu_logoff_request logoff_request;
	} data;
};

la_dict const lpdu_type_descriptions[] = {
	{ .id = UNNUMBERED_DATA,            .val = "Unnumbered data" },
	{ .id = UNNUMBERED_ACKED_DATA,      .val = "Unnumbered ack'ed data" },
	{ .id = LOGON_DENIED,               .val = "Logon denied" },
	{ .id = LOGOFF_REQUEST,             .val = "Logoff request" },
	{ .id = LOGON_RESUME_CONFIRM,       .val = "Logon resume confirm" },
	{ .id = LOGON_RESUME,               .val = "Logon resume" },
	{ .id = LOGON_REQUEST_NORMAL,       .val = "Logon request (normal)" },
	{ .id = LOGON_CONFIRM,              .val = "Logon confirm" },
	{ .id = LOGON_REQUEST_DLS,          .val = "Logon request (DLS)" },
	{ .id = 0,                          .val = 0 }
};

la_dict const logoff_request_reason_codes[] = {
	{ .id = 0x01,   .val = "Not within slot boundaries" },
	{ .id = 0x02,   .val = "Downlink set in uplink slot" },
	{ .id = 0x03,   .val = "RLS protocol error" },
	{ .id = 0x04,   .val = "Invalid aircraft ID" },
	{ .id = 0x05,   .val = "HFDL Ground Station subsystem does not support RLS" },
	{ .id = 0x06,   .val = "Other" },
	{ .id = 0x00,   .val = NULL }
};

la_dict const logon_denied_reason_codes[] = {
	{ .id = 0x01,   .val = "Aircraft ID not available" },
	{ .id = 0x02,   .val = "HFDL Ground Station subsystem does not support RLS" },
	{ .id = 0x00,   .val = NULL }
};

// Forward declarations
la_type_descriptor const proto_DEF_hfdl_lpdu;

static int32_t logon_confirm_parse(uint8_t *buf, uint32_t len, struct lpdu_logon_confirm *result) {
#define LOGON_CONFIRM_LPDU_LEN 8
	ASSERT(buf);
	ASSERT(result);

	if(len < LOGON_CONFIRM_LPDU_LEN) {
		return -1;
	}
	result->icao_address = parse_icao_hex(buf + 1);
	result->ac_id = buf[4];
	return LOGON_CONFIRM_LPDU_LEN;
}

static int32_t logon_request_parse(uint8_t *buf, uint32_t len, struct lpdu_logon_request *result) {
#define LOGON_REQUEST_LPDU_LEN 4
	if(len < LOGON_REQUEST_LPDU_LEN) {
		return -1;
	}
	result->icao_address = parse_icao_hex(buf + 1);
	return LOGON_REQUEST_LPDU_LEN;
}

static int32_t logoff_request_parse(uint8_t *buf, uint32_t len, struct lpdu_logoff_request *result) {
#define LOGOFF_REQUEST_LPDU_LEN 5
	ASSERT(buf);
	ASSERT(result);

	if(len < LOGOFF_REQUEST_LPDU_LEN) {
		return -1;
	}
	result->icao_address = parse_icao_hex(buf + 1);
	result->reason_code = buf[4];
	return LOGOFF_REQUEST_LPDU_LEN;
}

la_proto_node *lpdu_parse(uint8_t *buf, uint32_t len, struct hfdl_pdu_hdr_data
		mpdu_header, la_reasm_ctx *reasm_ctx, struct timeval rx_timestamp) {
	ASSERT(buf);

	int32_t freq = mpdu_header.freq;
	statsd_increment_per_channel(freq, "lpdus.processed");

	NEW(struct hfdl_lpdu, lpdu);
	lpdu->pdu = octet_string_new(buf, len);
	lpdu->mpdu_header = mpdu_header;
	la_proto_node *lpdu_node = la_proto_node_new();
	lpdu_node->td = &proto_DEF_hfdl_lpdu;
	lpdu_node->data = lpdu;

	if(len < 3) {       // Need at least LPDU type + FCS
		debug_print(D_PROTO, "Too short: %u < 3\n", len);
		lpdu->err = true;
		statsd_increment_per_channel(freq, "lpdu.errors.too_short");
		goto end;
	}

	len -= 2;           // Strip FCS
	lpdu->crc_ok = hfdl_pdu_fcs_check(buf, len);
	if(!lpdu->crc_ok) {
		lpdu->err = true;
		statsd_increment_per_channel(freq, "lpdu.errors.bad_fcs");
		goto end;
	}
	statsd_increment_per_channel(freq, "lpdus.good");

	int32_t consumed_len = 0;
	lpdu->type = buf[0];
	switch(lpdu->type) {
		case UNNUMBERED_DATA:
		case UNNUMBERED_ACKED_DATA:
			consumed_len = 1;       // Consume LPDU type octet only, a HFNPDU should follow next
			break;
		case LOGON_DENIED:
		case LOGOFF_REQUEST:
			consumed_len = logoff_request_parse(buf, len, &lpdu->data.logoff_request);
			if(consumed_len > 0) {
				AC_cache_lock();
				ac_cache_entry_delete(hfdl->AC_cache, freq, lpdu->data.logoff_request.icao_address);
				AC_cache_unlock();
			}
			break;
		case LOGON_CONFIRM:
		case LOGON_RESUME_CONFIRM:
			consumed_len = logon_confirm_parse(buf, len, &lpdu->data.logon_confirm);
			if(consumed_len > 0) {
				AC_cache_lock();
				ac_cache_entry_create(hfdl->AC_cache, freq, lpdu->data.logon_confirm.ac_id,
						lpdu->data.logon_confirm.icao_address);
				AC_cache_unlock();
			}
			break;
		case LOGON_RESUME:
		case LOGON_REQUEST_NORMAL:
		case LOGON_REQUEST_DLS:
			consumed_len = logon_request_parse(buf, len, &lpdu->data.logon_request);
			break;
		default:
			lpdu_node->next = unknown_proto_pdu_new(buf, len);
			consumed_len = len;
			break;
	}
	if(consumed_len < 0) {
		lpdu->err = true;
	} else if((uint32_t)consumed_len < len) {
		lpdu_node->next = hfnpdu_parse(buf + consumed_len, len - consumed_len, mpdu_header.direction,
				reasm_ctx, rx_timestamp);
	}
end:
	if(lpdu->err && !hfdl->Config.output_corrupted_pdus) {
		la_proto_tree_destroy(lpdu_node);
		lpdu_node = NULL;
	}
	return lpdu_node;
}

static void lpdu_format_text(la_vstring *vstr, void const *data, int32_t indent) {
	ASSERT(vstr != NULL);
	ASSERT(data);
	ASSERT(indent >= 0);

	struct hfdl_lpdu const *lpdu = data;
	if(hfdl->Config.output_raw_frames == true) {
		append_hexdump_with_indent(vstr, lpdu->pdu->buf, lpdu->pdu->len, indent+1);
	}
	if(lpdu->err) {
		LA_ISPRINTF(vstr, indent, "-- Unparseable LPDU%s\n",
				lpdu->crc_ok ? "" : " (CRC check failed)");
		return;
	}
	if(lpdu->mpdu_header.direction == UPLINK_PDU) {
		LA_ISPRINTF(vstr, indent, "Uplink LPDU:\n");
		indent++;
		gs_id_format_text(vstr, indent, "Src GS", lpdu->mpdu_header.src_id);
		ac_id_format_text(vstr, indent, "Dst AC", lpdu->mpdu_header.freq, lpdu->mpdu_header.dst_id);
	} else {
		LA_ISPRINTF(vstr, indent, "Downlink LPDU:\n");
		indent++;
		ac_id_format_text(vstr, indent, "Src AC", lpdu->mpdu_header.freq, lpdu->mpdu_header.src_id);
		gs_id_format_text(vstr, indent, "Dst GS", lpdu->mpdu_header.dst_id);
	}
	char const *lpdu_type = la_dict_search(lpdu_type_descriptions, lpdu->type);
	if(lpdu_type != NULL) {
		LA_ISPRINTF(vstr, indent, "Type: %s\n", lpdu_type);
	} else {
		LA_ISPRINTF(vstr, indent, "Type: unknown (0x%02x)\n", lpdu->type);
	}
	indent++;
	char *descr = NULL;
	switch(lpdu->type) {
		case LOGON_DENIED:
		case LOGOFF_REQUEST:
			LA_ISPRINTF(vstr, indent, "ICAO: %06X\n", lpdu->data.logoff_request.icao_address);
			ac_data_format_text(vstr, indent + 1, lpdu->data.logoff_request.icao_address);
			descr = la_dict_search(
					lpdu->type == LOGON_DENIED ? logon_denied_reason_codes : logoff_request_reason_codes,
					lpdu->data.logoff_request.reason_code
					);
			LA_ISPRINTF(vstr, indent, "Reason: %u (%s)\n", lpdu->data.logoff_request.reason_code,
					descr ? descr : "Reserved");
			break;
		case LOGON_CONFIRM:
		case LOGON_RESUME_CONFIRM:
			LA_ISPRINTF(vstr, indent, "ICAO: %06X\n", lpdu->data.logon_confirm.icao_address);
			ac_data_format_text(vstr, indent + 1, lpdu->data.logon_confirm.icao_address);
			LA_ISPRINTF(vstr, indent, "Assigned AC ID: %u\n", lpdu->data.logon_confirm.ac_id);
			break;
		case LOGON_RESUME:
		case LOGON_REQUEST_NORMAL:
		case LOGON_REQUEST_DLS:
			LA_ISPRINTF(vstr, indent, "ICAO: %06X\n", lpdu->data.logon_request.icao_address);
			ac_data_format_text(vstr, indent + 1, lpdu->data.logon_request.icao_address);
			break;
		default:
			return;
	}
}

struct position_info *lpdu_position_info_extract(la_proto_node *tree) {
	ASSERT(tree);

	// Don't bother with analyzing uplinks.
	la_proto_node *lpdu_node = la_proto_tree_find_protocol(tree, &proto_DEF_hfdl_lpdu);
	if(lpdu_node == NULL) {
		return NULL;
	}
	struct hfdl_lpdu const *lpdu = lpdu_node->data;
	if(lpdu->mpdu_header.direction != DOWNLINK_PDU) {
		return NULL;
	}

	// LPDUs never contain position information, hence we
	// go looking for it in the HFNPDU straight away.
	struct position_info *pos_info = hfnpdu_position_info_extract(tree);
	if(pos_info == NULL) {
		return NULL;
	}
	// Position information has been found. Check for any missing fields
	// and fill them in, if possible.
	if(pos_info->aircraft.icao_address_present == false) {
		switch(lpdu->type) {
			// These LPDU types have ICAO given directly
			case LOGON_RESUME:
			case LOGON_REQUEST_NORMAL:
			case LOGON_REQUEST_DLS:
				pos_info->aircraft.icao_address = lpdu->data.logon_request.icao_address;
				pos_info->aircraft.icao_address_present = true;
				debug_print(D_MISC, "icao_address: %06X (from LPDU)\n",
						pos_info->aircraft.icao_address);
				break;
				// For other LPDU types we need to look up the ICAO in the AC cache
			default:
				AC_cache_lock();
				uint8_t ac_id = lpdu->mpdu_header.direction == UPLINK_PDU ?
					lpdu->mpdu_header.dst_id : lpdu->mpdu_header.src_id;
				struct ac_cache_entry *entry = ac_cache_entry_lookup(hfdl->AC_cache,
						lpdu->mpdu_header.freq, ac_id);
				if(entry != NULL) {
					pos_info->aircraft.icao_address = entry->icao_address;
					pos_info->aircraft.icao_address_present = true;
					debug_print(D_MISC, "icao_address: %06X (from cache)\n",
							pos_info->aircraft.icao_address);
				}
				AC_cache_unlock();
				break;
		}
	}
	if(pos_info->aircraft.icao_address_present == true) {
		return pos_info;
	}
	// Position information without ICAO address is incomplete. Drop it.
	debug_print(D_MISC, "pos_info (%f, %f) incomplete: unknown icao_address\n",
			pos_info->position.location.lat, pos_info->position.location.lon);
	position_info_destroy(pos_info);
	return NULL;
}

static void lpdu_destroy(void *data) {
	if(data == NULL) {
		return;
	}
	struct hfdl_lpdu *lpdu = data;
	// No octet_string_destroy() here, because it frees the buffer, which is
	// invalid here (pdu->buf is a pointer into an existing MPDU buffer)
	XFREE(lpdu->pdu);
	XFREE(lpdu);
}

la_type_descriptor const proto_DEF_hfdl_lpdu = {
	.format_text = lpdu_format_text,
	.destroy = lpdu_destroy
};
