#pragma once

void nb_Wild_init(int rx_chan, TYPEREAL nb_param[NOISE_PARAMS]);
void nb_Wild_process(int rx_chan, int nsamps, TYPEMONO16 *in, TYPEMONO16 *out);

void nr_spectral_init(int rx_chan, TYPEREAL nr_param[NOISE_PARAMS]);
void nr_spectral_process(int rx_chan, int nsamps, TYPEMONO16 *in, TYPEMONO16 *out);
