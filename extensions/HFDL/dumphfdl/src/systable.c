#include <stdint.h>
#include <stdbool.h>
#include <string.h>                 // strdup
#include <math.h>                   // fabs
#include <libconfig.h>              // config_*
#include <libacars/libacars.h>      // la_proto_node
#include <libacars/dict.h>          // la_dict_*
#include <libacars/list.h>          // la_list
#include "systable.h"
#include "util.h"                   // NEW, XFREE, struct location, parse_coordinate

enum systable_err_code {
	ST_ERR_OK = 0,
	ST_ERR_LIBCONFIG,
	ST_ERR_VERSION_MISSING,
	ST_ERR_VERSION_OUT_OF_RANGE,
	ST_ERR_STATIONS_MISSING,
	ST_ERR_STATION_WRONG_TYPE,
	ST_ERR_STATION_ID_MISSING,
	ST_ERR_STATION_ID_OUT_OF_RANGE,
	ST_ERR_STATION_ID_DUPLICATE,
	ST_ERR_STATION_NAME_WRONG_TYPE,
	ST_ERR_STATION_COORDINATE_MISSING,
	ST_ERR_STATION_COORDINATE_WRONG_TYPE,
	ST_ERR_FREQUENCIES_MISSING,
	ST_ERR_FREQUENCY_WRONG_TYPE,
	ST_ERR_MAX
};

static la_dict const systable_error_messages[] = {
	{
		.id = ST_ERR_OK,
		.val = "no error"
	},
	{
		.id = ST_ERR_VERSION_MISSING,
		.val = "version missing or wrong type (must be an integer)"
	},
	{
		.id = ST_ERR_VERSION_OUT_OF_RANGE,
		.val = "version out of range"
	},
	{
		.id = ST_ERR_STATIONS_MISSING,
		.val = "stations missing or wrong type (must be a list)"
	},
	{
		.id = ST_ERR_STATION_WRONG_TYPE,
		.val = "station setting has wrong type (must be a group)"
	},
	{
		.id = ST_ERR_STATION_ID_MISSING,
		.val = "station id missing or wrong type (must be an integer)"
	},
	{
		.id = ST_ERR_STATION_ID_OUT_OF_RANGE,
		.val = "station id out of range"
	},
	{
		.id = ST_ERR_STATION_ID_DUPLICATE,
		.val = "duplicate station id"
	},
	{
		.id = ST_ERR_STATION_NAME_WRONG_TYPE,
		.val = "name setting has wrong type (must be a string)"
	},
	{
		.id = ST_ERR_STATION_COORDINATE_MISSING,
		.val = "station latitude or longitude missing (need both or neither)"
	},
	{
		.id = ST_ERR_STATION_COORDINATE_WRONG_TYPE,
		.val = "station coordinate has wrong type (must be a floating-point number)"
	},
	{
		.id = ST_ERR_FREQUENCIES_MISSING,
		.val = "frequencies missing or wrong type (must be a list)"
	},
	{
		.id = ST_ERR_FREQUENCY_WRONG_TYPE,
		.val = "frequency setting has wrong type (must be a number)"
	},
	{
		.id = 0,
		.val = NULL
	}
};

#define STATION_ID_MAX 127
#define SYSTABLE_VERSION_MAX 4095

// Parameters of a single ground station
// as decoded from a System table PDU set
struct systable_gs_data {
	struct location gs_location;
	uint32_t frequencies[GS_MAX_FREQ_CNT];
	uint8_t master_frame_slots[GS_MAX_FREQ_CNT];
	uint8_t gs_id;
	uint8_t freq_cnt;
	uint8_t spdu_version;
	bool utc_sync;
};

// Decoded System table PDU set
// Returned as la_proto_node.
struct systable_decoding_result {
	la_list *gs_list;                                       // a list of structs systable_gs_data
	int16_t version;
	bool err;
};

// Storage for received System Table PDUs (raw buffers) to be decoded after reassembly
struct systable_pdu_set {
	struct octet_string **pdus;                             // an array of 'len' received PDUs
	int16_t version;                                        // systable version contained in this PDU set
	uint8_t len;                                            // total number of PDUs in set
};

