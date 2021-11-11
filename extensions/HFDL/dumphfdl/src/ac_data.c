/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <stdint.h>
#include <stdio.h>
#include "hfdl_config.h"         // WITH_SQLITE
#include "ac_data.h"        // struct ac_data_entry
#include "util.h"           // UNUSED

#ifdef WITH_SQLITE

#include <stdbool.h>
#include <string.h>         // strdup
#include <time.h>           // time_t, time()
#include <sqlite3.h>
#include "cache.h"          // cache_*
#include "statsd.h"         // statsd_*

struct ac_data {
	cache *cache;
	sqlite3 *db;
	sqlite3_stmt *stmt;
};

#define AC_DATA_TTL 3600L
#define AC_DATA_EXPIRATION_INTERVAL 305L
#define BS_DB_COLUMNS "Registration,ICAOTypeCode,OperatorFlagCode,Manufacturer,Type,RegisteredOwners"

/******************************
 * Forward declarations
 ******************************/

static struct cache_vtable ac_data_vtable;
static struct ac_data_entry *ac_data_entry_fetch_from_db(struct ac_data *ac_data, uint32_t icao_address);
static void ac_data_entry_create(ac_data *ac_data, uint32_t icao_address, struct ac_data_entry *entry);

// XXX: statsd counters are global across all ac_data objects
// For dumphfdl this is not an issue , since there is only one such object in every program run.
#ifdef WITH_STATSD
static char *ac_data_counters[] = {
	"ac_data.db.hits",
	"ac_data.db.misses",
	"ac_data.db.errors",
	NULL
};
static bool statsd_counters_initialized = false;
#endif

struct ac_data *ac_data_create(char const *bs_db_file) {
	if(bs_db_file == NULL) {
		return NULL;
	}
	NEW(struct ac_data, ac_data);
	ac_data->db = NULL;

	int32_t rc = sqlite3_open_v2(bs_db_file, &ac_data->db, SQLITE_OPEN_READONLY, NULL);
	if(rc != 0){
		fprintf(stderr, "Can't open database %s: %s\n", bs_db_file, sqlite3_errmsg(ac_data->db));
		goto fail;
	}
	rc = sqlite3_prepare_v2(ac_data->db, "SELECT " BS_DB_COLUMNS " FROM Aircraft WHERE ModeS = ?", -1, &ac_data->stmt, NULL);
	if(rc != SQLITE_OK) {
		fprintf(stderr, "%s: could not query Aircraft table: %s\n", bs_db_file, sqlite3_errmsg(ac_data->db));
		goto fail;
	}
	ac_data->cache = cache_create("ac_data", &ac_data_vtable, AC_DATA_TTL, AC_DATA_EXPIRATION_INTERVAL);
	ASSERT(ac_data->cache != NULL);
// XXX: statsd counters are global across all ac_data objects
#ifdef WITH_STATSD
	if(statsd_counters_initialized == false) {
		statsd_initialize_counter_set(ac_data_counters);
		statsd_counters_initialized = true;
	}
#endif
	if(ac_data_entry_lookup(ac_data, 0) == NULL) {
		fprintf(stderr, "%s: test query failed, database is unusable.\n", bs_db_file);
		goto fail;
	}
	fprintf(stderr, "%s: database opened\n", bs_db_file);
	return ac_data;
fail:
	ac_data_destroy(ac_data);
	return NULL;
}

struct ac_data_entry *ac_data_entry_lookup(ac_data *ac_data, uint32_t icao_address) {
	ASSERT(ac_data);

	// Periodic cache expiration
	cache_expire(ac_data->cache, time(NULL));

	struct ac_data_entry *entry = cache_entry_lookup(ac_data->cache, &icao_address);
	if(entry != NULL) {
		debug_print(D_CACHE, "%06X: %s cache hit\n", icao_address, entry->exists ? "positive" : "negative");
	} else {
		entry = ac_data_entry_fetch_from_db(ac_data, icao_address);
		if(entry != NULL) {
			debug_print(D_CACHE, "%06X: %sfound in BS DB\n", icao_address, entry->exists ? "" : "not ");
			ac_data_entry_create(ac_data, icao_address, entry);
		} else {
			debug_print(D_CACHE, "%06X: BS DB query failure\n", icao_address);
		}
	}
	return entry;
}

void ac_data_destroy(struct ac_data *ac_data) {
	if(ac_data) {
		sqlite3_finalize(ac_data->stmt);
		sqlite3_close(ac_data->db);
		cache_destroy(ac_data->cache);
		XFREE(ac_data);
	}
}

