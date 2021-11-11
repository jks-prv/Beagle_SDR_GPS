/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdint.h>
#include <string.h>                 // memcpy
#include <sys/time.h>               // struct timeval
#include <libacars/libacars.h>      // la_type_descriptor, la_proto_node, LA_MSG_DIR_*
#include <libacars/reassembly.h>    // la_reasm_ctx
#include <libacars/dict.h>          // la_dict
#include "pdu.h"                    // enum hfdl_pdu_direction
#include "acars.h"                  // acars_parse
#include "position.h"               // position_info
#include "util.h"                   // ASSERT, NEW, XCALLOC, XFREE, freq_list_format_text, gs_id_format_text

// HFNPDU types
#define SYSTEM_TABLE            0xD0
#define PERFORMANCE_DATA        0xD1
#define SYSTEM_TABLE_REQUEST    0xD2
#define FREQUENCY_DATA          0xD5
#define DELAYED_ECHO            0xDE
#define ENVELOPED_DATA          0xFF

#define extract_uint16_t(buf) ((uint16_t)((buf)[0]) | (uint16_t)((buf)[1]) << 8)

struct time {
	uint8_t hour, min, sec;
};

struct flight_leg_stats {
	uint16_t freq_search_cnt;
	uint16_t hf_data_disabled_duration;
};

struct mpdu_stats {
	uint8_t cnt_1800bps, cnt_1200bps, cnt_600bps, cnt_300bps;
};

struct hfnpdu_perf_data {
	char flight_id[7];
	struct location location;
	struct time utc_time;
	uint8_t version;
	uint8_t flight_leg;
	uint8_t gs_id;
	uint8_t freq_id;
	struct flight_leg_stats prev_leg, cur_leg;
	struct mpdu_stats mpdus_rx, mpdus_rx_errs, mpdus_tx, mpdus_delivered;
	uint16_t spdus_rx,spdus_rx_errs;
	uint8_t freq_change_code;
};

struct prop_freqs_data {
	uint8_t gs_id;
	uint32_t prop_freqs;
	uint32_t tuned_freqs;
};

#define PROP_FREQS_CNT_MAX 6
struct hfnpdu_freq_data {
	char flight_id[7];
	uint32_t propagating_freqs_cnt;
	struct location location;
	struct time utc_time;
	struct prop_freqs_data propagating_freqs[PROP_FREQS_CNT_MAX];
};

struct hfnpdu_systable_request_data {
	uint16_t request_data;
};

struct hfnpdu_systable_data {
	uint16_t systable_version;
	uint8_t total_pdu_cnt;
	uint8_t pdu_seq_num;
};

struct hfdl_hfnpdu {
	int32_t type;
	bool err;
	union {
		struct hfnpdu_perf_data perf_data;
		struct hfnpdu_freq_data freq_data;
		struct hfnpdu_systable_request_data systable_request_data;
		struct hfnpdu_systable_data systable_data;
	} data;
};

la_dict const hfnpdu_type_descriptions[] = {
	{ .id = SYSTEM_TABLE,         .val = "System table (partial)" },
	{ .id = PERFORMANCE_DATA,     .val = "Performance data" },
	{ .id = SYSTEM_TABLE_REQUEST, .val = "System table request" },
	{ .id = FREQUENCY_DATA,       .val = "Frequency data" },
	{ .id = DELAYED_ECHO,         .val = "Delayed echo" },
	{ .id = ENVELOPED_DATA,       .val = "Enveloped data" },
	{ .id = 0,                    .val = NULL }
};

// Forward declarations
la_type_descriptor const proto_DEF_hfdl_hfnpdu;

static struct time parse_utc_time(uint32_t t) {
	return (struct time){
		.hour = t / (60 * 60),
		.min = t % (60 * 60) / 60,
		.sec = t % 60
	};
}

