// Copyright (c) 2016 John Seamons, ZL/KF6VO

#include "ext.h"	// all calls to the extension interface begin with "ext_", e.g. ext_register()

#include "gps.h"
#include "st4285.h"
#include "fmdemod.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>
#include <strings.h>

//#define DEBUG_MSG	true
#define DEBUG_MSG	false

#define AFFT_WIDTH 1024

// rx_chan is the receiver channel number we've been assigned, 0..RX_CHAN
// We need this so the extension can support multiple users, each with their own audio_fft[] data structure.

#define N_CH		2
#define N_HISTORY	2
#define MAX_BINS	200

struct audio_fft_t {
	int rx_chan;
	int run, draw;
	float fft_scale;
	
	struct {
		u1_t bin;
		u1_t fft[AFFT_WIDTH];
	} dsp;
	
	bool ig_reset;
	double ig_time, ig_fft_sec;
	int ig_bins;
	double ig_fi;
	float ig_pwr[MAX_BINS][AFFT_WIDTH];
	int ig_ncma[MAX_BINS];
};

static audio_fft_t audio_fft[RX_CHANS];

#define	FFT		0
#define	CLEAR	1

void audio_fft_data(int rx_chan, int ch, int ratio, int nsamps, TYPECPX *samps)
{
	audio_fft_t *e = &audio_fft[rx_chan];
	int cmd = (e->draw << 1) + (ch & 1);
	int i;
	
	// capture the ratio one time after each integration time change
	if (e->ig_reset) {
		e->ig_fft_sec = 1.0 / (ext_get_sample_rateHz() * ratio / AFFT_WIDTH);
		e->ig_bins = trunc(e->ig_time / e->ig_fft_sec);
		ext_send_msg(e->rx_chan, DEBUG_MSG, "EXT bins=%d", e->ig_bins);
		e->ig_fi = 0;
		printf("itime %.1f fft_ms %.1f bins %d\n", e->ig_time, e->ig_fft_sec*1e3, e->ig_bins);
		memset(&e->ig_pwr, 0, sizeof(e->ig_pwr));
		memset(&e->ig_ncma, 0, sizeof(e->ig_ncma));
		e->ig_reset = false;
	}
	
	double fr = fmod(e->ig_fi, e->ig_time);
	double fb = fr / e->ig_time;
	double fbin = fb * e->ig_bins;
	int bin = trunc(fbin);
	
	e->ig_fi += e->ig_fft_sec;
	
	//printf("bin %d fi %.3f\n", bin, e->ig_fi);
	//assert(bin < MAX_BINS);
	if (bin >= MAX_BINS) return;	// punt for now
	
	assert(nsamps == AFFT_WIDTH);
	float pwr;

	int ncma = e->ig_ncma[bin];
	int meas_bin = 22, meas_px = 300, noise_px = 200;
	double meas_signal = 10000000, noise_pwr;
	
	meas_px = (e->ig_time == 3.6)? (515 + (int) trunc(6 * fb) * 4) : -1;

	for (i=0; i < AFFT_WIDTH; i++) {
		float re = samps[i].re, im = samps[i].im;
		pwr = re*re + im*im;
		
		if (i == meas_px) pwr = meas_signal;
		//if (bin == meas_bin && i == noise_px) noise_pwr = pwr;
		//if (bin >= (meas_bin-4) && bin <= (meas_bin+4) && i == meas_px) pwr = meas_signal;
		//if (bin == 0) pwr = 40000*40000;
		
		#if 0
		e->ig_pwr[bin][i] = (e->ig_pwr[bin][i] * ncma) + pwr;
		e->ig_pwr[bin][i] /= ncma+1;
		#else
		e->ig_pwr[bin][i] += pwr;
		#endif
		
		#if 0
		if (bin == meas_bin && i == meas_px) printf("%d s %e %e n %e %e s/n %e\n", ncma,
			meas_signal, e->ig_pwr[bin][i],
			noise_pwr, e->ig_pwr[bin][noise_px],
			e->ig_pwr[bin][i] - e->ig_pwr[bin][noise_px]);
		#endif
	}
	e->ig_ncma[bin]++;
	
	float fft_scale = e->fft_scale;
	float dB, y;

	//float m_dB[AFFT_WIDTH];
	
	for (i=0; i < AFFT_WIDTH; i++) {
		pwr = e->ig_pwr[bin][i];
		dB = 10.0 * log10f(pwr * fft_scale + (float) 1e-30);
		//m_dB[i] = dB;
		y = dB + SMETER_CALIBRATION;
		if (y >= 0) y = -1;
		if (y < -200.0) y = -200.0;

		int unwrap = (i < AFFT_WIDTH/2)? AFFT_WIDTH/2 : -AFFT_WIDTH/2;
		e->dsp.fft[i+unwrap] = (u1_t) (int) y;
	}
	//print_max_min_f("dB", m_dB, AFFT_WIDTH);

	e->dsp.bin = bin;
	//printf("bin %d\n", bin);
	ext_send_data_msg(rx_chan, DEBUG_MSG, cmd, (u1_t *) &e->dsp, sizeof(e->dsp));
}

bool audio_fft_msgs(char *msg, int rx_chan)
{
	audio_fft_t *e = &audio_fft[rx_chan];
	int n;
	
	printf("### audio_fft_msgs RX%d <%s>\n", rx_chan, msg);
	
	if (strcmp(msg, "SET ext_server_init") == 0) {
		e->rx_chan = rx_chan;	// remember our receiver channel number
		ext_send_msg(e->rx_chan, DEBUG_MSG, "EXT ready");
		return true;
	}

	n = sscanf(msg, "SET run=%d", &e->run);
	if (n == 1) {
		if (e->run) {
			e->fft_scale = 10.0 * 2.0 / (CUTESDR_MAX_VAL * CUTESDR_MAX_VAL * AFFT_WIDTH * AFFT_WIDTH);
			ext_register_receive_FFT_samps(audio_fft_data, rx_chan, false);
		} else {
			ext_unregister_receive_FFT_samps(rx_chan);
		}
		return true;
	}
	
	int draw;
	n = sscanf(msg, "SET draw=%d", &draw);
	if (n == 1) {
		e->draw = draw;
		printf("draw %d\n", draw);
		return true;
	}
	
	double itime;
	n = sscanf(msg, "SET itime=%lf", &itime);
	if (n == 1) {
		e->ig_time = itime;
		e->ig_reset = true;
		return true;
	}
	
	int maxdb;
	n = sscanf(msg, "SET maxdb=%d", &maxdb);
	if (n == 1) {
		e->draw = draw;
		printf("draw %d\n", draw);
		return true;
	}
	
	n = strcmp(msg, "SET clear");
	if (n == 0) {
		e->ig_reset = true;
		return true;
	}
	
	return false;
}

void audio_fft_close(int rx_chan)
{

}

void audio_fft_main();

ext_t audio_fft_ext = {
	"audio_fft",
	audio_fft_main,
	audio_fft_close,
	audio_fft_msgs,
};

void audio_fft_main()
{
    double frate = ext_get_sample_rateHz();
    //printf("audio_fft_main audio sample rate = %.1f\n", frate);

	ext_register(&audio_fft_ext);
}
