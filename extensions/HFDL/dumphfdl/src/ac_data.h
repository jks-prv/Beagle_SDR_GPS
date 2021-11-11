/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef struct ac_data ac_data;

struct ac_data_entry {
	char *registration;
	char *icaotypecode;
	char *operatorflagcode;
	char *manufacturer;
	char *type;
	char *registeredowners;
	bool exists;
};

ac_data *ac_data_create(char const *bs_db_file);
struct ac_data_entry *ac_data_entry_lookup(ac_data *ac_data, uint32_t icao_address);
void ac_data_destroy(ac_data *ac_data);
