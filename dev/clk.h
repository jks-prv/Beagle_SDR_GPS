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

// Copyright (c) 2014-2016 John Seamons, ZL4VO/KF6VO

#pragma once

#include "types.h"
#include "config.h"
#include "kiwi.h"
#include "web.h"

// ADC clk generated from FPGA via multiplied GPS TCXO
#define	GPS_CLOCK		    (16.368*MHz)		// 61.095 ns
#define ADC_CLOCK_NOM	    (66.666600*MHz)		// 66.6666 MHz 15.0 ns
#define ADC_CLOCK_TYP	    (66.665900*MHz)		// typical 20 degC value
#define ADC_CLOCK_PPM_TYP   50                  // XO spec
#define ADC_CLOCK_PPM_LIMIT 100                 // max manual adjustment we allow

// works with float or int args
// if using an int for clk_hz make it u64_t or constant ULL to prevent overflow
#define PPM_TO_HZ(clk_hz, ppm) ((clk_hz) * (ppm) / 1000000)

enum adc_clk2_corr_e {
    ADC_CLK_CORR_DISABLED = 0,
    ADC_CLK_CORR_CONTINUOUS = 1,
    ADC_CLK_CORR_EVEN_2_MIN = 2,
    ADC_CLK_CORR_5_MIN = 3,
    ADC_CLK_CORR_15_MIN = 4,
    ADC_CLK_CORR_30_MIN = 5
};

typedef struct {
    int do_corrections;
    bool is_corr;
    bool ext_ADC_clk;
    int adc_clk_corrections;        // manual and GPS corrections
    int last_adc_clk_corrections;
    int adc_gps_clk_corrections;    // GPS-derived corrections
    int temp_correct_offset;
    double adc_clock_base, gps_secs, adc_clock_corrected;
    int manual_adj;
    u64_t ticks;                    // ticks value captured at the corresponding gps_secs
} clk_t;

extern clk_t clk;

double adc_clock_system();
void clock_manual_adj(int manual_adj);
void clock_init();
void clock_conn_init(conn_t *conn);
void clock_correction(double t_rx, u64_t ticks);
void tod_correction(u4_t week, int sat);
int *ClockBins();
