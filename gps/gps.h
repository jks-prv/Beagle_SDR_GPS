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

#pragma once

#include "types.h"
#include "kiwi.h"
#include "config.h"
#include "printf.h"
#include "coroutines.h"

#include <inttypes.h>
#include <math.h>

// select debugging
//#define TEST_VECTOR
#define	QUIET

void gps_main(int argc, char *argv[]);

///////////////////////////////////////////////////////////////////////////////
// Frequencies

// MAX2769B / SE4150L
#define FC 4.092e6		// Carrier @ 2nd IF
#define FS 16.368e6     // Sampling rate
#define FS_I 16368000

#define CPS 1.023e6		// Chip rate
#define CPS_I 1023000

#define L1_f 1575.42e6  // L1 carrier
#define L1_CODE_PERIOD (L1_CODELEN*1000/CPS_I)
#define L1_BPS 50.0     // NAV data rate

#define E1B_f 1575.42e6 // E1B carrier
#define E1B_CODE_PERIOD (E1B_CODELEN*1000/CPS_I)
#define E1B_BPS 250.0   // NAV data rate

///////////////////////////////////////////////////////////////////////////////
// Parameters

#define	MIN_SIG     16

#define DECIM       4
//#define DECIM       8
#define SAMPLE_RATE (FS_I / DECIM)

#define GPS_FFT_POW2 1
//#define GPS_FFT_POW2 0
#if GPS_FFT_POW2
    const float BIN_SIZE = 249.755859375;     // Hz, 4 ms

    #if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 9)
     const int   FFT_LEN  = FS_I/BIN_SIZE/DECIM;
     const int   NSAMPLES = FS_I/BIN_SIZE;
    #else
     #define FFT_LEN  	(65536/DECIM)   // (FS_I/BIN_SIZE/DECIM)
     #define NSAMPLES  	65536           // (FS_I/BIN_SIZE)
    #endif
#else
    #define	BIN_SIZE	250		// Hz, 4 ms
    #define FFT_LEN  	(FS_I/BIN_SIZE/DECIM)
    #define NSAMPLES  	(FS_I/BIN_SIZE)
#endif

///////////////////////////////////////////////////////////////////////////////
// Official GPS constants

const double PI = 3.1415926535898;

const double MU = 3.986005e14;          // WGS 84: earth's gravitational constant for GPS user
const double OMEGA_E = 7.2921151467e-5; // WGS 84: earth's rotation rate

const double C = 2.99792458e8; // Speed of light

const double F = -4.442807633e-10; // -2*sqrt(MU)/pow(C,2)

///////////////////////////////////////////////////////////////////////////////

typedef enum { Navstar, SBAS, QZSS, E1B } sat_e;
const char sat_s[sizeof(sat_e)] = { 'N', 'S', 'Q', 'E' };

typedef struct {
    int prn;
    union {
        struct {
            int T1, T2;
        };
        struct {
            int G2_delay, G2_init;
        };
    };
    sat_e type;
    
    int sat;
    char *prn_s;
    bool busy;
} SATELLITE;

#define is_Navstar(sat)     (Sats[sat].type == Navstar)
#define is_SBAS(sat)        (Sats[sat].type == SBAS)
#define is_QZSS(sat)        (Sats[sat].type == QZSS)
#define is_E1B(sat)         (Sats[sat].type == E1B)

#define MAX_SATS    64

extern SATELLITE Sats[];

#define G2_INIT     0x400

// maximum number of sats possible, not current number of active sats
#define NUM_NAVSTAR_SATS    32
#define NUM_E1B_SATS        50

extern u1_t E1B_code1[NUM_E1B_SATS][E1B_CODELEN];

#define PRN(sat)        (Sats[sat].prn_s)

//////////////////////////////////////////////////////////////
// Search

void SearchInit();
void SearchFree();
void SearchTask(void *param);
void SearchTaskRun();
void SearchEnable(int sat);
void SearchParams(int argc, char *argv[]);

//////////////////////////////////////////////////////////////
// Tracking

#define SUBFRAMES 5
#define PARITY 6