struct systable_err {
	config_t *cfg;                                          // the config on which the last operation has failed
	enum systable_err_code code;                            // error code of the last operation
	enum systable_err_t type;                               // last error type
};

struct _systable {
	config_t cfg;                                           // system table as a libconfig object
	config_setting_t const *stations[STATION_ID_MAX+1];     // quick access to GS params (indexed by GS ID)
	struct systable_pdu_set *pdu_set;                       // System Table PDUs received from ground stations
	char *savefile_path;                                    // where to save updated table
	struct systable_err err;                                // diagnostics for the last operation
	bool available;                                         // is this system table ready to use
};

// Main system table object
struct systable {
	struct _systable *current, *new;
	struct systable_err err;                                // diagnostics for the last operation
};

/******************************
 * Forward declarations
 ******************************/

la_type_descriptor proto_DEF_systable_decoding_result;
static bool systable_validate(struct _systable *st);
static bool systable_populate_stations_cache(struct _systable *st);
static bool systable_is_newer(int32_t v_old, int32_t v_new);
static bool systable_generate_config(config_t *cfg, struct systable_decoding_result *result);
static bool systable_save_config(struct _systable *st);
static struct _systable *_systable_create(char const *savefile);
static struct systable_pdu_set *pdu_set_create(uint8_t len);
static struct systable_decoding_result *systable_decode(uint8_t *buf, uint32_t len);
static void systable_copy_station_names(config_t *dst, config_setting_t const **stations);
static void pdu_set_destroy(struct systable_pdu_set *ps);

/******************************
 * Public methods
 ******************************/

systable *systable_create(char const *savefile) {
	NEW(systable, st);
	st->current = _systable_create(savefile);
	st->new = _systable_create(savefile);
	return st;
}

bool systable_read_from_file(systable *st, char const *file) {
	ASSERT(st);
	ASSERT(file);
	config_init(&st->current->cfg);
	if(config_read_file(&st->current->cfg, file) != CONFIG_TRUE) {
		st->err.code = ST_ERR_LIBCONFIG;
		st->err.cfg = &st->current->cfg;
		return false;
	}
	bool result = systable_validate(st->current);
	if(result) {
		result &= systable_populate_stations_cache(st->current);
	}
	// If there was an error, then propagate it up from st->new to st,
	// so that systable_error_text() can return the correct error message.
	if(!result) {
		st->err = st->current->err;
	}
	st->current->available = result;
	return st->current->available;
}

char const *systable_error_text(systable const *st) {
	if(st == NULL) {
		return NULL;
	}
	ASSERT(st->err.code < ST_ERR_MAX);
	if(st->err.code == ST_ERR_LIBCONFIG) {
		return config_error_text(st->err.cfg);
	}
	return la_dict_search(systable_error_messages, st->err.code);
}

int32_t systable_file_error_line(systable const *st) {
	if(st == NULL) {
		return 0;
	}
	return config_error_line(st->err.cfg);
}

enum systable_err_t systable_error_type(systable const *st) {
	if(st == NULL) {
		return SYSTABLE_ERR_NONE;
	} else if(st->err.code == ST_ERR_LIBCONFIG) {
		config_error_t err = config_error_type(st->err.cfg);
		switch(err) {
			case CONFIG_ERR_FILE_IO:
				return SYSTABLE_ERR_IO;
			case CONFIG_ERR_PARSE:
				return SYSTABLE_ERR_FILE_PARSE;
			case CONFIG_ERR_NONE:
			default:
				return SYSTABLE_ERR_NONE;
		}
	}
	return st->err.type;
}

int32_t systable_get_version(systable const *st) {
	int32_t version = -1;       // invalid default value
	if(systable_is_available(st)) {
		config_lookup_int(&st->current->cfg, "version", &version);
	}
	return version;
}

char const *systable_get_station_name(systable const *st, int32_t id) {
	char const *name = NULL;
	if(systable_is_available(st) && id >= 0 && id < STATION_ID_MAX &&
			st->current->stations[id] != NULL) {
		config_setting_lookup_string(st->current->stations[id], "name", &name);
	}
	return name;
}

