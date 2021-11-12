/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include <stdint.h>
#include <sys/time.h>           // struct timeval
#include <libacars/libacars.h>  // la_msg_dir
#include "hfdl_config.h"             // WITH_STATSD
#include "util.h"               // nop()

#ifdef WITH_STATSD
int32_t statsd_initialize(char *statsd_addr);
void statsd_initialize_counters_per_channel(int32_t freq);
void statsd_initialize_counters_per_msgdir();
void statsd_initialize_counter_set(char **counter_set);
// Can't have char const * pointers here, because statsd-c-client
// may potentially modify their contents
void statsd_counter_per_channel_increment(int32_t freq, char *counter);
void statsd_timing_delta_per_channel_send(int32_t freq, char *timer, struct timeval ts);
void statsd_counter_per_msgdir_increment(la_msg_dir msg_dir, char *counter);
void statsd_counter_increment(char *counter);
void statsd_gauge_set(char *gauge, size_t value);

#define statsd_increment_per_channel(freq, counter) statsd_counter_per_channel_increment(freq, counter)
#define statsd_timing_delta_per_channel(freq, timer, start) statsd_timing_delta_per_channel_send(freq, timer, start)
#define statsd_increment_per_msgdir(counter, msgdir) statsd_counter_per_msgdir_increment(counter, msgdir)
#define statsd_increment(counter) statsd_counter_increment(counter)
#define statsd_set(gauge, value) statsd_gauge_set(gauge, value)
#else
#define statsd_increment_per_channel(freq, counter) nop()
#define statsd_timing_delta_per_channel(freq, timer, start) nop()
#define statsd_increment_per_msgdir(counter, msgdir) nop()
#define statsd_increment(counter) nop()
#define statsd_set(gauge, value) nop()
#endif
