/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include <stdint.h>
#include <time.h>
#include <libacars/hash.h>

struct cache_vtable {
	la_hash_func *cache_key_hash;
	la_hash_compare_func *cache_key_compare;
	void (*cache_key_destroy)(void *key);
	void (*cache_entry_data_destroy)(void *entry);
};

typedef struct cache cache;

cache *cache_create(char const *cache_name, struct cache_vtable const *vtable,
		time_t ttl, time_t expiration_interval);
bool cache_entry_create(cache *c, void *key, void *value,
		time_t created_time);
bool cache_entry_delete(cache *c, void *key);
void *cache_entry_lookup(cache *c, void const *key);
int32_t cache_expire(cache *c, time_t current_timestamp);
void cache_destroy(cache *c);
