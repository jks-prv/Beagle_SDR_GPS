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
#include "misc.h"
#include "gps.h"
#include "spi.h"
#include "cacode.h"

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

static fftwf_complex fwd_buf[FFT_LEN],
                     rev_buf[FFT_LEN];

static fftwf_plan fwd_plan, rev_plan;

static fftwf_complex code2[NUM_SATS][FFT_LEN];

static fftwf_complex fwd_buf2[FFT_LEN/2],
                     rev_buf2[FFT_LEN/2];

static fftwf_plan fwd_plan2, rev_plan2;

static fftwf_complex copy_buf[FFT_LEN];

///////////////////////////////////////////////////////////////////////////////////////////////

float inline Bipolar(int bit) {
    return bit? -1.0 : +1.0;
}

#include <ctype.h>

static int decim=DECIM_DEF, min_sig=MIN_SIG_DECIM, test_mode;
 
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

static void Decimate2_binary(int size, char ibuf[][2], fftwf_complex obuf[]) {
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
		
		if (((i>>1)&DECIM_TSLICE) == DECIM_TSLICE) NextTaskL("Decimate2_binary");
	}
}

static void Decimate2_float(int size, fftwf_complex ibuf[], fftwf_complex obuf[]) {
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

		if (((i>>1)&DECIM_TSLICE) == DECIM_TSLICE) NextTaskL("Decimate2_float");
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////

int SearchInit() {

    float ca_rate = CPS/FS;
	float ca_phase=0;

    fwd_plan = fftwf_plan_dft_1d(FFT_LEN, fwd_buf, fwd_buf, FFTW_FORWARD,  FFTW_ESTIMATE);
    rev_plan = fftwf_plan_dft_1d(FFT_LEN, rev_buf, rev_buf, FFTW_BACKWARD, FFTW_ESTIMATE);

    fwd_plan2 = fftwf_plan_dft_1d(FFT_LEN/decim, fwd_buf2, fwd_buf2, FFTW_FORWARD,  FFTW_ESTIMATE);
    rev_plan2 = fftwf_plan_dft_1d(FFT_LEN/decim, rev_buf2, rev_buf2, FFTW_BACKWARD, FFTW_ESTIMATE);

    for (int sv=0; sv<NUM_SATS; sv++) {

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

            fwd_buf[i][0] = chip;
            fwd_buf[i][1] = 0;

			copy_buf[i][0] = chip;
			copy_buf[i][1] = 0;
		}

		if ((decim == 1) || DECIM_CMP) {
			fftwf_execute(fwd_plan);
			memcpy(code[sv], fwd_buf, sizeof fwd_buf);
		}

		if (decim > 1) {
			int nsamples = NSAMPLES;
	
			if (decim > 2) {
				Decimate2_float(nsamples, copy_buf, copy_buf);
				nsamples>>=1;
		
				for (int i=decim>>2; i>1; i>>=1) {
					Decimate2_float(nsamples, copy_buf, copy_buf);
					nsamples>>=1;
				}
			}
	
			Decimate2_float(nsamples, copy_buf, fwd_buf2);
			fftwf_execute(fwd_plan2);
			memcpy(code2[sv], fwd_buf2, sizeof fwd_buf2);
		}
    }

    return 0;
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

    const int MS = 1000/BIN_SIZE; // Sample length
    const int PACKET = 512;		// = 16x16x2 fetched per cmd

    float lo_phase=0; // NCO phase accumulator
    int i=0, j, b;
	
	spi_set(CmdSample); // Trigger sampler and reset code generator in FPGA
	TimerWait(MS);

	while (i < NSAMPLES) {
        SPI_MISO rx;
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

				if ((decim == 1) || DECIM_CMP) {
					fwd_buf[i][0] = Bipolar(bit ^ lo_sin[int(lo_phase)]);
					fwd_buf[i][1] = Bipolar(bit ^ lo_cos[int(lo_phase)]);
				}

				bits[i][0] = bit ^ lo_sin[int(lo_phase)];
				bits[i][1] = bit ^ lo_cos[int(lo_phase)];

				i++;
                lo_phase += lo_rate;
                if (lo_phase>=4) lo_phase-=4;
            }
        }
    }

    NextTaskL("samp1");

	if ((decim == 1) || DECIM_CMP) {
    	fftwf_execute(fwd_plan); // Transform to frequency domain
	}

	if (decim > 1) {
		int nsamples = NSAMPLES;
	
		if (decim == 2) {
			Decimate2_binary(nsamples, bits, fwd_buf2);
		} else {
			Decimate2_binary(nsamples, bits, copy_buf);
			nsamples>>=1;
    		NextTaskL("samp2");
		
			for (i=decim>>2; i>1; i>>=1) {
				Decimate2_float(nsamples, copy_buf, copy_buf);
				nsamples>>=1;
    			NextTaskL("samp3");
			}
			
			Decimate2_float(nsamples, copy_buf, fwd_buf2);
		}
    	NextTaskL("samp4");
	
		fftwf_execute(fwd_plan2); // Transform to frequency domain
	}

    NextTaskL("samp5");
}

