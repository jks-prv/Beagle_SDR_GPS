#pragma once

void wdsp_SAM_demod_init();
void wdsp_SAM_reset(int rx_chan);
f32_t wdsp_SAM_carrier(int rx_chan);
void wdsp_SAM_demod(int rx_chan, int mode, int ns_out, TYPECPX *in, TYPEMONO16 *out);

void wdsp_ANR_init(int rx_chan, nr_type_e nr_type, TYPEREAL nr_param[NOISE_PARAMS]);
void wdsp_ANR_filter(int rx_chan, nr_type_e nr_type, int ns_out, TYPEMONO16 *in, TYPEMONO16 *out);