double systable_get_station_frequency(systable const *st, int32_t gs_id, int32_t freq_id) {
	if(systable_is_available(st) && gs_id >= 0 && gs_id < STATION_ID_MAX &&
			st->current->stations[gs_id] != NULL) {
		config_setting_t *frequencies = config_setting_get_member(st->current->stations[gs_id],
					"frequencies");
		config_setting_t *freq = config_setting_get_elem(frequencies, freq_id);
		if(freq != NULL) {
			int32_t type = config_setting_type(freq);
			if(type == CONFIG_TYPE_FLOAT) {
				return config_setting_get_float(freq);
			} else if(type == CONFIG_TYPE_INT) {
				return (double)config_setting_get_int(freq);
			}
		}
	}
	return -1.0;
}

bool systable_is_available(systable const *st) {
	return st != NULL && st->current->available;
}

void _systable_destroy(struct _systable *st) {
	if(st != NULL) {
		config_destroy(&st->cfg);
		XFREE(st->savefile_path);
		XFREE(st);
	}
}

void systable_destroy(systable *st) {
	if(st != NULL) {
		_systable_destroy(st->current);
		_systable_destroy(st->new);
		XFREE(st);
	}
}

void systable_store_pdu(systable const *st, int16_t version, uint8_t idx,
		uint8_t pdu_set_len, uint8_t *buf, uint32_t buf_len) {
	if(st == NULL || pdu_set_len < 1 || idx >= pdu_set_len) {
		return;
	}
	struct systable_pdu_set *ps = st->new->pdu_set;
	// If we have some PDUs stored already but the version or the PDU set
	// length do not match, then ditch the old PDU set and start over.
	if(ps != NULL && (ps->version != version || ps->len != pdu_set_len)) {
		debug_print(D_MISC, "PDU set params do not match (version: %d -> %d, set_len: %hhu -> %hhu), "
				"discarding PDU set\n", ps->version, version, ps->len, pdu_set_len);
		pdu_set_destroy(ps);
		ps = NULL;
	}
	// Allocate a PDU set if not done before
	if(ps == NULL) {
		ps = st->new->pdu_set = pdu_set_create(pdu_set_len);
		ps->version = version;
	}
	// If there is a PDU stored already at idx and its contents is different
	// than the new one, then replace it.
	if(ps->pdus[idx] != NULL) {
		if(ps->pdus[idx]->len != buf_len || memcmp(ps->pdus[idx]->buf, buf, buf_len) != 0) {
			debug_print(D_MISC, "PDU %hhu already exists and is different - replacing\n", idx);
			octet_string_destroy(ps->pdus[idx]);
			ps->pdus[idx] = NULL;
		} else {
			debug_print(D_MISC, "PDU %hhu already exists - skipping\n", idx);
		}
	}
	if(ps->pdus[idx] == NULL) {
		ps->pdus[idx] = octet_string_copy(&(struct octet_string){ .buf = buf, .len = buf_len });
	}
}

