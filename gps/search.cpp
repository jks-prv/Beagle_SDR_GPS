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
#include "clk.h"
#include "cfg.h"
#include "misc.h"
#include "gps.h"
#include "spi.h"
#include "cacode.h"
#include "debug.h"
#include "simd.h"

///////////////////////////////////////////////////////////////////////////////////////////////

static bool Busy[NUM_SATS];

///////////////////////////////////////////////////////////////////////////////////////////////

static fftwf_plan fwd_plan, rev_plan;

// code[sv][...] holds two copies of the FFT: modulo operation on the index is not needed
static fftwf_complex code[NUM_SATS][2*FFT_LEN] __attribute__ ((aligned (16)));

// fwd_buf is also used for decimating the data
static fftwf_complex fwd_buf[NSAMPLES] __attribute__ ((aligned (16)));
static fftwf_complex rev_buf[FFT_LEN]  __attribute__ ((aligned (16)));

///////////////////////////////////////////////////////////////////////////////////////////////

static float inline Bipolar(int bit) {
	// this is a branchless version of
	//	return bit ? -1.0 : +1.0;
    return -1.0f*(bit!=0) + 1.0f*(bit==0);
}

#include <ctype.h>

static int min_sig=MIN_SIG, test_mode;
 
void SearchParams(int argc, char *argv[]) {
	int i;
	
	for (i=1; i<argc; ) {
		char *v = argv[i];
		if (strcmp(v, "?")==0 || strcmp(v, "-?")==0 || strcmp(v, "--?")==0 || strcmp(v, "-h")==0 ||
			strcmp(v, "h")==0 || strcmp(v, "-help")==0 || strcmp(v, "--h")==0 || strcmp(v, "--help")==0) {
			printf("GPS args:\n\t-gsig signal_threshold\n\t-gt test mode\n");
			xit(0);
		}
		if (strcmp(v, "-gsig")==0) {
			i++; min_sig = strtol(argv[i], 0, 0);
			printf("GPS min_sig=%d\n", min_sig);
		} else
		if (strcmp(v, "-gt")==0) {
			test_mode = 1;
			printf("GPS test_mode\n");
		}
		i++;
		while (i<argc && ((argv[i][0] != '+') && (argv[i][0] != '-'))) {
			i++;
		}
	}
}

static char bits[NSAMPLES][2]  __attribute__ ((aligned (16)));
#define NTAPS	31
 
// half-band filter
#define FT	0
static float COEF[NTAPS][2] __attribute__ ((aligned (16))) = {
	// remez	    firwin
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
 } ;

#define	DECIM_TSLICE	(128-1)

static int DecimateBy2float(int size, const fftwf_complex ibuf[], fftwf_complex obuf[], bool yield) {
	const float coef_0 = COEF[0][FT];
	const float coef_m = COEF[(NTAPS-1)/2][FT];
	
	for (int i=0, o=0; i<size; i+=2, ++o) {
		float accI = ibuf[i][0]*coef_0;
		float accQ = ibuf[i][1]*coef_0;

		for (int j=2; j<NTAPS; j+=2) {
			const float coef = COEF[j][FT];
			accI += ibuf[i+j][0]*coef;
			accQ += ibuf[i+j][1]*coef;
		}

		accI += ibuf[i+(NTAPS-1)/2][0]*coef_m;
		accQ += ibuf[i+(NTAPS-1)/2][1]*coef_m;

		obuf[o][0] = accI;
		obuf[o][1] = accQ;

		if (yield && ((i>>1)&DECIM_TSLICE) == DECIM_TSLICE) NextTask("DecimateBy2float");
	}
	return size/2;
}

static int DecimateBy2binary(int size, const char ibuf[][2], fftwf_complex obuf[], bool yield) {
	// (1) convert the input to a float array
	simd_bit2float(2*size, (int8_t*)(ibuf), (float*)(obuf));
	// (2) then use the float version
	size = DecimateBy2float(size, obuf, obuf, yield);
	// (3) set output to +-1
	for (int i=0; i<size; ++i) {
	 	obuf[i][0] = Bipolar(obuf[i][0] >= 0);
	  	obuf[i][1] = Bipolar(obuf[i][1] >= 0);
	}
	return size;
}

///////////////////////////////////////////////////////////////////////////////////////////////

