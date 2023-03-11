#pragma once

#include "rx_noise.h"

#define CHAN_NULL_WHICH     0x0003
#define     CHAN_NULL_NONE  0
#define     CHAN_NULL_LSB   1
#define     CHAN_NULL_USB   2
#define FADE_LEVELER        0x0004
#define DC_BLOCK            0x0008

void wdsp_SAM_demod_init();

const int PLL_RESET = -1, PLL_DX = 0, PLL_MED = 1, PLL_FAST = 2;
void wdsp_SAM_PLL(int rx_chan, int type);

f32_t wdsp_SAM_carrier(int rx_chan);
bool wdsp_SAM_demod(int rx_chan, int mode, u4_t SAM_mparam, int ns_out, TYPECPX *in, TYPEMONO16 *out);

void wdsp_ANR_init(int rx_chan, nr_type_e nr_type, TYPEREAL nr_param[NOISE_PARAMS]);
void wdsp_ANR_filter(int rx_chan, nr_type_e nr_type, int ns_out, TYPEMONO16 *in, TYPEMONO16 *out);