// System table is split across several PDUs which have to be reassembled before
// decoding. This routine parses only the initial part (up to System table version field),
// which is repeated in every PDU.
static int32_t systable_parse(uint8_t *buf, uint32_t len, struct hfnpdu_systable_data *result) {
#define SYSTABLE_HFNPDU_MIN_LEN 5       // From HFNPDU type to system table version
	ASSERT(buf);
	ASSERT(result);

	if(len < SYSTABLE_HFNPDU_MIN_LEN) {
		debug_print(D_PROTO, "Too short: %u < %u\n", len, SYSTABLE_HFNPDU_MIN_LEN);
		return -1;
	}
	result->total_pdu_cnt = (buf[2] >> 4) + 1;
	result->pdu_seq_num = buf[2] & 0xF;
	result->systable_version = buf[3] >> 4 | buf[4] << 4;
	return SYSTABLE_HFNPDU_MIN_LEN;
}

static int32_t systable_request_parse(uint8_t *buf, uint32_t len, struct hfnpdu_systable_request_data *result) {
#define SYSTABLE_REQUEST_HFNPDU_LEN 4
	ASSERT(buf);
	ASSERT(result);

	if(len < SYSTABLE_REQUEST_HFNPDU_LEN) {
		debug_print(D_PROTO, "Too short: %u < %u\n", len, SYSTABLE_REQUEST_HFNPDU_LEN);
		return -1;
	}
	result->request_data = extract_uint16_t(buf + 2);
	return SYSTABLE_REQUEST_HFNPDU_LEN;
}

static int32_t performance_data_parse(uint8_t *buf, uint32_t len, struct hfnpdu_perf_data *result) {
#define PERFORMANCE_DATA_HFNPDU_LEN 47
	ASSERT(buf);
	ASSERT(result);

	if(len < PERFORMANCE_DATA_HFNPDU_LEN) {
		debug_print(D_PROTO, "Too short: %u < %u\n", len, PERFORMANCE_DATA_HFNPDU_LEN);
		return -1;
	}
	memcpy(result->flight_id, buf + 2, 6);
	result->flight_id[6] = '\0';

	uint32_t coord = buf[8] | buf[9] << 8 | (buf[10] & 0xF) << 16;
	result->location.lat = parse_coordinate(coord);
	coord = (buf[10] & 0xF0) >> 4 | buf[11] << 4 | buf[12] << 12;
	result->location.lon = parse_coordinate(coord);

	result->utc_time = parse_utc_time(2 * extract_uint16_t(buf + 13));
	result->version = buf[15];
	result->flight_leg = buf[16];
	result->gs_id = buf[17] & 0x7F;
	result->freq_id = buf[18];
	result->prev_leg.freq_search_cnt = extract_uint16_t(buf + 19);
	result->cur_leg.freq_search_cnt = extract_uint16_t(buf + 21);
	result->prev_leg.hf_data_disabled_duration = extract_uint16_t(buf + 23);
	result->cur_leg.hf_data_disabled_duration = extract_uint16_t(buf + 25);
	result->mpdus_rx = (struct mpdu_stats){
		.cnt_1800bps = buf[27],
		.cnt_1200bps = buf[28],
		.cnt_600bps  = buf[29],
		.cnt_300bps  = buf[30]
	};
	result->mpdus_rx_errs = (struct mpdu_stats){
		.cnt_1800bps = buf[31],
		.cnt_1200bps = buf[32],
		.cnt_600bps  = buf[33],
		.cnt_300bps  = buf[34]
	};
	result->spdus_rx = extract_uint16_t(buf + 35);
	result->spdus_rx_errs = buf[37];
	result->mpdus_tx = (struct mpdu_stats){
		.cnt_1800bps = buf[38],
		.cnt_1200bps = buf[39],
		.cnt_600bps  = buf[40],
		.cnt_300bps  = buf[41]
	};
	result->mpdus_delivered = (struct mpdu_stats){
		.cnt_1800bps = buf[42],
		.cnt_1200bps = buf[43],
		.cnt_600bps  = buf[44],
		.cnt_300bps  = buf[45]
	};
	result->freq_change_code = buf[46] & 0xF;
	return PERFORMANCE_DATA_HFNPDU_LEN;
}

