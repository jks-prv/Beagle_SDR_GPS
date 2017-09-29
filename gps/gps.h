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
//#define PRN_LIST
//#define FOLLOW_NAV
//#define PRN_LIST

#define MAX(a,b) ((a)>(b)?(a):(b))
#define MIN(a,b) ((a)<(b)?(a):(b))

void gps_main(int argc, char *argv[]);

///////////////////////////////////////////////////////////////////////////////
// Frequencies

#define DECIM_DEF	16	// gives a roughly 4ms FFT runtime needed for data pump latency reasons

#define	MIN_SIG_DECIM		16
#define	MIN_SIG_NO_DECIM	75

#define	QUIET

// SE4150L
#define FC 4.092e6		// Carrier @ 2nd IF
#define FS 16.368e6		// Sampling rate
#define FS_I 16368000

#define L1 1575.42e6	// L1 carrier
#define CPS 1.023e6		// Chip rate
#define BPS 50.0		// NAV data rate

///////////////////////////////////////////////////////////////////////////////
// Parameters

#define	BIN_SIZE	250		// Hz, 4 ms
#define FFT_LEN  	(FS_I/BIN_SIZE)
#define NSAMPLES  	(FS_I/BIN_SIZE)
#define NUM_SATS    32

///////////////////////////////////////////////////////////////////////////////
// Official GPS constants

const double PI = 3.1415926535898;

const double MU = 3.986005e14;          // WGS 84: earth's gravitational constant for GPS user
const double OMEGA_E = 7.2921151467e-5; // WGS 84: earth's rotation rate

const double C = 2.99792458e8; // Speed of light

const double F = -4.442807633e-10; // -2*sqrt(MU)/pow(C,2)

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
int  ChanReset(void);
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
		int hold, ca_unlocked, parity;
		int sub, sub_renew;
		int novfl, frames, par_errs;
		int az, el;
	} ch[GPS_CHANS];
	
	//#define AZEL_NSAMP (4*60)
	#define AZEL_NSAMP 60
	int az[AZEL_NSAMP][NUM_SATS];
	int el[AZEL_NSAMP][NUM_SATS];
	int last_samp;
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
