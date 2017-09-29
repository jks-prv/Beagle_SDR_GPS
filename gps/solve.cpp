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

#include <memory.h>
#include <stdio.h>
#include <math.h>

#include "types.h"
#include "gps.h"
#include "clk.h"
#include "ephemeris.h"
#include "spi.h"

#define MAX_ITER 20

#define WGS84_A     (6378137.0)
#define WGS84_F_INV (298.257223563)
#define WGS84_B     (6356752.31424518)
#define WGS84_E2    (0.00669437999014132)

///////////////////////////////////////////////////////////////////////////////////////////////

struct SNAPSHOT {
    EPHEM eph;
    float power;
    int ch, sv, ms, bits, g1, ca_phase;
    bool LoadAtomic(int ch, uint16_t *up, uint16_t *dn);
    double GetClock();
};

static SNAPSHOT Replicas[GPS_CHANS];
static u64_t ticks;

///////////////////////////////////////////////////////////////////////////////////////////////
// Gather channel data and consistent ephemerides

bool SNAPSHOT::LoadAtomic(int ch_, uint16_t *up, uint16_t *dn) {

    /* Called inside atomic section - yielding not allowed */

    if (ChanSnapshot(
        ch_,     // in: channel id
        up[1],  // in: FPGA circular buffer pointer
        &sv,    // out: satellite id
        &bits,  // out: total bits held locally (CHANNEL struct) + remotely (FPGA)
        &power) // out: received signal strength ^ 2
    && Ephemeris[sv].Valid()) {

        ms = up[0];
        g1 = dn[0] & 0x3FF;
        ca_phase = dn[0] >> 10;

        memcpy(&eph, Ephemeris+sv, sizeof eph);
        return true;
    }
    else
        return false; // channel not ready
}

///////////////////////////////////////////////////////////////////////////////////////////////

