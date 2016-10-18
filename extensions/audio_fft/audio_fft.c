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
	
	float ig_time, ig_fft_ms;
	int ig_bins;
	double ig_fi;
	float ig_pwr[MAX_BINS][AFFT_WIDTH];
	int ig_ncma[MAX_BINS];
	
	float gain;
	double cma;
	u4_t ncma;
	int ring[N_CH], points;
	#define N_IQ_RING (16*1024)
	float iq[N_CH][N_IQ_RING][IQ];
	u1_t plot[N_CH][N_IQ_RING][N_HISTORY][IQ];
	u1_t map[N_IQ_RING][IQ];
};

static audio_fft_t audio_fft[RX_CHANS];

#define	IQ_POINTS		0
#define	IQ_DENSITY		1
#define	IQ_CLEAR		4

// FIXME: needs improvement
#define PLOT_AUTO_SCALE(d, iq) { \
	abs_iq = fabs(iq); \
	e->cma = (e->cma * e->ncma) + abs_iq; \
	e->ncma++; \
	e->cma /= e->ncma; \
	t = iq / (4 * e->cma) * 255; \
	t += 127; \
	if (t > 255) t = 255; \
	if (t < 0) t = 0; \
	d = (u1_t) t; \
}

void audio_fft_data(int rx_chan, int ch, int nsamps, TYPECPX *samps)
{
	audio_fft_t *e = &audio_fft[rx_chan];
	int cmd = (e->draw << 1) + (ch & 1);
	int i;
	
	double fr = fmod(e->ig_fi, e->ig_time);
	double fb = fr / e->ig_time;
	double fbin = fb * e->ig_bins;
	int bin = trunc(fbin);
	assert(bin < MAX_BINS);
	e->ig_fi += e->ig_fft_ms;
	
	assert(nsamps == AFFT_WIDTH);
	//float range_dB = ext_max_dB(rx_chan) - ext_min_dB(rx_chan);
	//float pix_per_dB = 255.0 / range_dB;
	float pwr;

	int ncma = e->ig_ncma[bin];
	int meas_bin = 22, meas_px = 300, noise_px = 200;
	double meas_signal = 10000000, noise_pwr;
	
	meas_px = 515 + (int) trunc(6 * fb) * 4;

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
			e->ig_time = 3.6;	// Alpha
			//e->ig_time = 3.558;		// WI-254
			e->ig_fft_ms = 1.0 / (ext_get_sample_rateHz() / AFFT_WIDTH);
			e->ig_bins = trunc(e->ig_time / e->ig_fft_ms);
			e->ig_fi = 0;
			memset(&e->ig_pwr, 0, sizeof(e->ig_pwr));
			memset(&e->ig_ncma, 0, sizeof(e->ig_ncma));
			
			e->fft_scale = 10.0 * 2.0 / (CUTESDR_MAX_VAL * CUTESDR_MAX_VAL * AFFT_WIDTH * AFFT_WIDTH);
			ext_register_receive_FFT_samps(audio_fft_data, rx_chan, false);
		} else {
			ext_unregister_receive_FFT_samps(rx_chan);
		}
		if (e->points == 0)
			e->points = 1024;
		return true;
	}
	
	int gain;
	n = sscanf(msg, "SET gain=%d", &gain);
	if (n == 1) {
		// 0 .. +100 dB of CUTESDR_MAX_VAL
		#ifdef NBFM_PLL_DEBUG
			static int initGain;
			if (!initGain) {
				printf("initGain\n");
				gain = 76;
				initGain = 1;
			}
		#endif
		e->gain = gain? pow(10.0, ((float) gain - 50) / 10.0) : 0;
		printf("e->gain %d dB %.6f\n", gain-50, e->gain);
		return true;
	}
	
	int points;
	n = sscanf(msg, "SET points=%d", &points);
	if (n == 1) {
		e->points = points;
		printf("points %d\n", points);
		return true;
	}
	
	int draw;
	n = sscanf(msg, "SET draw=%d", &draw);
	if (n == 1) {
		e->draw = draw;
		printf("draw %d\n", draw);
		return true;
	}
	
	float offset;
	n = sscanf(msg, "SET offset=%f", &offset);
	if (n == 1) {
		adc_clock -= adc_clock_offset;
		adc_clock_offset = offset;
		adc_clock += adc_clock_offset;
		gps.adc_clk_corr++;
		printf("adc_clock %.6f offset %.2f\n", adc_clock/1e6, offset);
		return true;
	}
	
	n = strcmp(msg, "SET clear");
	if (n == 0) {
		e->cma = e->ncma = 0;
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
