/*
This file is part of libcsdr.

	Copyright (c) Andras Retzler, HA7ILM <randras@sdr.hu>
	Copyright (c) Warren Pratt, NR0V <warren@wpratt.com>
	Copyright 2006,2010,2012 Free Software Foundation, Inc.

    libcsdr is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    libcsdr is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with libcsdr.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once
#include <stdint.h>
#include <complex.h>

typedef struct shift_addition_data_s
{
	float sindelta;
	float cosdelta;
	float rate;
} shift_addition_data_t;

shift_addition_data_t shift_addition_init(float rate);

typedef struct decimating_shift_addition_status_s
{
	int32_t decimation_remain;
	float starting_phase;
	int32_t output_size;
} decimating_shift_addition_status_t;

decimating_shift_addition_status_t decimating_shift_addition_cc(float complex
		*input, float complex* output, int32_t input_size, shift_addition_data_t d,
		int32_t decimation, decimating_shift_addition_status_t s);
shift_addition_data_t decimating_shift_addition_init(float rate, int
		decimation);

