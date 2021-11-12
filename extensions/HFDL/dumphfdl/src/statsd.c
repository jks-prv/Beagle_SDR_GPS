/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifdef WITH_STATSD
#include <stdio.h>                  // snprintf
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>                 // strtok
#include <sys/time.h>               // gettimeofday, struct timeval
#include <statsd/statsd-client.h>   // statsd_*
#include <libacars/libacars.h>      // la_msg_dir
#include <libacars/vstring.h>       // la_vstring
#include "globals.h"                // Config
#include "util.h"                   // debug_print

#define STATSD_NAMESPACE "dumphfdl"
static statsd_link *statsd = NULL;

static char const *counters_per_channel[] = {
	"demod.preamble.A2_found",
	"demod.preamble.M1_found",
	"demod.preamble.errors.M1_not_found",
	"frame.errors.bad_fcs",
	"frame.errors.too_short",
	"frame.dir.air2gnd",
	"frame.dir.gnd2air",
	"frames.good",
	"frames.processed",
	"lpdu.errors.bad_fcs",
	"lpdu.errors.too_short",
	"lpdus.good",
	"lpdus.processed",
	NULL
};

static char const *counters_per_msgdir[] = {
	"acars.reasm.unknown",
	"acars.reasm.complete",
	// "acars.reasm.in_progress",   // we report final reasm states only
	"acars.reasm.skipped",
	"acars.reasm.duplicate",
	"acars.reasm.out_of_seq",
	"acars.reasm.invalid_args",
	NULL
};

static char const *msg_dir_labels[] = {
	[LA_MSG_DIR_UNKNOWN] = "unknown",
	[LA_MSG_DIR_AIR2GND] = "air2gnd",
	[LA_MSG_DIR_GND2AIR] = "gnd2air"
};

int32_t statsd_initialize(char *statsd_addr) {
	ASSERT(statsd_addr != NULL);
	char *addr;
	char *port;

	if((addr = strtok(statsd_addr, ":")) == NULL) {
		return -1;
	}
	if((port = strtok(NULL, ":")) == NULL) {
		return -1;
	}
	la_vstring *statsd_namespace = la_vstring_new();
	la_vstring_append_sprintf(statsd_namespace, "%s", STATSD_NAMESPACE);
	if(hfdl->Config.station_id != NULL) {
		fprintf(stderr, "Using extended statsd namespace %s.%s\n", STATSD_NAMESPACE, hfdl->Config.station_id);
		la_vstring_append_sprintf(statsd_namespace, ".%s", hfdl->Config.station_id);
	}
	statsd = statsd_init_with_namespace(addr, atoi(port), statsd_namespace->str);
	la_vstring_destroy(statsd_namespace, true);
	if(statsd == NULL) {
		return -2;
	}
	return 0;
}

void statsd_initialize_counters_per_channel(int32_t freq) {
	if(statsd == NULL) {
		return;
	}
	char metric[256];
	for(int32_t n = 0; counters_per_channel[n] != NULL; n++) {
		snprintf(metric, sizeof(metric), "channels.%d.%s", freq, counters_per_channel[n]);
		statsd_count(statsd, metric, 0, 1.0);
	}
}

static void statsd_initialize_counters_for_msg_dir(char const *counters[], la_msg_dir msg_dir) {
	char metric[256];
	for(int32_t n = 0; counters[n] != NULL; n++) {
		snprintf(metric, sizeof(metric), "%s.%s", counters[n], msg_dir_labels[msg_dir]);
		statsd_count(statsd, metric, 0, 1.0);
	}
}

void statsd_initialize_counters_per_msgdir() {
	if(statsd == NULL) {
		return;
	}
	statsd_initialize_counters_for_msg_dir(counters_per_msgdir, LA_MSG_DIR_AIR2GND);
	statsd_initialize_counters_for_msg_dir(counters_per_msgdir, LA_MSG_DIR_GND2AIR);
}

void statsd_initialize_counter_set(char **counter_set) {
	if(statsd == NULL) {
		return;
	}
	for(int32_t n = 0; counter_set[n] != NULL; n++) {
		statsd_count(statsd, counter_set[n], 0, 1.0);
	}
}

void statsd_counter_per_channel_increment(int32_t freq, char *counter) {
	if(statsd == NULL) {
		return;
	}
	char metric[256];
	snprintf(metric, sizeof(metric), "channels.%d.%s", freq, counter);
	statsd_inc(statsd, metric, 1.0);
}

void statsd_counter_per_msgdir_increment(la_msg_dir msg_dir, char *counter) {
	if(statsd == NULL) {
		return;
	}
	char metric[256];
	snprintf(metric, sizeof(metric), "%s.%s", counter, msg_dir_labels[msg_dir]);
	statsd_inc(statsd, metric, 1.0);
}

void statsd_counter_increment(char *counter) {
	if(statsd == NULL) {
		return;
	}
	statsd_inc(statsd, counter, 1.0);
}

void statsd_gauge_set(char *gauge, size_t value) {
	if(statsd == NULL) {
		return;
	}
	statsd_gauge(statsd, gauge, value);
}

void statsd_timing_delta_per_channel_send(int32_t freq, char *timer, struct timeval ts) {
	if(statsd == NULL) {
		return;
	}
	char metric[256];
	struct timeval te;
	uint32_t tdiff;
	gettimeofday(&te, NULL);
	if(te.tv_sec < ts.tv_sec || (te.tv_sec == ts.tv_sec && te.tv_usec < ts.tv_usec)) {
		debug_print(D_STATS, "timediff is negative: ts.tv_sec=%lu ts.tv_usec=%lu te.tv_sec=%lu te.tv_usec=%lu\n",
				ts.tv_sec, ts.tv_usec, te.tv_sec, te.tv_usec);
		return;
	}
	tdiff = ((te.tv_sec - ts.tv_sec) * 1000000UL + te.tv_usec - ts.tv_usec) / 1000;
	debug_print(D_STATS, "tdiff: %u ms\n", tdiff);
	snprintf(metric, sizeof(metric), "%d.%s", freq, timer);
	statsd_timing(statsd, metric, tdiff);
}
#endif
