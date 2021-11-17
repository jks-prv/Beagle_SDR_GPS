/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdbool.h>
#include <math.h>                       // round
#include <time.h>                       // strftime, gmtime, localtime
#include <sys/time.h>                   // struct timeval
#include <libacars/libacars.h>          // la_proto_node
#include <libacars/vstring.h>           // la_vstring
#include "fmtr-text.h"
#include "output-common.h"              // fmtr_descriptor_t
#include "util.h"                       // struct octet_string, Config, EOL
#include "pdu.h"                        // struct hfdl_pdu_metadata
#include "bits.h"

static bool fmtr_text_supports_data_type(fmtr_input_type_t type) {
	return(type == FMTR_INTYPE_DECODED_FRAME);
}

static la_vstring *format_timestamp(struct timeval tv) {
	int32_t millis = 0;
	if(hfdl->Config.milliseconds == true) {
		millis = round(tv.tv_usec / 1000.0);
		if(millis > 999) {
		    millis -= 1000;
		    tv.tv_sec++;
		}
	}
	struct tm *tmstruct = (hfdl->Config.utc == true ? gmtime(&tv.tv_sec) : localtime(&tv.tv_sec));

	char tbuf[30], tzbuf[8];
	strftime(tbuf, sizeof(tbuf), "%F %T", tmstruct);
	strftime(tzbuf, sizeof(tzbuf), "%Z", tmstruct);

	la_vstring *vstr = la_vstring_new();
	la_vstring_append_sprintf(vstr, "%s", tbuf);
	if(hfdl->Config.milliseconds == true) {
		la_vstring_append_sprintf(vstr, ".%03d", millis);
	}
	la_vstring_append_sprintf(vstr, " %s", tzbuf);
	return vstr;
}

static struct octet_string *fmtr_text_format_decoded_msg(struct metadata *metadata, la_proto_node *root) {
	ASSERT(metadata != NULL);
	ASSERT(root != NULL);

	struct hfdl_pdu_metadata *hm = container_of(metadata, struct hfdl_pdu_metadata,
			metadata);
	la_vstring *timestamp = format_timestamp(metadata->rx_timestamp);
	la_vstring *vstr = la_vstring_new();

	la_vstring_append_sprintf(vstr, YELLOW "[%s] [%.1f kHz] [%.1f Hz] [%.1f/%.1f dBFS] [%.1f dB] [%d bps] [%c]" NORM,
			timestamp->str,
			//(float)hm->freq / 1000.f,
			hfdl->freq_kHz,
			hm->freq_err_hz,
			hm->rssi,
			hm->noise_floor,
			hm->rssi - hm->noise_floor,
			hm->bit_rate,
			hm->slot
			);
	la_vstring_destroy(timestamp, true);

	EOL(vstr);

	vstr = la_proto_tree_format_text(vstr, root);
	EOL(vstr);
	struct octet_string *ret = octet_string_new(vstr->str, vstr->len);
	la_vstring_destroy(vstr, false);
	return ret;
}

fmtr_descriptor_t fmtr_DEF_text = {
	.name = "text",
	.description = "Human readable text",
	.format_decoded_msg = fmtr_text_format_decoded_msg,
	.format_raw_msg = NULL,
	.supports_data_type = fmtr_text_supports_data_type,
	.output_format = OFMT_TEXT,
};
