/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include <stdint.h>
#include <libacars/list.h>          // la_list
#include "util.h"                   // struct octet_string

la_list *spdu_parse(struct octet_string *pdu, int32_t freq);
