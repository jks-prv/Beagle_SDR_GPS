/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include <stdint.h>
#include <time.h>
#include "cache.h"              // struct cache_entry

typedef struct ac_cache ac_cache;

struct ac_cache_entry {
	char *callsign;
	uint32_t icao_address;
};

ac_cache *ac_cache_create(void);
void ac_cache_entry_create(ac_cache *cache, int32_t freq,
		uint8_t id, uint32_t icao_address);
bool ac_cache_entry_delete(ac_cache *cache, int32_t freq,
		uint32_t icao_address);
struct ac_cache_entry *ac_cache_entry_lookup(ac_cache *cache,
		int32_t freq, uint8_t id);
void ac_cache_destroy(ac_cache *cache);
