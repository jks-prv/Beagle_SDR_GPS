/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include <stdint.h>
#include <libacars/libacars.h>      // la_proto_node
#include <libacars/reassembly.h>    // la_reasm_ctx
#include "pdu.h"                    // enum hfdl_pdu_direction
#include "position.h"               // struct position_info

la_proto_node *acars_parse(uint8_t *buf, uint32_t len, enum hfdl_pdu_direction direction,
		la_reasm_ctx *reasm_ctx, struct timeval rx_timestamp);
struct position_info *acars_position_info_extract(la_proto_node *tree);
