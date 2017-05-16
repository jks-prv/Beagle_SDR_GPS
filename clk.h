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

// Copyright (c) 2014-2016 John Seamons, ZL/KF6VO

#pragma once

#include "types.h"

// ADC clk generated from FPGA via multiplied GPS TCXO
#define	GPS_CLOCK			(16.368*MHz)		// 61.095 ns
#define ADC_CLOCK_66M_NOM	(66.666600*MHz)		// 66.6666 MHz 15.0 ns
#define ADC_CLOCK_66M_TYP	(66.666070*MHz)		// typical 25 degC value on proto1

#define PPM_TO_HZ(clk_hz, ppm) ((clk_hz) / 1e6 * (ppm))

struct clk_t {
    int adc_clk_corr, temp_correct_offset;
    double gps_secs, offset, manual_offset, srate;
    u64_t ticks;
};

extern clk_t clk;
