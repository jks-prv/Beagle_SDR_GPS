/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdbool.h>
#include <time.h>                       // struct tm, strftime
#include "globals.h"                    // AC_cache
#include "output-common.h"              // fmtr_descriptor_t
#include "position.h"                   // position_info_*
#include "util.h"                       // ASSERT, XCALLOC, XFREE, struct octet_string

#define POSITION_MAX_AGE 300            // seconds

static bool fmtr_basestation_supports_data_type(fmtr_input_type_t type) {
	return(type == FMTR_INTYPE_DECODED_FRAME);
}

static char *format_timestamp(struct tm *tm) {
	ASSERT(tm);

	size_t bufsize = 30;
	char *tbuf = XCALLOC(bufsize, sizeof(char));
	strftime(tbuf, bufsize, "%Y/%m/%d,%T.000", tm);
	return tbuf;
}

static struct octet_string *fmtr_basestation_format_decoded_msg(struct metadata *metadata,
		la_proto_node *root) {
	UNUSED(metadata);
	ASSERT(root != NULL);

	struct position_info *pos_info = position_info_extract(root);
	if(pos_info == NULL) {
	    return NULL;
	}

	struct octet_string *result = NULL;
	time_t now = time(NULL);
	if(pos_info->position.timestamp.t > now) {
		debug_print(D_MISC, "pos_info rejected: timestamp %ld is in the future\n",
				pos_info->position.timestamp.t);
		goto cleanup;
	} else if(pos_info->position.timestamp.t + POSITION_MAX_AGE < now) {
		debug_print(D_MISC, "pos_info (%f, %f) rejected: timestamp %ld too old\n",
				pos_info->position.location.lat,
				pos_info->position.location.lon,
				pos_info->position.timestamp.t);
		goto cleanup;
	}

	char *timestamp = format_timestamp(&pos_info->position.timestamp.tm);
	la_vstring *vstr = la_vstring_new();

	la_vstring_append_sprintf(vstr, "MSG,3,1,1,%06X,1,%s,%s,%s,,,,%f,%f,,,,,,0\n",
			pos_info->aircraft.icao_address,
			timestamp,
			timestamp,
			pos_info->aircraft.flight_id != NULL ? pos_info->aircraft.flight_id : "",
			pos_info->position.location.lat,
			pos_info->position.location.lon
			);

	XFREE(timestamp);

	result = octet_string_new(vstr->str, vstr->len);
	la_vstring_destroy(vstr, false);
cleanup:
	position_info_destroy(pos_info);
	return result;
}

fmtr_descriptor_t fmtr_DEF_basestation = {
	.name = "basestation",
	.description = "Position data in Basestation format (CSV)",
	.format_decoded_msg = fmtr_basestation_format_decoded_msg,
	.format_raw_msg = NULL,
	.supports_data_type = fmtr_basestation_supports_data_type,
	.output_format = OFMT_TEXT,
};