void SearchInit() {
    const float ca_rate = CPS/FS;
	float ca_phase=0;

    //#define QZSS_PRN_TEST
    #ifdef QZSS_PRN_TEST
        printf("QZSS PRN test:\n");
        for (int sv = 0; sv < NUM_SATS; sv++) {
            if (Sats[sv].prn <= NAVSTAR_PRN_MAX) continue;
            CACODE ca(Sats[sv].T1, Sats[sv].T2);
            int chips = 0;
            for (int i=1; i<=10; i++) {
                chips <<= 1; chips |= ca.Chip(); ca.Clock();
            }
            printf("\tPRN%d first 10 chips: 0%o\n", Sats[sv].prn, chips);
        }
	#endif

	printf("DECIM %d FFT %d planning..\n", DECIM, FFT_LEN);
    fwd_plan = fftwf_plan_dft_1d(FFT_LEN, fwd_buf, fwd_buf, FFTW_FORWARD,  FFTW_ESTIMATE);
    rev_plan = fftwf_plan_dft_1d(FFT_LEN, rev_buf, rev_buf, FFTW_BACKWARD, FFTW_ESTIMATE);

    for (int sv=0; sv<NUM_SATS; sv++) {
		//printf("computing CODE FFT for PRN%d\n", Sats[sv].prn);
        CACODE ca(Sats[sv].T1, Sats[sv].T2);

        for (int i=0; i<NSAMPLES; i++) {

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
		}

		assert(DECIM > 1);
		int nsamples = NSAMPLES;
		for (int i=DECIM; i>1; i>>=1) {
			nsamples = DecimateBy2float(nsamples, fwd_buf, fwd_buf, false);
		}
		assert(nsamples == NSAMPLES/DECIM && nsamples == FFT_LEN);

		fftwf_execute(fwd_plan);

		// make two copies of the FFT results in order to avoid modulo operation on the index in Correlate(..)
		memcpy(code[sv],          fwd_buf, nsamples*sizeof(fftwf_complex));
		memcpy(code[sv]+nsamples, fwd_buf, nsamples*sizeof(fftwf_complex));
    }

    CreateTask(SearchTask, 0, GPS_ACQ_PRIORITY);
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

    const int US = int(0.5+1000000/BIN_SIZE); // Sample length
    const int PACKET = GPS_SAMPS * 2;

    float lo_phase=0; // NCO phase accumulator
    int i=0;
	
	spi_set(CmdSample); // Trigger sampler and reset code generator in FPGA
	TaskSleepUsec(US);

	while (i < NSAMPLES) {
        static SPI_MISO rx;
		spi_get(CmdGetGPSSamples, &rx, PACKET);

        for (int j=0; j<PACKET; ++j) {
			int byte = rx.byte[j];

            for (int b=0; b<8; ++b, ++i, byte>>=1) {
            	const int bit = (byte&1);

                // Down convert to complex (IQ) baseband by mixing (XORing)
                // samples with quadrature local oscillators (mix down by FC)
                if (i >= NSAMPLES)
                	break;

				bits[i][0] = bit ^ lo_sin[int(lo_phase)];
				bits[i][1] = bit ^ lo_cos[int(lo_phase)];

                lo_phase += lo_rate;
				lo_phase -= 4*(lo_phase >= 4);
            }
        }
    }

    NextTask("samp0");
	assert(DECIM > 1);
	int nsamples = DecimateBy2binary(NSAMPLES, bits, fwd_buf, true);
	NextTask("samp2");
	for (i=DECIM>>1; i>1; i>>=1) {
		nsamples = DecimateBy2float(nsamples, fwd_buf, fwd_buf, true);
		NextTask("samp3");
	}		
	assert(nsamples == NSAMPLES/DECIM);
	NextTask("samp4");
	fftwf_execute(fwd_plan); // Transform to frequency domain
    NextTask("samp5");
}

///////////////////////////////////////////////////////////////////////////////////////////////

static float Correlate(int sv, const fftwf_complex *data, int *max_snr_dop, int *max_snr_i) {
    fftwf_complex *prod = rev_buf;
    float max_snr=0;
    int i;
    
    // see paper about baseband FFT symmetry (since input from GPS FE is a real signal)
    // this simulates throwing away the upper 1/2 of the FFT so subsequent FFT
    // output processing can be 1/2 the size (the FFT itself has to be the same size).
    //if (test_mode) for (i=fft_len/2; i<fft_len; i++) data[i][0] = data[i][1] = 0;

	// +/- 5 kHz doppler search
    for (int dop=-5000/BIN_SIZE; dop<=5000/BIN_SIZE; dop++) {
        float max_pwr=0, tot_pwr=0;
        int max_pwr_i=0;

		// prod = conj(data)*code, with doppler shifting applied to C/A code FFT
		simd_multiply_conjugate(FFT_LEN, data, code[sv]+FFT_LEN-dop, prod);
        NextTaskP("corr FFT LONG RUN", NT_LONG_RUN);
		fftwf_execute(rev_plan);
        NextTask("corr FFT end");

        for (i=0; i<SAMPLE_RATE/1000; i++) {		// 1 msec of samples
            const float pwr = prod[i][0]*prod[i][0] + prod[i][1]*prod[i][1];
            if (pwr>max_pwr) max_pwr=pwr, max_pwr_i=i;
            tot_pwr += pwr;
        }
        NextTask("corr pwr");

        const float ave_pwr = tot_pwr/i;
        const float snr = max_pwr/ave_pwr;
        if (snr>max_snr) max_snr=snr, *max_snr_dop=dop, *max_snr_i=max_pwr_i;
    }		
    return max_snr;
}

