// Copyright (c) 2023 John Seamons, ZL4VO/KF6VO

#pragma once

#include "types.h"
#include "config.h"

#define FT8_PASSBAND_LO     100
#define FT8_PASSBAND_HI     3100

typedef struct {
    u64_t freq_offset_Hz;
    bool arun_restart_offset;
    bool arun_suspend_restart_victims;
    
    float SNR_adj;
    float dT_adj;

    int test;
    s2_t *s2p_start, *s2p_end;
    int tsamps;
    
	bool GPS_update_grid;
    char rgrid[6+1];

    int syslog;
} ft8_conf_t;

extern ft8_conf_t ft8_conf;

// server => ft8_lib
C_LINKAGE(void decode_ft8_init(int rx_chan, int proto));
C_LINKAGE(void decode_ft8_free(int rx_chan));
C_LINKAGE(void decode_ft8_setup(int rx_chan, int debug));
C_LINKAGE(void decode_ft8_protocol(int rx_chan, u64_t freqHz, int proto));
C_LINKAGE(void decode_ft8_samples(int rx_chan, TYPEMONO16 *samps, int nsamps, int freqHz, u1_t *start_test));

void ft8_update_rgrid(char *rgrid);
bool ft8_update_vars_from_config(bool called_at_init_or_restart);
void ft8_autorun_start(bool initial);
void ft8_autorun_restart();
void ft8_update_spot_count(int rx_chan, u4_t spot_count);
