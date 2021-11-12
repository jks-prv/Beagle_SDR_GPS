/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <math.h>                   // trunc
#include <libacars/libacars.h>      // la_type_descriptor, la_proto_node, LA_MSG_DIR_*
#include <libacars/reassembly.h>    // la_reasm_ctx
#include <libacars/acars.h>         // la_acars_parse
#include <libacars/adsc.h>          // la_adsc_*
#include <libacars/dict.h>          // la_dict
#include <libacars/list.h>          // la_list
#include "hfdl_config.h"                 // WITH_STATSD
#include "pdu.h"                    // enum hfdl_pdu_direction
#include "position.h"               // position_info
#include "statsd.h"                 // statsd_*
#include "util.h"                   // ASSERT

/******************************
 * Forward declarations
 ******************************/

//static void update_statsd_acars_metrics(la_msg_dir msg_dir, la_proto_node *root);
static struct position_info *adsc_position_info_extract(la_proto_node *tree);

/******************************
 * Public methods
 ******************************/

la_proto_node *acars_parse(uint8_t *buf, uint32_t len, enum hfdl_pdu_direction direction,
		la_reasm_ctx *reasm_ctx, struct timeval rx_timestamp) {
	ASSERT(buf);
	la_proto_node *node = NULL;
	if(len > 0 && buf[0] == 1) {        // ACARS SOH byte
		la_msg_dir msg_dir = (direction == UPLINK_PDU ? LA_MSG_DIR_GND2AIR : LA_MSG_DIR_AIR2GND);
		node = la_acars_parse_and_reassemble(buf + 1, len - 1, msg_dir, reasm_ctx, rx_timestamp);
#ifdef WITH_STATSD
		update_statsd_acars_metrics(msg_dir, node);
#endif
	}
	return node;
}

struct position_info *acars_position_info_extract(la_proto_node *tree) {
	ASSERT(tree);

	struct position_info *pos_info = NULL;
	pos_info = adsc_position_info_extract(tree);

	return pos_info;
}

/****************************************
 * Private variables and methods
 ****************************************/

#ifdef WITH_STATSD
static void update_statsd_acars_metrics(la_msg_dir msg_dir, la_proto_node *root) {
	static la_dict const reasm_status_counter_names[] = {
		{ .id = LA_REASM_UNKNOWN, .val = "acars.reasm.unknown" },
		{ .id = LA_REASM_COMPLETE, .val = "acars.reasm.complete" },
		// { .id = LA_REASM_IN_PROGRESS, .val = "acars.reasm.in_progress" },    // report final states only
		{ .id = LA_REASM_SKIPPED, .val = "acars.reasm.skipped" },
		{ .id = LA_REASM_DUPLICATE, .val = "acars.reasm.duplicate" },
		{ .id = LA_REASM_FRAG_OUT_OF_SEQUENCE, .val = "acars.reasm.out_of_seq" },
		{ .id = LA_REASM_ARGS_INVALID, .val = "acars.reasm.invalid_args" },
		{ .id = 0, .val = NULL }
	};
	la_proto_node *node = la_proto_tree_find_acars(root);
	if(node == NULL) {
		return;
	}
	la_acars_msg *amsg = node->data;
	if(amsg->err == true) {
		return;
	}
	char const *metric = la_dict_search(reasm_status_counter_names, amsg->reasm_status);
	if(metric == NULL) {
		return;
	}
	// Dropping const on metric is allowed here, because dumphfdl metric names
	// do not contain any characters that statsd-c-client library would need to
	// sanitize (replace with underscores)
	statsd_increment_per_msgdir(msg_dir, (char *)metric);
}
#endif

static struct position_info *adsc_position_info_extract(la_proto_node *tree) {
	ASSERT(tree);

	struct position_info *pos_info = NULL;
	la_proto_node *adsc_node = la_proto_tree_find_adsc(tree);
	if(adsc_node == NULL) {
		return NULL;
	}
	la_adsc_msg_t *msg = adsc_node->data;
	ASSERT(msg != NULL);
	if(msg->err == true || msg->tag_list == NULL) {
		return NULL;
	}
	bool position_present = false;
	bool icao_address_present = false;
	bool flight_id_present = false;
	double lat = 0.0, lon = 0.0, timestamp = 0.0;
	uint32_t icao_address = 0;
	char *flight_id = NULL;
	la_adsc_basic_report_t *rpt_tag = NULL;
	la_adsc_flight_id_t *flight_id_tag = NULL;
	la_adsc_airframe_id_t *airframe_id_tag = NULL;
	for(la_list *l = msg->tag_list; l != NULL; l = la_list_next(l)) {
		la_adsc_tag_t *tag = l->data;
		ASSERT(tag != NULL);
		switch(tag->tag) {
			// Slightly ugly, but libacars does not have any better API
			// for searching / matching tags
			case 7:     // Basic report
			case 9:     // Emergency basic report
			case 10:    // Lateral deviation change event
			case 18:    // Vertical rate change event
			case 19:    // Altitude range event
			case 20:    // Waypoint change event
				rpt_tag = tag->data;
				ASSERT(rpt_tag != NULL);
				lat = rpt_tag->lat;
				lon = rpt_tag->lon;
				timestamp = rpt_tag->timestamp;
				position_present = true;
				break;
			case 17:    // Airframe ID
				airframe_id_tag = tag->data;
				ASSERT(airframe_id_tag != NULL);
				uint8_t *hex = airframe_id_tag->icao_hex;
				icao_address = (uint32_t)hex[0] << 16 | (uint32_t)hex[1] << 8 | (uint32_t)hex[2];
				icao_address_present = true;
				break;
			case 12:    // Flight ID
				flight_id_tag = tag->data;
				ASSERT(flight_id_tag != NULL);
				flight_id = flight_id_tag->id;
				flight_id_present = true;
				break;
			default:
				break;
		}
	}

	if(position_present == false) {
		debug_print(D_MISC, "No position found\n");
		return NULL;
	}
	pos_info = position_info_create();
	pos_info->position.location.lat = lat;
	pos_info->position.location.lon = lon;
	pos_info->position.timestamp.tm.tm_min = trunc(timestamp / 60.0);
	pos_info->position.timestamp.tm.tm_sec = trunc(timestamp - 60.0 * trunc(timestamp / 60.0));
	pos_info->position.timestamp.tm_min_present = true;
	pos_info->position.timestamp.tm_sec_present = true;
	if(icao_address_present) {
		pos_info->aircraft.icao_address = icao_address;
		pos_info->aircraft.icao_address_present = true;
	}
	if(flight_id_present) {
		pos_info->aircraft.flight_id = flight_id;
		pos_info->aircraft.flight_id_present = true;
	}
	debug_print(D_MISC, "lat: %f lon: %f t: %02d:%02d icao_addr: %06X flight_id: %s\n",
			pos_info->position.location.lat,
			pos_info->position.location.lon,
			pos_info->position.timestamp.tm.tm_min,
			pos_info->position.timestamp.tm.tm_sec,
			icao_address_present ? pos_info->aircraft.icao_address : 0,
			flight_id_present ? pos_info->aircraft.flight_id : ""
			);
	return pos_info;
}
