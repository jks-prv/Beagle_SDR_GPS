//////////////////////////////////////////////////////////////////////////
// Homemade GPS Receiver
// Copyright (C) 2013 Andrew Holme
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// http://www.holmea.demon.co.uk/GPS/Main.htm
//////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <sys/file.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <memory.h>
#include <fftw3.h>
#include <math.h>

#include "types.h"
#include "kiwi.h"
#include "cfg.h"
#include "misc.h"
#include "gps.h"
#include "spi.h"
#include "cacode.h"
#include "debug.h"

///////////////////////////////////////////////////////////////////////////////////////////////

struct SATELLITE {
    int prn, navstar, T1, T2;
};

static const SATELLITE Sats[NUM_SATS] = {
    { 1,  63,  2,  6},
    { 2,  56,  3,  7},
    { 3,  37,  4,  8},
    { 4,  35,  5,  9},
    { 5,  64,  1,  9},
    { 6,  36,  2, 10},
    { 7,  62,  1,  8},
    { 8,  44,  2,  9},
    { 9,  33,  3, 10},
    {10,  38,  2,  3},
    {11,  46,  3,  4},
    {12,  59,  5,  6},
    {13,  43,  6,  7},
    {14,  49,  7,  8},
    {15,  60,  8,  9},
    {16,  51,  9, 10},
    {17,  57,  1,  4},
    {18,  50,  2,  5},
    {19,  54,  3,  6},
    {20,  47,  4,  7},
    {21,  52,  5,  8},
    {22,  53,  6,  9},
    {23,  55,  1,  3},
    {24,  23,  4,  6},
    {25,  24,  5,  7},
    {26,  26,  6,  8},
    {27,  27,  7,  9},
    {28,  48,  8, 10},
    {29,  61,  1,  6},
    {30,  39,  2,  7},
    {31,  58,  3,  8},
    {32,  22,  4,  9},
};

static bool Busy[NUM_SATS];

///////////////////////////////////////////////////////////////////////////////////////////////

static fftwf_complex code[NUM_SATS][FFT_LEN];

static fftwf_complex fwd_buf[FFT_LEN/2],
                     rev_buf[FFT_LEN/2];

static fftwf_plan fwd_plan, rev_plan;

static fftwf_complex copy_buf[FFT_LEN];

///////////////////////////////////////////////////////////////////////////////////////////////

float inline Bipolar(int bit) {
    return bit? -1.0 : +1.0;
}

#include <ctype.h>

static int decim=DECIM_DEF, decim_nom=0, min_sig=MIN_SIG_DECIM, test_mode;
 
void SearchParams(int argc, char *argv[]) {
	int i;
	
	for (i=1; i<argc; ) {
		char *v = argv[i];
		if (strcmp(v, "?")==0 || strcmp(v, "-?")==0 || strcmp(v, "--?")==0 || strcmp(v, "-h")==0 ||
			strcmp(v, "h")==0 || strcmp(v, "-help")==0 || strcmp(v, "--h")==0 || strcmp(v, "--help")==0) {
			printf("GPS args:\n\t-gp decimation_factor signal_threshold\n\t-gt test mode\n");
			xit(0);
		}
		if (strcmp(v, "-gp")==0) {
			i++; decim = strtol(argv[i], 0, 0); i++; min_sig = strtol(argv[i], 0, 0);
			printf("GPS decim=%d min_sig=%d\n", decim, min_sig);
		} else
		if (strcmp(v, "-gsig")==0) {
			i++; min_sig = strtol(argv[i], 0, 0);
			printf("GPS min_sig=%d\n", min_sig);
		} else
		if (strcmp(v, "-gnom")==0) {
			i++; decim_nom = strtol(argv[i], 0, 0);
			printf("GPS decim_nom=%d\n", decim_nom);
		} else
		if (strcmp(v, "-gt")==0) {
			decim = 1; test_mode = 1;
			printf("GPS test_mode\n");
		}
		i++;
		while (i<argc && ((argv[i][0] != '+') && (argv[i][0] != '-'))) {
			i++;
		}
	}
}

