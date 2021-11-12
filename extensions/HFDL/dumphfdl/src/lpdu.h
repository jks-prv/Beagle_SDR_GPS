/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include <stdint.h>
#include <sys/time.h>                   // struct timeval
#include <stdint.h>
#include <libacars/libacars.h>          // la_type_descriptor, la_proto_node
#include <libacars/reassembly.h>        // la_reasm_ctx
#include "pdu.h"                        // struct hfdl_pdu_hdr_data
#include "position.h"                   // struct position_info

la_proto_node *lpdu_parse(uint8_t *buf, uint32_t len, struct hfdl_pdu_hdr_data
		mpdu_header, la_reasm_ctx *reasm_ctx, struct timeval rx_timestamp);
struct position_info *lpdu_position_info_extract(la_proto_node *tree);
