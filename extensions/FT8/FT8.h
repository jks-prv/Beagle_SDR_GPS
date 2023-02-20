
#pragma once

#include "types.h"
#include "kiwi.h"
#include "config.h"

// server => ft8_lib
C_LINKAGE(void decode_ft8_init(int rx_chan, int proto));
C_LINKAGE(void decode_ft8_free(int rx_chan));
C_LINKAGE(void decode_ft8_setup(int rx_chan));
C_LINKAGE(void decode_ft8_samples(int rx_chan, TYPEMONO16 *samps, int nsamps, bool *start_test));
