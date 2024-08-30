// Copyright (c) 2016-2020 John Seamons, ZL4VO/KF6VO

#include "ext.h"	// all calls to the extension interface begin with "ext_", e.g. ext_register()

#include "kiwi.h"
#include "mode.h"
#include "misc.h"
#include "cuteSDR.h"
#include "rx_sound.h"
#include "rx_util.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>
#include <strings.h>

//#define DEBUG_MSG	true
#define DEBUG_MSG	false

//#define PN_F_DEBUG
#ifdef PN_F_DEBUG
    #define P0_F p_f[0]
    #define P1_F p_f[1]
    #define P2_F p_f[2]
    #define P3_F p_f[3]
#else
    #define P0_F 0
    #define P1_F 0
    #define P2_F 0
    #define P3_F 0
#endif

#define INTEG_WIDTH CONV_FFT_SIZE		// as defined by cuteSDR.h, typ 1024

// rx_chan is the receiver channel number we've been assigned, 0..rx_chans
// We need this so the extension can support multiple users, each with their own fft[] data structure.

#define MAX_BINS	200			// FIXME: max height of extension data canvas

typedef struct {
	int rx_chan;
	int run, instance, mode;
	float fft_scale, spectrum_scale;
	u4_t last_ms;
	
	TYPECPX iswap[INTEG_WIDTH], isamps[INTEG_WIDTH];
	
	struct {
		u1_t bin;
		u1_t fft[INTEG_WIDTH];
	} dsp;
	
	bool reset, stop;
	double time, fft_sec;
	int bins, last_bin;
	double fi;
	float pwr[MAX_BINS][INTEG_WIDTH];
	int ncma[MAX_BINS];
} fft_t;

static fft_t fft[MAX_RX_CHANS];

#define	FFT		0
#define	CLEAR	1

enum data_e { FUNC_OFF=-1, FUNC_WF=0, FUNC_SPEC=1, FUNC_INTEG=2 };
static const char* func_s[] = { "off", "wf", "spec", "integ" };

