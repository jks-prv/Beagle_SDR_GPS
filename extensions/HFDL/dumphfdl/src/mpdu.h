/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include <stdint.h>
#include <sys/time.h>               // struct timeval
#include <libacars/reassembly.h>    // la_reasm_ctx
#include <libacars/list.h>          // la_list
#include "util.h"                   // struct octet_string

la_list *mpdu_parse(struct octet_string *pdu, la_reasm_ctx *reasm_ctx, struct
		timeval rx_timestamp, int32_t freq);
