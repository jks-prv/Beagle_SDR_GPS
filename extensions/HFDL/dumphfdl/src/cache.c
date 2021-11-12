/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <string.h>                         // strdup
#include <time.h>                           // time_t
#include <libacars/hash.h>                  // la_hash_*
#include <libacars/vstring.h>               // la_vstring
#include "util.h"                           // NEW, debug_print
#include "cache.h"
#include "statsd.h"                         // statsd_*

struct cache_entry {
	time_t created_time;
	void (*data_destroy)(void *data);
	void *data;
};

struct cache {
	struct cache_vtable const *vtable;
	la_hash *table;
	la_vstring *statsd_metric_name__entry_count;
	char *name;
	time_t last_expiration_time;
	time_t ttl;
	time_t expiration_interval;
	uint32_t num_entries;
};

#define CACHE_DEFAULT_NAME "__default__"
#ifdef WITH_STATSD
#define CACHE_ENTRY_COUNT_ADD(cache, n) do { \
	if((n) < 0 && (cache)->num_entries < (uint32_t)(-(n))) { \
		(cache)->num_entries = 0; \
	} else { \
		(cache)->num_entries += (n); \
	} \
	statsd_set((cache)->statsd_metric_name__entry_count->str, (cache)->num_entries); \
	debug_print(D_CACHE, "%s: num_entries: %u\n", (cache)->name, (cache)->num_entries); \
} while(0)
#else
#define CACHE_ENTRY_COUNT_ADD(cache, n) nop()
#endif


/******************************
 * Forward declarations
 ******************************/

static bool is_cache_entry_expired(void const *key, void const *value, void *ctx);
static void cache_entry_destroy(void *entry);

/******************************
 * Public methods
 ******************************/

cache *cache_create(char const *cache_name, struct cache_vtable const *vtable,
		time_t ttl, time_t expiration_interval) {
	ASSERT(vtable);

	NEW(struct cache, cache);
	cache->vtable = vtable;
	cache->ttl = ttl;
	cache->expiration_interval = expiration_interval;
	cache->table = la_hash_new(vtable->cache_key_hash, vtable->cache_key_compare,
			vtable->cache_key_destroy, cache_entry_destroy);
	cache->name = strdup(cache_name);

#ifdef WITH_STATSD
	cache->statsd_metric_name__entry_count = la_vstring_new();
	la_vstring_append_sprintf(cache->statsd_metric_name__entry_count,
			"cache.%s.entries", cache_name ? cache_name : CACHE_DEFAULT_NAME);
#endif

	cache->last_expiration_time = time(NULL);
	return cache;
}

bool cache_entry_create(cache *c, void *key, void *value, time_t created_time) {
	ASSERT(c);
	ASSERT(key);
	// NULL 'value' ptr is allowed

	NEW(struct cache_entry, entry);
	entry->data = value;
	entry->created_time = created_time;
	entry->data_destroy = c->vtable->cache_entry_data_destroy;
	bool result = la_hash_insert(c->table, key, entry);
	if(!result) {       // increment entry count only when the entry was added, not replaced
		CACHE_ENTRY_COUNT_ADD(c, 1);
	}
	return result;
}

bool cache_entry_delete(cache *c, void *key) {
	ASSERT(c);
	ASSERT(key);

	bool result = la_hash_remove(c->table, key);
	if(result) {        // decrement entry count only when we've actually deleted something
		CACHE_ENTRY_COUNT_ADD(c, -1);
	}
	return result;
}

void *cache_entry_lookup(cache *c, void const *key) {
	ASSERT(c);
	ASSERT(key);

	struct cache_entry *e = la_hash_lookup(c->table, key);
	if(e == NULL) {
		return NULL;
	} else if(e->created_time + c->ttl < time(NULL)) {
		debug_print(D_CACHE, "%s: key %p: entry expired\n", c->name, key);
		return NULL;
	}
	return e->data;
}

int32_t cache_expire(cache *c, time_t current_timestamp) {
	ASSERT(c);

	int32_t expired_cnt = 0;
	if(c->last_expiration_time + c->expiration_interval <= current_timestamp) {
		time_t min_created_time = current_timestamp - c->ttl;
		expired_cnt = la_hash_foreach_remove(c->table, is_cache_entry_expired,
				&min_created_time);
		CACHE_ENTRY_COUNT_ADD(c, -expired_cnt);
		debug_print(D_CACHE, "%s: last_gc: %ld, current_timestamp: %ld, expired %d cache entries\n",
				c->name, c->last_expiration_time, current_timestamp, expired_cnt);
		c->last_expiration_time = current_timestamp;
	}
	return expired_cnt;
}

void cache_destroy(cache *c) {
	if(c != NULL) {
		la_hash_destroy(c->table);
		la_vstring_destroy(c->statsd_metric_name__entry_count, true);
		XFREE(c->name);
		XFREE(c);
	}
}

/****************************************
 * Private variables and methods
 ****************************************/

// Callback for la_hash_foreach_remove
// Used to expire old entries from the cache
static bool is_cache_entry_expired(void const *key, void const *value, void *ctx) {
	UNUSED(key);

	struct cache_entry const *entry = value;
	time_t min_created_time = *(time_t *)ctx;
	return (entry->created_time <= min_created_time);
}

static void cache_entry_destroy(void *entry) {
	if(entry != NULL) {
		struct cache_entry *e = entry;
		if(e->data_destroy) {
			e->data_destroy(e->data);
		}
		XFREE(e);
	}
}