static int LoadAtomic() {

	// i.e. { ticks[47:0], srq[GPS_CHANS-1:0], { GPS_CHANS { clock_replica } } }
	// clock_replica = { ch_NAV_MS[15:0], ch_NAV_BITS[15:0], ca_phase_code[15:0] }
	// NB: ca_phase_code is in reverse GPS_CHANS order, hence use of "up" and "dn" logic below

    const int WPT=3;	// words per ticks field
    const int WPS=1;	// words per SRQ field
    const int WPC=3;	// words per clock replica field

    SPI_MISO clocks;
    int chans=0;

    // Yielding to other tasks not allowed after spi_get_noduplex returns
	spi_get_noduplex(CmdGetClocks, &clocks, S2B(WPT) + S2B(WPS) + S2B(GPS_CHANS*WPC));

    uint16_t srq = clocks.word[WPT+0];              // un-serviced epochs
    uint16_t *up = clocks.word+WPT+1;               // Embedded CPU memory containing ch_NAV_MS and ch_NAV_BITS
    uint16_t *dn = clocks.word+WPT+WPC*GPS_CHANS;   // FPGA clocks (in reverse order)

    // NB: see tools/ext64.c for why the (u64_t) casting is very important
    ticks = ((u64_t) clocks.word[0]<<32) | ((u64_t) clocks.word[1]<<16) | clocks.word[2];

    for (int ch=0; ch<gps_chans; ch++, srq>>=1, up+=WPC, dn-=WPC) {

        up[0] += (srq&1); // add 1ms for un-serviced epochs

        if (Replicas[chans].LoadAtomic(ch,up,dn))
            Replicas[chans++].ch=ch;
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
    for (int i=0; i<pass1; i++) {
        int ch = Replicas[i].ch;
        if (glitches[0].word[ch]!=glitches[1].word[ch]) continue;
        if (i>pass2) memcpy(Replicas+pass2, Replicas+i, sizeof(SNAPSHOT));
        pass2++;
    }

    return pass2;
}

///////////////////////////////////////////////////////////////////////////////////////////////

double SNAPSHOT::GetClock() {

    // Find 10-bit shift register in 1023 state sequence
    int chips = SearchCode(sv, g1);
    if (chips == -1) return NAN;

    // TOW refers to leading edge of next (un-processed) subframe.
    // Channel.cpp processes NAV data up to the subframe boundary.
    // Un-processed bits remain in holding buffers.

    return // Un-corrected satellite clock
        eph.tow * 6 +                   // Time of week in seconds
        bits / BPS  +                   // NAV data bits buffered
        ms * 1e-3   +                   // Milliseconds since last bit (0...20)
        chips / CPS +                   // Code chips (0...1022)
        ca_phase * pow(2, -6) / CPS;    // Code NCO phase
}

///////////////////////////////////////////////////////////////////////////////////////////////

// i.e. converts ECEF (WGS84) to ellipsoidal coordinates

static void LatLonAlt(
    double x_n_ecef, double y_n_ecef, double z_n_ecef,  // m
    double *lat, double *lon, double *alt) {

    const double a  = WGS84_A;
    const double e2 = WGS84_E2;

    const double p = sqrt(x_n_ecef*x_n_ecef + y_n_ecef*y_n_ecef);

    *lon = 2.0 * atan2(y_n_ecef, x_n_ecef + p);
    *lat = atan(z_n_ecef / (p * (1.0 - e2)));
    *alt = 0.0;

    for (;;) {
        double tmp = *alt;
        double N = a / sqrt(1.0 - e2*pow(sin(*lat),2));
        *alt = p/cos(*lat) - N;
        *lat = atan(z_n_ecef / (p * (1.0 - e2*N/(N + *alt))));
        if (fabs(*alt-tmp)<1e-3) break;
    }
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
static void lat_lon_to_ECI(
    time_t utc_time,
    double lon, double lat, double alt,     // alt (m)
    double *x, double *y, double *z         // ECI, km
    ) {

    // http://celestrak.com/columns/v02n03/
    double F = 1.0 / WGS84_F_INV;
    double A = WGS84_A / 1e3;       // rEarth (km)
    
    double theta = fmod(gmt_sidereal_rad(utc_time) + lon, K_2PI);
    double c = 1.0 / sqrt(1.0 + F * (F - 2) * pow(sin(lat), 2));
    double sq = c * pow((1.0 - F), 2);

    alt /= 1e3;     // km
    double achcp = (A * c + alt) * cos(lat);
    *x = achcp * cos(theta);    // km
    *y = achcp * sin(theta);
    *z = (A * sq + alt) * sin(lat);
}

// Calculate observers look angle to a satellite
static void ECI_pair_to_az_el(
    time_t utc_time,
    double pos_x, double pos_y, double pos_z,       // all positions ECI, km
    double opos_x, double opos_y, double opos_z,
    double lon, double lat,     // rad
    double *az, double *el) {   // deg

    // http://celestrak.com/columns/v02n02/
    // utc_time: Observation time
    // x, y, z: ECI positions of satellite and observer
    // Return: (Azimuth, Elevation)

    double theta = fmod(gmt_sidereal_rad(utc_time) + lon, K_2PI);

    double rx = pos_x - opos_x;
    double ry = pos_y - opos_y;
    double rz = pos_z - opos_z;

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

static int Solve(int chans, double *lat, double *lon, double *alt) {
    int i, j, r, c;

    double t_tx[GPS_CHANS]; // Clock replicas in seconds since start of week

    double x_sv_ecef[GPS_CHANS],
           y_sv_ecef[GPS_CHANS],
           z_sv_ecef[GPS_CHANS];

    double t_pc;    // Uncorrected system time when clock replica snapshots taken
    double t_rx;    // Corrected GPS time

    double dPR[GPS_CHANS]; // Pseudo range error

    double jac[GPS_CHANS][4], ma[4][4], mb[4][4], mc[4][GPS_CHANS], md[4];

    double weight[GPS_CHANS];

    double x_n_ecef, y_n_ecef, z_n_ecef, t_bias;

    x_n_ecef = y_n_ecef = z_n_ecef = t_bias = t_pc = 0;

    for (i=0; i<chans; i++) {
        NextTask("solve1");

        weight[i] = Replicas[i].power;

        // Un-corrected time of transmission
        t_tx[i] = Replicas[i].GetClock();
        if (t_tx[i] == NAN) return MAX_ITER;

        // Clock correction
        t_tx[i] -= Replicas[i].eph.GetClockCorrection(t_tx[i]);

        // Get SV position in ECEF coords
        Replicas[i].eph.GetXYZ(x_sv_ecef+i, y_sv_ecef+i, z_sv_ecef+i, t_tx[i]);

        t_pc += t_tx[i];
    }
    
    // Approximate starting value for receiver clock
    t_pc = t_pc/chans + 75e-3;

    // Iterate to user xyzt solution using Taylor Series expansion:
    for (j=0; chans >= 4 && j < MAX_ITER; j++) {
        NextTask("solve2");

        t_rx = t_pc - t_bias;

        for (i=0; i<chans; i++) {
            // Convert SV position to ECI coords (20.3.3.4.3.3.2)
            double theta = (t_tx[i] - t_rx) * OMEGA_E;
            //printf("GPS %d: ECI  t_tx[i]=%f t_rx=%f diff=%f theta=%f/%f/%f\n",
            //    i, t_tx[i], t_rx, t_tx[i] - t_rx, theta, sin(theta), cos(theta));

            double x_sv_eci = x_sv_ecef[i]*cos(theta) - y_sv_ecef[i]*sin(theta);
            double y_sv_eci = x_sv_ecef[i]*sin(theta) + y_sv_ecef[i]*cos(theta);
            double z_sv_eci = z_sv_ecef[i];

            // Geometric range (20.3.3.4.3.4)
            double gr = sqrt(pow(x_n_ecef - x_sv_eci, 2) +
                             pow(y_n_ecef - y_sv_eci, 2) +
                             pow(z_n_ecef - z_sv_eci, 2));

            dPR[i] = C*(t_rx - t_tx[i]) - gr;

            jac[i][0] = (x_n_ecef - x_sv_eci) / gr;
            jac[i][1] = (y_n_ecef - y_sv_eci) / gr;
            jac[i][2] = (z_n_ecef - z_sv_eci) / gr;
            jac[i][3] = C;
        }

        // ma = transpose(H) * W * H
        for (r=0; r<4; r++)
            for (c=0; c<4; c++) {
            ma[r][c] = 0;
            for (i=0; i<chans; i++) ma[r][c] += jac[i][r]*weight[i]*jac[i][c];
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
            mc[r][c] = 0;
            for (i=0; i<4; i++) mc[r][c] += mb[r][i]*jac[c][i];
        }

        // md = inverse(transpose(H)*W*H) * transpose(H) * W * dPR
        for (r=0; r<4; r++) {
            md[r] = 0;
            for (i=0; i<chans; i++) md[r] += mc[r][i]*weight[i]*dPR[i];
        }

        double dx = md[0];
        double dy = md[1];
        double dz = md[2];
        double dt = md[3];

        double err_mag = sqrt(dx*dx + dy*dy + dz*dz);

        // printf("%14g%14g%14g%14g%14g\n", err_mag, t_bias, x_n_ecef, y_n_ecef, z_n_ecef);

        if (err_mag<1.0) break;

        x_n_ecef += dx;
        y_n_ecef += dy;
        z_n_ecef += dz;
        t_bias   += dt;
    }

    // if enough good sats compute new Kiwi lat/lon and do clock correction
	if (chans >= 4) {
	    if (j == MAX_ITER || t_rx == 0) return MAX_ITER;
        GPSstat(STAT_TIME, t_rx);
        clock_correction(t_rx, ticks);
        
        LatLonAlt(x_n_ecef, y_n_ecef, z_n_ecef, lat, lon, alt);
        if (*alt > 9000 || *alt < -100) return MAX_ITER;
    } else {
        j = MAX_ITER;
    }
    
    if ((*lat == 0 && *lon == 0)) return j;     // no lat/lon yet

    // ECI depends on current time so can't cache like lat/lon
    time_t now = time(NULL);
    double opos_x, opos_y, opos_z;
    lat_lon_to_ECI(now, *lon, *lat, *alt, &opos_x, &opos_y, &opos_z);
    //printf("GPS U: ECI  x=%10.3f y=%10.3f z=%10.3f lat=%11.6f lon=%11.6f alt=%4.0f ECEF x=%10.3f y=%10.3f z=%10.3f\n",
    //    opos_x, opos_y, opos_z, RAD_2_DEG(*lat), RAD_2_DEG(*lon), *alt, x_n_ecef/1e3, y_n_ecef/1e3, z_n_ecef/1e3);
    
    // update sat az/el even if not enough good sats to compute new Kiwi lat/lon
    // (Kiwi is not moving so use last computed lat/lon)
    for (i=0; i<chans; i++) {
        int prn = Replicas[i].sv+1;
        int prn_i = prn-1;
        
        // already have az/el for this prn in this sample period?
        if (gps.el[gps.last_samp][prn_i]) continue;
        
        //printf("GPS %d: ECEF x=%10.3f y=%10.3f z=%10.3f PRN%02d\n",
        //    i, x_sv_ecef[i]/1e3, y_sv_ecef[i]/1e3, z_sv_ecef[i]/1e3, prn);
        double az_f, el_f;
        double spos_x, spos_y, spos_z;
        double s_lat, s_lon, s_alt;
        LatLonAlt(x_sv_ecef[i], y_sv_ecef[i], z_sv_ecef[i], &s_lat, &s_lon, &s_alt);
        //printf("GPS %d: L/L  lat=%11.6f lon=%11.6f alt=%f PRN%02d\n",
        //    i, RAD_2_DEG(s_lat), RAD_2_DEG(s_lon), s_alt/1e3, prn);
        lat_lon_to_ECI(now, s_lon, s_lat, s_alt, &spos_x, &spos_y, &spos_z);
        ECI_pair_to_az_el(now, spos_x, spos_y, spos_z, opos_x, opos_y, opos_z, *lon, *lat, &az_f, &el_f);
        int az = round(az_f);
        int el = round(el_f);
        //printf("GPS %d: ECI  x=%10.3f y=%10.3f z=%10.3f PRN%02d EL/AZ=%2d %3d\n",
        //    i, spos_x, spos_y, spos_z, prn, el, az);

        //real_printf("PRN%02d EL/AZ=%2d %3d samp=%d\n", prn, el, az, gps.last_samp);
        if (el <= 0) continue;
        gps.az[gps.last_samp][prn_i] = az;
        gps.el[gps.last_samp][prn_i] = el;
        
        // add az/el to channel data
        for (int ch = 0; ch < GPS_CHANS; ch++) {
            gps_stats_t::gps_chan_t *chp = &gps.ch[ch];
            if (chp->prn == prn) {
                chp->az = az;
                chp->el = el;
            }
        }
    }
	
    return j;
}

///////////////////////////////////////////////////////////////////////////////////////////////

void SolveTask(void *param) {
    double lat=0, lon=0, alt;
    
    for (;;) {
		TaskSleepMsec(4000);
        int good = LoadReplicas();

        time_t t; time(&t);
        struct tm tm; gmtime_r(&t, &tm);
        //int samp = (tm.tm_hour & 3)*60 + tm.tm_min;
        int samp = tm.tm_min;

        if (gps.last_samp != samp) {
            gps.last_samp = samp;
            for (int prn_i = 0; prn_i < NUM_SATS; prn_i++) {
                gps.az[gps.last_samp][prn_i] = 0;
                gps.el[gps.last_samp][prn_i] = 0;
            }
        }

        gps.good = good;
        bool enable = SearchTaskRun();
        if (!enable || good == 0) continue;
        
        int iter = Solve(good, &lat, &lon, &alt);
        TaskStat(TSTAT_INCR|TSTAT_ZERO, 0, 0, 0);
        if (iter == MAX_ITER) continue;
        
        if (alt > 9000 || alt < -100)
        	continue;

        gps.fixes++;
        GPSstat(STAT_LAT, RAD_2_DEG(lat));
        GPSstat(STAT_LON, RAD_2_DEG(lon));
        GPSstat(STAT_ALT, alt);
    }
}