// A one-shot function which:
// - reassembles the PDU set if it's complete
// - decodes the reassembled System Table message
// - if the new system table is newer than the current one
//   (or if there is no current one):
//   - replaces the currently used system table with the new one
//   - stores the new system table in the savefile, if configured to do so
// Returns the decoded system table as a la_proto_node or NULL
// if decoding was not attempted due to incomplete PDU set.
la_proto_node *systable_process_pdu_set(systable *st) {
	if(st == NULL) {
		return NULL;
	}
	// Check if we have a complete PDU set
	uint32_t total_len = 0;
	struct systable_pdu_set *ps = st->new->pdu_set;
	if(ps != NULL && ps->len > 0 && ps->pdus != NULL) {
		for(uint8_t i = 0; i < ps->len; i++) {
			if(ps->pdus[i] == NULL) {
				debug_print(D_MISC, "Not ready to decode systable, PDU %hhu missing\n", i);
				return NULL;
			}
			total_len += ps->pdus[i]->len;
		}
	} else {
		return NULL;
	}
	// All PDUs present - reassemble the complete message and decode it
	uint8_t *buf = XCALLOC(total_len, sizeof(uint8_t));
	uint32_t pos = 0;
	for(uint8_t i = 0; i < ps->len; i++) {
		memcpy(buf + pos, ps->pdus[i]->buf, ps->pdus[i]->len);
		pos += ps->pdus[i]->len;
	}
	debug_print_buf_hex(D_PROTO_DETAIL, buf, total_len, "Reassembled message:\n");
	struct systable_decoding_result *result = systable_decode(buf, total_len);
	ASSERT(result != NULL);
	result->version = ps->version;

	pdu_set_destroy(st->new->pdu_set);
	st->new->pdu_set = NULL;
	XFREE(buf);

	if(!result->err &&
			(!systable_is_available(st) || systable_is_newer(systable_get_version(st), result->version))) {
		debug_print(D_MISC, "Decoded systable is newer than the current one (%d > %d), updating\n",
				result->version, systable_get_version(st));
		config_init(&st->new->cfg);
		if(systable_generate_config(&st->new->cfg, result)) {
			systable_copy_station_names(&st->new->cfg, st->current->stations);
			if(systable_save_config(st->new)) {
				fprintf(stderr, "System table version %d saved to %s\n", result->version, st->new->savefile_path);
			} else {
				st->err = st->new->err;
				fprintf(stderr, "Could not save system table to %s: %s\n", st->new->savefile_path,
						systable_error_text(st));
			}
			if(systable_populate_stations_cache(st->new)) {
				_systable_destroy(st->current);
				st->current = st->new;
				st->current->available = true;
			} else {
				st->err = st->new->err;
				fprintf(stderr, "Failed to populate ground station cache: %s\n",
						systable_error_text(st));
				fprintf(stderr,	"Keeping the old system table\n");
				_systable_destroy(st->new);
			}
			st->new = _systable_create(st->current->savefile_path);
		}
	}

	la_proto_node *node = la_proto_node_new();
	node->data = result;
	node->td = &proto_DEF_systable_decoding_result;
	return node;
}

/****************************************
 * Private methods
 ****************************************/

#define validation_success() do { \
	st->err.code = ST_ERR_OK; \
	st->err.type = SYSTABLE_ERR_NONE; \
	return true; } while(0)
#define validation_error(error) do { \
	st->err.code = (error); \
	st->err.type = SYSTABLE_ERR_VALIDATE; \
	return false; } while(0)
#define FAIL_IF(cond) do { if(cond) { return false; } } while(0)


#define SYSTABLE_GS_DATA_MIN_LEN 8  // from GS ID to Master Slot Offset, excl. frequencies

struct systable_gs_decoding_result {
	struct systable_gs_data *data;
	int32_t consumed_len;
	bool err;
};

static bool systable_validate_version(struct _systable *st);
static bool systable_validate_stations(struct _systable *st);
static bool systable_validate_station(struct _systable *st, config_setting_t const *station);
static bool systable_validate_station_id(struct _systable *st, config_setting_t const *station);
static bool systable_validate_station_name(struct _systable *st, config_setting_t const *station);
static bool systable_validate_station_coordinates(struct _systable *st, config_setting_t const *station);
static bool systable_validate_frequencies(struct _systable *st, config_setting_t const *station);
static bool systable_add_station_cache_entry(struct _systable *st, config_setting_t const *station);
static struct systable_gs_decoding_result systable_decode_gs(uint8_t *buf, uint32_t len);
static uint32_t systable_decode_frequency(uint8_t const buf[3]);
static void systable_gs_data_format_text(la_vstring *vstr, int32_t indent, struct systable_gs_data const *data);
static bool systable_generate_station_config(struct systable_gs_data const *gs_data, config_setting_t *s);
static bool systable_station_locations_match(config_setting_t const *s1, config_setting_t const *s2);

static struct _systable *_systable_create(char const *savefile) {
	NEW(struct _systable, _st);
	if(savefile != NULL) {
		_st->savefile_path = strdup(savefile);
	}
	return _st;
}

static bool systable_validate(struct _systable *st) {
	ASSERT(st);

	bool result = true;
	FAIL_IF(!systable_validate_version(st));
	FAIL_IF(!systable_validate_stations(st));
	return result;
}

static bool systable_validate_version(struct _systable *st) {
	ASSERT(st);

	int32_t version = 0;
	if(config_lookup_int(&st->cfg, "version", &version) == CONFIG_FALSE) {
		validation_error(ST_ERR_VERSION_MISSING);
	}
	if(version < 0 || version > SYSTABLE_VERSION_MAX) {
		validation_error(ST_ERR_VERSION_OUT_OF_RANGE);
	}
	validation_success();
}

