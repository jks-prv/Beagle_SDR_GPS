/*
--------------------------------------------------------------------------------
This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public
License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.
This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.
You should have received a copy of the GNU Library General Public
License along with this library; if not, write to the
Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
Boston, MA  02110-1301, USA.
--------------------------------------------------------------------------------
*/

// Copyright (c) 2017 John Seamons, ZL/KF6VO

#include "types.h"
#include "config.h"
#include "kiwi.h"
#include "clk.h"
#include "gps.h"
#include "timing.h"
#include "web.h"
#include "ext_int.h"

//#define CLK_PRINTF
#ifdef CLK_PRINTF
	#define clk_printf(fmt, ...) \
		printf(fmt, ## __VA_ARGS__)
#else
	#define clk_printf(fmt, ...)
#endif

clk_t clk;

static double adc_clock_initial = ADC_CLOCK_TYP;
static double last_t_rx;
static u64_t last_ticks;
static int outside_window;

static int ns_bin[1024];
static int ns_nom;

void clock_init()
{
    clk.adc_clock_system = ADC_CLOCK_TYP;
}

double clock_initial()
{
    return adc_clock_initial;
}

void clock_conn_init(conn_t *conn)
{
	conn->adc_clock_corrected = clock_initial();
	conn->adc_clk_corrections = 0;
	conn->manual_offset = 0;
	conn->adjust_clock = true;
}

// Compute corrected ADC clock based on GPS time.
// Called on each GPS solution.
void clock_correction(double t_rx, u64_t ticks)
{
    conn_t *c;

    u64_t diff_ticks = time_diff48(ticks, last_ticks);
    double gps_secs = t_rx - last_t_rx;
    double new_adc_clock = diff_ticks / gps_secs;
    static double prev_new;

    //printf("GT %6.3f ticks %08x|%08x = %08x|%08x - %08x|%08x\n", gps_secs,
    //    PRINTF_U64_ARG(diff_ticks), PRINTF_U64_ARG(ticks), PRINTF_U64_ARG(last_ticks));

    last_ticks = ticks;
    last_t_rx = t_rx;
		
    // First correction allows wider window to capture temperature error.
    // Subsequent corrections use a much tighter window to remove bad GPS solution outliers.
    // Also use wider window if too many sequential solutions outside window.
    #define MAX_OUTSIDE 8
    bool first_time_temp_correction = (clk.adc_clk_corrections == 0);
    
    double offset_window =
        (first_time_temp_correction || outside_window > MAX_OUTSIDE)? PPM_TO_HZ(ADC_CLOCK_TYP, 50) : PPM_TO_HZ(ADC_CLOCK_TYP, 1);
    double offset = new_adc_clock - clk.adc_clock_system;      // offset from previous clock value

    // limit offset to a window to help remove outliers
    if (offset < -offset_window || offset > offset_window) {
        outside_window++;
        clk_printf("CLK BAD %d off %.0f win %.0f SYS %.6f NEW %.6f GT %6.3f ticks %08x|%08x\n", outside_window,
            offset, offset_window, clk.adc_clock_system/1e6, new_adc_clock/1e6, gps_secs, PRINTF_U64_ARG(ticks));
        return;
    }
    outside_window = 0;
    
    // first correction handles XO temperature offset
    if (first_time_temp_correction) {
        adc_clock_initial = prev_new = new_adc_clock;
        clk.temp_correct_offset = offset;
    }

    // Perform modified moving average which seems to keep up well during
    // diurnal temperature drifting while dampening-out any transients.
    // Keeps up better than a simple cumulative moving average.
    
    static double adc_clock_mma;
    #define MMA_PERIODS 16
    if (adc_clock_mma == 0) adc_clock_mma = new_adc_clock;
    adc_clock_mma = ((adc_clock_mma * (MMA_PERIODS-1)) + new_adc_clock) / MMA_PERIODS;
    clk.adc_clk_corrections++;
    
    double diff_mma = adc_clock_mma - clk.adc_clock_system, diff_new = new_adc_clock - prev_new;
    clk_printf("CLK %3d win %4.0lf MMA %.6lf(%5.1f) %5.1f NEW %.6lf(%5.1f) ratio %.6f\n",
        clk.adc_clk_corrections, offset_window,
        adc_clock_mma/1e6, diff_mma, offset, new_adc_clock/1e6, diff_new, diff_new / diff_mma);
    prev_new = new_adc_clock;

    clk.adc_clock_system = adc_clock_mma;
    
    // even if !adjust_clock mode is set adjust for first_time_temp_correction
    for (c = conns; c < &conns[N_CONNS]; c++) {
        if (!c->valid || (!c->adjust_clock && !first_time_temp_correction)) continue;
        c->adc_clock_corrected = new_adc_clock;
        c->adc_clk_corrections++;
    }

    // record stats after clock adjustment
    clk.gps_secs = t_rx;
    clk.ticks = ticks;

    #define GPS_SETS_TOD
    #ifdef GPS_SETS_TOD
    if (gps.tLS_valid) {
        static int msg;
        double gps_utc_fsecs = gps.StatSec - gps.delta_tLS;
        int gps_utc_isecs = floor(gps_utc_fsecs);
        UMS hms(gps_utc_fsecs/60/60);
        time_t t; time(&t); struct tm tm; gmtime_r(&t, &tm);
        struct timespec ts; clock_gettime(CLOCK_REALTIME, &ts);
        double tm_fsec = ts.tv_nsec/1e9 + tm.tm_sec;
        double delta = gps_utc_fsecs - (tm_fsec + tm.tm_min*60 + tm.tm_hour*60*60);
        double frac_sec = gps_utc_fsecs - gps_utc_isecs;
        
        #define MAX_CLOCK_ERROR_SECS 2.0
        if (gps.StatDay == tm.tm_wday && fabs(delta) > MAX_CLOCK_ERROR_SECS) {
            tm.tm_hour = hms.u;
            tm.tm_min = hms.m;
            tm.tm_sec = hms.s;
            ts.tv_sec = mktime(&tm);
            ts.tv_nsec = frac_sec * 1e9;
            msg = 4;

            if (clock_settime(CLOCK_REALTIME, &ts) < 0) {
                perror("clock_settime");
            }
        }
        
        if (msg) {
            printf("GPS %02d:%02d:%04.1f (%+d) UTC %02d:%02d:%04.1f deltaT %.3f %s\n",
                hms.u, hms.m, hms.s, gps.delta_tLS, tm.tm_hour, tm.tm_min, tm_fsec, delta,
                (msg == 4)? "SET":"");
            msg--;
        }
    }
    #endif

    #if 0
    if (!ns_nom) ns_nom = clk.adc_clock_system;
    int bin = ns_nom - clk.adc_clock_system;
    ns_bin[bin+512]++;
    #endif
}

int *ClockBins() {
	return ns_bin;
}