static int32_t frequency_data_parse(uint8_t *buf, uint32_t len, struct hfnpdu_freq_data *result) {
#define FREQUENCY_DATA_HFNPDU_MIN_LEN 15
#define PROP_FREQ_DATA_LEN 6
	ASSERT(buf);
	ASSERT(result);

	if(len < FREQUENCY_DATA_HFNPDU_MIN_LEN) {
		debug_print(D_PROTO, "Too short: %u < %u\n", len, FREQUENCY_DATA_HFNPDU_MIN_LEN);
		return -1;
	}
	memcpy(result->flight_id, buf + 2, 6);
	result->flight_id[6] = '\0';

	uint32_t coord = buf[8] | buf[9] << 8 | (buf[10] & 0xF) << 16;
	result->location.lat = parse_coordinate(coord);
	coord = (buf[10] & 0xF0) >> 4 | buf[11] << 4 | buf[12] << 12;
	result->location.lon = parse_coordinate(coord);

	result->utc_time = parse_utc_time(2 * extract_uint16_t(buf + 13));

	uint32_t consumed_len = FREQUENCY_DATA_HFNPDU_MIN_LEN;
	for(uint32_t f = 0; f < PROP_FREQS_CNT_MAX; f++) {
		uint32_t pos = FREQUENCY_DATA_HFNPDU_MIN_LEN + f * PROP_FREQ_DATA_LEN;
		if(pos + PROP_FREQ_DATA_LEN <= len) {
			result->propagating_freqs[f].gs_id = buf[pos] & 0x7F;
			result->propagating_freqs[f].prop_freqs =
				buf[pos+1] | buf[pos+2] << 8 | (buf[pos+3] & 0xF) << 16;
			result->propagating_freqs[f].tuned_freqs =
				(buf[pos+3] & 0xF0) >> 4 | buf[pos+4] << 4 | buf[pos+5] << 12;
			result->propagating_freqs_cnt++;
			consumed_len += PROP_FREQ_DATA_LEN;
		} else {
			break;
		}
	}
	debug_print(D_PROTO, "prop_freq_data_cnt: %u octets left: %u\n",
			result->propagating_freqs_cnt, len - consumed_len);
	return consumed_len;
}

la_proto_node *hfnpdu_parse(uint8_t *buf, uint32_t len, enum hfdl_pdu_direction direction,
		la_reasm_ctx *reasm_ctx, struct timeval rx_timestamp) {
	ASSERT(buf);

	if(len == 0) {
		return NULL;
	}
	if(buf[0] != 0xFF) {
		debug_print(D_PROTO, "Not a HFNPDU\n");
		return unknown_proto_pdu_new(buf, len);
	}
	if(len < 2) {
		debug_print(D_PROTO, "Too short: %u < 2\n", len);
		return NULL;
	}

	NEW(struct hfdl_hfnpdu, hfnpdu);
	la_proto_node *node = la_proto_node_new();
	node->td = &proto_DEF_hfdl_hfnpdu;
	node->data = hfnpdu;

	int32_t consumed_len = 0;
	hfnpdu->type = buf[1];
	switch(hfnpdu->type) {
		case SYSTEM_TABLE:
			// Parse the initial part of the PDU (it gets repeated in all PDUs in the set)
			consumed_len = systable_parse(buf, len, &hfnpdu->data.systable_data);
			if(consumed_len > 0 && (uint32_t)consumed_len < len) {
				Systable_lock();
				// If there is any data left, then store it for reassembly
				systable_store_pdu(hfdl->Systable,
						hfnpdu->data.systable_data.systable_version,
						hfnpdu->data.systable_data.pdu_seq_num,
						hfnpdu->data.systable_data.total_pdu_cnt,
						buf + consumed_len, len - consumed_len);
				// Try to reassemble and decode the complete table
				node->next = systable_process_pdu_set(hfdl->Systable);
				Systable_unlock();
			}
			break;
		case PERFORMANCE_DATA:
			consumed_len = performance_data_parse(buf, len, &hfnpdu->data.perf_data);
			break;
		case SYSTEM_TABLE_REQUEST:
			consumed_len = systable_request_parse(buf, len, &hfnpdu->data.systable_request_data);
			break;
		case FREQUENCY_DATA:
			consumed_len = frequency_data_parse(buf, len, &hfnpdu->data.freq_data);
			break;
		case DELAYED_ECHO:
			break;
		case ENVELOPED_DATA:
			node->next = acars_parse(buf + 2, len - 2, direction, reasm_ctx, rx_timestamp);
			if(node->next == NULL) {
				node->next = unknown_proto_pdu_new(buf + 2, len - 2);
			}
			break;
		default:
			break;
	}
	if(consumed_len < 0) {
		hfnpdu->err = true;
	}
	return node;
}