static char bits[NSAMPLES][2];
#define NTAPS	31
 
 // half-band filter
 #define FT	0
 static float COEF[NTAPS][2] = {
 //	remez		firwin
	-0.010233,   -0.001888,
	0.000000,    0.000000,
	0.010668,    0.003862,
	0.000000,    0.000000,
	-0.016324,   -0.008242,
	0.000000,    0.000000,
	0.024377,    0.015947,
	0.000000,    0.000000,
	-0.036482,   -0.028677,
	0.000000,    0.000000,
	0.056990,    0.050719,
	0.000000,    0.000000,
	-0.101993,   -0.098016,
	0.000000,    0.000000,

	0.316926,    0.315942,
	0.500009,    0.500706,
	0.316926,    0.315942,

	0.000000,    0.000000,
	-0.101993,   -0.098016,
	0.000000,    0.000000,
	0.056990,    0.050719,
	0.000000,    0.000000,
	-0.036482,   -0.028677,
	0.000000,    0.000000,
	0.024377,    0.015947,
	0.000000,    0.000000,
	-0.016324,   -0.008242,
	0.000000,    0.000000,
	0.010668,    0.003862,
	0.000000,    0.000000,
	-0.010233,   -0.001888,
 };

#define	DECIM_TSLICE	(128-1)

static void DecimateBy2binary(int size, char ibuf[][2], fftwf_complex obuf[]) {
	int i, j, o;
	float accI, accQ, coef;
	float coef_0 = COEF[0][FT];
	float coef_m = COEF[(NTAPS-1)/2][FT];
 
	
	for (i=o=0; i<size; i+=2, o++) {
		accI = ibuf[i][0]? coef_0:-coef_0;
		accQ = ibuf[i][1]? coef_0:-coef_0;

		for (j=2; j<NTAPS; j+=2) {
			coef = COEF[j][FT];
			accI += ibuf[i+j][0]? coef:-coef;
			accQ += ibuf[i+j][1]? coef:-coef;
		}
		
		accI += ibuf[i+(NTAPS-1)/2][0]? coef_m:-coef_m;
		accQ += ibuf[i+(NTAPS-1)/2][1]? coef_m:-coef_m;

		obuf[o][0] = Bipolar((accI >= 0)? 1:0);
		obuf[o][1] = Bipolar((accQ >= 0)? 1:0);
		
		if (((i>>1)&DECIM_TSLICE) == DECIM_TSLICE) NextTask("DecimateBy2binary");
	}
}

