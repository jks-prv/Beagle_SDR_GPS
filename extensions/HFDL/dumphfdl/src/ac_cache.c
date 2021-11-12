/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <time.h>                           // time_t
#include <libacars/hash.h>                  // la_hash_*
#include "util.h"                           // NEW, debug_print
#include "cache.h"                          // cache_*
#include "ac_cache.h"                       // ac_cache

// This object is used to cache mappings between aircraft ID numbers (extracted
// from PDU headers) and their ICAO hex addresses. fwd_cache is used for
// forward lookups (eg. to replace aircraft ID with its ICAO code in formatted
// output), while inv_cache is used for inverse lookups, which are necessary
// for deletion of entries from the first map when a Logoff confirm LPDU is
// received (it contains the destination aircraft's ICAO code, but is always
// sent to the broadcast Aircraft ID (255), so an inverse lookup is necessary
// to locate the entry in fwd_cache for cleaning).

struct ac_cache {
	struct cache *fwd_cache;
	struct cache *inv_cache;
};

// Hash key definitions.
// Forward hash is keyed by frequency+ID, as IDs are channel-specific.
struct ac_cache_fwd_key {
	int32_t freq;
	uint8_t id;
};
// Inverse hash is keyed with icao_address only, as every aircraft may
// be logged on on a single frequency at a time.
struct ac_cache_inv_key {
	uint32_t icao_address;
};

// Inverse mapping cache entry definition.
// This one is used for internal purposes only, so it's defined here
// rather than in ac_cache.h.
struct ac_cache_inv_entry {
	int32_t freq;
	uint8_t id;
};

#define AC_CACHE_TTL 3600L
#define AC_CACHE_EXPIRATION_INTERVAL 309L

/******************************
 * Forward declarations
 ******************************/

static struct cache_vtable ac_cache_fwd_vtable;
static struct cache_vtable ac_cache_inv_vtable;
static bool ac_cache_fwd_entry_create(ac_cache const *cache, int32_t freq,
		uint8_t id, uint32_t icao_address, time_t created_time);
static bool ac_cache_inv_entry_create(ac_cache const *cache, int32_t freq,
		uint8_t id, uint32_t icao_address, time_t created_time);
static bool ac_cache_entry_perform_delete(ac_cache const *cache, int32_t freq,
		uint32_t icao_address, bool check_frequency);

/******************************
 * Public methods
 ******************************/

ac_cache *ac_cache_create(void) {
	NEW(ac_cache, cache);
	cache->fwd_cache = cache_create("ac_fwd", &ac_cache_fwd_vtable, AC_CACHE_TTL, AC_CACHE_EXPIRATION_INTERVAL);
	cache->inv_cache = cache_create("ac_inv", &ac_cache_inv_vtable, AC_CACHE_TTL, AC_CACHE_EXPIRATION_INTERVAL);
	return cache;
}

void ac_cache_entry_create(ac_cache *cache, int32_t freq,
		uint8_t id, uint32_t icao_address) {
	ASSERT(cache != NULL);

	// Check if there is an entry for this (freq,id) key in the cache.
	// If there is, then delete it using ac_cache_entry_perform_delete()
	// which makes sure that the entry gets deleted from both caches
	// (forward and inverse). Just running ac_cache_fwd_entry_create()
	// would cause the existing forward entry to be deleted, but it would
	// leave a stray inverse entry.
	struct ac_cache_entry *entry = ac_cache_entry_lookup(cache, freq, id);
	if(entry != NULL) {
		// Get a copy of the address, so that it's still accessible
		// to debug_print() after ac_cache_entry_perform_delete() destroys the entry.
		uint32_t icao_address_copy = entry->icao_address;
		if(ac_cache_entry_perform_delete(cache, freq, icao_address_copy, false)) {
			debug_print(D_CACHE, "%hhu@%d: Existing entry deleted (was for %06X)\n",
					id, freq, icao_address_copy);
		}
	}
	// Check if there is an entry for this icao_address in the cache.
	// If there is, then delete it (regardless of the frequency), since
	// the aircraft can only be logged on on a single frequency at a time.
	if(ac_cache_entry_perform_delete(cache, freq, icao_address, false) == true) {
		debug_print(D_CACHE, "Existing entry for %06X deleted\n", icao_address);
	}

	time_t now = time(NULL);
	bool result = ac_cache_fwd_entry_create(cache, freq, id, icao_address, now);
	UNUSED(result);     // silence compiler warning when DEBUG=off
	if(result) {
		debug_print(D_CACHE, "%hhu@%d: warning: forward entry overwritten\n", id, freq);
	}
	result = ac_cache_inv_entry_create(cache, freq, id, icao_address, now);
	if(result) {
		debug_print(D_CACHE, "%06X: warning: inverse entry overwritten\n", icao_address);
	}

	debug_print(D_CACHE, "new entry: %hhu@%d: %06X\n",
			id, freq, icao_address);
}

bool ac_cache_entry_delete(ac_cache *cache, int32_t freq,
		uint32_t icao_address) {
	ASSERT(cache != NULL);

	return ac_cache_entry_perform_delete(cache, freq, icao_address, true);
}

struct ac_cache_entry *ac_cache_entry_lookup(ac_cache *cache, int32_t freq, uint8_t id) {
	ASSERT(cache != NULL);

