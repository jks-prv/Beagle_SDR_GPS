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

#ifndef	__GPS_H_
#define	__GPS_H_

#include "types.h"
#include "kiwi.h"
#include "config.h"
#include "printf.h"
#include "coroutines.h"

#include <inttypes.h>
#include <math.h>

// select debugging
#define	QUIET

#define MAX(a,b) ((a)>(b)?(a):(b))
#define MIN(a,b) ((a)<(b)?(a):(b))

void gps_main(int argc, char *argv[]);

///////////////////////////////////////////////////////////////////////////////
// Frequencies

// SE4150L
#define FC 4.092e6		// Carrier @ 2nd IF
#define FS 16.368e6		// Sampling rate
#define FS_I 16368000

#define L1 1575.42e6	// L1 carrier
#define CPS 1.023e6		// Chip rate
#define BPS 50.0		// NAV data rate

///////////////////////////////////////////////////////////////////////////////
// Parameters

#define	MIN_SIG     16

#define DECIM           16
#define SAMPLE_RATE     (FS_I / DECIM)

const float BIN_SIZE = 249.755859375;     // Hz, 4 ms

#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 9)
 const int   FFT_LEN  = FS_I/BIN_SIZE/DECIM;
 const int   NSAMPLES = FS_I/BIN_SIZE;
#else
 #define FFT_LEN  	(65536/DECIM)   // (FS_I/BIN_SIZE/DECIM)
 #define NSAMPLES  	65536           // (FS_I/BIN_SIZE)
#endif

#define NUM_SATS    42

///////////////////////////////////////////////////////////////////////////////
// Official GPS constants

const double PI = 3.1415926535898;

const double MU = 3.986005e14;          // WGS 84: earth's gravitational constant for GPS user
const double OMEGA_E = 7.2921151467e-5; // WGS 84: earth's rotation rate

const double C = 2.99792458e8; // Speed of light

const double F = -4.442807633e-10; // -2*sqrt(MU)/pow(C,2)

///////////////////////////////////////////////////////////////////////////////

struct SATELLITE {
    int prn, T1, T2;
};

static const SATELLITE Sats[NUM_SATS] = {
    { 1,  2,  6},
    { 2,  3,  7},
    { 3,  4,  8},
    { 4,  5,  9},
    { 5,  1,  9},
    { 6,  2, 10},
    { 7,  1,  8},
    { 8,  2,  9},
    { 9,  3, 10},
    {10,  2,  3},
    {11,  3,  4},
    {12,  5,  6},
    {13,  6,  7},
    {14,  7,  8},
    {15,  8,  9},
    {16,  9, 10},
    {17,  1,  4},
    {18,  2,  5},
    {19,  3,  6},
    {20,  4,  7},
    {21,  5,  8},
    {22,  6,  9},
    {23,  1,  3},
    {24,  4,  6},
    {25,  5,  7},
    {26,  6,  8},
    {27,  7,  9},
    {28,  8, 10},
    {29,  1,  6},
    {30,  2,  7},
    {31,  3,  8},
    {32,  4,  9},
    
    // QZSS sats that specify PRN with delay and G2 init value (octal) instead of as taps
    {193, 339, 01050},
    {194, 208, 01607},
    {195, 711, 01747},
    {196, 189, 01305},
    {197, 263, 00540},
    {198, 537, 01363},
    {199, 663, 00727},
    {200, 942, 00147},
    {201, 173, 01206},
    {202, 900, 01045},
};

#define NAVSTAR_PRN_MAX     32
#define G2_INIT             0x400

//////////////////////////////////////////////////////////////
// Search

void SearchInit();
void SearchFree();
void SearchTask(void *param);
bool SearchTaskRun();
void SearchEnable(int sv);
int  SearchCode(int sv, unsigned int g1);
void SearchParams(int argc, char *argv[]);

//////////////////////////////////////////////////////////////
// Tracking

#define SUBFRAMES 5
#define PARITY 6

void ChanTask(void *param);
int  ChanReset(int sv);
void ChanStart(int ch, int sv, int t_sample, int taps, int lo_shift, int ca_shift);
bool ChanSnapshot(int ch, uint16_t wpos, int *p_sv, int *p_bits, float *p_pwr);

//////////////////////////////////////////////////////////////
// Solution

void SolveTask(void *param);

//////////////////////////////////////////////////////////////
// User interface

enum STAT {
    STAT_PRN,
    STAT_POWER,
    STAT_WDOG,
    STAT_SUB,
    STAT_LAT,
    STAT_LON,
    STAT_ALT,
    STAT_TIME,
    STAT_DOP,
    STAT_LO,
    STAT_CA,
    STAT_PARAMS,
    STAT_ACQUIRE,
    STAT_EPL,
    STAT_NOVFL,
    STAT_DEBUG
};

struct azel_t {
    int az, el;
};

struct gps_stats_t {
	bool acquiring, tLS_valid;
	unsigned start, ttff;
	int tracking, good, fixes, FFTch;
	double StatSec, StatLat, StatLon, StatAlt, sgnLat, sgnLon;
	int StatDay;    // 0 = Sunday
	int StatNS, StatEW;
    signed delta_tLS, delta_tLSF;
	
	struct gps_chan_t {
		int prn;
		int snr;
		int rssi, gain;
		int wdog;
		int hold, ca_unlocked, parity, alert;
		int sub, sub_renew;
		int novfl, frames, par_errs;
		int az, el;
	} ch[GPS_CHANS];
	
	//#define AZEL_NSAMP (4*60)
	#define AZEL_NSAMP 60
	int az[AZEL_NSAMP][NUM_SATS];
	int el[AZEL_NSAMP][NUM_SATS];
	int last_samp;
	
	u4_t shadow_map[360];
	azel_t qzs_3;
	
	int IQ_data_ch;
	s2_t IQ_data[GPS_IQ_SAMPS_W];
	u4_t IQ_seq_w, IQ_seq_r;
};

extern gps_stats_t gps;

extern const char *Week[];

struct UMS {
    int u, m;
    double fm, s;
    UMS(double x) {
        u = trunc(x); x = (x-u)*60; fm = x;
        m = trunc(x); s = (x-m)*60;
    }
};

unsigned bin(char *s, int n);
void StatTask(void *param);
void GPSstat(STAT st, double, int=0, int=0, int=0, int=0, double=0);

#endif