static bool systable_validate_stations(struct _systable *st) {
	ASSERT(st);

	config_setting_t *stations = config_lookup(&st->cfg, "stations");
	if(stations == NULL || !config_setting_is_list(stations)) {
		validation_error(ST_ERR_STATIONS_MISSING);
	}
	config_setting_t *station = NULL;
	uint32_t idx = 0;
	while((station = config_setting_get_elem(stations, idx)) != NULL) {
		if(systable_validate_station(st, station) == false) {
			return false;
		}
		idx++;
	}
	validation_success();
}

static bool systable_validate_station(struct _systable *st, config_setting_t const *station) {
	ASSERT(st);
	ASSERT(station);

	if(!config_setting_is_group(station)) {
		validation_error(ST_ERR_STATION_WRONG_TYPE);
	}
	bool result = true;
	FAIL_IF(!systable_validate_station_id(st, station));
	FAIL_IF(!systable_validate_station_name(st, station));
	FAIL_IF(!systable_validate_station_coordinates(st, station));
	FAIL_IF(!systable_validate_frequencies(st, station));
	return result;
}

static bool systable_validate_station_id(struct _systable *st, config_setting_t const *station) {
	ASSERT(st);
	ASSERT(station);

	int32_t id = 0;
	if(config_setting_lookup_int(station, "id", &id) == CONFIG_FALSE) {
		validation_error(ST_ERR_STATION_ID_MISSING);
	}
	if(id < 0 || id > STATION_ID_MAX) {
		validation_error(ST_ERR_STATION_ID_OUT_OF_RANGE);
	}
	validation_success();
}

static bool systable_validate_frequencies(struct _systable *st, config_setting_t const *station) {
	ASSERT(st);
	ASSERT(station);

	config_setting_t *frequencies = config_setting_get_member(station, "frequencies");
	if(frequencies == NULL || !config_setting_is_list(frequencies)) {
		validation_error(ST_ERR_FREQUENCIES_MISSING);
	}
	config_setting_t *frequency = NULL;
	uint32_t idx = 0;
	while((frequency = config_setting_get_elem(frequencies, idx)) != NULL) {
		if(config_setting_is_number(frequency) == false) {
			validation_error(ST_ERR_FREQUENCY_WRONG_TYPE);
		}
		idx++;
	}
	validation_success();
}

static bool systable_validate_station_name(struct _systable *st, config_setting_t const *station) {
	ASSERT(st);
	ASSERT(station);

	config_setting_t *name = config_setting_get_member(station, "name");
	if(name == NULL) {
		// this setting is optional
		validation_success();
	}
	if(config_setting_type(name) != CONFIG_TYPE_STRING) {
		validation_error(ST_ERR_STATION_NAME_WRONG_TYPE);
	}
	validation_success();
}

static bool systable_validate_station_coordinates(struct _systable *st, config_setting_t const *station) {
	ASSERT(st);
	ASSERT(station);

	config_setting_t *lat = config_setting_get_member(station, "lat");
	config_setting_t *lon = config_setting_get_member(station, "lon");
	// Coordinates are optional, but either both or neither coordinate shall be present
	if(lat == NULL && lon == NULL) {
		validation_success();
	} else if(lat == NULL || lon == NULL) {
		validation_error(ST_ERR_STATION_COORDINATE_MISSING);
	}
	if(config_setting_type(lat) != CONFIG_TYPE_FLOAT || config_setting_type(lon) != CONFIG_TYPE_FLOAT) {
		validation_error(ST_ERR_STATION_COORDINATE_WRONG_TYPE);
	}
	validation_success();
}

static bool systable_populate_stations_cache(struct _systable *st) {
	ASSERT(st);

	config_setting_t *stations = config_lookup(&st->cfg, "stations");
	if(stations == NULL) {
		validation_error(ST_ERR_STATIONS_MISSING);
	}
	config_setting_t *station = NULL;
	uint32_t idx = 0;
	while((station = config_setting_get_elem(stations, idx)) != NULL) {
		if(systable_add_station_cache_entry(st, station) == false) {
			return false;
		}
		idx++;
	}
	validation_success();
}

