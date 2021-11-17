/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "pthr.h"
#include "config.h"         // DATADUMPS
#include "systable.h"
#include "ac_cache.h"
#include "ac_data.h"
#include "kiwi-hfdl.h"
#include <glib.h>           // GAsyncQueue, g_async_queue_*

enum ac_data_details {
	AC_DETAILS_NORMAL = 0,
	AC_DETAILS_VERBOSE = 1
};

// global config
struct dumphfdl_config {
#ifdef DEBUG
	uint32_t debug_filter;
#endif
	char *station_id;
	int32_t output_queue_hwm;
	enum ac_data_details ac_data_details;
	bool utc;
	bool milliseconds;
	bool output_raw_frames;
	bool output_mpdus;
	bool output_corrupted_pdus;
	bool ac_data_available;
#ifdef DATADUMPS
	bool datadumps;
#endif
};

#define STATION_ID_LEN_MAX 255

// version.c
#ifdef HFDL_KIWI
#else
    extern char const * const DUMPHFDL_VERSION;
#endif

#define Systable_lock() do { pthr_mutex_lock(&hfdl->Systable_lock); } while(0)
#define Systable_unlock() do { pthr_mutex_unlock(&hfdl->Systable_lock); } while(0)

#define AC_cache_lock() do { pthr_mutex_lock(&hfdl->AC_cache_lock); } while(0)
#define AC_cache_unlock() do { pthr_mutex_unlock(&hfdl->AC_cache_lock); } while(0)

void instance_init(uint32_t rx_chan, int outputBlockSize);


// reentrancy

typedef struct {
    int32_t rx_chan;
	int outputBlockSize;
    double freq_kHz;
    
    struct dumphfdl_config Config;
    
    // Global system table
    // Kiwi: for now one Systable is maintained per kiwi rx channel
    systable *Systable;
    pthr_mutex_t Systable_lock;

    // HFDL ID -> ICAO mapping table
    ac_cache *AC_cache;
    pthr_mutex_t AC_cache_lock;

    // basestation.sqb cache
    ac_data *AC_data;
    
    GAsyncQueue *pdu_decoder_queue;
    bool pdu_decoder_thread_active;

    int32_t do_exit;
} hfdl_t;

#define hfdl hfdl_f()
extern hfdl_t hfdl_d[MAX_RX_CHANS];
hfdl_t *hfdl_f();
