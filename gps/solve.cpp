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

#include <array>
#include <memory.h>
#include <stdio.h>
#include <math.h>

#include "types.h"
#include "gps.h"
#include "clk.h"
#include "ephemeris.h"
#include "spi.h"
#include "timer.h"
#include "PosSolver.h"

#include <time.h>

#define MAX_ITER 20

// user-equivalen range error (m)
#define UERE 6.0

#define WGS84_A     (6378137.0)
#define WGS84_F_INV (298.257223563)
#define WGS84_B     (6356752.31424518)
#define WGS84_E2    (0.00669437999014132)

///////////////////////////////////////////////////////////////////////////////////////////////

struct SNAPSHOT {
    EPHEM eph;
    float power;
    int ch, sat, srq, ms;
    mutable int bits;
    int bits_tow, chips, cg_phase;
    bool isE1B;
    mutable bool tow_delayed;
    bool LoadAtomic(int ch, uint16_t *up, uint16_t *dn, int srq);
    double GetClock() const;
};

static SNAPSHOT Replicas[GPS_CHANS];
static u64_t ticks;

///////////////////////////////////////////////////////////////////////////////////////////////
// Gather channel data and consistent ephemerides

bool SNAPSHOT::LoadAtomic(int ch_, uint16_t *up, uint16_t *dn, int srq_) {

    /* Called inside atomic section - yielding not allowed */

    if (ChanSnapshot(
        ch_,        // in: channel id
        up[1],      // in: FPGA circular buffer pointer
        &sat,       // out: satellite id
        &bits,      // out: total bits held locally (CHANNEL struct) + remotely (FPGA)
        &bits_tow,  // out: bits since last valid TOW
        &power)     // out: received signal strength ^ 2
    && Ephemeris[sat].Valid()) {

        isE1B = is_E1B(sat);
        srq = srq_;
        ms = up[0];
        if (srq) ms += isE1B? 4:1;      // add one code period for un-serviced epochs
        chips = ((dn[0] & 0x3) << 10) | (dn[-1] & 0x3FF);
        //if (isE1B) printf("%s %d 0x%x = [0x%x,0x%x]\n", PRN(sat), chips, chips, dn[0], dn[-1]&0x3ff);
        cg_phase = dn[-1] >> 10;

        memcpy(&eph, Ephemeris+sat, sizeof eph);
        if (0) printf("%s copy TOW %d(%d) %s%d|%d bits %d bits_tow %d %.3f\n",
            PRN(sat), eph.tow/6, eph.tow, isE1B? "pg":"sf", eph.sub, eph.tow_pg, bits, bits_tow, (float) bits_tow/50);  //jks2
        return true;
    }
    else {
        if (0 && is_E1B(sat))
        printf("%s copy TOW %d(%d) %s%d|%d bits %d bits_tow %d %.3f NOT READY\n",
            PRN(sat), Ephemeris[sat].tow/6, Ephemeris[sat].tow, isE1B? "pg":"sf", Ephemeris[sat].tow_pg, Ephemeris[sat].sub,
            bits, bits_tow, (float) bits_tow/50);  //jks2
        return false; // channel not ready
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////

static int LoadAtomic() {

	// i.e. { ticks[47:0], srq[GPS_CHANS-1:0], { GPS_CHANS { clock_replica } } }
	// clock_replica = { ch_NAV_MS[15:0], ch_NAV_BITS[15:0], cg_phase_code[15:0] }
	// NB: cg_phase_code is in reverse GPS_CHANS order, hence use of "up" and "dn" logic below

    const int WPT = 3;      // words per ticks field
    const int WPS = 1;      // words per SRQ field
    const int WPC = 2+2;    // words per clock replica field

    SPI_MISO clocks;
    int chans=0;

    // Yielding to other tasks not allowed after spi_get_noduplex returns.
    // Why? Because below we need to snapshot the ephemerides state that match the just loaded clock replicas.
	spi_get_noduplex(CmdGetClocks, &clocks, S2B(WPT) + S2B(WPS) + S2B(GPS_CHANS*WPC));

    uint16_t srq = clocks.word[WPT+0];              // un-serviced epochs
    uint16_t *up = clocks.word+WPT+WPS;             // Embedded CPU memory containing ch_NAV_MS and ch_NAV_BITS
    uint16_t *dn = clocks.word+WPT+WPC*GPS_CHANS;   // FPGA clocks (in reverse order)

    // NB: see tools/ext64.c for why the (u64_t) casting is very important
    ticks = ((u64_t) clocks.word[0]<<32) | ((u64_t) clocks.word[1]<<16) | clocks.word[2];

    for (int ch=0; ch<gps_chans; ch++, srq>>=1, up+=WPC, dn-=WPC) {

        if (Replicas[chans].LoadAtomic(ch,up,dn,srq&1))
            Replicas[chans++].ch = ch;
    }

    // Safe to yield again ...
    return chans;
}

///////////////////////////////////////////////////////////////////////////////////////////////

static int LoadReplicas() {
    const int GLITCH_GUARD=500;
    SPI_MISO glitches[2];

    // Get glitch counters "before"
    spi_get(CmdGetGlitches, glitches+0, GPS_CHANS*2);
    TaskSleepMsec(GLITCH_GUARD);

    // Gather consistent snapshot of all channels
    int pass1 = LoadAtomic();
    int pass2 = 0;

    // Get glitch counters "after"
    TaskSleepMsec(GLITCH_GUARD);
    spi_get(CmdGetGlitches, glitches+1, GPS_CHANS*2);

    // Strip noisy channels
    bool include_E1B = admcfg_bool("include_E1B", NULL, CFG_REQUIRED);
    for (int i=0; i<pass1; i++) {
        int ch = Replicas[i].ch;
        if (glitches[0].word[ch] != glitches[1].word[ch]) continue;
        if (Replicas[i].isE1B && !include_E1B) continue;
        if (i>pass2) memcpy(Replicas+pass2, Replicas+i, sizeof(SNAPSHOT));
        pass2++;
    }

    return pass2;
}

///////////////////////////////////////////////////////////////////////////////////////////////

double SNAPSHOT::GetClock() const {

    // TOW refers to leading edge of next (un-processed) subframe.
    // Channel.cpp processes NAV data up to the subframe boundary.
    // Un-processed bits remain in holding buffers.
    // 15 nsec resolution due to inclusion of cg_phase.

    if (isE1B) {
        if (bits < 0 && bits > 500+MAX_NAV_BITS) {
            lprintf("GetClock %s tow=%d bits=%d ms=%d chips=0x%x cg_phase=%d\n",
                PRN(sat), eph.tow, bits, ms, chips, cg_phase);
            lprintf("E1B bits %d\n", bits);
            panic("E1B bits");
        }
        assert(ms == 0 || ms == 4);     // because ms == 4 for un-serviced epochs (see code above)
        assert(chips >= 0 && chips <= 4091);
        assert(cg_phase >= 0 && cg_phase <= 63);
    }
    
    //jks2
    if (0) {
        u4_t diff = timer_ms() - eph.tow_time;
        printf("%s clk  TOW %d(%d) %s%d|%d bits %d bits_tow %d diff %.3f %s\n",
            PRN(sat), eph.tow/6, eph.tow, isE1B? "pg":"sf", eph.sub, eph.tow_pg, bits, bits_tow, (float) diff/1e3,
            (bits != bits_tow)? "************************************************":"");
    }

    tow_delayed = false;
    #define MAX_TOW_DELAY   (5*500)
    if (bits != bits_tow && bits_tow < MAX_TOW_DELAY) {
        bits = bits_tow;
        tow_delayed = true;
    }

    // Un-corrected satellite clock
    double clock = isE1B?
                                        //                                      min    max         step (secs)
        (eph.tow +                      // Time of week in seconds (0...604794) 0      604794      2.000
        bits / E1B_BPS +                // NAV data bits buffered (0...500+)    0.000  2.000+      0.004 (250 Hz, ~1200km)
        ms * 1e-3   +                   // Un-serviced epoch adj (0 or 4)       0.000  0.004       0.004
        chips / CPS + 0.25/CPS +        // Code chips (0...4091)                0.000  0.003999    0.000000976 (~1 usec, ~300m)
        cg_phase * pow(2, -6) / CPS)    // Code NCO phase (0...63)              0.000  0.00000096  0.000000015 (15 nsec, ~4.5m)
    :
                                        //                                      min    max         step (secs)
        (eph.tow +                      // Time of week in seconds (0...604794) 0      604794      6.000
        bits / L1_BPS +                 // NAV data bits buffered (0...300+)    0.000  6.000+      0.020 (50 Hz)
        ms * 1e-3   +                   // Milliseconds since last bit (0...19) 0.000  0.019       0.001
        chips / CPS +                   // Code chips (0...1022)                0.000  0.000999    0.000000976 (~1 usec)
        cg_phase * pow(2, -6) / CPS);   // Code NCO phase (0...63)              0.000  0.00000096  0.000000015 (15 nsec)
    
    return clock;
}

// GNSSDataForEpoch holds all data for a position solution in a given epoch:
//  * satellite (X,Y,Z)             ...     sv(:,0:2)  [m]
//  * clock corrected time t*C      ...     sv(:,3)    [m]
//  * weight=signal power           ... weight(:)
//  * 48-bit ADC clock tick counter ... ticks          [ADC clock ticks]
class GNSSDataForEpoch {
public:
    typedef PosSolver::vec_type vec_type;
    typedef PosSolver::mat_type mat_type;
    typedef TNT::Array1D<int>  ivec_type;

    GNSSDataForEpoch(int max_channels)
        : _chans(0)
        , _sv(4, max_channels, 0.0)
        , _weight(max_channels, 0.0)
        , _sat(max_channels, 0)
        , _ch(max_channels, 0)
        , _prn(max_channels, 0)
        , _type(max_channels, 0)
        , _adc_ticks(0ULL) {}

    int       chans() const { return _chans; }
    mat_type     sv() const { return _chans ? _sv.subarray(0,3,0,_chans-1).copy() : PosSolver::mat_type(); }
    vec_type weight() const { return _chans ? _weight.subarray(0,_chans-1).copy() : PosSolver::vec_type(); }
    u64_t adc_ticks() const { return _adc_ticks; }
    int    prn(int i) const { return _prn[i]; }
    int    sat(int i) const { return _sat[i]; }
    int     ch(int i) const { return _ch[i]; }
    int   type(int i) const { return _type[i]; }

    u4_t ch_has_soln() const {
        u4_t bitmap = 0;
        for (int i=0; i<_chans; ++i)
            bitmap |= (1 << ch(i));
        return bitmap;
    }

    template<typename PRED>
    vec_type weight(PRED const& pred) const {
        if (!_chans)
            return PosSolver::vec_type();
        vec_type weight_filtered = weight().copy();
        int i_filtered=0;
        for (int i=0; i<_chans; ++i) {
            if (!pred(type(i)))
                continue;
            weight_filtered[i_filtered] = _weight[i];
            ++i_filtered;
        }
        if (!i_filtered)
            return PosSolver::vec_type();
        return weight_filtered.subarray(0,i_filtered-1).copy();
    }
    template<typename PRED>
    mat_type sv(PRED const& pred) const {
        if (!_chans)
            return PosSolver::mat_type();
        mat_type sv_filtered = sv().copy();
        int i_filtered=0;
        for (int i=0; i<_chans; ++i) {
            if (!pred(type(i)))
                continue;
            for (int j=0; j<4; ++j)
                sv_filtered[j][i_filtered] = _sv[j][i];
            ++i_filtered;
        }
        if (!i_filtered)
            return PosSolver::mat_type();
        return sv_filtered.subarray(0,3,0,i_filtered-1).copy();
    }

    bool LoadFromReplicas(int chans, const SNAPSHOT* replicas, u64_t adc_ticks) {
        clear();
        _adc_ticks = adc_ticks;
        _chans     = 0;
        for (int i=0; i<chans; ++i) {
            NextTask("solve1");

            // power of received signal
            _weight[_chans] = replicas[i].power;

            // remove satellites with unreasonable signal power
            if (_weight[_chans] < 1e5 || _weight[_chans] > 5e6)
                continue;

            // un-corrected time of transmission
            double t_tx = replicas[i].GetClock();
            if (t_tx == NAN)
                continue;

            // apply clock correction
            t_tx -= replicas[i].eph.GetClockCorrection(t_tx);
            _sv[3][_chans] = C*t_tx; // [s] -> [m]

            // get SV position in ECEF coords
            replicas[i].eph.GetXYZ(&_sv[0][_chans],
                                   &_sv[1][_chans],
                                   &_sv[2][_chans],
                                   t_tx);

            _sat[_chans]  = Replicas[i].sat;
            _ch[_chans]   = Replicas[i].ch;
            _prn[_chans]  = Sats[_sat[i]].prn;
            _type[_chans] = Sats[_sat[i]].type;
            _chans       += 1;
        }
        return (_chans > 0);
    }
protected:
    void clear() {
        _adc_ticks = 0;
        _chans     = 0;
        _weight    = 0;
        _sv        = 0;
        _weight    = 0;
        _sat       = 0;
        _ch        = 0;
        _prn       = 0;
        _type      = 0;
    }

private:
    int       _chans;     // number of good channels
    mat_type  _sv;        // sat. x,y,z,ct [m,m,m,m]
    vec_type  _weight;    // weights = sat. signal power
    ivec_type _sat;       // sat  number
    ivec_type _ch;        // channel
    ivec_type _prn;       // prn
    ivec_type _type;      // type
    u64_t     _adc_ticks; // ADC clock ticks
} ;

///////////////////////////////////////////////////////////////////////////////////////////////

// i.e. converts ECEF (WGS84) to/from ellipsoidal coordinates

static void ECEF_to_LatLonAlt(
    double x_ecef, double y_ecef, double z_ecef,  // m
    double *lat, double *lon, double *alt) {

    const double a  = WGS84_A;
    const double e2 = WGS84_E2;

    const double p = sqrt(x_ecef*x_ecef + y_ecef*y_ecef);

    *lon = 2.0 * atan2(y_ecef, x_ecef + p);
    *lat = atan(z_ecef / (p * (1.0 - e2)));
    *alt = 0.0;

    for (;;) {
        double tmp = *alt;
        double N = a / sqrt(1.0 - e2*pow(sin(*lat),2));
        *alt = p/cos(*lat) - N;
        *lat = atan(z_ecef / (p * (1.0 - e2*N/(N + *alt))));
        if (fabs(*alt-tmp)<1e-3) break;
    }
}

static void LatLonAlt_to_ECEF(
    double lat, double lon, double alt,
    double *x_ecef, double *y_ecef, double *z_ecef) { // m

    const double a  = WGS84_A;
    const double e2 = WGS84_E2;

    double slat = sin(lat);
    double clat = cos(lat);
    
    double N = a / sqrt(1 - e2 * slat*slat);
    
    *x_ecef = (N + alt) * clat * cos(lon);
    *y_ecef = (N + alt) * clat * sin(lon);
    *z_ecef = (N * (1 - e2) + alt) * slat;
}

///////////////////////////////////////////////////////////////////////////////////////////////

// fractional Julian days since year 2000
static double jdays2000(time_t utc_time) {
    static bool have_j2000_time;
    static time_t j2000_time;
    
    if (!have_j2000_time) {
        struct tm tm;
        memset(&tm, 0, sizeof (tm));
        tm.tm_isdst = 0;
        tm.tm_yday = 0;     // Jan 1
        tm.tm_wday = 6;     // Sat
        tm.tm_year = 100;
        tm.tm_mon = 0;      // Jan
        tm.tm_mday = 1;     // Jan 1
        tm.tm_hour = 12;    // noon
        tm.tm_min = 0;
        tm.tm_sec = 0;
        j2000_time = timegm(&tm);
        have_j2000_time = true;
    }
    
    return (double) (utc_time - j2000_time) / (24*3600);
}

// Greenwich mean sidereal time, in radians
static double gmt_sidereal_rad(time_t utc_time) {
    if (utc_time == 0) return 0;

    // As defined in the AIAA 2006 implementation:
    // http://www.celestrak.com/publications/AIAA/2006-6753/
    double ut1 = jdays2000(utc_time) / 36525.0;
    double ut2 = ut1*ut1;
    double ut3 = ut2*ut1;
    double theta_sec =
        67310.54841 +
        ut1 * (876600.0*3600.0 + 8640184.812866) +
        ut2 * 0.093104 +
        ut3 * -6.2e-6;
    double rad = fmod(DEG_2_RAD(theta_sec / 240.0), K_2PI);
    if (rad < 0) rad += K_2PI;  // quadrant correction
    //printf("GPS utc_time=%d jdays2000=%f ut1=%f gmt_sidereal_rad=%f\n", utc_time, jdays2000(utc_time), ut1, rad);
    return rad;
}

// Calculate observer ECI position
static void lat_lon_alt_to_ECI(
    time_t utc_time,
    double lon, double lat, double alt,     // alt (m)
    double *x, double *y, double *z         // ECI, km
    ) {

    // http://celestrak.com/columns/v02n03/
    double F = 1.0 / WGS84_F_INV;
    double A = M_2_KM(WGS84_A);     // rEarth (km)
    
    double theta = fmod(gmt_sidereal_rad(utc_time) + lon, K_2PI);
    double c = 1.0 / sqrt(1.0 + F * (F - 2) * pow(sin(lat), 2));
    double sq = c * pow((1.0 - F), 2);

    alt = M_2_KM(alt);      // km
    double achcp = (A * c + alt) * cos(lat);
    *x = achcp * cos(theta);    // km
    *y = achcp * sin(theta);
    *z = (A * sq + alt) * sin(lat);
}

// Calculate observers look angle to a satellite
static void ECI_pair_to_az_el(
    time_t utc_time,
    double pos_x, double pos_y, double pos_z,       // all positions ECI, km
    double kpos_x, double kpos_y, double kpos_z,
    double lon, double lat,     // rad
    double *az, double *el) {   // deg

    // http://celestrak.com/columns/v02n02/
    // utc_time: Observation time
    // x, y, z: ECI positions of satellite and observer
    // Return: (Azimuth, Elevation)

    double theta = fmod(gmt_sidereal_rad(utc_time) + lon, K_2PI);

    double rx = pos_x - kpos_x;
    double ry = pos_y - kpos_y;
    double rz = pos_z - kpos_z;

    double sin_lat = sin(lat);
    double cos_lat = cos(lat);
    double sin_theta = sin(theta);
    double cos_theta = cos(theta);

    double top_s =
        sin_lat * cos_theta*rx +
        sin_lat * sin_theta*ry -
        cos_lat * rz;
    double top_e =
        -sin_theta*rx +
        cos_theta*ry;
    double top_z =
        cos_lat * cos_theta*rx +
        cos_lat * sin_theta*ry +
        sin_lat * rz;

    top_s = (top_s == 0)? 1e-10 : top_s;
    double az_ = atan(-top_e / top_s);

    az_ = (top_s > 0)? az_ + K_PI : az_;
    az_ = (az_ < 0)? az_ + K_2PI : az_;
    double rg_ = sqrt(rx * rx + ry * ry + rz * rz);
    rg_ = (rg_ == 0)? 1e-10 : rg_;
    double el_ = asin(top_z / rg_);

    *az = RAD_2_DEG(az_);
    *el = RAD_2_DEG(el_);
}

///////////////////////////////////////////////////////////////////////////////////////////////

#define ALT_MAX 9000
#define ALT_MIN -100

static double x_sat_ecef[GPS_CHANS],
              y_sat_ecef[GPS_CHANS],
              z_sat_ecef[GPS_CHANS];

static double x_kiwi_ecef,
              y_kiwi_ecef,
              z_kiwi_ecef;

enum which_t { NO_E1B, USE_E1B, E1B_ONLY };

static int Solve2(int chans, int *nchans_meeting_criteria, which_t which_sats, int *num_E1B, int *num_non_E1B,
                  double *lat, double *lon, double *alt, double *p_t_rx, bool dump_data=false) {
    int i, iter, r, c;
    
    int use[GPS_CHANS];

    double t_rx;            // Corrected GPS time
    double t_tx[GPS_CHANS]; // Clock replicas in seconds since start of week

    double t_pc;    // Uncorrected system time when clock replica snapshots taken
    double t_bias;

    double dPR[GPS_CHANS]; // Pseudo range error

    double jac[GPS_CHANS][4], ma[4][4], mb[4][4], mc[4][GPS_CHANS], md[4];

    double weight[GPS_CHANS];

    x_kiwi_ecef = y_kiwi_ecef = z_kiwi_ecef = t_bias = t_pc = 0;
    
    int nchans = 0;
    *num_E1B = *num_non_E1B = 0;
    memset(use, 0, sizeof(use));
    
    if (dump_data)
        printf("GNSS_start %" PRIu64 "\n", ticks);

    for (i=0; i<chans; i++) {
        SNAPSHOT *r = &Replicas[i];
        NextTask("solve1");
        
        // filter channels based on satellite criteria
        if (r->isE1B) *num_E1B += 1; else *num_non_E1B += 1;
        if ( r->isE1B && which_sats == NO_E1B) continue;
        if (!r->isE1B && which_sats == E1B_ONLY) continue;
        use[i] = 1;
        nchans++;

        weight[i] = r->power;

        // Un-corrected time of transmission
        t_tx[i] = r->GetClock();
        if (t_tx[i] == NAN) {
            //jksp printf("Solve ##FAIL## %s t_tx == NAN\n", PRN(r->sat));
            *nchans_meeting_criteria = nchans;
            *p_t_rx = 0;
            return MAX_ITER;
        }
        
        // Clock correction
        double clockCorrection = r->eph.GetClockCorrection(t_tx[i]);
        t_tx[i] -= clockCorrection;

        // Get sat position in ECEF coords
        r->eph.GetXYZ(x_sat_ecef+i, y_sat_ecef+i, z_sat_ecef+i, t_tx[i]);

        int sat = r->sat;
        EPHEM *e = &Ephemeris[sat];

        // dump raw GNSS data
        if (dump_data)
            printf("GNSS_sat %s  %13.3f %13.3f %13.3f %16.9f %11.3f\n",
                   Sats[sat].prn_s, x_sat_ecef[i], y_sat_ecef[i], z_sat_ecef[i], t_tx[i], weight[i]);
        
        #if 0
        if (gps_debug < 0) printf("%s TOW%c%d(%d) t_tx %.6f %.6f cc %12.9f x %6.0f y %6.0f z %6.0f f0 %9.6f f1 %5.2g f2 %5.2g gd %5.2g W %.0f\n",
            PRN(r->sat), r->tow_delayed? '*':' ', r->eph.tow/6, r->eph.tow, t_tx[i]/6, t_tx[i]/6 - e->t_tx_prev/6, clockCorrection,
            M_2_KM(x_sat_ecef[i]), M_2_KM(y_sat_ecef[i]), M_2_KM(z_sat_ecef[i]),
            r->eph.a_f[0], r->eph.a_f[1], r->eph.a_f[2], r->eph.t_gd, weight[i]);
        #endif

        if (1) {
            double s_lat, s_lon, s_alt;
            ECEF_to_LatLonAlt(x_sat_ecef[i], y_sat_ecef[i], z_sat_ecef[i], &s_lat, &s_lon, &s_alt);
            double d_lat = s_lat - e->L_lat;
            double d_lon = s_lon - e->L_lon;
            #if 0
            printf("%s TOW%c%d(%d) t_tx %.6f(%.6f) lat %11.6f(%9.6f)(%9.6f) lon %11.6f(%9.6f)(%9.6f) alt %5.0f(%2.0f) srq%d\n",
                PRN(r->sat), r->tow_delayed? '*':' ', r->eph.tow/6, r->eph.tow, t_tx[i]/6, t_tx[i]/6 - e->t_tx_prev/6,
                RAD_2_DEG(s_lat), RAD_2_DEG(d_lat), RAD_2_DEG(d_lat - e->L_d_lat),
                RAD_2_DEG(s_lon), RAD_2_DEG(d_lon), RAD_2_DEG(d_lon - e->L_d_lon),
                M_2_KM(s_alt), M_2_KM(s_alt-e->L_alt), r->srq);
            #else
            if (0 /*jksp*/) printf("%s TOW%c%d(%d) t_tx %.6f(%.6f) lat %11.6f(%9.6f) lon %11.6f(%9.6f) alt %5.0f bits %d srq%d chips %d phase %d\n",
                PRN(r->sat), r->tow_delayed? '*':' ', r->eph.tow/6, r->eph.tow, t_tx[i]/6, t_tx[i]/6 - e->t_tx_prev/6,
                RAD_2_DEG(s_lat), RAD_2_DEG(d_lat),
                RAD_2_DEG(s_lon), RAD_2_DEG(d_lon),
                M_2_KM(s_alt),
                r->bits, r->srq, r->chips, r->cg_phase);
            #endif
            e->L_lat = s_lat;
            e->L_d_lat = d_lat;
            e->L_lon = s_lon;
            e->L_d_lon = d_lon;
            e->L_alt = s_alt;
        }
        e->t_tx_prev = t_tx[i];
        
        if (gps_debug < 0 /*|| r->isE1B*/) {
            double s_lat, s_lon, s_alt;
            ECEF_to_LatLonAlt(x_sat_ecef[i], y_sat_ecef[i], z_sat_ecef[i], &s_lat, &s_lon, &s_alt);
            printf("%s L/L  lat=%11.6f lon=%11.6f alt=%f\n",
                PRN(r->sat), RAD_2_DEG(s_lat), RAD_2_DEG(s_lon), M_2_KM(s_alt));
        }

        t_pc += t_tx[i];
    }
    if (dump_data)
        printf("GNSS_end\n");
    
    // need minimum number of satellites for a solution
    *nchans_meeting_criteria = nchans;
    if (nchans < 4) {
        *p_t_rx = 0;
        return MAX_ITER;
    }

    // Approximate starting value for receiver clock
    t_pc = t_pc / nchans + 75e-3;

    // Iterate to user xyzt solution using Taylor Series expansion:
    for (iter = 0; iter < MAX_ITER; iter++) {
        NextTask("solve2");

        t_rx = t_pc - t_bias;

        for (i=0; i<chans; i++) {
            if (!use[i]) continue;
            
            // Convert sat position to ECI coords (20.3.3.4.3.3.2)
            double theta = (t_tx[i] - t_rx) * OMEGA_E;
            //printf("GPS %d: ECI  t_tx[i]=%f t_rx=%f diff=%f theta=%f/%f/%f\n",
            //    i, t_tx[i], t_rx, t_tx[i] - t_rx, theta, sin(theta), cos(theta));

            double x_sat_eci = x_sat_ecef[i]*cos(theta) - y_sat_ecef[i]*sin(theta);
            double y_sat_eci = x_sat_ecef[i]*sin(theta) + y_sat_ecef[i]*cos(theta);
            double z_sat_eci = z_sat_ecef[i];

            // Geometric range (20.3.3.4.3.4)
            double gr = sqrt(pow(x_kiwi_ecef - x_sat_eci, 2) +
                             pow(y_kiwi_ecef - y_sat_eci, 2) +
                             pow(z_kiwi_ecef - z_sat_eci, 2));

            dPR[i] = C*(t_rx - t_tx[i]) - gr;

            jac[i][0] = (x_kiwi_ecef - x_sat_eci) / gr;
            jac[i][1] = (y_kiwi_ecef - y_sat_eci) / gr;
            jac[i][2] = (z_kiwi_ecef - z_sat_eci) / gr;
            jac[i][3] = C;
        }

        // ma = transpose(H) * W * H
        for (r=0; r<4; r++)
            for (c=0; c<4; c++) {
            ma[r][c] = 0;
            for (i=0; i<chans; i++) if (use[i]) ma[r][c] += jac[i][r]*weight[i]*jac[i][c];
        }

        double determinant =
            ma[0][3]*ma[1][2]*ma[2][1]*ma[3][0] - ma[0][2]*ma[1][3]*ma[2][1]*ma[3][0] - ma[0][3]*ma[1][1]*ma[2][2]*ma[3][0] + ma[0][1]*ma[1][3]*ma[2][2]*ma[3][0]+
            ma[0][2]*ma[1][1]*ma[2][3]*ma[3][0] - ma[0][1]*ma[1][2]*ma[2][3]*ma[3][0] - ma[0][3]*ma[1][2]*ma[2][0]*ma[3][1] + ma[0][2]*ma[1][3]*ma[2][0]*ma[3][1]+
            ma[0][3]*ma[1][0]*ma[2][2]*ma[3][1] - ma[0][0]*ma[1][3]*ma[2][2]*ma[3][1] - ma[0][2]*ma[1][0]*ma[2][3]*ma[3][1] + ma[0][0]*ma[1][2]*ma[2][3]*ma[3][1]+
            ma[0][3]*ma[1][1]*ma[2][0]*ma[3][2] - ma[0][1]*ma[1][3]*ma[2][0]*ma[3][2] - ma[0][3]*ma[1][0]*ma[2][1]*ma[3][2] + ma[0][0]*ma[1][3]*ma[2][1]*ma[3][2]+
            ma[0][1]*ma[1][0]*ma[2][3]*ma[3][2] - ma[0][0]*ma[1][1]*ma[2][3]*ma[3][2] - ma[0][2]*ma[1][1]*ma[2][0]*ma[3][3] + ma[0][1]*ma[1][2]*ma[2][0]*ma[3][3]+
            ma[0][2]*ma[1][0]*ma[2][1]*ma[3][3] - ma[0][0]*ma[1][2]*ma[2][1]*ma[3][3] - ma[0][1]*ma[1][0]*ma[2][2]*ma[3][3] + ma[0][0]*ma[1][1]*ma[2][2]*ma[3][3];

        // mb = inverse(ma) = inverse(transpose(H)*W*H)
        mb[0][0] = (ma[1][2]*ma[2][3]*ma[3][1] - ma[1][3]*ma[2][2]*ma[3][1] + ma[1][3]*ma[2][1]*ma[3][2] - ma[1][1]*ma[2][3]*ma[3][2] - ma[1][2]*ma[2][1]*ma[3][3] + ma[1][1]*ma[2][2]*ma[3][3]) / determinant;
        mb[0][1] = (ma[0][3]*ma[2][2]*ma[3][1] - ma[0][2]*ma[2][3]*ma[3][1] - ma[0][3]*ma[2][1]*ma[3][2] + ma[0][1]*ma[2][3]*ma[3][2] + ma[0][2]*ma[2][1]*ma[3][3] - ma[0][1]*ma[2][2]*ma[3][3]) / determinant;
        mb[0][2] = (ma[0][2]*ma[1][3]*ma[3][1] - ma[0][3]*ma[1][2]*ma[3][1] + ma[0][3]*ma[1][1]*ma[3][2] - ma[0][1]*ma[1][3]*ma[3][2] - ma[0][2]*ma[1][1]*ma[3][3] + ma[0][1]*ma[1][2]*ma[3][3]) / determinant;
        mb[0][3] = (ma[0][3]*ma[1][2]*ma[2][1] - ma[0][2]*ma[1][3]*ma[2][1] - ma[0][3]*ma[1][1]*ma[2][2] + ma[0][1]*ma[1][3]*ma[2][2] + ma[0][2]*ma[1][1]*ma[2][3] - ma[0][1]*ma[1][2]*ma[2][3]) / determinant;
        mb[1][0] = (ma[1][3]*ma[2][2]*ma[3][0] - ma[1][2]*ma[2][3]*ma[3][0] - ma[1][3]*ma[2][0]*ma[3][2] + ma[1][0]*ma[2][3]*ma[3][2] + ma[1][2]*ma[2][0]*ma[3][3] - ma[1][0]*ma[2][2]*ma[3][3]) / determinant;
        mb[1][1] = (ma[0][2]*ma[2][3]*ma[3][0] - ma[0][3]*ma[2][2]*ma[3][0] + ma[0][3]*ma[2][0]*ma[3][2] - ma[0][0]*ma[2][3]*ma[3][2] - ma[0][2]*ma[2][0]*ma[3][3] + ma[0][0]*ma[2][2]*ma[3][3]) / determinant;
        mb[1][2] = (ma[0][3]*ma[1][2]*ma[3][0] - ma[0][2]*ma[1][3]*ma[3][0] - ma[0][3]*ma[1][0]*ma[3][2] + ma[0][0]*ma[1][3]*ma[3][2] + ma[0][2]*ma[1][0]*ma[3][3] - ma[0][0]*ma[1][2]*ma[3][3]) / determinant;
        mb[1][3] = (ma[0][2]*ma[1][3]*ma[2][0] - ma[0][3]*ma[1][2]*ma[2][0] + ma[0][3]*ma[1][0]*ma[2][2] - ma[0][0]*ma[1][3]*ma[2][2] - ma[0][2]*ma[1][0]*ma[2][3] + ma[0][0]*ma[1][2]*ma[2][3]) / determinant;
        mb[2][0] = (ma[1][1]*ma[2][3]*ma[3][0] - ma[1][3]*ma[2][1]*ma[3][0] + ma[1][3]*ma[2][0]*ma[3][1] - ma[1][0]*ma[2][3]*ma[3][1] - ma[1][1]*ma[2][0]*ma[3][3] + ma[1][0]*ma[2][1]*ma[3][3]) / determinant;
        mb[2][1] = (ma[0][3]*ma[2][1]*ma[3][0] - ma[0][1]*ma[2][3]*ma[3][0] - ma[0][3]*ma[2][0]*ma[3][1] + ma[0][0]*ma[2][3]*ma[3][1] + ma[0][1]*ma[2][0]*ma[3][3] - ma[0][0]*ma[2][1]*ma[3][3]) / determinant;
        mb[2][2] = (ma[0][1]*ma[1][3]*ma[3][0] - ma[0][3]*ma[1][1]*ma[3][0] + ma[0][3]*ma[1][0]*ma[3][1] - ma[0][0]*ma[1][3]*ma[3][1] - ma[0][1]*ma[1][0]*ma[3][3] + ma[0][0]*ma[1][1]*ma[3][3]) / determinant;
        mb[2][3] = (ma[0][3]*ma[1][1]*ma[2][0] - ma[0][1]*ma[1][3]*ma[2][0] - ma[0][3]*ma[1][0]*ma[2][1] + ma[0][0]*ma[1][3]*ma[2][1] + ma[0][1]*ma[1][0]*ma[2][3] - ma[0][0]*ma[1][1]*ma[2][3]) / determinant;
        mb[3][0] = (ma[1][2]*ma[2][1]*ma[3][0] - ma[1][1]*ma[2][2]*ma[3][0] - ma[1][2]*ma[2][0]*ma[3][1] + ma[1][0]*ma[2][2]*ma[3][1] + ma[1][1]*ma[2][0]*ma[3][2] - ma[1][0]*ma[2][1]*ma[3][2]) / determinant;
        mb[3][1] = (ma[0][1]*ma[2][2]*ma[3][0] - ma[0][2]*ma[2][1]*ma[3][0] + ma[0][2]*ma[2][0]*ma[3][1] - ma[0][0]*ma[2][2]*ma[3][1] - ma[0][1]*ma[2][0]*ma[3][2] + ma[0][0]*ma[2][1]*ma[3][2]) / determinant;
        mb[3][2] = (ma[0][2]*ma[1][1]*ma[3][0] - ma[0][1]*ma[1][2]*ma[3][0] - ma[0][2]*ma[1][0]*ma[3][1] + ma[0][0]*ma[1][2]*ma[3][1] + ma[0][1]*ma[1][0]*ma[3][2] - ma[0][0]*ma[1][1]*ma[3][2]) / determinant;
        mb[3][3] = (ma[0][1]*ma[1][2]*ma[2][0] - ma[0][2]*ma[1][1]*ma[2][0] + ma[0][2]*ma[1][0]*ma[2][1] - ma[0][0]*ma[1][2]*ma[2][1] - ma[0][1]*ma[1][0]*ma[2][2] + ma[0][0]*ma[1][1]*ma[2][2]) / determinant;

        // mc = inverse(transpose(H)*W*H) * transpose(H)
        for (r=0; r<4; r++)
            for (c=0; c<chans; c++) {
                if (!use[c]) continue;
                mc[r][c] = 0;
                for (i=0; i<4; i++) mc[r][c] += mb[r][i]*jac[c][i];
        }

        // md = inverse(transpose(H)*W*H) * transpose(H) * W * dPR
        for (r=0; r<4; r++) {
            md[r] = 0;
            for (i=0; i<chans; i++) if (use[i]) md[r] += mc[r][i]*weight[i]*dPR[i];
        }

        double dx = md[0];
        double dy = md[1];
        double dz = md[2];
        double dt = md[3];

        double err_mag = sqrt(dx*dx + dy*dy + dz*dz);

        // printf("%14g%14g%14g%14g%14g\n", err_mag, t_bias, x_kiwi_ecef, y_kiwi_ecef, z_kiwi_ecef);

        if (err_mag<1.0) break;

        x_kiwi_ecef += dx;
        y_kiwi_ecef += dy;
        z_kiwi_ecef += dz;
        t_bias   += dt;
    }
    
    *p_t_rx = t_rx;
    return iter;
}

enum result_t { SOLN, ITER_OR_ALT, TOO_FEW_SATS };

// called even when chans < 4 (no solution possible) so az/el and map can be updated
// from sat clock and (previously stored) reference lat/lon

static result_t Solve(int chans, double *lat, double *lon, double *alt) {
    int i, useable_chans, iter;
    int num_E1B, num_non_E1B;
    result_t result = SOLN;
    double t_rx;
    
    gps_pos_t *pos = &gps.POS_data[MAP_WITH_E1B][gps.POS_next];
    pos->x = pos->y = pos->lat = pos->lon = 0;

    gps_map_t *map = &gps.MAP_data[MAP_WITH_E1B][gps.MAP_next];
    map->lat = map->lon = 0;
    map = &gps.MAP_data[MAP_ONLY_E1B][gps.MAP_next];
    map->lat = map->lon = 0;

    iter = Solve2(chans, &useable_chans, USE_E1B, &num_E1B, &num_non_E1B, lat, lon, alt, &t_rx, true);
    
    // if enough good sats compute new Kiwi lat/lon and do clock correction
	if (useable_chans >= 4) {
	    if (iter == MAX_ITER || t_rx == 0) {
	        //jksp printf("Solve ##FAIL## MAX_ITER %d t_rx == 0 %d\n", iter == MAX_ITER, t_rx == 0);
	        result = ITER_OR_ALT;
	    } else {
            GPSstat(STAT_TIME, t_rx);
            clock_correction(t_rx, ticks);
            tod_correction();

            ECEF_to_LatLonAlt(x_kiwi_ecef, y_kiwi_ecef, z_kiwi_ecef, lat, lon, alt);
            if (*alt > ALT_MAX || *alt < ALT_MIN) {
                //jksp printf("Solve ##FAIL## alt %.1f\n", *alt);
                result = ITER_OR_ALT;
            } else
            if (gps.have_ref_lla) {
                bool E1B_plot_separately = admcfg_bool("plot_E1B", NULL, CFG_REQUIRED);
                gps.E1B_plot_separately = E1B_plot_separately;
                int which_map = E1B_plot_separately? MAP_WITH_E1B : MAP_ALL;
            
                pos = &gps.POS_data[which_map][gps.POS_next];
                pos->x = y_kiwi_ecef;       // NB: swapped
                pos->y = x_kiwi_ecef;
                pos->lat = RAD_2_DEG(*lat);
                pos->lon = RAD_2_DEG(*lon);

                map = &gps.MAP_data[which_map][gps.MAP_next];
                map->lat = RAD_2_DEG(*lat);
                map->lon = RAD_2_DEG(*lon);
                gps.MAP_seq_w++;
                gps.MAP_data_seq[gps.MAP_next] = gps.MAP_seq_w;

                if (E1B_plot_separately) {
                    int t_iter, t_useable_chans, dummy;
                    double t_lat, t_lon, t_alt, t_t_rx;
                
                    if (num_non_E1B >= 4) {
                        t_iter = Solve2(chans, &t_useable_chans, NO_E1B, &dummy, &dummy, &t_lat, &t_lon, &t_alt, &t_t_rx);
                        if (t_useable_chans >= 4 && t_iter < MAX_ITER && t_t_rx != 0) {
                            ECEF_to_LatLonAlt(x_kiwi_ecef, y_kiwi_ecef, z_kiwi_ecef, &t_lat, &t_lon, &t_alt);
                            if (t_alt < ALT_MAX && t_alt > ALT_MIN) {
                                pos = &gps.POS_data[MAP_WITHOUT_E1B][gps.POS_next];
                                pos->x = y_kiwi_ecef;       // NB: swapped
                                pos->y = x_kiwi_ecef;
                                pos->lat = RAD_2_DEG(t_lat);
                                pos->lon = RAD_2_DEG(t_lon);
    
                                map = &gps.MAP_data[MAP_WITHOUT_E1B][gps.MAP_next];
                                map->lat = RAD_2_DEG(t_lat);
                                map->lon = RAD_2_DEG(t_lon);
                            }
                        }
                    }
                    
                    if (num_E1B >= 4) {
                        t_iter = Solve2(chans, &t_useable_chans, E1B_ONLY, &dummy, &dummy, &t_lat, &t_lon, &t_alt, &t_t_rx);
                        if (t_useable_chans >= 4 && t_iter < MAX_ITER && t_t_rx != 0) {
                            ECEF_to_LatLonAlt(x_kiwi_ecef, y_kiwi_ecef, z_kiwi_ecef, &t_lat, &t_lon, &t_alt);
                            if (t_alt < ALT_MAX && t_alt > ALT_MIN) {
                                map = &gps.MAP_data[MAP_ONLY_E1B][gps.MAP_next];
                                map->lat = RAD_2_DEG(t_lat);
                                map->lon = RAD_2_DEG(t_lon);
                            }
                        }
                    }
                }

                gps.POS_next++;
                if (gps.POS_next >= GPS_POS_SAMPS) gps.POS_next = 0;
                if (gps.POS_len < GPS_POS_SAMPS) gps.POS_len++;
                gps.POS_seq_w++;

                gps.MAP_next++;
                if (gps.MAP_next >= GPS_MAP_SAMPS) gps.MAP_next = 0;
                if (gps.MAP_len < GPS_MAP_SAMPS) gps.MAP_len++;
            }
        }
    } else {
        result = TOO_FEW_SATS;
    }
    
    if (!gps.have_ref_lla && result == SOLN && gps.fixes >= 3) {
        gps.ref_lat = RAD_2_DEG(*lat);
        gps.ref_lon = RAD_2_DEG(*lon);
        gps.have_ref_lla = true;
    }
    
    u4_t ch_has_soln = 0, e1b_word;
    bool soln_uses_E1B = false;
    for (i=0; i<chans; i++) {
        ch_has_soln |= 1 << Replicas[i].ch;
        int sat = Replicas[i].sat;
        if (is_E1B(sat)) {
            soln_uses_E1B = true;
            e1b_word = Ephemeris[sat].sub;
        }
    }
    
    int grn_yel_red = (result == SOLN)? 0 : ((result == TOO_FEW_SATS)? 1:2);
    GPSstat(STAT_SOLN, 0, grn_yel_red, ch_has_soln);

    #if 0
    static const char *result_s[] = { "GOOD/GRN", "ITER/RED", "#SAT/YEL" };
    double t_alt = *alt;
    if (t_alt > ALT_MAX || t_alt < ALT_MIN) t_alt = 9999;
    printf("%s ch= 0x%03x(%d) lat= %9.4f lon= %9.4f alt= %4.0f fixes= %4d iter= %2d t_rx= %.1f\n",
        result_s[result], ch_has_soln, useable_chans, RAD_2_DEG(*lat), RAD_2_DEG(*lon), t_alt, gps.fixes, iter, t_rx);
    #endif

    if (result == ITER_OR_ALT || (*lat == 0 && *lon == 0)) return result;  // no solution or no lat/lon yet

    // ECI depends on current time so can't cache like lat/lon
    time_t now = utc_time();
    double kpos_x, kpos_y, kpos_z;
    lat_lon_alt_to_ECI(now, *lon, *lat, *alt, &kpos_x, &kpos_y, &kpos_z);

    #if 0
    if (useable_chans >= 4) {
        printf("Solve GOOD soln %d fixes %d\n", useable_chans, gps.fixes);
        double lat_deg = RAD_2_DEG(*lat);
        double lon_deg = RAD_2_DEG(*lon);
        printf("kiwi ECI  x=%10.3f y=%10.3f z=%10.3f wikimapia.org/#lang=en&lat=%9.6f&lon=%9.6f&z=18&m=b alt=%4.0f | %5d %5d %s\n",
            kpos_x, kpos_y, kpos_z, lat_deg, lon_deg, *alt, (int) ((gps.ref_lat - lat_deg)*1e6), (int) ((gps.ref_lon - lon_deg)*1e6),
            soln_uses_E1B? stprintf("E1B W%d", e1b_word) : "");
        //printf("kiwi ECEF x=%10.3f y=%10.3f z=%10.3f\n", M_2_KM(x_kiwi_ecef), M_2_KM(y_kiwi_ecef), M_2_KM(z_kiwi_ecef));
    }
    #endif

    // update sat az/el even if not enough good sats to compute new Kiwi lat/lon
    // (Kiwi is not moving so use last computed lat/lon)
    for (i=0; i<chans; i++) {
        NextTask("solve3");
        int sat = Replicas[i].sat;
        
        // already have az/el for this sat in this sample period?
        if (gps.el[gps.last_samp][sat]) continue;
        
        if (0 && is_E1B(sat))
        printf("%s ECEF x=%10.3f y=%10.3f z=%10.3f\n",
            PRN(sat), M_2_KM(x_sat_ecef[i]), M_2_KM(y_sat_ecef[i]), M_2_KM(z_sat_ecef[i]));
        double az_f, el_f;
        double spos_x, spos_y, spos_z;
        double s_lat, s_lon, s_alt;
        ECEF_to_LatLonAlt(x_sat_ecef[i], y_sat_ecef[i], z_sat_ecef[i], &s_lat, &s_lon, &s_alt);
        if (0 && is_E1B(sat))
        printf("%s L/L  lat=%11.6f lon=%11.6f alt=%f\n",
            PRN(sat), RAD_2_DEG(s_lat), RAD_2_DEG(s_lon), M_2_KM(s_alt));
        lat_lon_alt_to_ECI(now, s_lon, s_lat, s_alt, &spos_x, &spos_y, &spos_z);

        ECI_pair_to_az_el(now, spos_x, spos_y, spos_z, kpos_x, kpos_y, kpos_z, *lon, *lat, &az_f, &el_f);
        int az = round(az_f);
        int el = round(el_f);
        if ((0 && is_E1B(sat)) || true)
            printf("%s ECI  x=%10.3f y=%10.3f z=%10.3f EL/AZ=%2d %3d\n",
                   PRN(sat), spos_x, spos_y, spos_z, el, az);

        //real_printf("%s EL/AZ=%2d %3d samp=%d\n", PRN(sat), el, az, gps.last_samp);
        if (az < 0 || az >= 360 || el <= 0 || el > 90) continue;
        gps.az[gps.last_samp][sat] = az;
        gps.el[gps.last_samp][sat] = el;
        
        gps.shadow_map[az] |= 1 << (int) round(el/90.0*31.0);
        
        // add az/el to channel data
        for (int ch = 0; ch < GPS_CHANS; ch++) {
            gps_chan_t *chp = &gps.ch[ch];
            if (chp->sat == sat) {
                chp->az = az;
                chp->el = el;
            }
        }
    }
    
    #define QZS_3_LAT   0.0
    #define QZS_3_LON   126.95
    #define QZS_3_ALT   35783.2
    
    // don't use first lat/lon which is often wrong
    if (gps.qzs_3.el <= 0 && gps.fixes >= 3 && gps.fixes <= 5) {
        double q_az, q_el;
        double qpos_x, qpos_y, qpos_z;

        lat_lon_alt_to_ECI(now, DEG_2_RAD(QZS_3_LON), QZS_3_LAT, KM_2_M(QZS_3_ALT), &qpos_x, &qpos_y, &qpos_z);
        ECI_pair_to_az_el(now, qpos_x, qpos_y, qpos_z, kpos_x, kpos_y, kpos_z, *lon, *lat, &q_az, &q_el);
        int az = round(q_az);
        int el = round(q_el);
        if (!(az < 0 || az >= 360 || el <= 0 || el > 90)) {
            gps.qzs_3.az = az;
            gps.qzs_3.el = el;
            printf("QZS-3 az=%d el=%d\n", az, el);
        }
    }
	
    return result;
}

void update_gps_info_before()
{
    int samp_hour=0, samp_min=0;
    utc_hour_min_sec(&samp_hour, &samp_min, NULL);

    if (gps.last_samp_hour != samp_hour) {
        gps.fixes_hour = gps.fixes_hour_incr;
        gps.fixes_hour_incr = 0;
        gps.fixes_hour_samples++;
        gps.last_samp_hour = samp_hour;
    }
        
    //printf("GPS last_samp=%d samp_min=%d fixes_min=%d\n", gps.last_samp, samp_min, gps.fixes_min);
    if (gps.last_samp != samp_min) {
        //printf("GPS fixes_min=%d fixes_min_incr=%d\n", gps.fixes_min, gps.fixes_min_incr);
        gps.fixes_min = gps.fixes_min_incr;
        gps.fixes_min_incr = 0;
        for (int sat = 0; sat < MAX_SATS; sat++) {
            gps.az[gps.last_samp][sat] = 0;
            gps.el[gps.last_samp][sat] = 0;
        }
        gps.last_samp = samp_min;
    }
}

void update_gps_info_after(GNSSDataForEpoch const& gnssDataForEpoch,
                           std::array<PosSolver::sptr, 3> const& pos_solvers,
                           bool plot_E1B)
{
    gps_pos_t *pos = &gps.POS_data[MAP_WITH_E1B][gps.POS_next];
    pos->x = pos->y = pos->lat = pos->lon = 0;

    gps_map_t *map = &gps.MAP_data[MAP_WITH_E1B][gps.MAP_next];
    map->lat = map->lon = 0;
    map = &gps.MAP_data[MAP_ONLY_E1B][gps.MAP_next];
    map->lat = map->lon = 0;

    if (pos_solvers[0]->ekf_valid() || pos_solvers[0]->spp_valid()) { // solution using all satellites
        
        GPSstat(STAT_TIME, pos_solvers[0]->t_rx());
        clock_correction(pos_solvers[0]->t_rx(), gnssDataForEpoch.adc_ticks()); // TODO
        tod_correction();

        const PosSolver::LonLatAlt llh = pos_solvers[0]->llh();

        if (gps.have_ref_lla) {
            gps.E1B_plot_separately = plot_E1B;
            const int which_map = (plot_E1B ? MAP_WITH_E1B : MAP_ALL);

            pos = &gps.POS_data[which_map][gps.POS_next];
            pos->x = pos_solvers[0]->pos()(1);       // NB: swapped
            pos->y = pos_solvers[0]->pos()(0);
            pos->lat = llh.lat();
            pos->lon = llh.lon();

            map = &gps.MAP_data[which_map][gps.MAP_next];
            map->lat = llh.lat();
            map->lon = llh.lon();
            gps.MAP_seq_w++;
            gps.MAP_data_seq[gps.MAP_next] = gps.MAP_seq_w;

            if (gps.E1B_plot_separately) {
                if (pos_solvers[1]->ekf_valid() || pos_solvers[1]->spp_valid()) { // not Galileo
                    const PosSolver::LonLatAlt llh1 = pos_solvers[1]->llh();
                    pos = &gps.POS_data[MAP_WITHOUT_E1B][gps.POS_next];
                    pos->x = pos_solvers[1]->pos()(1);       // NB: swapped
                    pos->y = pos_solvers[1]->pos()(0);
                    pos->lat = llh1.lat();
                    pos->lon = llh1.lon();
                    
                    map = &gps.MAP_data[MAP_WITHOUT_E1B][gps.MAP_next];
                    map->lat = llh1.lat();
                    map->lon = llh1.lon();
                }
                if (pos_solvers[2]->ekf_valid() || pos_solvers[2]->spp_valid()) { // only Galileo
                    const PosSolver::LonLatAlt llh2 = pos_solvers[2]->llh();
                    map = &gps.MAP_data[MAP_ONLY_E1B][gps.MAP_next];
                    map->lat = llh2.lat();
                    map->lon = llh2.lon();
                }
            } // gps.E1B_plot_separately

            gps.POS_next++;
            if (gps.POS_next >= GPS_POS_SAMPS) gps.POS_next = 0;
            if (gps.POS_len < GPS_POS_SAMPS) gps.POS_len++;
            gps.POS_seq_w++;
            
            gps.MAP_next++;
            if (gps.MAP_next >= GPS_MAP_SAMPS) gps.MAP_next = 0;
            if (gps.MAP_len < GPS_MAP_SAMPS) gps.MAP_len++;
        } // gps.have_ref_lla

        if (!gps.have_ref_lla && gps.fixes >= 3) {
            gps.ref_lat = llh.lat();
            gps.ref_lon = llh.lon();
            gps.have_ref_lla = true;
        }

    } // solution with all satellites found

    // green  -> EKF
    // yellow -> SPP
    // red    -> no position solution
    const int grn_yel_red = (pos_solvers[0]->ekf_valid() ? 0 : (pos_solvers[0]->spp_valid() ? 1 : 2));
    GPSstat(STAT_SOLN, 0, grn_yel_red, gnssDataForEpoch.ch_has_soln());

    // update az/el
    const auto elev_azim = pos_solvers[0]->elev_azim(gnssDataForEpoch.sv());
    if (pos_solvers[0]->ekf_valid() || pos_solvers[0]->spp_valid()) {
        for (int i=0; i<gnssDataForEpoch.chans(); ++i) {
            NextTask("solve3");
            const int sat = gnssDataForEpoch.sat(i);

            // already have az/el for this sat in this sample period? 
            if (gps.el[gps.last_samp][sat])
                continue;

            const int el = std::round(elev_azim[i].elev_deg);
            const int az = std::round(elev_azim[i].azim_deg);

            // printf("%s NEW EL/AZ=%2d %3d\n", PRN(sat), el, az);
            if (az < 0 || az >= 360 || el <= 0 || el > 90)
                continue;

            gps.az[gps.last_samp][sat] = az;
            gps.el[gps.last_samp][sat] = el;

            gps.shadow_map[az] |= (1 << int(std::round(el / 90.0 * 31.0)));

            // add az/el to channel data
            for (int ch=0; ch<GPS_CHANS; ++ch) {
                gps_chan_t *chp = &gps.ch[ch];
                if (chp->sat == sat) {
                    chp->az = az;
                    chp->el = el;
                }
            }
            // special treatment for QZS_3 
            if (gnssDataForEpoch.prn(i)  == 199 && gnssDataForEpoch.type(i) == QZSS) {
                gps.qzs_3.az = az;
                gps.qzs_3.el = el;
                printf("QZS-3 az=%d el=%d\n", az, el);
            }
            
        } // next satellite

        gps.fixes++; gps.fixes_min_incr++; gps.fixes_hour_incr++;
        
        // at startup immediately indicate first solution
        if (gps.fixes_min == 0) gps.fixes_min++;

        // at startup incrementally update until first hour sample period has ended
        if (gps.fixes_hour_samples <= 1) gps.fixes_hour++;
        
        const PosSolver::LonLatAlt llh = pos_solvers[0]->llh();
        GPSstat(STAT_LAT, llh.lat());
        GPSstat(STAT_LON, llh.lon());
        GPSstat(STAT_ALT, llh.alt());  
    }
}


// Used by the position solver classes, see kiwi_yield.h
class kiwi_next_task : public kiwi_yield {
public:
    kiwi_next_task() {}
    virtual ~kiwi_next_task() {}

    virtual void yield() const {
        NextTask("PosSolver yield");
    }
} ;

void SolveTask(void *param) {
    GNSSDataForEpoch gnssDataForEpoch(GPS_CHANS);
    auto yield = std::make_shared<kiwi_next_task>();

    std::array<PosSolver::sptr, 3> posSolvers = {
        PosSolver::make(UERE, ADC_CLOCK_TYP, yield), // all satellites
        PosSolver::make(UERE, ADC_CLOCK_TYP, yield), // not Galileo
        PosSolver::make(UERE, ADC_CLOCK_TYP, yield)  // only Galileo
    };

    auto const predNotGalileo  = [](int type) { return (type != E1B); };
    auto const predOnlyGalileo = [](int type) { return (type == E1B); };

    double lat=0, lon=0, alt;
    int good = -1;

    for (;;) {
    
        // while we're waiting send IQ values if requested
        u4_t now = timer_ms();
            int ch = gps.IQ_data_ch - 1;
            if (ch != -1) {
                spi_set(CmdIQLogReset, ch);
                //printf("SOLVE CmdIQLogReset ch=%d\n", ch);
                //TaskSleepMsec(1024 + 100);
                TaskSleepMsec(900);     //jks2
                static SPI_MISO rx;
                spi_get(CmdIQLogGet, &rx, S2B(GPS_IQ_SAMPS_W));
                memcpy(gps.IQ_data, rx.word, S2B(GPS_IQ_SAMPS_W));
               // printf("gps.IQ_data %d rx.word %d S2B(GPS_IQ_SAMPS_W) %d\n", \
                    sizeof(gps.IQ_data), sizeof(rx.word), S2B(GPS_IQ_SAMPS_W));
                gps.IQ_seq_w++;
                
                #if 0
                    int i;
                    #if 1
                        printf("I ");
                        for (i=0; i < 16; i++)
                            printf("%d|%d ", (s2_t) rx.word[i*4], (s2_t) rx.word[i*4+1]);
                        printf("\n");
                        printf("Q ");
                        for (i=0; i < 16; i++)
                            printf("%d|%d ", (s2_t) rx.word[i*4+2], (s2_t) rx.word[i*4+3]);
                        printf("\n");
                    #else
                        printf("I ");
                        for (i=0; i < GPS_IQ_SAMPS; i++) {
                            printf("%d|%d ", i, (s2_t) rx.word[i*2]);
                            if ((i % 8) == 7)
                                printf("\n");
                        }
                        printf("\n");
                    #endif
                #endif
            }
        //#define SOLVE_RATE  (4-1)   // 1 is GLITCH_GUARD*2
        #define SOLVE_RATE  (2-1)   // 1 is GLITCH_GUARD*2      jks2
        u4_t elapsed = timer_ms() - now;
        u4_t remaining = SEC_TO_MSEC(SOLVE_RATE) - elapsed;
        if (elapsed < SEC_TO_MSEC(SOLVE_RATE)) {
            //printf("ch=%d%s remaining=%d\n", ch+1, (ch+1 == 0)? "(off)":"", remaining);
		    TaskSleepMsec(remaining);
		}

        good = LoadReplicas();

        update_gps_info_before();
#if 0
        int samp_hour, samp_min;
        utc_hour_min_sec(&samp_hour, &samp_min, NULL);

        if (gps.last_samp_hour != samp_hour) {
            gps.fixes_hour = gps.fixes_hour_incr;
            gps.fixes_hour_incr = 0;
            gps.fixes_hour_samples++;
            gps.last_samp_hour = samp_hour;
        }
        
        //printf("GPS last_samp=%d samp_min=%d fixes_min=%d\n", gps.last_samp, samp_min, gps.fixes_min);
        if (gps.last_samp != samp_min) {
            //printf("GPS fixes_min=%d fixes_min_incr=%d\n", gps.fixes_min, gps.fixes_min_incr);
            gps.fixes_min = gps.fixes_min_incr;
            gps.fixes_min_incr = 0;
            for (int sat = 0; sat < MAX_SATS; sat++) {
                gps.az[gps.last_samp][sat] = 0;
                gps.el[gps.last_samp][sat] = 0;
            }
            gps.last_samp = samp_min;
        }
#endif
        // this needs to replaced, see (*)
        gps.good = good;
        bool enable = SearchTaskRun();
        if (!enable || good == 0) continue;

        const bool plot_E1B   = admcfg_bool("plot_E1B", NULL, CFG_REQUIRED);
        const bool use_kalman = admcfg_default_bool("use_kalman_position_solver", true, NULL);
        for (auto& p : posSolvers)
            p->set_use_kalman(use_kalman);

        if (gnssDataForEpoch.LoadFromReplicas(good, Replicas, ticks)) {
            // new code (*)
            posSolvers[0]->solve(gnssDataForEpoch.sv(),
                                 gnssDataForEpoch.weight(),
                                 gnssDataForEpoch.adc_ticks());

            if (plot_E1B) {
                // make separate position solutions for Galileo and ~Galileo stats
                posSolvers[1]->solve(gnssDataForEpoch.sv(predNotGalileo),
                                     gnssDataForEpoch.weight(predNotGalileo),
                                     gnssDataForEpoch.adc_ticks());
                
                posSolvers[2]->solve(gnssDataForEpoch.sv(predOnlyGalileo),
                                     gnssDataForEpoch.weight(predOnlyGalileo),
                                     gnssDataForEpoch.adc_ticks());
            }
        }

        update_gps_info_after(gnssDataForEpoch, posSolvers, plot_E1B);

        // result_t result = Solve(good, &lat, &lon, &alt);
        TaskStat(TSTAT_INCR|TSTAT_ZERO, 0, 0, 0);

#if 0
        if (result != SOLN || alt > ALT_MAX || alt < ALT_MIN)
        	continue;

        gps.fixes++; gps.fixes_min_incr++; gps.fixes_hour_incr++;
        
        // at startup immediately indicate first solution
        if (gps.fixes_min == 0) gps.fixes_min++;

        // at startup incrementally update until first hour sample period has ended
        if (gps.fixes_hour_samples <= 1) gps.fixes_hour++;
        
        GPSstat(STAT_LAT, RAD_2_DEG(lat));
        GPSstat(STAT_LON, RAD_2_DEG(lon));
        GPSstat(STAT_ALT, alt);
#endif
    }
}