static bool systable_add_station_cache_entry(struct _systable *st, config_setting_t const *station) {
	ASSERT(st);
	ASSERT(station);

	int32_t id = 0;
	// error checking has been done in systable_validate_station()
	config_setting_lookup_int(station, "id", &id);
	if(st->stations[id] != NULL) {
		validation_error(ST_ERR_STATION_ID_DUPLICATE);
	}
	st->stations[id] = station;
	validation_success();
}

static struct systable_pdu_set *pdu_set_create(uint8_t len) {
	ASSERT(len > 0);
	NEW(struct systable_pdu_set, ps);
	ps->pdus = XCALLOC(len, sizeof(struct octet_string *));
	ps->len = len;
	return ps;
}

static void pdu_set_destroy(struct systable_pdu_set *ps) {
	if(ps == NULL) {
		return;
	}
	for(uint8_t i = 0; i < ps->len; i++) {
		octet_string_destroy(ps->pdus[i]);
	}
	XFREE(ps->pdus);
	XFREE(ps);
}

static struct systable_decoding_result *systable_decode(uint8_t *buf, uint32_t len) {
	ASSERT(buf);
	ASSERT(len > 0);

	NEW(struct systable_decoding_result, result);
	while(len >= SYSTABLE_GS_DATA_MIN_LEN) {
		struct systable_gs_decoding_result gs = systable_decode_gs(buf, len);
		if(!gs.err) {
			buf += gs.consumed_len;
			len -= gs.consumed_len;
			result->gs_list = la_list_append(result->gs_list, gs.data);
		} else {
			goto fail;
		}
	}
	debug_print(D_MISC, "Decoding successful, %u octets left\n", len);
	goto end;
fail:
	result->err = true;
end:
	return result;
}

static struct systable_gs_decoding_result systable_decode_gs(uint8_t *buf, uint32_t len) {
#define FREQ_FIELD_LEN 3
#define SLOT_FIELD_LEN 1
	ASSERT(buf);
	ASSERT(len >= SYSTABLE_GS_DATA_MIN_LEN);

	bool err = true;        // Fail-safe default
	NEW(struct systable_gs_data, result);

	result->gs_id = buf[0] & 0x7F;
	result->utc_sync = (buf[0] & 0x80) != 0;

	uint32_t coord = buf[1] | buf[2] << 8 | (buf[3] & 0xF) << 16;
	result->gs_location.lat = parse_coordinate(coord);
	coord = buf[3] >> 4 | buf[4] << 4 | buf[5] << 12;
	result->gs_location.lon = parse_coordinate(coord);

	result->spdu_version = buf[6] & 7;
	result->freq_cnt = (buf[6] >> 3) & 0x1F;
	uint32_t consumed_len = SYSTABLE_GS_DATA_MIN_LEN - 1;
	if(result->freq_cnt > GS_MAX_FREQ_CNT) {
		debug_print(D_PROTO, "GS %d: too many frequencies (%d)\n",
				result->gs_id, result->freq_cnt);
		goto fail;
	}
	for(uint32_t f = 0; f < result->freq_cnt; f++) {
		uint32_t pos = SYSTABLE_GS_DATA_MIN_LEN - 1 + f * (FREQ_FIELD_LEN + SLOT_FIELD_LEN);
		if(pos + FREQ_FIELD_LEN + SLOT_FIELD_LEN <= len) {
			result->frequencies[f] = systable_decode_frequency(buf + pos);
			consumed_len += FREQ_FIELD_LEN;
			result->master_frame_slots[f] = buf[pos + FREQ_FIELD_LEN] & 0xF;
			consumed_len += SLOT_FIELD_LEN;
		} else {
			debug_print(D_PROTO, "End of buffer reached while decoding frequency %u "
					"for GS %hhu\n", f, result->gs_id);
			goto fail;
		}
	}
	debug_print(D_PROTO, "gs_id: %hhu freq_cnt: %u octets_left: %u\n",
			result->gs_id, result->freq_cnt, len - consumed_len);
	err = false;
	goto end;
fail:
	err = true;
	XFREE(result);
	result = NULL;
end:
	return (struct systable_gs_decoding_result){
		.data = result,
		.consumed_len = consumed_len,
		.err = err
	};
}

