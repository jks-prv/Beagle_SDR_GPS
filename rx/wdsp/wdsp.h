#pragma once

void wdsp_SAM_demod_init();
void wdsp_SAM_reset(int rx_chan);
f32_t wdsp_SAM_carrier(int rx_chan);
void wdsp_SAM_demod(int rx_chan, int mode, int ns_out, TYPECPX *in, TYPEMONO16 *out);
