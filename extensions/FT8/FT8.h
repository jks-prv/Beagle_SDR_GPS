// Copyright (c) 2023 John Seamons, ZL/KF6VO

#pragma once

#include "types.h"

typedef struct {
    float SNR_adj;
    float dT_adj;

    s2_t *s2p_start, *s2p_end;
    int tsamps;
} ft8_conf_t;

extern ft8_conf_t ft8_conf;

// server => ft8_lib
C_LINKAGE(void decode_ft8_init(int rx_chan, int proto));
C_LINKAGE(void decode_ft8_free(int rx_chan));
C_LINKAGE(void decode_ft8_setup(int rx_chan));
C_LINKAGE(void decode_ft8_protocol(int rx_chan, int proto));
C_LINKAGE(void decode_ft8_samples(int rx_chan, TYPEMONO16 *samps, int nsamps, u1_t *start_test));