static uint32_t systable_decode_frequency(uint8_t const buf[3]) {
	ASSERT(buf);

	return
		1e2 * (buf[0] & 0xF) +
		1e3 * ((buf[0] >> 4) & 0xF) +
		1e4 * (buf[1] & 0xF) +
		1e5 * ((buf[1] >> 4) & 0xF) +
		1e6 * (buf[2] & 0xF) +
		1e7 * ((buf[2] >> 4) & 0xF);
}

void systable_decoding_result_format_text(la_vstring *vstr, void const *data, int32_t indent) {
	ASSERT(vstr);
	ASSERT(data);
	ASSERT(indent >= 0);

	struct systable_decoding_result const *result = data;
	if(result->err) {
		LA_ISPRINTF(vstr, indent, "-- Unparseable System Table message\n");
		return;
	}
	LA_ISPRINTF(vstr, indent, "System Table (complete):\n");
	indent++;
	LA_ISPRINTF(vstr, indent, "Version: %hu\n", result->version);
	for(la_list *l = result->gs_list; l != NULL; l = l->next) {
		systable_gs_data_format_text(vstr, indent, l->data);
	}
}

static void systable_gs_data_format_text(la_vstring *vstr, int32_t indent, struct systable_gs_data const *data) {
	ASSERT(vstr);
	ASSERT(indent >= 0);
	ASSERT(data);

	LA_ISPRINTF(vstr, indent, "GS ID: %hhu\n", data->gs_id);
	indent++;
	LA_ISPRINTF(vstr, indent, "UTC sync: %d\n", data->utc_sync);
	LA_ISPRINTF(vstr, indent, "Location:\n");
	LA_ISPRINTF(vstr, indent+1, "Lat: %.7f\n", data->gs_location.lat);
	LA_ISPRINTF(vstr, indent+1, "Lon: %.7f\n", data->gs_location.lon);
	LA_ISPRINTF(vstr, indent, "Squitter version: %hhu\n", data->spdu_version);
	LA_ISPRINTF(vstr, indent, "Frequencies & master frame slots:\n");
	indent++;
	for(uint32_t f = 0; f < data->freq_cnt; f++) {
		LA_ISPRINTF(vstr, indent, "%8u (slot %2hhu)\n", data->frequencies[f],
				data->master_frame_slots[f]);
	}
}

static void systable_decoding_result_destroy(void *data) {
	if(data == NULL) {
		return;
	}
	struct systable_decoding_result *result = data;
	la_list_free(result->gs_list);
	XFREE(result);
}

la_type_descriptor proto_DEF_systable_decoding_result = {
	.format_text = systable_decoding_result_format_text,
	.destroy = systable_decoding_result_destroy
};

static bool systable_is_newer(int32_t v_old, int32_t v_new) {
	// Negative version indicates an error, so non-negative is always preferred.
	if(v_old < 0 && v_new >= 0) {
		return true;
	} else if(v_new < 0 && v_old >= 0) {
		return false;
	} else if(v_new == v_old) {
		return false;
	}
	return v_new > v_old ||
		// If v_new is smaller than v_old but the distance between v_old and v_new
		// modulo version space size is less than half of that size, then
		// assume that the version has wrapped and v_new is newer than v_old.
		v_new + SYSTABLE_VERSION_MAX - v_old < (SYSTABLE_VERSION_MAX + 1) >> 1;
}

static bool systable_generate_config(config_t *cfg, struct systable_decoding_result *result) {
	ASSERT(result);
	ASSERT(!result->err);
	ASSERT(cfg);

	config_setting_t *root = config_root_setting(cfg);
	FAIL_IF(!root);

	config_setting_t *s = config_setting_add(root, "version", CONFIG_TYPE_INT);
	FAIL_IF(!s);
	FAIL_IF(config_setting_set_int(s, result->version) != CONFIG_TRUE);

	config_setting_t *stations = config_setting_add(root, "stations", CONFIG_TYPE_LIST);
	FAIL_IF(!stations);
	for(la_list *l = result->gs_list; l != NULL; l = l->next) {
		FAIL_IF(systable_generate_station_config(l->data, stations) == false );
	}
	return true;

}

