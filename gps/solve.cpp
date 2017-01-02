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
static int ns_nom;

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

	// i.e. { ticks[47:0], srq[GPS_CHANS-1:0], {GPS_CHANS {clock_replica[15:0]}} }

    const int WPT=3;	// words per tick field
    const int WPS=1;	// words per SRQ field
    const int WPC=3;	// words per clock field

    SPI_MISO clocks;
    int chans=0;

    // Yielding to other tasks not allowed after spi_get_noduplex returns
	spi_get_noduplex(CmdGetClocks, &clocks, WPT*2 + WPS*2 + GPS_CHANS*WPC*2);

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
        NextTask("solve1");

        weight[i] = Replicas[i].power;

        // Un-corrected time of transmission
        t_tx[i] = Replicas[i].GetClock();
        if (t_tx[i] == NAN) return MAX_ITER;

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
        NextTask("solve2");

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

    //GPSstat(STAT_TIME, t_rx);
    
	if (j != MAX_ITER && t_rx != 0) {
    	GPSstat(STAT_TIME, t_rx);
	
		// compute corrected ADC clock based on GPS time
		u64_t diff_ticks = time_diff48(ticks, last_ticks);
		double gps_secs = t_rx - last_t_rx;
		double new_adc_clock = diff_ticks / gps_secs;
		double offset = adc_clock_nom - new_adc_clock;

		// limit to about +/- 50ppm tolerance of XO to help remove outliers
		if (offset >= -5000 && offset <= 5000) {	
			//printf("GPST %f ADCT %.0f ticks %lld offset %.1f\n",
			//	gps_secs, new_adc_clock, diff_ticks, offset);
			
			// Perform 8-period modified moving average which seems to keep up well during
			// diurnal temperature drifting while dampening-out any transients.
			// Keeps up better than a simple cumulative moving average.
			
			//static double adc_clock_cma;
			//adc_clock_cma = (adc_clock_cma * gps.adc_clk_corr) + new_adc_clock;
			static double adc_clock_mma;
			#define MMA_PERIODS 8
			if (adc_clock_mma == 0) adc_clock_mma = new_adc_clock;
			adc_clock_mma = ((adc_clock_mma * (MMA_PERIODS-1)) + new_adc_clock) / MMA_PERIODS;
			gps.adc_clk_corr++;
			//adc_clock_cma /= gps.adc_clk_corr;
			//printf("%d SAMP %.6f CMA %.6f MMA %.6f\n", gps.adc_clk_corr,
			//	new_adc_clock/1000000, adc_clock_cma/1000000, adc_clock_mma/1000000);

			adc_clock = adc_clock_mma + adc_clock_offset;
			
			#if 0
			if (!ns_nom) ns_nom = adc_clock;
			int bin = ns_nom - adc_clock;
			ns_bin[bin+512]++;
			#endif
		}
		last_ticks = ticks;
		last_t_rx = t_rx;
		//printf("SOLUTION worked %d %.1f\n", j, t_rx);
	} else {
		//printf("SOLUTION failed %.1f\n", t_rx);
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

void SolveTask(void *param) {
    double x, y, z, t_b, lat, lon, alt;
    
    for (;;) {
		TaskSleepMsec(4000);
        int good = LoadReplicas();
        gps.good = good;
        bool enable = SearchTaskRun();
        if (!enable || good < 4) continue;
        
        int iter = Solve(good, &x, &y, &z, &t_b);
        TaskStat(TSTAT_INCR|TSTAT_ZERO, 0, 0, 0);
        if (iter == MAX_ITER) continue;
        
        LatLonAlt(x, y, z, lat, lon, alt);

        if (alt > 9000 || alt < -100)
        	continue;

        gps.fixes++;
        GPSstat(STAT_LAT, lat*180/PI);
        GPSstat(STAT_LON, lon*180/PI);
        GPSstat(STAT_ALT, alt);
    }
}