bool fft_data(int rx_chan, int instance, int flags, int ratio, int nsamps, TYPECPX *samps)
{
	fft_t *e = &fft[rx_chan];
	if (instance != e->instance) return false;      // not the instance we want
	//real_printf("%d", e->instance); fflush(stdout);
	int i, bin;
	bool buf_changed = false;
	
	// capture the ratio one time after each integration time change
	if (e->reset) {
		e->fft_sec = 1.0 / (ext_update_get_sample_rateHz(rx_chan) * ratio / INTEG_WIDTH);
		e->bins = trunc(e->time / e->fft_sec);
		e->last_bin = 0;
		e->stop = false;
		e->fi = 0;
		//printf("itime %.1f fft_ms %.1f bins %d\n", e->time, e->fft_sec*1e3, e->bins);
		memset(&e->pwr, 0, sizeof(e->pwr));
		memset(&e->ncma, 0, sizeof(e->ncma));
		e->mode = -1;
		ext_send_msg(e->rx_chan, DEBUG_MSG, "EXT bins=%d", e->bins);
		e->reset = false;
	}
	
	if (e->run == FUNC_INTEG) {
        double fr = fmod(e->fi, e->time);
        double fb = fr / e->time;
        double fbin = fb * e->bins;
        bin = trunc(fbin);
        e->fi += e->fft_sec;
	    if (bin >= MAX_BINS) return false;  // punt for now
	} else {
	    bin = 0;
	}
	
	assert(nsamps == INTEG_WIDTH);
	float pwr;

	//int ncma = e->ncma[bin];
	
	#if 0
	int meas_bin = 22, meas_px = 300, noise_px = 200;
	double meas_signal = 10000000, noise_pwr;
	meas_px = (e->time == 3.6)? (515 + (int) trunc(6 * fb) * 4) : -1;
	#endif
	
    if (e->run != FUNC_INTEG) {
        for (i=0; i < INTEG_WIDTH; i++) {
            e->pwr[bin][i] = samps[i].re * samps[i].re;
        }
        #if 0
            if (bin < e->last_bin) {
                e->stop = true;
            }
            e->last_bin = bin;
        #endif
    } else {
    
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
    }
	
	float scale = (e->run == FUNC_SPEC)? e->spectrum_scale : e->fft_scale;
	float dB;

	//float m_dB[INTEG_WIDTH];
	
	for (i=0; i < INTEG_WIDTH; i++) {
		pwr = e->pwr[bin][i];
		dB = dB_fast(pwr * scale);
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
	
	// limit spectrum update rate as done for RF/IF spectrum
	bool skip = false;
	if (e->run == FUNC_SPEC) {
        u4_t ms = timer_ms();
        #define UPDATE_MS 100
        if (ms > e->last_ms + UPDATE_MS /* && !e->stop */) {
	        if (e->last_ms)
	            e->last_ms += UPDATE_MS;
	        else
	            e->last_ms = ms;
	    } else {
	        skip = true;
	    }
	}
	if (!skip)
        ext_send_msg_data(rx_chan, DEBUG_MSG, FFT, (u1_t *) &e->dsp, sizeof(e->dsp));
	
	return buf_changed;
}

bool fft_msgs(char *msg, int rx_chan)
{
	fft_t *e = &fft[rx_chan];
    e->rx_chan = rx_chan;	// remember our receiver channel number
	int n;
	
	//printf("FFT RX%d <%s>\n", rx_chan, msg);
	
	if (strcmp(msg, "SET ext_server_init") == 0) {
		ext_send_msg(rx_chan, DEBUG_MSG, "EXT ready");
		return true;
	}

	n = sscanf(msg, "SET run=%d", &e->run);
	if (n == 1) {
        snd_t *snd = &snd_inst[rx_chan];
        
		if (e->run != FUNC_OFF) {
			float scale = 10.0 * 2.0 / (CUTESDR_MAX_VAL * CUTESDR_MAX_VAL * INTEG_WIDTH * INTEG_WIDTH);
            float boost = 1;
			
			switch (e->run) {
			
			    case FUNC_WF:
			        e->instance = SND_INSTANCE_FFT_PASSBAND;
			        boost = (P3_F? P3_F : 1e4);
			        break;
			
			    case FUNC_SPEC:
			        e->instance = snd->isSAM? SND_INSTANCE_FFT_CHAN_NULL : SND_INSTANCE_FFT_PASSBAND;

                    // scale up to roughly match WF spectrum values
                    boost = snd->isSAM? ( (snd->mode == MODE_QAM)? (P2_F? P2_F : 100) : (P1_F? P1_F : 100) ) : (P0_F? P0_F : 1e6);
                    e->last_ms = 0;
			        break;
			
			    case FUNC_INTEG:
			        e->instance = SND_INSTANCE_FFT_PASSBAND;
			        boost = (P3_F? P3_F : 1e4);
			        break;
			}

            e->spectrum_scale = scale * boost;
            e->fft_scale      = scale * boost;
			ext_register_receive_FFT_samps(fft_data, rx_chan, POST_FILTERED);
            printf("FFT func=%s mode=%s isSAM=%d (scale=%g * boost=%.1g) => spectrum_scale=%g fft_scale=%g\n",
                func_s[e->run+1], mode_uc[snd->mode], snd->isSAM, scale, boost, e->spectrum_scale, e->fft_scale);
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

void FFT_main();

ext_t fft_ext = {
	"FFT",
	FFT_main,
	NULL,
	fft_msgs,
	EXT_NEW_VERSION,
	EXT_FLAGS_HEAVY
};

void FFT_main()
{
	ext_register(&fft_ext);
}

#if 0
	if (spectrum) {
        for (i = 0; i < INTEG_WIDTH; i++) {
		    int unswap = (i < INTEG_WIDTH/2)? INTEG_WIDTH/2 : -INTEG_WIDTH/2;
		    e->iswap[i+unswap] = samps[i];
		}
		
		#if 1
            for (i = 0; i < INTEG_WIDTH; i++) {
                e->isamps[i].re = 0;
                e->isamps[i].im = 0;
            }
            
            #define IL (INTEG_WIDTH/8)
            
            #if 0
                for (i = INTEG_WIDTH/2; i < INTEG_WIDTH/2+IL; i++) {
                    e->isamps[i].re = e->iswap[i].re;
                    e->isamps[i].im = e->iswap[i].im;
                }
            
                for (i = 0; i < IL; i++) {
                    // LSB -= USB
                    //e->isamps[INTEG_WIDTH*1/4 +i].re = e->iswap[INTEG_WIDTH/2 +i].re - e->iswap[INTEG_WIDTH/2 -1 -i].re;
                    //e->isamps[INTEG_WIDTH*1/4 +i].im = e->iswap[INTEG_WIDTH/2 +i].im - e->iswap[INTEG_WIDTH/2 -1 -i].im;
                
                    // USB -> LSB replication
                    //e->isamps[INTEG_WIDTH/2 -1 -i].re = e->iswap[INTEG_WIDTH/2 +i].re;
                    //e->isamps[INTEG_WIDTH/2 -1 -i].im = e->iswap[INTEG_WIDTH/2 +i].im;
                
                    // USB -> LSB replication, Hilbert?
                    //e->isamps[INTEG_WIDTH/2 -1 -i].re = e->iswap[INTEG_WIDTH/2 +i].re;
                    //TYPEREAL re = e->iswap[INTEG_WIDTH/2 +i].re;
                    //e->isamps[INTEG_WIDTH/2 -1 -i].im = (re > 0)? 1 : ((re < 0)? -1 : 0);
                
                    // USB -> LSB replication, complex conj
                    e->isamps[INTEG_WIDTH/2 -1 -i].re = e->iswap[INTEG_WIDTH/2 +i].re;
                    e->isamps[INTEG_WIDTH/2 -1 -i].im = - e->iswap[INTEG_WIDTH/2 +i].im;
                
                    // USB -> LSB replication, swap re/im
                    //e->isamps[INTEG_WIDTH/2 -1 -i].re = e->iswap[INTEG_WIDTH/2 +i].im;
                    //e->isamps[INTEG_WIDTH/2 -1 -i].im = e->iswap[INTEG_WIDTH/2 +i].re;
                
                    // pass through
                    //e->isamps[INTEG_WIDTH/2 -1 -i].re = e->iswap[INTEG_WIDTH/2 -1 -i].re;
                    //e->isamps[INTEG_WIDTH/2 -1 -i].im = e->iswap[INTEG_WIDTH/2 -1 -i].im;
                }

                buf_changed = true;
            #else
                for (i = 0; i < IL; i++) {
                    e->isamps[INTEG_WIDTH*1/4 +i   ].re = e->iswap[INTEG_WIDTH/2 +i   ].re; // I USB
                    e->isamps[INTEG_WIDTH*1/4 -i -1].re = e->iswap[INTEG_WIDTH/2 -i -1].re; // I LSB

                    e->isamps[INTEG_WIDTH*3/4 +i   ].re = e->iswap[INTEG_WIDTH/2 +i   ].im; // Q USB
                    e->isamps[INTEG_WIDTH*3/4 -i -1].re = e->iswap[INTEG_WIDTH/2 -i -1].im; // Q LSB

                    e->isamps[INTEG_WIDTH*7/16 +i  ].re = fabsf(e->iswap[INTEG_WIDTH/2 +i   ].re)  // I USB
                                                        - fabsf(e->iswap[INTEG_WIDTH/2 -i -1].re); // I LSB
                }
            #endif
        #endif

        for (i = 0; i < INTEG_WIDTH; i++) {
		    int swap = (i < INTEG_WIDTH/2)? INTEG_WIDTH/2 : -INTEG_WIDTH/2;
		    samps[i+swap] = e->isamps[i];
		}
		
    }
#endif
