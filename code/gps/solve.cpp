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

#include "gps.h"
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
static u64_t ticks, last_ticks;
static double last_t_rx;
static int ns_bin[1024];

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
    const int WPC=3;
    const int WPT=3;

    SPI_MISO clocks;
    int chans=0;

    // Yielding to other tasks not allowed after spi_get_noduplex returns
	spi_get_noduplex(CmdGetClocks, &clocks, WPT*2+2+GPS_CHANS*WPC*2);

    uint16_t srq = clocks.word[WPT+0];              // un-serviced epochs
    uint16_t *up = clocks.word+WPT+1;               // Embedded CPU memory
    uint16_t *dn = clocks.word+WPT+WPC*GPS_CHANS;   // FPGA clocks (in reverse order)

    ticks = ((u64_t) clocks.word[0]<<32) | (clocks.word[1]<<16) | clocks.word[2];

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
    TimerWait(GLITCH_GUARD);

    // Gather consistent snapshot of all channels
    int pass1 = LoadAtomic();
    int pass2 = 0;

    // Get glitch counters "after"
    TimerWait(GLITCH_GUARD);
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

static int Solve(int chans, double *x_n, double *y_n, double *z_n, double *t_bias) {
    int i, j, r, c;

    double t_tx[GPS_CHANS]; // Clock replicas in seconds since start of week

    double x_sv[GPS_CHANS],
           y_sv[GPS_CHANS],
           z_sv[GPS_CHANS];

    double t_pc;  // Uncorrected system time when clock replica snapshots taken
    double t_rx;    // Corrected GPS time

    double dPR[GPS_CHANS]; // Pseudo range error

    double jac[GPS_CHANS][4], ma[4][4], mb[4][4], mc[4][GPS_CHANS], md[4];

    double weight[GPS_CHANS];

    *x_n = *y_n = *z_n = *t_bias = t_pc = 0;

    for (i=0; i<chans; i++) {
        NextTask();

        weight[i] = Replicas[i].power;

        // Un-corrected time of transmission
        t_tx[i] = Replicas[i].GetClock();

        // Clock correction
        t_tx[i] -= Replicas[i].eph.GetClockCorrection(t_tx[i]);

        // Get SV position in ECEF coords
        Replicas[i].eph.GetXYZ(x_sv+i, y_sv+i, z_sv+i, t_tx[i]);

        t_pc += t_tx[i];
    }

    // Approximate starting value for receiver clock
    t_pc = t_pc/chans + 75e-3;

    // Iterate to user xyzt solution using Taylor Series expansion:
    for(j=0; j<MAX_ITER; j++) {
        NextTask();

        t_rx = t_pc - *t_bias;

        for (i=0; i<chans; i++) {
            // Convert SV position to ECI coords (20.3.3.4.3.3.2)
            double theta = (t_tx[i] - t_rx) * OMEGA_E;

            double x_sv_eci = x_sv[i]*cos(theta) - y_sv[i]*sin(theta);
            double y_sv_eci = x_sv[i]*sin(theta) + y_sv[i]*cos(theta);
            double z_sv_eci = z_sv[i];

            // Geometric range (20.3.3.4.3.4)
            double gr = sqrt(pow(*x_n - x_sv_eci, 2) +
                             pow(*y_n - y_sv_eci, 2) +
                             pow(*z_n - z_sv_eci, 2));

            dPR[i] = C*(t_rx - t_tx[i]) - gr;

            jac[i][0] = (*x_n - x_sv_eci) / gr;
            jac[i][1] = (*y_n - y_sv_eci) / gr;
            jac[i][2] = (*z_n - z_sv_eci) / gr;
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

        // printf("%14g%14g%14g%14g%14g\n", err_mag, *t_bias, *x_n, *y_n, *z_n);

        if (err_mag<1.0) break;

        *x_n    += dx;
        *y_n    += dy;
        *z_n    += dz;
        *t_bias += dt;
    }

    UserStat(STAT_TIME, t_rx);
    
	if (j != MAX_ITER) {
		double tick_secs = ((double) (ticks - last_ticks)) / adc_clock;
		double gps_secs = t_rx - last_t_rx;
		double diff = (gps_secs - tick_secs) / gps_secs;
		int ns = (int) (diff * 1e9);
		if (ns <= 1023 && ns >= -1024) ns_bin[(ns+1024)/2]++;
		int maxi=0, maxv=0;
		for (i=-1024; i<=1023; i++) {
			r = (i+1024)/2;
			if (ns_bin[r] > maxv) { maxi = r; maxv = ns_bin[r]; }
		}
		//printf("GPST %f ticks %f diff %e %d max %d(%d)\n",
		//	gps_secs, tick_secs, diff, ns, maxi*2-1024, maxv);
		last_ticks = ticks;
		last_t_rx = t_rx;
	}
	
    return j;
}

int *ClockBins() {
	return ns_bin;
}

///////////////////////////////////////////////////////////////////////////////////////////////

static void LatLonAlt(
    double x_n, double y_n, double z_n,
    double& lat, double& lon, double& alt) {

    const double a  = WGS84_A;
    const double e2 = WGS84_E2;

    const double p = sqrt(x_n*x_n + y_n*y_n);

    lon = 2.0 * atan2(y_n, x_n + p);
    lat = atan(z_n / (p * (1.0 - e2)));
    alt = 0.0;

    for (;;) {
        double tmp = alt;
        double N = a / sqrt(1.0 - e2*pow(sin(lat),2));
        alt = p/cos(lat) - N;
        lat = atan(z_n / (p * (1.0 - e2*N/(N + alt))));
        if (fabs(alt-tmp)<1e-3) break;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////

void SolveTask() {
    double x, y, z, t_b, lat, lon, alt;
    unsigned ttff = Microseconds();
    
    for (;;) {
		TimerWait(4000);
        int chans = LoadReplicas();
#if !defined(QUIET)
		printf("Solve: chans %d\n", chans);
#endif
        UserStat(STAT_CHANS, 0, chans);
        if (chans<4) continue;
        int iter = Solve(chans, &x, &y, &z, &t_b);
        if (iter==MAX_ITER) continue;
        LatLonAlt(x, y, z, lat, lon, alt);
        UserStat(STAT_LAT, lat*180/PI);
        UserStat(STAT_LON, lon*180/PI);
        UserStat(STAT_ALT, alt, chans);

        if (ttff) {
        	UserStat(STAT_TTFF, 0, (int) ((Microseconds() - ttff)/1000000));
        	ttff = 0;
        }
        
//        printf(
//            "\n%d,%3d,%10.6f,"
//          "%10.0f,%10.0f,%10.0f,"
//            "%10.5f LAT, %10.5f LON, %8.2f ALT\n\n",
//            chans, iter, t_b,
//          x, y, z,
//            lat*180/PI, lon*180/PI, alt);
    }
}