static void hfnpdu_destroy(void *data) {
	if(data == NULL) {
		return;
	}
	struct hfdl_hfnpdu *hfnpdu = data;
	XFREE(hfnpdu);
}

static void mpdu_stats_format_text(la_vstring *vstr, int32_t indent, struct mpdu_stats const *stats, char const *label) {
	ASSERT(vstr);
	ASSERT(stats);
	ASSERT(indent > 0);

	LA_ISPRINTF(vstr, indent, "%s: 300 bps: %3hhu   600 bps: %3hhu   1200 bps: %3hhu   1800 bps: %3hhu\n",
			label, stats->cnt_300bps, stats->cnt_600bps, stats->cnt_1200bps, stats->cnt_1800bps);
}

static la_dict const freq_change_code_descriptions[] = {
	{ .id = 0, .val = "First freq. search in this flight leg" },
	{ .id = 1, .val = "Too many NACKs" },
	{ .id = 2, .val = "SPDUs no longer received" },
	{ .id = 3, .val = "HFDL disabled" },
	{ .id = 4, .val = "GS frequency change" },
	{ .id = 5, .val = "GS down / channel down" },
	{ .id = 6, .val = "Poor uplink channel quality" },
	{ .id = 7, .val = "No change" },
	{ .id = 0, .val = NULL }
};

