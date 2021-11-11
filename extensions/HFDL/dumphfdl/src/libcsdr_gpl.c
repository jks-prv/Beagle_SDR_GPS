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

#include <math.h>               // M_PI
#include "libcsdr_gpl.h"
#include "complex2.h"

shift_addition_data_t shift_addition_init(float rate)
{
	rate*=2;
	shift_addition_data_t out;
	out.sindelta=sin(rate*M_PI);
	out.cosdelta=cos(rate*M_PI);
	out.rate=rate;
	return out;
}

shift_addition_data_t decimating_shift_addition_init(float rate, int32_t decimation)
{
	return shift_addition_init(rate*decimation);
}

decimating_shift_addition_status_t decimating_shift_addition_cc(float complex *input, float complex* output, int32_t input_size, shift_addition_data_t d, int32_t decimation, decimating_shift_addition_status_t s)
{
	//The original idea was taken from wdsp:
	//http://svn.tapr.org/repos_sdr_hpsdr/trunk/W5WC/PowerSDR_HPSDR_mRX_PS/Source/wdsp/shift.c
	//However, this method introduces noise (from floating point rounding errors), which increases until the end of the buffer.
	//fprintf(stderr, "cosd=%g sind=%g\n", d.cosdelta, d.sindelta);
	float cosphi=cos(s.starting_phase);
	float sinphi=sin(s.starting_phase);
	float cosphi_last, sinphi_last;
	int32_t i;
	int32_t k=0;
	for(i=s.decimation_remain;i<input_size;i+=decimation) //@shift_addition_cc: work
	{

		// FIXME: optimize this
		output[k] = CMPLXF(
					cosphi * crealf(input[i]) - sinphi * cimagf(input[i]),
					sinphi * crealf(input[i]) + cosphi * cimagf(input[i])
				);
		k++;
		//using the trigonometric addition formulas
		//cos(phi+delta)=cos(phi)cos(delta)-sin(phi)*sin(delta)
		cosphi_last=cosphi;
		sinphi_last=sinphi;
		cosphi=cosphi_last*d.cosdelta-sinphi_last*d.sindelta;
		sinphi=sinphi_last*d.cosdelta+cosphi_last*d.sindelta;
	}
	s.decimation_remain=i-input_size;
	s.starting_phase+=d.rate*M_PI*k;
	s.output_size=k;
	while(s.starting_phase>M_PI) s.starting_phase-=2*M_PI; //@shift_addition_cc: normalize starting_phase
	while(s.starting_phase<-M_PI) s.starting_phase+=2*M_PI;
	return s;
}
