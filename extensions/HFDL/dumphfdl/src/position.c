/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <math.h>                       // fabsf
#include <time.h>
#include <errno.h>
#include <libacars/libacars.h>          // la_proto_node
#include "position.h"
#include "lpdu.h"                       // lpdu_position_info_extract
#include "util.h"

/******************************
 * Forward declarations
 ******************************/

bool location_is_valid(struct location *loc);
void fixup_timestamp(struct timestamp *ts);

/******************************
 * Public methods
 ******************************/

struct position_info *position_info_create(void) {
	NEW(struct position_info, pos_info);
	return pos_info;
}

struct position_info *position_info_extract(la_proto_node *tree) {
	ASSERT(tree);

	struct position_info *pos_info = lpdu_position_info_extract(tree);
	if(pos_info != NULL && location_is_valid(&pos_info->position.location)) {
		fixup_timestamp(&pos_info->position.timestamp);
	} else {
		position_info_destroy(pos_info);
		pos_info = NULL;
	}
	return pos_info;
}

void position_info_destroy(struct position_info *pos_info) {
	XFREE(pos_info);
}

/****************************************
 * Private variables and methods
 ****************************************/

static void date_yesterday(struct tm *now, struct tm *result);

bool location_is_valid(struct location *loc) {
	ASSERT(loc);

	bool result = fabs(loc->lat) <= 90.0f;
	result &= fabs(loc->lon) <= 180.0f;
	debug_print(D_MISC, "location (%f, %f) is %svalid \n",
			loc->lat, loc->lon, result ? "" : "in");
	return result;
}

// Fills in missing struct tm fields.
// Three variants are supported:
// - hours, mins, secs present, day, month, year missing (as in Logon Request/Resume LPDUs)
// - hours, mins present, secs and date missing (as in CPDLC position reports)
// - mins, secs present, hours and date missing (as in ADS-C position reports)
// Missing fields are filled in to get the closest matching timestamp in the past.
void fixup_timestamp(struct timestamp *ts) {
	ASSERT(ts);

	struct tm tm_pos = ts->tm;
	time_t now = time(NULL);
	struct tm tm_now = {0};
	gmtime_r(&now, &tm_now);

	if(!ts->tm_sec_present) {
		tm_pos.tm_sec = 0;
		ts->tm_sec_present = true;
	}
	ASSERT(ts->tm_min_present);     // Minutes are always present in all message types
	if(!ts->tm_hour_present) {
		// Assume current hour and go back 1 hour if the resulting wallclock time
		// is later than now.
		if(tm_pos.tm_min < tm_now.tm_min ||
				(tm_pos.tm_min == tm_now.tm_min && tm_pos.tm_sec <= tm_now.tm_sec)) {
			tm_pos.tm_hour = tm_now.tm_hour;
		} else {
			tm_pos.tm_hour = tm_now.tm_hour > 0 ? tm_now.tm_hour - 1 : 23;
		}
		debug_print(D_MISC, "fixup_hour: tm_pos: :%02d:%02d now: %02d:%02d:%02d result: %02d\n",
				tm_pos.tm_min, tm_pos.tm_sec,
				tm_now.tm_hour, tm_now.tm_min, tm_now.tm_sec,
				tm_pos.tm_hour);
		ts->tm_hour_present = true;
	}
	if(!ts->tm_date_present) {
		tm_pos.tm_mday = tm_now.tm_mday;
		tm_pos.tm_mon = tm_now.tm_mon;
		tm_pos.tm_year = tm_now.tm_year;
		ts->tm_date_present = true;
	}
	// Verify if the result is in the past; if not, go back 1 day
	time_t t_pos = timegm(&tm_pos);
	ASSERT(t_pos != -1);
	if(t_pos > now) {
		struct tm tm_yesterday = {0};
		date_yesterday(&tm_now, &tm_yesterday);
		tm_pos.tm_mday = tm_yesterday.tm_mday;
		tm_pos.tm_mon = tm_yesterday.tm_mon;
		tm_pos.tm_year = tm_yesterday.tm_year;
		t_pos = timegm(&tm_pos);
		ASSERT(t_pos != -1);
		debug_print(D_MISC, "fixup_date: result: %04d-%02d-%02d \n",
				tm_pos.tm_year + 1900, tm_pos.tm_mon + 1, tm_pos.tm_mday);
	}
	ts->tm = tm_pos;
	ts->t = t_pos;
	debug_print(D_MISC, "result: %04d-%02d-%02d %02d:%02d:%02d t: %ld\n",
			tm_pos.tm_year + 1900, tm_pos.tm_mon + 1, tm_pos.tm_mday,
			tm_pos.tm_hour, tm_pos.tm_min, tm_pos.tm_sec, t_pos);
}

// Computes the date of the previous day
// Only tm_yday, tm_mon and tm_mday fields are valid in the result.
static void date_yesterday(struct tm *now, struct tm *result) {
	ASSERT(now);
	ASSERT(result);

	time_t day_seconds_elapsed = now->tm_hour * 60 * 60 + now->tm_min * 60 + now->tm_sec;
	// Now go back 10 seconds into yesterday and compute the date
	time_t t = mktime(now) - day_seconds_elapsed - 10;
	gmtime_r(&t, result);
	debug_print(D_MISC, "date_now: %04d-%02d-%02d result: %04d-%02d-%02d\n",
			now->tm_year + 1900, now->tm_mon + 1, now->tm_mday,
			result->tm_year + 1900, result->tm_mon + 1, result->tm_mday);
}