static void performance_data_format_text(la_vstring *vstr, int32_t indent, struct hfnpdu_perf_data const *pdu) {
	ASSERT(vstr);
	ASSERT(pdu);
	ASSERT(indent > 0);

	LA_ISPRINTF(vstr, indent, "Version: %hhu\n", pdu->version);
	LA_ISPRINTF(vstr, indent, "Flight ID: %s\n", pdu->flight_id);
	LA_ISPRINTF(vstr, indent, "Lat: %.7f\n", pdu->location.lat);
	LA_ISPRINTF(vstr, indent, "Lon: %.7f\n", pdu->location.lon);
	LA_ISPRINTF(vstr, indent, "Time: %02hhu:%02hhu:%02hhu\n",
			pdu->utc_time.hour, pdu->utc_time.min, pdu->utc_time.sec);
	LA_ISPRINTF(vstr, indent, "Flight leg: %hhu\n", pdu->flight_leg);
	gs_id_format_text(vstr, indent, "GS ID", pdu->gs_id);
	freq_list_format_text(vstr, indent, "Frequency", pdu->gs_id, 1u << pdu->freq_id);
	LA_ISPRINTF(vstr, indent, "Frequency search count:\n");
	LA_ISPRINTF(vstr, indent + 1, "This leg: %hu\n", pdu->cur_leg.freq_search_cnt);
	LA_ISPRINTF(vstr, indent + 1, "Prev leg: %hu\n", pdu->prev_leg.freq_search_cnt);
	LA_ISPRINTF(vstr, indent, "HFDL disabled duration:\n");
	LA_ISPRINTF(vstr, indent + 1, "This leg: %hu sec\n", pdu->cur_leg.hf_data_disabled_duration);
	LA_ISPRINTF(vstr, indent + 1, "Prev leg: %hu sec\n", pdu->prev_leg.hf_data_disabled_duration);
	mpdu_stats_format_text(vstr, indent, &pdu->mpdus_rx,        "MPDUs received             ");
	mpdu_stats_format_text(vstr, indent, &pdu->mpdus_rx_errs,   "MPDUs received with errors ");
	mpdu_stats_format_text(vstr, indent, &pdu->mpdus_tx,        "MPDUs transmitted          ");
	mpdu_stats_format_text(vstr, indent, &pdu->mpdus_delivered, "MPDUs delivered            ");
	LA_ISPRINTF(vstr, indent, "SPDUs received: %hu\n", pdu->spdus_rx);
	LA_ISPRINTF(vstr, indent, "SPDUs missed: %hu\n", pdu->spdus_rx_errs);
	char const *desc = la_dict_search(freq_change_code_descriptions, pdu->freq_change_code);
	LA_ISPRINTF(vstr, indent, "Last frequency change cause: %hhu (%s)\n", pdu->freq_change_code,
			desc ? desc : "unknown");
}

static void systable_format_text(la_vstring *vstr, int32_t indent, struct hfnpdu_systable_data const *data) {
	ASSERT(vstr);
	ASSERT(data);
	ASSERT(indent > 0);

	LA_ISPRINTF(vstr, indent, "Version: %hu\n", data->systable_version);
	LA_ISPRINTF(vstr, indent, "Part: %u of %hhu\n", data->pdu_seq_num + 1u, data->total_pdu_cnt);
}

static void systable_request_format_text(la_vstring *vstr, int32_t indent, struct hfnpdu_systable_request_data const *data) {
	ASSERT(vstr);
	ASSERT(data);
	ASSERT(indent > 0);

	LA_ISPRINTF(vstr, indent, "Request data: 0x%hx\n", data->request_data);
}

static void propagating_freqs_format_text(la_vstring *vstr, int32_t indent, struct prop_freqs_data const *data) {
	ASSERT(vstr);
	ASSERT(data);
	ASSERT(indent > 0);

	gs_id_format_text(vstr, indent, "GS ID", data->gs_id);
	indent++;
	freq_list_format_text(vstr, indent+1, "Listening on", data->gs_id, data->tuned_freqs);
	freq_list_format_text(vstr, indent+1, "Heard on", data->gs_id, data->prop_freqs);
}

static void frequency_data_format_text(la_vstring *vstr, int32_t indent, struct hfnpdu_freq_data const *pdu) {
	ASSERT(vstr);
	ASSERT(pdu);
	ASSERT(indent > 0);

	LA_ISPRINTF(vstr, indent, "Flight ID: %s\n", pdu->flight_id);
	LA_ISPRINTF(vstr, indent, "Lat: %.7f\n", pdu->location.lat);
	LA_ISPRINTF(vstr, indent, "Lon: %.7f\n", pdu->location.lon);
	LA_ISPRINTF(vstr, indent, "Time: %02hhu:%02hhu:%02hhu\n",
			pdu->utc_time.hour, pdu->utc_time.min, pdu->utc_time.sec);
	for(uint32_t f = 0; f < pdu->propagating_freqs_cnt; f++) {
		propagating_freqs_format_text(vstr, indent, &pdu->propagating_freqs[f]);
	}
}