static uint32_t uint32_t_hash(void const *key) {
	return *(uint32_t *)key;
}

static bool uint32_t_compare(void const *key1, void const *key2) {
	return *(uint32_t *)key1 == *(uint32_t *)key2;
}

static void ac_data_entry_destroy(void *data) {
	if(data != NULL) {
		struct ac_data_entry *e = data;
		XFREE(e->registration);
		XFREE(e->icaotypecode);
		XFREE(e->operatorflagcode);
		XFREE(e->manufacturer);
		XFREE(e->type);
		XFREE(e->registeredowners);
		XFREE(e);
	}
}

static struct cache_vtable ac_data_vtable = {
	.cache_key_hash = uint32_t_hash,
	.cache_key_compare = uint32_t_compare,
	.cache_key_destroy = la_simple_free,
	.cache_entry_data_destroy = ac_data_entry_destroy
};

static struct ac_data_entry *ac_data_entry_fetch_from_db(struct ac_data *ac_data, uint32_t icao_address) {
	ASSERT(ac_data);

	NEW(struct ac_data_entry, entry);

	char hex_address[7];
	if(snprintf(hex_address, sizeof(hex_address), "%06X", icao_address) != sizeof(hex_address) - 1) {
		debug_print(D_CACHE, "could not convert icao_address %u to ICAO hex string - too large?\n", icao_address);
		goto fail;
	}

	int32_t rc = sqlite3_reset(ac_data->stmt);
	if(rc != SQLITE_OK) {
		debug_print(D_CACHE, "sqlite3_reset() returned error %d\n", rc);
		statsd_increment("ac_data.db.errors");
		goto fail;
	}
	rc = sqlite3_bind_text(ac_data->stmt, 1, hex_address, -1, SQLITE_STATIC);
	if(rc != SQLITE_OK) {
		debug_print(D_CACHE, "sqlite3_bind_text('%s') returned error %d\n", hex_address, rc);
		statsd_increment("ac_data.db.errors");
		goto fail;
	}
	rc = sqlite3_step(ac_data->stmt);
	if(rc == SQLITE_ROW) {
		if(sqlite3_column_count(ac_data->stmt) < 6) {
			debug_print(D_CACHE, "%s: not enough columns in the query result\n", hex_address);
			goto fail;
		}
		statsd_increment("ac_data.db.hits");
		char const *field = NULL;
		if((field = (char *)sqlite3_column_text(ac_data->stmt, 0)) != NULL) entry->registration = strdup(field);
		if((field = (char *)sqlite3_column_text(ac_data->stmt, 1)) != NULL) entry->icaotypecode = strdup(field);
		if((field = (char *)sqlite3_column_text(ac_data->stmt, 2)) != NULL) entry->operatorflagcode = strdup(field);
		if((field = (char *)sqlite3_column_text(ac_data->stmt, 3)) != NULL) entry->manufacturer = strdup(field);
		if((field = (char *)sqlite3_column_text(ac_data->stmt, 4)) != NULL) entry->type = strdup(field);
		if((field = (char *)sqlite3_column_text(ac_data->stmt, 5)) != NULL) entry->registeredowners = strdup(field);
		entry->exists = true;
	} else if(rc == SQLITE_DONE) {
		// No rows found
		entry->exists = false;
		statsd_increment("ac_data.db.misses");
	} else {
		debug_print(D_CACHE, "%s: unexpected query return code %d\n", hex_address, rc);
		statsd_increment("ac_data.db.errors");
	}
	return entry;
fail:
	ac_data_entry_destroy(entry);
	return NULL;
}

static void ac_data_entry_create(ac_data *ac_data, uint32_t icao_address, struct ac_data_entry *entry) {
	ASSERT(ac_data);

	NEW(uint32_t, key);
	*key = icao_address;
	cache_entry_create(ac_data->cache, key, entry, time(NULL));
}

#else // !WITH_SQLITE

struct ac_data *ac_data_create(char const *bs_db_file) {
	UNUSED(bs_db_file);
	return NULL;
}

struct ac_data_entry *ac_data_entry_lookup(ac_data *ac_data, uint32_t icao_address) {
	UNUSED(ac_data);
	UNUSED(icao_address);
	return NULL;
}

void ac_data_destroy(ac_data *ac_data) {
	UNUSED(ac_data);
}

#endif // WITH_SQLITE