static void DecimateBy2float(int size, fftwf_complex ibuf[], fftwf_complex obuf[], bool yield) {
	int i, j, o;
	float accI, accQ, coef;
	float coef_0 = COEF[0][FT];
	float coef_m = COEF[(NTAPS-1)/2][FT];
	
	for (i=o=0; i<size; i+=2, o++) {
		coef = COEF[0][FT];
		accI = ibuf[i][0]*coef_0;
		accQ = ibuf[i][1]*coef_0;

		for (j=2; j<NTAPS; j+=2) {
			coef = COEF[j][FT];
			accI += ibuf[i+j][0]*coef;
			accQ += ibuf[i+j][1]*coef;
		}
		
		accI += ibuf[i+(NTAPS-1)/2][0]*coef_m;
		accQ += ibuf[i+(NTAPS-1)/2][1]*coef_m;
		
		obuf[o][0] = accI;
		obuf[o][1] = accQ;

		if (yield && ((i>>1)&DECIM_TSLICE) == DECIM_TSLICE) NextTask("DecimateBy2float");
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////

void SearchInit() {

    float ca_rate = CPS/FS;
	float ca_phase=0;

	printf("FFT %d/%d = %d planning..\n", FFT_LEN, decim, FFT_LEN/decim);
    fwd_plan = fftwf_plan_dft_1d(FFT_LEN/decim, fwd_buf, fwd_buf, FFTW_FORWARD,  FFTW_ESTIMATE);
    rev_plan = fftwf_plan_dft_1d(FFT_LEN/decim, rev_buf, rev_buf, FFTW_BACKWARD, FFTW_ESTIMATE);

    for (int sv=0; sv<NUM_SATS; sv++) {

		printf("computing CODE FFT for PRN%d\n", Sats[sv].prn);
        CACODE ca(Sats[sv].T1, Sats[sv].T2);

        for (int i=0; i<FFT_LEN; i++) {

            float chip = Bipolar(ca.Chip()); // chip at start of sample period

            ca_phase += ca_rate; // NCO phase at end of period

            if (ca_phase >= 1.0) { // reached or crossed chip boundary?
                ca_phase -= 1.0;
                ca.Clock();

                // These two lines do not make much difference
                chip *= 1.0 - ca_phase;                 // prev chip
                chip += ca_phase * Bipolar(ca.Chip());  // next chip
            }

			copy_buf[i][0] = chip;
			copy_buf[i][1] = 0;
		}

		int nsamples = NSAMPLES;

		if (decim > 2) {
			DecimateBy2float(nsamples, copy_buf, copy_buf, false);
			nsamples>>=1;
	
			for (int i=decim>>2; i>1; i>>=1) {
				DecimateBy2float(nsamples, copy_buf, copy_buf, false);
				nsamples>>=1;
			}
		}

		DecimateBy2float(nsamples, copy_buf, fwd_buf, false);
		fftwf_execute(fwd_plan);
		memcpy(code[sv], fwd_buf, sizeof fwd_buf);
    }

    CreateTask(SearchTask, 0, GPS_ACQ_PRIORITY);
}

void GenSamples(char *rbuf, int bytes) {
	int i;
	static int bc, rfd;

	if (!rfd) {
		rfd = open("gps.raw.data.bits", O_RDONLY);
		assert(rfd > 0);
	}

	i = read(rfd, rbuf, bytes);
	assert(i == bytes);

#ifndef QUIET
	static int rc, lc;
	
	rc = bc/(1024*1024);
	if (rc != lc) { printf("%dM file bytes, %dM samples\n", rc, rc*8); fflush(stdout); lc = rc; }
#endif

	bc += bytes;
}

///////////////////////////////////////////////////////////////////////////////////////////////

void SearchFree() {
    fftwf_destroy_plan(fwd_plan);
    fftwf_destroy_plan(rev_plan);
}

///////////////////////////////////////////////////////////////////////////////////////////////

static void Sample() {
    const int lo_sin[] = {1,1,0,0}; // Quadrature local oscillators
    const int lo_cos[] = {1,0,0,1};

    const float lo_rate = 4*FC/FS; // NCO rate

    const int US = 1000000/BIN_SIZE; // Sample length
    const int PACKET = GPS_SAMPS * 2;

    float lo_phase=0; // NCO phase accumulator
    int i=0, j, b;
	
	spi_set(CmdSample); // Trigger sampler and reset code generator in FPGA
	TaskSleep(US);

	while (i < NSAMPLES) {
        static SPI_MISO rx;
		spi_get(CmdGetGPSSamples, &rx, PACKET);

        for (j=0; j<PACKET; j++) {
			int byte = rx.byte[j];

            for (b=0; b<8; b++) {
            	int bit = byte&1;
				byte>>=1;

                // Down convert to complex (IQ) baseband by mixing (XORing)
                // samples with quadrature local oscillators (mix down by both FS and FC)
                if (i >= NSAMPLES)
                	break;

				bits[i][0] = bit ^ lo_sin[int(lo_phase)];
				bits[i][1] = bit ^ lo_cos[int(lo_phase)];

				i++;
                lo_phase += lo_rate;
                if (lo_phase>=4) lo_phase-=4;
            }
        }
    }

    NextTask("samp0");

	int nsamples = NSAMPLES;

	if (decim == 2) {
		DecimateBy2binary(nsamples, bits, fwd_buf);
	} else {
		DecimateBy2binary(nsamples, bits, copy_buf);
		nsamples>>=1;
		NextTask("samp2");
	
		for (i=decim>>2; i>1; i>>=1) {
			DecimateBy2float(nsamples, copy_buf, copy_buf, true);
			nsamples>>=1;
			NextTask("samp3");
		}
		
		DecimateBy2float(nsamples, copy_buf, fwd_buf, true);
	}
	NextTask("samp4");

	fftwf_execute(fwd_plan); // Transform to frequency domain

    NextTask("samp5");
}

///////////////////////////////////////////////////////////////////////////////////////////////

static float Correlate(int sv, int sample_rate, fftwf_complex *data, int *max_snr_dop, int *max_snr_i) {

	bool isDecim = ((sample_rate == (FS_I/decim)) && (decim != 1));
    fftwf_complex *prod = rev_buf;
    float max_snr=0;
    int i, fft_len = isDecim? FFT_LEN/decim : FFT_LEN;
    
    // see paper about baseband FFT symmetry (since input from GPS FE is a real signal)
    // this simulates throwing away the upper 1/2 of the FFT so subsequent FFT
    // output processing can be 1/2 the size (the FFT itself has to be the same size).
    if (test_mode) for (i=fft_len/2; i<fft_len; i++) data[i][0] = data[i][1] = 0;

	// +/- 5 kHz doppler search
    for (int dop=-5000/BIN_SIZE; dop<=5000/BIN_SIZE; dop++) {
        float max_pwr=0, tot_pwr=0;
        int max_pwr_i=0;

        // (a-ib)(x+iy) = (ax+by) + i(ay-bx)
        for (i=0; i<fft_len; i++) {
            int j=(i-dop+fft_len)%fft_len;	// doppler shifting applied to C/A code FFT
            prod[i][0] = data[i][0]*code[sv][j][0] + data[i][1]*code[sv][j][1];
            prod[i][1] = data[i][0]*code[sv][j][1] - data[i][1]*code[sv][j][0];
        }

        NextTaskP("coor FFT LONG RUN", NT_LONG_RUN);
        fftwf_execute(rev_plan);
        NextTask("corr FFT end");

        for (i=0; i<sample_rate/1000; i++) {		// 1 msec of samples
            float pwr = prod[i][0]*prod[i][0] + prod[i][1]*prod[i][1];
            if (pwr>max_pwr) max_pwr=pwr, max_pwr_i=i;
            tot_pwr += pwr;
        }
        NextTask("corr pwr");

        float ave_pwr = tot_pwr/i;
        float snr = max_pwr/ave_pwr;
        if (snr>max_snr) max_snr=snr, *max_snr_dop=dop, *max_snr_i=max_pwr_i;

#if 0
		int j;
		printf("SV-%d DOP %3d ", sv, dop);
		for (j=max_pwr_i-5; j<max_pwr_i+5; j++)
			printf("%5d:%5.2f ", j, (prod[j][0]*prod[j][0] + prod[j][1]*prod[j][1])/ave_pwr);
		printf("\n");
#endif

    }
    
    return max_snr;
}

///////////////////////////////////////////////////////////////////////////////////////////////

int SearchCode(int sv, unsigned int g1) { // Could do this with look-up tables
    int chips=0;
    for (CACODE ca(Sats[sv].T1, Sats[sv].T2); ca.GetG1()!=g1; ca.Clock()) {
    	chips++;
    	if (chips > 10240) {
    		printf("CACODE: for SV%d (PRN%d) never found G1 of 0x%03x in PRN sequence!\n", sv, Sats[sv].prn, g1);
    		return -1;
    	}
    }
    return chips;
}

///////////////////////////////////////////////////////////////////////////////////////////////

void SearchEnable(int sv) {
    Busy[sv] = false;
}

///////////////////////////////////////////////////////////////////////////////////////////////

static int searchTaskID = -1;

void SearchTask(void *param) {
    int i, us, ret, ch, last_ch=-1, sv, t_sample, lo_shift=0, ca_shift=0;
    float snr=0;

	searchTaskID = TaskID();

	if (do_sdr && !decim_nom) decim_nom = 8;
	else if (!decim_nom) decim_nom = 1;
    
    int decimation_passes = 0;
    GPSstat(STAT_PARAMS, 0, decim, min_sig);
	GPSstat(STAT_ACQUIRE, 0, 1);

    for(;;) {
        for (sv=0; sv<NUM_SATS; sv++) {

            if (Busy[sv]) {	// SV already acquired?
            	NextTask("busy1");		// let cpu run
                continue;
            }

			while((ch=ChanReset())<0) {		// all channels busy?
				TaskSleep(1000000);
				//NextTask("all chans busy");
			}
			
			if ((last_ch != ch) && (snr < min_sig)) GPSstat(STAT_PRN, 0, last_ch, 0, 0, 0);

#ifndef	QUIET
			printf("FFT-PRN%d\n", sv+1); fflush(stdout);
#endif
            us = t_sample = timer_us(); // sample time
            Sample();

			snr = Correlate(sv, FS_I/decim, fwd_buf, &lo_shift, &ca_shift);
			ca_shift *= decim;
            
            us = timer_us()-us;

#ifndef	QUIET
			printf("FFT-PRN%d %1.1f secs SNR=%1.1f\n", sv+1,
				(float)us/1000000.0, snr);
			fflush(stdout);
#endif

            GPSstat(STAT_PRN, snr, ch, Sats[sv].prn, snr < min_sig, us);
            last_ch = ch;

            if (snr < min_sig)
                continue;

            GPSstat(STAT_DOP, 0, ch, lo_shift, ca_shift);

			Busy[sv] = true;
			ChanStart(ch, sv, t_sample, (Sats[sv].T1<<4) +
										 Sats[sv].T2, lo_shift, ca_shift);
    	}
	}
}

static int gps_acquire = 1;

// Decide if the search task should run.
// Conditional because of the large load the acquisition FFT places on the Beagle.
bool SearchTaskRun()
{
	if (searchTaskID == -1) return false;
	
	bool start = false;
	int users = rx_server_users();
	
	// startup: no clock corrections done yet
	if (gps.adc_clk_corr == 0) start = true;
	else
	
	// no connections: might as well search
	if (users == 0) start = true;
	else
	
	// not too busy (only one user): search if not enough sats to generate new fixes
	if (users <= 1 && gps.good < 4) start = true;
	
	if (update_in_progress || sd_copy_in_progress) start = false;
	
	bool enable = (admcfg_bool("enable_gps", NULL, CFG_REQUIRED) == true);
	if (!enable) start = false;
	
	//printf("SearchTaskRun: acq %d start %d good %d users %d fixes %d clocks %d\n",
	//	gps_acquire, start, gps.good, users, gps.fixes, gps.adc_clk_corr);
	
	if (gps_acquire && !start) {
		printf("SearchTaskRun: $sleep\n");
		gps_acquire = 0;
		GPSstat(STAT_ACQUIRE, 0, gps_acquire);
		TaskSleepID(searchTaskID, 0);
	} else
	if (!gps_acquire && start) {
		printf("SearchTaskRun: $wakeup\n");
		gps_acquire = 1;
		GPSstat(STAT_ACQUIRE, 0, gps_acquire);
		TaskWakeup(searchTaskID, FALSE, 0);
	}
	
	return enable;
}
