/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "pthr.h"                   // pthr_mutex_t
#include "globals.h"                // dumphfdl_config
#include "systable.h"               // systable
#include "ac_cache.h"               // ac_cache
#include "ac_data.h"                // ac_data
#include "kiwi-hfdl.h"

hfdl_t hfdl_d[MAX_RX_CHANS];

hfdl_t *hfdl_f()
{
    return (hfdl_t *) FROM_VOID_PARAM(TaskGetUserParam());
}

void instance_init(uint32_t rx_chan, int outputBlockSize)
{
	TaskSetUserParam(TO_VOID_PARAM(&hfdl_d[rx_chan]));
	//printf("rx_chan=%d hfdl=%p/%p\n", rx_chan, &hfdl_d[rx_chan], hfdl);
    memset(hfdl, 0, sizeof(hfdl_t));
    hfdl->rx_chan = rx_chan;
    hfdl->outputBlockSize = outputBlockSize;
}

void dumphfdl_set_freq(int rx_chan, double freq_kHz)
{
    hfdl_d[rx_chan].freq_kHz = freq_kHz;
}