///////////////////////////////////////////////////////////////////////////////////////////////

#define CODE	(isDecim? code2 : code)

static float Correlate(int sv, int sample_rate, fftwf_complex *data, int *max_snr_dop, int *max_snr_i) {

	bool isDecim = ((sample_rate == (FS_I/decim)) && (decim != 1));
    fftwf_complex *prod = isDecim? rev_buf2 : rev_buf;
    float max_snr=0;
    int i, fft_len = isDecim? FFT_LEN/decim : FFT_LEN;
    
    // see paper about baseband FFT symmetry (since input from GPS FE is a real signal)
    // this simulates throwing away the upper 1/2 of the FFT so subsequent FFTs can be 1/2 the size
    if (test_mode) for (i=fft_len/2; i<fft_len; i++) data[i][0] = data[i][1] = 0;

	// +/- 5 KHz doppler search
    for (int dop=-5000/BIN_SIZE; dop<=5000/BIN_SIZE; dop++) {
        float max_pwr=0, tot_pwr=0;
        int max_pwr_i=0;

        // (a-ib)(x+iy) = (ax+by) + i(ay-bx)
        for (i=0; i<fft_len; i++) {
            int j=(i-dop+fft_len)%fft_len;	// doppler shifting applied to C/A code FFT
            prod[i][0] = data[i][0]*CODE[sv][j][0] + data[i][1]*CODE[sv][j][1];
            prod[i][1] = data[i][0]*CODE[sv][j][1] - data[i][1]*CODE[sv][j][0];
        }

        fftwf_execute(isDecim? rev_plan2 : rev_plan);
        NextTaskL("corr");

        for (i=0; i<sample_rate/1000; i++) {		// 1 msec of samples
            float pwr = prod[i][0]*prod[i][0] + prod[i][1]*prod[i][1];
            if (pwr>max_pwr) max_pwr=pwr, max_pwr_i=i;
            tot_pwr += pwr;
        }

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
    for (CACODE ca(Sats[sv].T1, Sats[sv].T2); ca.GetG1()!=g1; ca.Clock()) chips++;
    return chips;
}

///////////////////////////////////////////////////////////////////////////////////////////////

void SearchEnable(int sv) {
    Busy[sv] = false;
}

///////////////////////////////////////////////////////////////////////////////////////////////

#ifdef PRN_LIST
 static int prn_list[NUM_SATS] = PRN_VISIBLE;
#endif

void SearchTask() {
    int i, ch, last_ch=-1, sv, t_sample, lo_shift=0, ca_shift=0;
    float snr=0;

#ifdef DECIM_CMP
	int lo_shift2, ca_shift2;
	float snr2;
#endif
    
    UserStat(STAT_PARAMS, 0, decim, min_sig);

    for(;;) {
#ifdef PRN_LIST
        for (i=0; (sv=prn_list[i]-1) >= 0; i++) {
#else
        for (sv=0; sv<NUM_SATS; sv++) {
#endif

            if (Busy[sv]) {	// SV already acquired?
            	NextTaskL("busy1");		// let cpu run
                continue;
            }

			while((ch=ChanReset())<0) {		// all channels busy?
				NextTaskL("busy2");
			}
			if ((last_ch != ch) && (snr < min_sig)) UserStat(STAT_PRN, 0, last_ch, 0, 0, 0);

#ifndef	QUIET
			printf("FFT-PRN%d\n", sv+1); fflush(stdout);
#endif
            t_sample = Microseconds(); // sample time
			int ms = nonSim_Microseconds();
            Sample();

			if (decim > 1) {
            	snr = Correlate(sv, FS_I/decim, fwd_buf2, &lo_shift, &ca_shift);
            	ca_shift *= decim;
			} else {
            	snr = Correlate(sv, FS_I, fwd_buf, &lo_shift, &ca_shift);
            }
            
            ms = nonSim_Microseconds()-ms;

#ifndef	QUIET
			printf("FFT-PRN%d %1.1f secs SNR=%1.1f\n", sv+1,
				(float)ms/1000000.0, snr);
			fflush(stdout);
#endif

            UserStat(STAT_PRN, snr, ch, Sats[sv].prn, snr < min_sig, ms);
            last_ch = ch;

#ifdef SEARCH_ONLY
 #if DECIM_CMP
            snr2 = Correlate(sv, FS_I, fwd_buf, &lo_shift2, &ca_shift2);
            ca_shift2 *= decim;

            if ((snr >= min_sig) || (snr2 >= min_sig)) {
    			double secs = (Microseconds()-t_sample) / 1e6;
				double lo_dop = lo_shift*BIN_SIZE;
				double ca_dop = (lo_dop/L1)*CPS;
				int code_creep = nearbyint((ca_dop*secs/CPS)*FS);

				uint32_t ca_pau = ((FS_I*2/1000)-(ca_shift)) % (FS_I/1000);
				uint32_t ca_pause = ((FS_I*2/1000)-(ca_shift+code_creep)) % (FS_I/1000);

				uint32_t ca_pau2 = ((FS_I*2/1000)-(ca_shift2)) % (FS_I/1000);
				uint32_t ca_pause2 = ((FS_I*2/1000)-(ca_shift2+code_creep)) % (FS_I/1000);
				int ca_diff = ca_pause-ca_pause2;

            	printf("DEC PRN SNR   %%  LO     CA   CC    CAp    CAP\n");
            	printf("%3d %3d %3.0f %3.0f %3d %6d %4d %6d %6d %d %d\n",
            		decim, Sats[sv].prn, snr, snr2/snr*100, lo_shift, ca_shift, code_creep, ca_pau, ca_pause,
            		ca_shift%(FS_I/decim), ca_shift%((FS_I/1000)/decim));
            	printf("                %3d %6d    \" %6d %6d %3d %3d %3d\n",
            		lo_shift2, ca_shift2, ca_pau2, ca_pause2, ca_shift-ca_shift2, ca_pau-ca_pau2,
            		ca_pause-ca_pause2);
			}
 #else
            if (snr >= min_sig) printf("SV-%d PRN-%d lo_shift %d ca_shift %d snr %.1f\n",
            	sv, Sats[sv].prn, lo_shift, ca_shift, snr);
 #endif
#else
 #ifndef QUIET
            if (snr >= min_sig) printf("SV-%d PRN-%d lo_shift %d ca_shift %d snr %.1f\n",
            	sv, Sats[sv].prn, lo_shift, ca_shift, snr);
 #endif
#endif

            if (snr < min_sig)
                continue;

            UserStat(STAT_DOP, 0, ch, lo_shift, ca_shift);

#if DECIM_CMP
			if (decim > 1) {
            	snr2 = Correlate(sv, FS_I, fwd_buf, &lo_shift2, &ca_shift2);
           	 	UserStat(STAT_DOP2, snr2, ch, lo_shift2, ca_shift2);
            }
#endif

#ifndef SEARCH_ONLY
            Busy[sv] = true;
            ChanStart(ch, sv, t_sample, (Sats[sv].T1<<4) +
                                         Sats[sv].T2, lo_shift, ca_shift);
#endif
        }
    }
}