static void hfnpdu_format_text(la_vstring *vstr, void const *data, int32_t indent) {
	ASSERT(vstr != NULL);
	ASSERT(data);
	ASSERT(indent >= 0);

	struct hfdl_hfnpdu const *hfnpdu = data;
	if(hfnpdu->err) {
		LA_ISPRINTF(vstr, indent, "-- Unparseable HFNPDU\n");
		return;
	}
	char const *hfnpdu_type = la_dict_search(hfnpdu_type_descriptions, hfnpdu->type);
	if(hfnpdu_type != NULL) {
		LA_ISPRINTF(vstr, indent, "%s:\n", hfnpdu_type);
	} else {
		LA_ISPRINTF(vstr, indent, "Unknown HFNPDU type (0x%02x):\n", hfnpdu->type);
	}
	indent++;
	switch(hfnpdu->type) {
		case SYSTEM_TABLE:
			systable_format_text(vstr, indent, &hfnpdu->data.systable_data);
			break;
		case PERFORMANCE_DATA:
			performance_data_format_text(vstr, indent, &hfnpdu->data.perf_data);
			break;
		case SYSTEM_TABLE_REQUEST:
			systable_request_format_text(vstr, indent, &hfnpdu->data.systable_request_data);
			break;
		case FREQUENCY_DATA:
			frequency_data_format_text(vstr, indent, &hfnpdu->data.freq_data);
			break;
		case DELAYED_ECHO:
		case ENVELOPED_DATA:
		default:
			break;
	}
}

struct position_info *hfnpdu_position_info_extract(la_proto_node *tree) {
	ASSERT(tree);

	struct position_info *pos_info = NULL;
	la_proto_node *hfnpdu_node = la_proto_tree_find_protocol(tree, &proto_DEF_hfdl_hfnpdu);
	if(hfnpdu_node != NULL) {
		struct hfdl_hfnpdu *hfnpdu = hfnpdu_node->data;
		struct timestamp *ts = NULL;
		struct location *loc = NULL;
		switch(hfnpdu->type) {
			case PERFORMANCE_DATA:
				pos_info = position_info_create();
				struct hfnpdu_perf_data *perf_data = &hfnpdu->data.perf_data;
				pos_info->aircraft.flight_id = perf_data->flight_id;
				if(pos_info->aircraft.flight_id[0] != '\0') {
					pos_info->aircraft.flight_id_present = true;
				}

				ts = &pos_info->position.timestamp;
				ts->tm.tm_hour = perf_data->utc_time.hour;
				ts->tm.tm_min  = perf_data->utc_time.min;
				ts->tm.tm_sec  = perf_data->utc_time.sec;
				ts->tm_hour_present = ts->tm_min_present =
					ts->tm_sec_present = true;

				loc = &pos_info->position.location;
				loc->lat = perf_data->location.lat;
				loc->lon = perf_data->location.lon;
				break;
			case FREQUENCY_DATA:
				pos_info = position_info_create();
				struct hfnpdu_freq_data *freq_data = &hfnpdu->data.freq_data;
				pos_info->aircraft.flight_id = freq_data->flight_id;
				if(pos_info->aircraft.flight_id[0] != '\0') {
					pos_info->aircraft.flight_id_present = true;
				}
				ts = &pos_info->position.timestamp;
				ts->tm.tm_hour = freq_data->utc_time.hour;
				ts->tm.tm_min  = freq_data->utc_time.min;
				ts->tm.tm_sec  = freq_data->utc_time.sec;
				ts->tm_hour_present = ts->tm_min_present =
					ts->tm_sec_present = true;

				loc = &pos_info->position.location;
				loc->lat = freq_data->location.lat;
				loc->lon = freq_data->location.lon;
				break;
			case ENVELOPED_DATA:
				pos_info = acars_position_info_extract(tree);
				break;
			default:
				break;
		}
	}
	return pos_info;
}

la_type_descriptor const proto_DEF_hfdl_hfnpdu = {
	.format_text = hfnpdu_format_text,
	.destroy = hfnpdu_destroy
};
