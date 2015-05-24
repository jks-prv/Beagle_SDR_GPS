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

#include <inttypes.h>

#include "types.h"
#include "wrx.h"
#include "config.h"
//#include "io.h"
#include "coroutines.h"

// select debugging
//#define PRN_LIST
//#define FOLLOW_NAV
//#define PRN_LIST

#define MAX(a,b) ((a)>(b)?(a):(b))
#define MIN(a,b) ((a)<(b)?(a):(b))

void gps(int argc, char *argv[]);

///////////////////////////////////////////////////////////////////////////////
// Frequencies

// currently overhead
//#define PRN_LIST
#define PRN_VISIBLE { 5, 29, -1 }
#define DECIM_CMP	0
#define        SE4150L_FREQ
//#define SEARCH_ONLY
//#define	FASTGPS_FREQ
//#define	WAVE_FREQ
//#define	CODE_FREQ
//#define	ORIG_FREQ

#define DECIM_DEF	8

#define	MIN_SIG_DECIM		16
#define	MIN_SIG_NO_DECIM	75

#ifdef SEARCH_ONLY
#else
 #define	QUIET
#endif

#ifdef PRIMO_FREQ
 #define FC 4.092e6     // Carrier @ 2nd IF
 #define FS 5.456e6      // Sampling rate
 #define FS_I 5456000
 //#define IB 14
 #define IB 15
#endif

#ifdef SE4150L_FREQ
 #define FC 4.092e6     // Carrier @ 2nd IF
 #define FS 16.368e6      // Sampling rate
 #define FS_I 16368000
 #define IB 15
#endif

#ifdef FASTGPS_FREQ
 #define FC 4.1304e6     // Carrier @ 2nd IF
 #define FS 16.3676e6      // Sampling rate
 #define FS_I 16367600
 #define IB 15
#endif

#ifdef WAVE_FREQ
 #define FC (5.400e6/4.0)     // ??? Carrier @ 2nd IF
 #define FS 5.400e6      // ??? Sampling rate
 #define FS_I 5400000		// ???
 #define IB ?
#endif

#ifdef CODE_FREQ
 #define CF 4
 #define FC (1.023e6/4.0*CF)     // Carrier @ 2nd IF
 #define FS (1.023e6*CF)      // Sampling rate
 #define FS_I (1023000*CF)
 #define IB 14
#endif

#ifdef ORIG_FREQ
 #define FC 2.6e6     // Carrier @ 2nd IF
 #define FS 10e6      // Sampling rate
 #define FS_I 10000000
 #define IB 14
#endif

#define L1 1575.42e6 // L1 carrier
#define CPS 1.023e6  // Chip rate
#define BPS 50.0     // NAV data rate

///////////////////////////////////////////////////////////////////////////////
// Parameters

#define	BIN_SIZE	250		// Hz, 4 ms
#define FFT_LEN  	(FS_I/BIN_SIZE)
#define NSAMPLES  	(FS_I/BIN_SIZE)
#define NUM_SATS    32
#define	IOVFL		(1 << IB)
#define	ISGN		(IOVFL >> 1)
#define IM			(IOVFL - 1)

///////////////////////////////////////////////////////////////////////////////
// Official GPS constants

const double PI = 3.1415926535898;

const double MU = 3.986005e14;          // WGS 84: earth's gravitational constant for GPS user
const double OMEGA_E = 7.2921151467e-5; // WGS 84: earth's rotation rate

const double C = 2.99792458e8; // Speed of light

const double F = -4.442807633e-10; // -2*sqrt(MU)/pow(C,2)

//////////////////////////////////////////////////////////////
// Events

#define JOY_MASK 0x1F

#define JOY_RIGHT    (1<<0)
#define JOY_LEFT     (1<<1)
#define JOY_DOWN     (1<<2)
#define JOY_UP       (1<<3)
#define JOY_PUSH     (1<<4)
#define EVT_EXIT     (1<<5)
#define EVT_BARS     (1<<6)
#define EVT_POS      (1<<7)
#define EVT_TIME     (1<<8)
#define EVT_PRN      (1<<9)
#define EVT_SHUTDOWN (1<<10)

unsigned EventCatch(unsigned);
void     EventRaise(unsigned);

//////////////////////////////////////////////////////////////
// Search

int  SearchInit();
void SearchFree();
void SearchTask();
void SearchEnable(int sv);
int  SearchCode(int sv, unsigned int g1);
void SearchParams(int argc, char *argv[]);

//////////////////////////////////////////////////////////////
// Tracking

void ChanTask(void);
int  ChanReset(void);
void ChanStart(int ch, int sv, int t_sample, int taps, int lo_shift, int ca_shift);
bool ChanSnapshot(int ch, uint16_t wpos, int *p_sv, int *p_bits, float *p_pwr);

//////////////////////////////////////////////////////////////
// Solution

void SolveTask();
int *ClockBins();

//////////////////////////////////////////////////////////////
// User interface

enum STAT {
    STAT_PRN,
    STAT_POWER,
    STAT_WDOG,
    STAT_SUB,
    STAT_CHANS,
    STAT_LAT,
    STAT_LON,
    STAT_ALT,
    STAT_TIME,
    STAT_TTFF,
    STAT_DOP,
    STAT_DOP2,
    STAT_LO,
    STAT_CA,
    STAT_PARAMS,
    STAT_EPL,
    STAT_DEBUG
};

void UserTask();
void UserStat(STAT st, double, int=0, int=0, int=0, int=0, double=0);

#endif
