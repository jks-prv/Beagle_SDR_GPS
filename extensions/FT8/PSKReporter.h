// Copyright (c) 2023 John Seamons, ZL/KF6VO

#pragma once

#include "types.h"
#include "ft8/constants.h"

#define PR_USE_CALLSIGN_HASHTABLE

C_LINKAGE(void PSKReporter_init());
C_LINKAGE(int PSKReporter_setup(int rx_chan));
C_LINKAGE(int PSKReporter_spot(int rx_chan, const char *call, u4_t passband_freq, ftx_protocol_t protocol, const char *grid, u4_t slot_time));