///////////////////////////////////////////////////////////////////////////////////////////////

int SearchCode(int sv, unsigned int g1) { // Could do this with look-up tables
    int chips=0;
    for (CACODE ca(Sats[sv].T1, Sats[sv].T2); ca.GetG1()!=g1; ca.Clock()) {
    	chips++;
    	if (chips > 10240) {
    		printf("SearchCode: for PRN%d never found G1 of 0x%03x in PRN sequence!\n", Sats[sv].prn, g1);
    		return -1;
    	}
    }
    return chips;
}

///////////////////////////////////////////////////////////////////////////////////////////////

static int searchRestart;

void SearchEnable(int sv) {
    Busy[sv] = false;
    searchRestart = sv+1;
}

///////////////////////////////////////////////////////////////////////////////////////////////

static int searchTaskID = -1;

void SearchTask(void *param) {
    int i, us, ret, ch, last_ch=-1, sv, t_sample, lo_shift=0, ca_shift=0;
    float snr=0;

	searchTaskID = TaskID();

    GPSstat(STAT_PARAMS, 0, DECIM, min_sig);
	GPSstat(STAT_ACQUIRE, 0, 1);

    for(;;) {
        for (sv=0; sv<NUM_SATS; sv++) {
        
            if (searchRestart) {
                sv = searchRestart-1;
                //printf("--- SEARCH RESTART sv=%d ---\n", sv);
                sv--;
                searchRestart = 0;
                continue;
            }

            if (Busy[sv]) {	// SV already acquired?
                gps.include_alert_gps = admcfg_bool("include_alert_gps", NULL, CFG_REQUIRED);
            	NextTask("busy1");		// let cpu run
                continue;
            }

			while ((ch = ChanReset(sv)) < 0) {		// all channels busy?
				TaskSleepMsec(1000);
				//NextTask("all chans busy");
			}
			
			if ((last_ch != ch) && (snr < min_sig)) GPSstat(STAT_PRN, 0, last_ch, 0, 0, 0);
#ifndef	QUIET
			printf("FFT-PRN%d\n", Sats[sv].prn); fflush(stdout);
#endif
            us = t_sample = timer_us(); // sample time
            Sample();

			snr = Correlate(sv, fwd_buf, &lo_shift, &ca_shift);
			ca_shift *= DECIM;
            
            us = timer_us()-us;
#ifndef	QUIET
			printf("FFT-PRN%d %8.6f secs SNR=%1.1f\n", Sats[sv].prn,
				(float)us/1000000.0, snr);
			fflush(stdout);
#endif

            GPSstat(STAT_PRN, snr, ch, Sats[sv].prn, snr < min_sig, us);
            last_ch = ch;

            if (snr < min_sig)
                continue;

            GPSstat(STAT_DOP, 0, ch, lo_shift, ca_shift);

			Busy[sv] = true;

			int taps = (Sats[sv].T2 > 10)? (G2_INIT | Sats[sv].T2) : ((Sats[sv].T1<<4) | Sats[sv].T2);
			//printf("ch%d PRN%d sv=%d snr=%f taps=0x%x/0x%x T1=%d T2=%d lo_shift=%d ca_shift=%d\n",
			//    ch, Sats[sv].prn, sv, snr, taps, Sats[sv].T2, Sats[sv].T1, Sats[sv].T2, lo_shift, ca_shift);
			ChanStart(ch, sv, t_sample, taps, lo_shift, ca_shift);
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
	if (clk.adc_gps_clk_corrections == 0) start = true;
	
	// no connections: might as well search
	if (users == 0) start = true;
	
	// not too busy (only one user): search if not enough sats to generate new fixes
	//if (users <= 1 && gps.good < 4) start = true;
	
	// search if not enough sats to generate new fixes
	if (gps.good < 5) start = true;
	
	if (admcfg_bool("always_acq_gps", NULL, CFG_REQUIRED) == true) start = true;
	
	if (update_in_progress || sd_copy_in_progress || backup_in_progress) start = false;
	
	bool enable = (admcfg_bool("enable_gps", NULL, CFG_REQUIRED) == true);
	if (!enable) start = false;
	
	//printf("SearchTaskRun: acq %d start %d good %d users %d fixes %d gps_corr %d\n",
	//	gps_acquire, start, gps.good, users, gps.fixes, clk.adc_gps_clk_corrections);
	
	if (gps_acquire && !start) {
		//printf("SearchTaskRun: $sleep\n");
		gps_acquire = 0;
		GPSstat(STAT_ACQUIRE, 0, gps_acquire);
		TaskSleepID(searchTaskID, 0);
	} else
	if (!gps_acquire && start) {
		//printf("SearchTaskRun: $wakeup\n");
		gps_acquire = 1;
		GPSstat(STAT_ACQUIRE, 0, gps_acquire);
		TaskWakeup(searchTaskID, FALSE, 0);
	}
	
	return enable;
}
