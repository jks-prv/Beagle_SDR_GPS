/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <libacars/libacars.h>          // la_proto_node

typedef struct systable systable;

enum systable_err_t {
	SYSTABLE_ERR_NONE,
	SYSTABLE_ERR_IO,
	SYSTABLE_ERR_FILE_PARSE,
	SYSTABLE_ERR_VALIDATE
};

systable *systable_create(char const *savefile);
bool systable_read_from_file(systable *st, char const *file);
void systable_destroy(systable *st);

char const *systable_error_text(systable const *st);
enum systable_err_t systable_error_type(systable const *st);
int32_t systable_file_error_line(systable const *st);

int32_t systable_get_version(systable const *st);
char const *systable_get_station_name(systable const *st, int32_t id);
double systable_get_station_frequency(systable const *st, int32_t gs_id, int32_t freq_id);
bool systable_is_available(systable const *st);

void systable_store_pdu(systable const *st, int16_t version, uint8_t seq_num,
		uint8_t pdu_set_len, uint8_t *buf, uint32_t len);
la_proto_node *systable_process_pdu_set(systable *st);
