// Copyright (c) 2023 John Seamons, ZL4VO/KF6VO

#pragma once

#include "types.h"
#include "ft8/constants.h"

//#define PR_TESTING
#ifdef PR_TESTING
    // report at: pskreporter.info/cgi-bin/psk-analysis.pl
    #define PR_UPLOAD_PORT      14739   // report.pskreporter.info (test)
    #define PR_UPLOAD_MINUTES   1
    #define PR_INFO_DESC_RPT    1
    #define PR_LIMITER          3
    #define pr_printf(fmt, ...) printf(fmt, ## __VA_ARGS__)
#else
    #define PR_UPLOAD_PORT      4739    // report.pskreporter.info (LIVE)
    #define PR_UPLOAD_MINUTES   5
    #define PR_INFO_DESC_RPT    3
    #define PR_LIMITER          0
    #define pr_printf(fmt, ...)
#endif

#define PR_USE_CALLSIGN_HASHTABLE

C_LINKAGE(void PSKReporter_init());
C_LINKAGE(int PSKReporter_setup(int rx_chan));
C_LINKAGE(int PSKReporter_distance(const char *grid));
C_LINKAGE(int PSKReporter_spot(int rx_chan, const char *call, u4_t passband_freq, s1_t snr, ftx_protocol_t protocol, const char *grid, u4_t slot_time, u4_t slot));
C_LINKAGE(u4_t PSKReporter_num_uploads(int rx_chan));
C_LINKAGE(void PSKReporter_reset(int rx_chan));
