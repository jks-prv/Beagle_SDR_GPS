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

#include <time.h>

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
    bool err;       // all CFG_OPTIONAL because don't get defaulted early enough
    clk.do_corrections = cfg_bool("ADC_clk_corr", &err, CFG_OPTIONAL);
    if (err) clk.do_corrections = true;
    clk.ext_ADC_clk = cfg_bool("ext_ADC_clk", &err, CFG_OPTIONAL);
    strcpy(gps.a, "<hfz>1jqB5loF");
    if (err) clk.ext_ADC_clk = false;
    double ext_clk_freq = (double) cfg_int("ext_ADC_freq", &err, CFG_OPTIONAL);
    if (err) ext_clk_freq = (int) round(ADC_CLOCK_TYP);
    
    clk.adc_clock_base = clk.ext_ADC_clk? ext_clk_freq : ADC_CLOCK_TYP;
    printf("ADC_CLOCK: %s%.6f MHz\n",
        clk.ext_ADC_clk? "EXTERNAL, J5 connector, " : "", clk.adc_clock_base/MHz);
    
    clk.manual_adj = 0;
}

double clock_initial()
{
    return adc_clock_initial;
}

void clock_conn_init(conn_t *conn)
{
	conn->adc_clock_corrected = clock_initial();
	conn->manual_offset = 0;
	conn->adjust_clock = true;
}

double adc_clock_system()
{
    double new_clk = clk.adc_clock_base + clk.manual_adj;

    for (conn_t *c = conns; c < &conns[N_CONNS]; c++) {
        if (!c->valid) continue;
        
        // adc_clock_corrected is what the sound and waterfall channels use for the NCOs
        c->adc_clock_corrected = new_clk;
    }

    if (clk.adc_clk_corrections != clk.last_adc_clk_corrections) {
        clk_printf("adc_clock_system base=%.0f man_adj=%d clk=%.0f(%d)\n",
            clk.adc_clock_base, clk.manual_adj, new_clk, clk.adc_clk_corrections);
        clk.last_adc_clk_corrections = clk.adc_clk_corrections;
    }

    return new_clk;
}

void clock_manual_adj(int manual_adj)
{
    clk.manual_adj = manual_adj;
    adc_clock_system();
    clk.adc_clk_corrections++;      // otherwise c2s_sound() and c2s_waterfall() won't update
}

// Compute corrected ADC clock based on GPS time.
// Called on each GPS solution.
void clock_correction(double t_rx, u64_t ticks)
{
    // record stats
    clk.gps_secs = t_rx;
    clk.ticks = ticks;

    if (!clk.do_corrections) {
        clk_printf("!clk.do_corrections\n");
        return;
    }
    
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
        (first_time_temp_correction || outside_window > MAX_OUTSIDE)? PPM_TO_HZ(ADC_CLOCK_TYP, ADC_CLOCK_PPM_TYP) : PPM_TO_HZ(ADC_CLOCK_TYP, 1);
    double offset = new_adc_clock - clk.adc_clock_base;      // offset from previous clock value

    // limit offset to a window to help remove outliers
    if (offset < -offset_window || offset > offset_window) {
        outside_window++;
        clk_printf("CLK BAD %d off %.0f win %.0f SYS %.6f NEW %.6f GT %6.3f ticks %08x|%08x\n", outside_window,
            offset, offset_window, clk.adc_clock_base/1e6, new_adc_clock/1e6, gps_secs, PRINTF_U64_ARG(ticks));
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
    clk.adc_gps_clk_corrections++;
    
    #ifdef CLK_PRINTF
        double diff_mma = adc_clock_mma - clk.adc_clock_base, diff_new = new_adc_clock - prev_new;
    #endif
    clk_printf("CLK %3d win %4.0lf MMA %.6lf(%5.1f) %5.1f NEW %.6lf(%5.1f) ratio %.6f\n",
        clk.adc_clk_corrections, offset_window,
        adc_clock_mma/1e6, diff_mma, offset, new_adc_clock/1e6, diff_new, diff_new / diff_mma);
    prev_new = new_adc_clock;

    clk.manual_adj = 0;     // remove any manual adjustment now that we're automatically correcting
    clk.adc_clock_base = adc_clock_mma;
    
    /*  jksx FIXME XXX WRONG-WRONG-WRONG
    // even if !adjust_clock mode is set adjust for first_time_temp_correction
    conn_t *c;
    for (c = conns; c < &conns[N_CONNS]; c++) {
        if (!c->valid || (!c->adjust_clock && !first_time_temp_correction)) continue;
        c->adc_clock_corrected = adc_clock_mma;
        c->adc_clk_corrections++;
    }
    */

    #if 0
    if (!ns_nom) ns_nom = clk.adc_clock_base;
    int bin = ns_nom - clk.adc_clock_base;
    ns_bin[bin+512]++;
    #endif
}

void tod_correction()
{
    // corrects the time, but not the date

    #if 0
        if (!gps.tLS_valid) {
            gps.delta_tLS = 18;
            printf("GPS/UTC +%d sec (faked)\n", gps.delta_tLS);
            gps.tLS_valid = true;
        }
    #endif
    
    if (gps.tLS_valid) {
        static int msg;
        double gps_utc_fsecs = gps.StatSec - gps.delta_tLS;
        double gps_utc_frac_sec = gps_utc_fsecs - floor(gps_utc_fsecs);
        double gps_utc_fhours = gps_utc_fsecs/60/60;
        UMS hms(gps_utc_fhours);
        // GPS time HH:MM:SS.sss = hms.u, hms.m, hms.s

        time_t t; time(&t); struct tm tm; gmtime_r(&t, &tm);
        struct timespec ts; clock_gettime(CLOCK_REALTIME, &ts);
        double tm_fsec = (double) ts.tv_nsec/1e9 + (double) tm.tm_sec;
        double host_utc_fsecs = (double) tm.tm_hour*60*60 + (double) tm.tm_min*60 + tm_fsec;
        // Host time HH:MM:SS.sss = tm.tm_hour, tm.tm_min, tm_fsec

        double delta = gps_utc_fsecs - host_utc_fsecs;
        
        #define MAX_CLOCK_ERROR_SECS 2.0
        // require same day to avoid boundary problem at day wrap (23:59:59 -> 00:00:00)
        if (gps.StatDay == tm.tm_wday && fabs(delta) > MAX_CLOCK_ERROR_SECS) {
            tm.tm_hour = hms.u;
            tm.tm_min = hms.m;
            tm.tm_sec = (int) floor(hms.s);
            ts.tv_sec = timegm(&tm);

            // NB: doesn't work without the intermediate cast to (int)
            ts.tv_nsec = (time_t) (int) (gps_utc_frac_sec * 1e9);
            msg = 4;

            if (clock_settime(CLOCK_REALTIME, &ts) < 0) {
                perror("clock_settime");
            }
        }
        
        if (msg) {
            printf("GPS %02d:%02d:%04.3f (%+d) UTC %02d:%02d:%04.3f deltaT %.3f %s\n",
                hms.u, hms.m, hms.s, gps.delta_tLS, tm.tm_hour, tm.tm_min, tm_fsec, delta, (msg == 4)? "SET" : "CHECK");
            msg--;
        }
    }
}

int *ClockBins() {
	return ns_bin;
}
