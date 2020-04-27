// Copyright (c) 2016 John Seamons, ZL/KF6VO

#include "ext.h"	// all calls to the extension interface begin with "ext_", e.g. ext_register()

#include "kiwi.h"
#include "misc.h"
#include "cuteSDR.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>
#include <strings.h>

//#define DEBUG_MSG	true
#define DEBUG_MSG	false

#define INTEG_WIDTH CONV_FFT_SIZE		// as defined by cuteSDR.h, typ 1024

// rx_chan is the receiver channel number we've been assigned, 0..rx_chans
// We need this so the extension can support multiple users, each with their own integrate[] data structure.

#define MAX_BINS	200			// FIXME: max height of extension data canvas

typedef struct {
	int rx_chan;
	int run, draw;
	float fft_scale;
	
	struct {
		u1_t bin;
		u1_t fft[INTEG_WIDTH];
	} dsp;
	
	bool reset;
	double time, fft_sec;
	int bins;
	double fi;
	float pwr[MAX_BINS][INTEG_WIDTH];
	int ncma[MAX_BINS];
} integrate_t;

static integrate_t integrate[MAX_RX_CHANS];

#define	FFT		0
#define	CLEAR	1

void integrate_data(int rx_chan, int ch, int ratio, int nsamps, TYPECPX *samps)
{
	integrate_t *e = &integrate[rx_chan];
	int cmd = (e->draw << 1) + (ch & 1);
	int i;
	
	// capture the ratio one time after each integration time change
	if (e->reset) {
		e->fft_sec = 1.0 / (ext_update_get_sample_rateHz(rx_chan) * ratio / INTEG_WIDTH);
		e->bins = trunc(e->time / e->fft_sec);
		e->fi = 0;
		printf("itime %.1f fft_ms %.1f bins %d\n", e->time, e->fft_sec*1e3, e->bins);
		memset(&e->pwr, 0, sizeof(e->pwr));
		memset(&e->ncma, 0, sizeof(e->ncma));
		ext_send_msg(e->rx_chan, DEBUG_MSG, "EXT bins=%d", e->bins);
		e->reset = false;
	}
	
	double fr = fmod(e->fi, e->time);
	double fb = fr / e->time;
	double fbin = fb * e->bins;
	int bin = trunc(fbin);
	
	e->fi += e->fft_sec;
	
	//printf("bin %d fi %.3f\n", bin, e->fi);
	//assert(bin < MAX_BINS);
	if (bin >= MAX_BINS) return;	// punt for now
	
	assert(nsamps == INTEG_WIDTH);
	float pwr;

	//int ncma = e->ncma[bin];
	
	#if 0
	int meas_bin = 22, meas_px = 300, noise_px = 200;
	double meas_signal = 10000000, noise_pwr;
	meas_px = (e->time == 3.6)? (515 + (int) trunc(6 * fb) * 4) : -1;
	#endif
	

	for (i=0; i < INTEG_WIDTH; i++) {
		float re = samps[i].re, im = samps[i].im;
		pwr = re*re + im*im;
		
		//if (i == meas_px) pwr = meas_signal;
		//if (bin == meas_bin && i == noise_px) noise_pwr = pwr;
		//if (bin >= (meas_bin-4) && bin <= (meas_bin+4) && i == meas_px) pwr = meas_signal;
		//if (bin == 0) pwr = 40000*40000;
		
		#if 0
		e->pwr[bin][i] = (e->pwr[bin][i] * ncma) + pwr;
		e->pwr[bin][i] /= ncma+1;
		#else
		e->pwr[bin][i] += pwr;
		#endif
		
		#if 0
		if (bin == meas_bin && i == meas_px) printf("%d s %e %e n %e %e s/n %e\n", ncma,
			meas_signal, e->pwr[bin][i],
			noise_pwr, e->pwr[bin][noise_px],
			e->pwr[bin][i] - e->pwr[bin][noise_px]);
		#endif
	}
	e->ncma[bin]++;
	
	float fft_scale = e->fft_scale;
	float dB;

	//float m_dB[INTEG_WIDTH];
	
	for (i=0; i < INTEG_WIDTH; i++) {
		pwr = e->pwr[bin][i];
		dB = 10.0 * log10f(pwr * fft_scale + (float) 1e-30);
		//m_dB[i] = dB;
		if (dB > 0) dB = 0;
		if (dB < -200.0) dB = -200.0;
		dB--;

		int unwrap = (i < INTEG_WIDTH/2)? INTEG_WIDTH/2 : -INTEG_WIDTH/2;
		e->dsp.fft[i+unwrap] = (u1_t) (int) dB;
	}
	//print_max_min_f("dB", m_dB, INTEG_WIDTH);

	e->dsp.bin = bin;
	//printf("bin %d\n", bin);
	ext_send_msg_data(rx_chan, DEBUG_MSG, cmd, (u1_t *) &e->dsp, sizeof(e->dsp));
}

bool integrate_msgs(char *msg, int rx_chan)
{
	integrate_t *e = &integrate[rx_chan];
	int n;
	
	printf("### integrate_msgs RX%d <%s>\n", rx_chan, msg);
	
	if (strcmp(msg, "SET ext_server_init") == 0) {
		e->rx_chan = rx_chan;	// remember our receiver channel number
		ext_send_msg(e->rx_chan, DEBUG_MSG, "EXT ready");
		return true;
	}

	n = sscanf(msg, "SET run=%d", &e->run);
	if (n == 1) {
		if (e->run) {
			e->draw = FFT;
			e->fft_scale = 10.0 * 2.0 / (CUTESDR_MAX_VAL * CUTESDR_MAX_VAL * INTEG_WIDTH * INTEG_WIDTH);
			ext_register_receive_FFT_samps(integrate_data, rx_chan, POST_FILTERED);
		} else {
			ext_unregister_receive_FFT_samps(rx_chan);
		}
		return true;
	}
	
	double itime;
	n = sscanf(msg, "SET itime=%lf", &itime);
	if (n == 1) {
		e->time = itime;
		e->reset = true;
		return true;
	}
	
	n = strcmp(msg, "SET clear");
	if (n == 0) {
		e->reset = true;
		return true;
	}
	
	return false;
}

void integrate_main();

ext_t integrate_ext = {
	"integrate",
	integrate_main,
	NULL,
	integrate_msgs,
	EXT_NEW_VERSION,
	EXT_FLAGS_HEAVY
};

void integrate_main()
{
	ext_register(&integrate_ext);
}