void ChanTask(void *param);
int  ChanReset(int sat, int codegen_init);
void ChanStart(int ch, int sat, int t_sample, int lo_shift, int ca_shift, int snr);
bool ChanSnapshot(int ch, uint16_t wpos, int *p_sat, int *p_bits, int *p_bits_tow, float *p_pwr);
void ChanRemove(sat_e type);

//////////////////////////////////////////////////////////////
// Solution

void SolveTask(void *param);

//////////////////////////////////////////////////////////////
// User interface

typedef enum {
    STAT_SAT,
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
    STAT_DEBUG,
    STAT_SOLN
} STAT;

#define GPS_ERR_SLIP    1
#define GPS_ERR_CRC     2
#define GPS_ERR_ALERT   3
#define GPS_ERR_OOS     4
#define GPS_ERR_PAGE    5

typedef struct {
    int az, el;
} azel_t;

typedef struct {
    int sat;
    int snr;
    int rssi, gain;
    #define GPS_N_AGE (8 + SPACE_FOR_NULL)
    char age[GPS_N_AGE];
    bool too_old;
    int wdog;
    int hold, ca_unlocked, parity, alert;
    int sub, sub_renew;
    int novfl, frames, par_errs;
    int az, el;
    int has_soln;
    int ACF_mode;
} gps_chan_t;

typedef struct {
    int x, y;
    float lat, lon;
} gps_pos_t;

typedef struct {
    float lat, lon;
} gps_map_t;

typedef struct {
    int n_Navstar, n_QZSS, n_E1B;
    bool acq_Navstar, acq_QZSS, QZSS_prio, acq_Galileo;
	bool acquiring, tLS_valid;
	unsigned start, ttff;
	int tracking, good, FFTch;

    int last_samp_hour;
	u4_t fixes, fixes_min, fixes_min_incr;
	u4_t fixes_hour, fixes_hour_incr, fixes_hour_samples;

	double StatWeekSec, StatDaySec;
	int StatDay;    // 0 = Sunday
	double StatLat, StatLon, StatAlt, sgnLat, sgnLon;
	int StatNS, StatEW;
    signed delta_tLS, delta_tLSF;
    bool include_alert_gps;
    bool include_E1B;
    bool set_date, date_set;
    int tod_chan;
    int soln_type, E1B_plot_separately;
	gps_chan_t ch[GPS_MAX_CHANS];
	
	//#define AZEL_NSAMP (4*60)
	#define AZEL_NSAMP 60
	int az[AZEL_NSAMP][MAX_SATS];
	int el[AZEL_NSAMP][MAX_SATS];
	int last_samp;
	
	u4_t shadow_map[360];
	azel_t qzs_3;
	
	int IQ_data_ch;
	s2_t IQ_data[GPS_IQ_SAMPS_W];
	u4_t IQ_seq_w, IQ_seq_r;

    // reference lat/lon from early GPS fix
	bool have_ref_lla;
	float ref_lat, ref_lon, ref_alt;

    // E1B_plot_separately == false
    #define MAP_ALL 0           // green map pin
    
    // E1B_plot_separately == true
    #define MAP_WITHOUT_E1B 0   // green map pin
    #define MAP_WITH_E1B 1      // red map pin
    #define MAP_ONLY_E1B 2      // yellow map pin

    #define GPS_NPOS 2
    #define GPS_POS_SAMPS 64
	gps_pos_t POS_data[GPS_NPOS][GPS_POS_SAMPS];
	u4_t POS_seq, POS_next, POS_len, POS_seq_w, POS_seq_r;
	
    #define GPS_NMAP 3
    #define GPS_MAP_SAMPS 16
	gps_map_t MAP_data[GPS_NMAP][GPS_MAP_SAMPS];
	u4_t MAP_data_seq[GPS_MAP_SAMPS];
	u4_t MAP_next, MAP_len, MAP_seq_w, MAP_seq_r;
	
	int gps_gain, kick_lo_pll_ch;
	char a[32];
} gps_t;

extern gps_t gps;

extern const char *Week[];

struct UMS {
    int u, m;
    double fm, s;
    UMS(double x) {
        u = trunc(x); x = (x-u)*60; fm = x;
        m = trunc(x); s = (x-m)*60;
    }
};

void GPSstat(STAT st, double, int=0, int=0, int=0, int=0, double=0);

unsigned bin(char *s, int n);
void StatTask(void *param);
void GPSstat_init();
