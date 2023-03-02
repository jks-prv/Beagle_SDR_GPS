// Copyright (c) 2023 John Seamons, ZL/KF6VO

#pragma once

#include "types.h"
#include "ft8/constants.h"

//#define PR_TESTING
#ifdef PR_TESTING
    #define PR_UPLOAD_MINUTES   1
    #define PR_INFO_DESC_RPT    1
    #define PR_LIMITER          3
#else
    #define PR_UPLOAD_MINUTES   5
    #define PR_INFO_DESC_RPT    3
    #define PR_LIMITER          0
#endif

#define PR_USE_CALLSIGN_HASHTABLE

C_LINKAGE(void PSKReporter_init());
C_LINKAGE(int PSKReporter_setup(int rx_chan));
C_LINKAGE(int PSKReporter_spot(int rx_chan, const char *call, u4_t passband_freq, ftx_protocol_t protocol, const char *grid, u4_t slot_time));