static bool systable_generate_station_config(struct systable_gs_data const *gs_data, config_setting_t *gs_list) {
	ASSERT(gs_data);
	ASSERT(gs_list);

	config_setting_t *gs = config_setting_add(gs_list, NULL, CONFIG_TYPE_GROUP);
	FAIL_IF(!gs);
	config_setting_t *s = config_setting_add(gs, "id", CONFIG_TYPE_INT);
	FAIL_IF(!s);
	FAIL_IF(config_setting_set_int(s, gs_data->gs_id) == CONFIG_FALSE);

	s = config_setting_add(gs, "lat", CONFIG_TYPE_FLOAT);
	FAIL_IF(!s);
	FAIL_IF(config_setting_set_float(s, gs_data->gs_location.lat) == CONFIG_FALSE);
	s = config_setting_add(gs, "lon", CONFIG_TYPE_FLOAT);
	FAIL_IF(!s);
	FAIL_IF(config_setting_set_float(s, gs_data->gs_location.lon) == CONFIG_FALSE);

	config_setting_t *freqs = config_setting_add(gs, "frequencies", CONFIG_TYPE_LIST);
	FAIL_IF(!freqs);
	for(int32_t i = 0; i < gs_data->freq_cnt; i++) {
		config_setting_t *f = config_setting_set_float_elem(freqs, -1, gs_data->frequencies[i] / 1000.0);
		FAIL_IF(!f);
	}
	return true;
}

// Copies station names from the old station list to the new config.
// The name gets copied from an existing station with the same ID provided that
// its location did not change.
static void systable_copy_station_names(config_t *dst, config_setting_t const **stations) {
	ASSERT(dst);
	ASSERT(stations);

	config_setting_t *stations_dst = config_lookup(dst, "stations");
	if(stations_dst == NULL || !config_setting_is_list(stations_dst)) {
		return;
	}
	config_setting_t *station = NULL, *n = NULL;
	uint32_t idx = 0;
	int32_t id = 0;
	char const *name = NULL;
	while((station = config_setting_get_elem(stations_dst, idx)) != NULL) {
		if(config_setting_lookup_int(station, "id", &id) == CONFIG_TRUE &&
				stations[id] != NULL &&
				systable_station_locations_match(station, stations[id]) &&
				(n = config_setting_get_member(stations[id], "name")) != NULL &&
				(name = config_setting_get_string(n)) != NULL) {
			n = config_setting_add(station, "name", CONFIG_TYPE_STRING);
			config_setting_set_string(n, name);
		}
		idx++;
	}
}

#define get_coord(setting_var, setting, name, var) \
	setting_var = config_setting_get_member(setting, name); \
	if(!setting_var || config_setting_type(setting_var) != CONFIG_TYPE_FLOAT) { \
		return false; \
	} \
	double var = config_setting_get_float(setting_var)

static bool systable_station_locations_match(config_setting_t const *s1, config_setting_t const *s2) {
	ASSERT(s1);
	ASSERT(s2);

	config_setting_t *s = NULL;
	get_coord(s, s1, "lat", lat1);
	get_coord(s, s1, "lon", lon1);
	get_coord(s, s2, "lat", lat2);
	get_coord(s, s2, "lon", lon2);

	// A naive check if the coordinates point at (almost) the same location.
	// It's good enough for the current set of ground stations - neither of them
	// is located exactly on the 0th meridian or the equator, so we may safely
	// assume that they won't suddenly jump over to the other hemisphere.
	if(fabs(lat1-lat2) < 1.0 && fabs(lon1-lon2) < 1.0) {
		return true;
	}
	return false;
}

static bool systable_save_config(struct _systable *st) {
	ASSERT(st);
	if(st->savefile_path == NULL) {      // Save file path was not specified - this is not an error
		goto success;
	}
	if(config_write_file(&st->cfg, st->savefile_path) == CONFIG_TRUE) {
		goto success;
	} else {
		st->err.code = ST_ERR_LIBCONFIG;
		st->err.cfg = &st->cfg;
		return false;
	}
success:
	st->err.code = ST_ERR_OK;
	return true;
}