	// Periodic cache expiration
	time_t now = time(NULL);
	cache_expire(cache->fwd_cache, now);
	cache_expire(cache->inv_cache, now);

	struct ac_cache_fwd_key fwd_key = { .freq = freq, .id = id };
	struct ac_cache_entry *e = cache_entry_lookup(cache->fwd_cache, &fwd_key);
	if(e != NULL) {
		debug_print(D_CACHE, "%hhu@%d: %06X\n", id, freq, e->icao_address);
	} else {
		debug_print(D_CACHE, "%hhu@%d: not found\n", id, freq);
	}
	return e;
}

void ac_cache_destroy(ac_cache *cache) {
	if(cache != NULL) {
		cache_destroy(cache->fwd_cache);
		cache_destroy(cache->inv_cache);
		XFREE(cache);
	}
}

/****************************************
 * Private variables and methods
 ****************************************/

static uint32_t ac_cache_fwd_key_hash(void const *key);
static bool ac_cache_fwd_key_compare(void const *key1, void const *key2);
static void ac_cache_fwd_entry_destroy(void *data);
static struct cache_vtable ac_cache_fwd_vtable = {
	.cache_key_hash = ac_cache_fwd_key_hash,
	.cache_key_compare = ac_cache_fwd_key_compare,
	.cache_key_destroy = la_simple_free,
	.cache_entry_data_destroy = ac_cache_fwd_entry_destroy
};

static uint32_t ac_cache_inv_key_hash(void const *key);
static bool ac_cache_inv_key_compare(void const *key1, void const *key2);
static struct cache_vtable ac_cache_inv_vtable = {
	.cache_key_hash = ac_cache_inv_key_hash,
	.cache_key_compare = ac_cache_inv_key_compare,
	.cache_key_destroy = la_simple_free,
	.cache_entry_data_destroy = la_simple_free
};

static uint32_t ac_cache_fwd_key_hash(void const *key) {
	ASSERT(key);
	struct ac_cache_fwd_key const *k = key;
	return k->freq + k->id;
}

static bool ac_cache_fwd_key_compare(void const *key1, void const *key2) {
	ASSERT(key1);
	ASSERT(key2);
	struct ac_cache_fwd_key const *k1 = key1;
	struct ac_cache_fwd_key const *k2 = key2;
	return (k1->freq == k2->freq && k1->id == k2->id);
}

static void ac_cache_fwd_entry_destroy(void *data) {
	if(data != NULL) {
		struct ac_cache_entry *e = data;
		if(e->callsign) {
			XFREE(e->callsign);
		}
		XFREE(e);
	}
}

static uint32_t ac_cache_inv_key_hash(void const *key) {
	struct ac_cache_inv_key const *k = key;
	return k->icao_address;
}

static bool ac_cache_inv_key_compare(void const *key1, void const *key2) {
	ASSERT(key1);
	ASSERT(key2);
	struct ac_cache_inv_key const *k1 = key1;
	struct ac_cache_inv_key const *k2 = key2;
	return (k1->icao_address == k2->icao_address);
}

static bool ac_cache_fwd_entry_create(ac_cache const *cache, int32_t freq,
		uint8_t id, uint32_t icao_address, time_t created_time) {
	ASSERT(cache != NULL);
	NEW(struct ac_cache_entry, fwd_entry);
	fwd_entry->icao_address = icao_address;
	NEW(struct ac_cache_fwd_key, fwd_key);
	fwd_key->freq = freq;
	fwd_key->id = id;
	return cache_entry_create(cache->fwd_cache, fwd_key, fwd_entry, created_time);
}

static bool ac_cache_inv_entry_create(ac_cache const *cache, int32_t freq,
		uint8_t id, uint32_t icao_address, time_t created_time) {
	ASSERT(cache != NULL);

	NEW(struct ac_cache_inv_entry, inv_entry);
	inv_entry->id = id;
	inv_entry->freq = freq;
	NEW(struct ac_cache_inv_key, inv_key);
	inv_key->icao_address = icao_address;
	return cache_entry_create(cache->inv_cache, inv_key, inv_entry, created_time);
}

static bool ac_cache_entry_perform_delete(ac_cache const *cache, int32_t freq,
		uint32_t icao_address, bool check_frequency) {
	ASSERT(cache != NULL);

	bool result = false;
	struct ac_cache_inv_key inv_key = { .icao_address = icao_address };
	struct ac_cache_inv_entry *e = cache_entry_lookup(cache->inv_cache, &inv_key);
	if(e != NULL) {
		if(check_frequency && e->freq != freq) {
			debug_print(D_CACHE, "%06X: entry is on a different frequency (requested: %d cached %d), delete skipped\n",
					icao_address, freq, e->freq);
			result = false;
		} else {
			struct ac_cache_fwd_key fwd_key = { .freq = e->freq, .id = e->id };
			result  = cache_entry_delete(cache->inv_cache, &inv_key);
			result &= cache_entry_delete(cache->fwd_cache, &fwd_key);
			if(result) {
				debug_print(D_CACHE, "entry deleted: %06X@%d: %hhu\n", icao_address, fwd_key.freq, fwd_key.id);
			} else {
				debug_print(D_CACHE, "entry deletion failed: %06X@%d: %hhu\n", icao_address, fwd_key.freq, fwd_key.id);
			}
		}
	} else {
		debug_print(D_CACHE, "entry not deleted: %06X@%d: not found\n", icao_address, freq);
	}
	return result;
}
