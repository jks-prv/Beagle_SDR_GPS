#include "stdafx.h"
/*
 *
 * Receive module for serial tone HF modem.
 *
 *
 */

// Copyright (c) 2000-2016, C.H Brain, G4GUO

#include "ext.h"

#ifdef EXT_S4285

#include <stdio.h>
#define _USE_MATH_DEFINES
#include <math.h>
#include "st4285.h"

extern FComplex rx_preamble_lookup[CSt4285::PREAMBLE_LENGTH];
extern float rx_coffs[CSt4285::RX_FILTER_SIZE];

#define AGC_LAMBDA 0.02
#define SEEK_FREQ              35
#define CENTER_FREQ            (float)((2*M_PI*(CENTER_FREQUENCY))/sample_rate)
#define HI_FREQ                (float)((2*M_PI*(CENTER_FREQUENCY+SEEK_FREQ))/sample_rate)
#define LO_FREQ                (float)((2*M_PI*(CENTER_FREQUENCY-SEEK_FREQ))/sample_rate)

void CSt4285::report_frequency_error( float delta, int channel )
{        
	switch(channel)
	{
		case 0:
		    m_frequency_error = - SEEK_FREQ;
		    break;
		case 1:
		    m_frequency_error = 0;
		    break;
		case 2:
		    m_frequency_error = + SEEK_FREQ;
		    break;
		default:
		    m_frequency_error = 0;
		    break;
	}
	m_frequency_error -= (float)(delta*sample_rate/(4.0*M_PI));
	sprintf(frequency_error,"Frequency error = %.4f ",m_frequency_error);
	status_string();
}

FComplex CSt4285::rx_filter( FComplex *in )
{
	int      i;
	FComplex out;
	
	out.re = in[0].re*rx_coffs[0];
	out.im = in[0].im*rx_coffs[0];
	
	for( i = 1; i < RX_FILTER_SIZE; i++ )
	{
		out.re += in[i].re*rx_coffs[i];
		out.im += in[i].im*rx_coffs[i];
	}
	return(out);
}

float CSt4285::preamble_correlate( FComplex *in )
{
	int i;
	float real,imag;
	
	real = in[0].re*rx_preamble_lookup[0].re;
	imag = in[0].im*rx_preamble_lookup[0].re;
	
	for( i = 1; i < PREAMBLE_LENGTH; i++ )
	{
		real += in[i*SPS].re*rx_preamble_lookup[i].re;
		imag += in[i*SPS].im*rx_preamble_lookup[i].re;
	}
	return((real*real)+(imag*imag));
}

int CSt4285::preamble_hunt( FComplex *in, float *mag )
{
	int   i;
	int	  max_index = 0;
	float max_value = 0;
	float val;
	
	/* We are still hunting for the transmission */
	/* Search the entire frame for the preamble  */
	for( i = 0; i < HALF_SAMPLE_BLOCK_SIZE; i++ )
	{
		val = preamble_correlate( &in[i] );
		if( val > max_value )
		{
			max_value = val;
			max_index = i;
		}
	}
	*mag = max_value;
	return max_index;
}

int CSt4285::preamble_check( FComplex *in )
{
	int i;
	float real, imag;
	float val_a;
	float val_b;
	float val_c;

	// Break the preamble into two PPN_31 chip sequences.
	// Compare aligned and non aligned magnitudes and make decision.
	// i.e. kind of like an E/P/L detector
	
	#define PN_MAG(_offset, _scale, _val) \
		real = in[_offset].re * rx_preamble_lookup[0].re; \
		imag = in[_offset].im * rx_preamble_lookup[0].re; \
		for( i = 1; i < PPN_31 ; i++ ) { \
			real += in[(i*SPS)+_offset].re * rx_preamble_lookup[i].re; \
			imag += in[(i*SPS)+_offset].im * rx_preamble_lookup[i].re; \
		} \
		_val = ((real*real)+(imag*imag))*_scale;
	
	PN_MAG(0,		1,	val_a);
	PN_MAG(PPN_31,	10,	val_b);		// because of SPS == 2 this is spanning the first two 31-bit PNs
	PN_MAG(PPN_62,	1,	val_c);

	//printf("preamble_check %f %f %f\n", val_a, val_b, val_c);
	return ( ( val_a > val_b ) && ( val_b < val_c ) )? 1 : 0;
}

/*
 *
 * Automatic Gain control.
 *
 */
FComplex CSt4285::agc( FComplex in )
{
	double        h,mag;
	
	mag = (in.re*in.re)+(in.im*in.im);
				
	h = (AGC_LAMBDA*mag) + (1.0-AGC_LAMBDA)*hold;
	
	hold = h;		

	h       = 1.0/sqrt(h);	
	in.re = (float)(in.re*h);
	in.im = (float)(in.im*h);

	return in;
}

int CSt4285::is_dcd_detected( void )
{
	if(  rx_state == RX_HUNTING )
		return 0;
	else
		return 1;
}

void CSt4285::reset_receive( int mode )
{
	rx_state = RX_HUNTING;
	rx_mode  = mode;
	
	delta[LOW_RX_CHAN] = LO_FREQ;
	delta[MID_RX_CHAN] = CENTER_FREQ;
	delta[HIH_RX_CHAN] = HI_FREQ;
	equalize_init();
}

// error is the vector angle difference between A and B
// and works given that the PPN_31/18 sequence is repeating
//
// |----31----|----31----|-18-| 31+31+18 = 80
// |===A===========|----31----| 80-31 = 49
// |----31----|===B===========|

doppler_t CSt4285::doppler_error( FComplex *in )
{
	int i;
	doppler_t real, imag, error;	

	real = cmultRealConj(in[0],in[PPN_62]);
	imag = cmultImagConj(in[0],in[PPN_62]);

	for( i = 2 ; i < (PREAMBLE_LENGTH-PPN_31)*2 ; i+=2 )
	{
		real += cmultRealConj(in[i],in[i+PPN_62]);
		imag += cmultImagConj(in[i],in[i+PPN_62]);
	}
	if( real == 0.0 ) {
		printf("doppler_error: DIV0\n");
		//real = (doppler_t)0.0000000001;/* No divide by zero */
	}
	//doppler_t error_old  = (doppler_t)(-atan2(imag,real)*0.008064516129);	// * (1/124)
	error  = (doppler_t)(-atan2(imag, real) / (PPN_31*SPS*2));
	//printf("doppler_error: %e %e %e error %.9f %.9f\n", real, imag, imag/real, error_old, error);
	
	return error;
}

doppler_t CSt4285::initial_doppler_correct( FComplex *in , doppler_t *delta )
{
	doppler_t error = doppler_error(in);
	*delta = error*2.0f;
	//printf("initial_doppler_correct: delta %.15f\n", *delta);
//*delta=0;	//jks
	
	return error;
}

doppler_t CSt4285::doppler_correct( FComplex *in , doppler_t *delta )
{
	doppler_t error = doppler_error(in);
	//printf("doppler_correct: delta %f %f %f ==========================\n", *delta, error*0.1, *delta + error*0.1);
	*delta = *delta + error*0.1;
	//*delta = *delta + error*0.8;
	//*delta = *delta + error;
	//printf("doppler_correct: delta %.15f\n", *delta);
//*delta=0;	//jks
	
	return error;
}

int CSt4285::train_and_equalize_on_preamble( FComplex *in )
{
	int       i;
	int       count;
	FComplex  symbol;
    	
	for( i = 0, count = 0 ; i < PREAMBLE_LENGTH; i++ )
	{
		symbol = equalize_train( &in[(i*SPS)], rx_preamble_lookup[i] );	
TaskFastIntr("s4285_tp0");
		if(symbol.re*rx_preamble_lookup[i].re > 0) count++;	
	}	
	return count;
}

// frate_b is sized [SAMPLE_BLOCK_SIZE+RX_FILTER_SIZE] because sampled at full rate and does overlapped filtering
// hrate_b is sized [HALF_SAMPLE_BLOCK_SIZE*3] because sampled at half rate and [fixme: why *3?]

void CSt4285::rx_downconvert( float *in, FComplex *frate_b, FComplex *hrate_b, float *acc, float delta )
{
	int i;

	//
	// step1: copy overlapped samples for filtering
	// |--RX_FILTER_SIZE_IQ--|--------SAMPLE_BLOCK_SIZE_IQ--------|
	//                       |--RX_FILTER_SIZE_IQ--|
	//          v---------------------<
	// |--RX_FILTER_SIZE_IQ--|
	
	/* Update Received Data, in place of circular addressing */
	for( i = 0; i < RX_FILTER_SIZE; i++ )
	{
		frate_b[i] =  frate_b[i+SAMPLE_BLOCK_SIZE];
	}

	// step2: real-to-complex mix new samples at full sample rate
	//                       |--------SAMPLE_BLOCK_SIZE_RE--------| in real
	//                                        mix
	//                       |--------SAMPLE_BLOCK_SIZE_IQ--------| osc IQ
	//                                         v
	// |--RX_FILTER_SIZE_IQ--|--------SAMPLE_BLOCK_SIZE_IQ--------| frate_b IQ
	
	for( i = 0; i < SAMPLE_BLOCK_SIZE; i++ )
	{
		/* Update with new samples */
		frate_b[i+RX_FILTER_SIZE].re =  (float)cos(*acc)*in[i];	// real -> complex mix
		frate_b[i+RX_FILTER_SIZE].im = -(float)sin(*acc)*in[i];
		*acc += delta;
		if( *acc >= 2*M_PI ) *acc -= (float)(2*M_PI);			
	}

	// step3: shift half-rate samples
	// |-----------------------------|--HALF_SAMPLE_BLOCK_SIZE_IQ--|--HALF_SAMPLE_BLOCK_SIZE_IQ--| hrate_b IQ
	//                               v-----------------------------<
	// |--HALF_SAMPLE_BLOCK_SIZE_IQ--|--HALF_SAMPLE_BLOCK_SIZE_IQ--|-----------------------------| hrate_b IQ
	
	/* Update Received Data, in place of circular addressing  */
	for( i = 0; i < SAMPLE_BLOCK_SIZE ; i++ )
	{
		hrate_b[i] = hrate_b[i+HALF_SAMPLE_BLOCK_SIZE];
	}

	// step4: decimate-by-2 full-rate samples from step 2, filter, AGC and append to half-rate samples
	// |--RX_FILTER_SIZE_IQ--|--------SAMPLE_BLOCK_SIZE_IQ--------| frate_b IQ
	//                       >------------------ decimate-by-2 ---------------------v
	// |--HALF_SAMPLE_BLOCK_SIZE_IQ--|--HALF_SAMPLE_BLOCK_SIZE_IQ--|--HALF_SAMPLE_BLOCK_SIZE_IQ--| hrate_b IQ
	
	/* Decimate by two and filter */
	for( i = 0; i < HALF_SAMPLE_BLOCK_SIZE; i++ )
	{
		/* Run the channel filter  */
		hrate_b[i+SAMPLE_BLOCK_SIZE] = agc( rx_filter( &frate_b[i*2] ));
	}

	TaskFastIntr("s4285_dn0");
}

/*
 *
 * This is a final post mix downconvert.
 * It is a bit of a kludge.
 *
 */
#if 1
void CSt4285::rx_final_downconvert( FComplex *in, float *delta )
{
	int i;
	FComplex mix, osc;
	
	for( i = 0; i < HALF_SAMPLE_BLOCK_SIZE; i++ )
	{
		/* Update with new samples */
		osc.re =  (float)cos(accumulator);
		osc.im =  -(float)sin(accumulator);
		mix.re = cmultReal(osc,in[i]);		// complex -> complex mix
		mix.im = cmultImag(osc,in[i]);
		in[i] = mix;		
		accumulator += *delta;
		if( accumulator >= 2*M_PI ) accumulator -= (float)(2*M_PI);			
	}
	printf("mix %f %f osc %f %f\n", mix.re, mix.im, osc.re, osc.im);
}
#else

#if 1
#define PLL_ALPHA	0.002
#define PLL_BETA	0.044721359549		// sqrt(PLL_ALPHA)

// from CuteSDR with srate = 4800
//#define PLL_ALPHA	0.001308996939
//#define PLL_BETA	0.185092167174
#else
#define PLL_ALPHA	0
#define PLL_BETA	0
#endif

void CSt4285::rx_final_downconvert( FComplex *in, float *delta )
{
	int i;
	FComplex mix, osc, integ;
	float phzerror;
	
	//printf("delta %f -------------------------------\n", *delta);
	
	for( i = 0; i < HALF_SAMPLE_BLOCK_SIZE; i++ )
	{
		/* Update with new samples */
		osc.re =  (float)cos(accumulator);
		osc.im =  -(float)sin(accumulator);
		mix.re = cmultReal(osc,in[i]);		// complex -> complex mix
		mix.im = cmultImag(osc,in[i]);
		in[i] = mix;
		
		// if input signal were sinusoid this would suffice
		//phzerror = atan2(mix.im, mix.re);
		
		// but for PSK must use Costas loop to generate proper error signal
		integ.re += mix.re;
		integ.im += mix.im;
		int n = i+1;
		//phzerror = (integ.re/n * integ.im/n) / (3*3);
		//integ.re /= n;
		//integ.im /= n;
		phzerror = (integ.re * integ.im) / HALF_SAMPLE_BLOCK_SIZE;
		//printf("%d ire=%f iim=%f e=%.15f\n", i, integ.re, integ.im, phzerror);
		//phzerror = 0;
		
		float alpha = phzerror * PLL_ALPHA, beta = phzerror * PLL_BETA;
		//printf("%d acc=%f d=%f e=%f a=%f b=%f\n", i, accumulator, *delta, phzerror, alpha, beta);
		//alpha = beta = 0;
		*delta = *delta + alpha;
		// FIXME: +/- clamp?
		accumulator += *delta + beta;
		if( accumulator >= 2*M_PI ) accumulator -= (float)(2*M_PI);			
	}
}
#endif

#define T_EN(t) \
	u4_t t_##t = timer_us();
//#define T_EN(t) T_EN2(t_ ## t);

#define T_EX(t) \
	float f_##t = (float) (timer_us() - t_##t) / 1e3;

// process one rx block of length RX_BLOCK_LENGTH
// which contains enough samples for one frame worth of symbols

void CSt4285::process_rx_block( float *in )
{
	int			i;
	int			start[RX_CHANNELS];
	float		mag[RX_CHANNELS], max_mag;
	int			updated_preamble_start;
	int			do_demod = 0;
			
	if( rx_state == RX_HUNTING )
	{
		delta[LOW_RX_CHAN]   = LO_FREQ;
		delta[MID_RX_CHAN]   = CENTER_FREQ;
		delta[HIH_RX_CHAN]   = HI_FREQ;
		max_mag              = 0;
		m_viterbi_confidence = 0;
		m_frequency_error    = 0;

		/* Check all three channels */
		
		for( i = 0; i < RX_CHANNELS; i++ )
		{
			rx_downconvert( in, frate_b[i], hrate_b[i], &acc[i], delta[i] );
	        start[i] = preamble_hunt( hrate_b[i], &mag[i] );
	       	
			if( mag[i] > max_mag )
			{
				preamble_start = start[i];
				data_start     = preamble_start + (PREAMBLE_LENGTH*SPS);
				rx_chan        = i;
				max_mag        = mag[i];
			}
		}	     
		printf("s4285: %c0/%c1/%c2 %3d/%3d/%3d %f/%f/%f\n",
			(rx_chan==0)?'*':' ', (rx_chan==1)?'*':' ', (rx_chan==2)?'*':' ',
			start[0], start[1], start[2], mag[0], mag[1], mag[2]
		);
//return;
TaskFastIntr("s4285_rx1");
		
		/* Train on the probe sequence of the best channel */
		equalize_reset();
		preamble_matches = train_and_equalize_on_preamble( &hrate_b[rx_chan][preamble_start] );
TaskFastIntr("s4285_rx2");

		m_preamble_errors = PREAMBLE_LENGTH - preamble_matches;
		//printf("s4285 RX_HUNTING: INDEX %3d MATCHES %3d ERRORS %3d MSEC %.3f\n",
		//	preamble_start,preamble_matches,m_preamble_errors, (float) t_us / 1000);
		
		if( ( m_preamble_errors <= 15 ) && ( preamble_check( &hrate_b[rx_chan][preamble_start] ) != 0 ) )
		{
			printf("s4285 RX_HUNTING: preamble okay!\n");
		    /* Find frequency error */
			initial_doppler_correct( &hrate_b[rx_chan][preamble_start], &sync_delta );
			report_frequency_error( sync_delta, rx_chan );

			/* correct entire buffered samples */
			rx_final_downconvert( &hrate_b[rx_chan][0], &sync_delta );
			rx_final_downconvert( &hrate_b[rx_chan][HALF_SAMPLE_BLOCK_SIZE], &sync_delta );
			rx_final_downconvert( &hrate_b[rx_chan][SAMPLE_BLOCK_SIZE], &sync_delta );

			/* re-equalize */
			equalize_reset();
TaskFastIntr("s4285_rx3");

			/* Train and equalize on the frequency corrected preamble */
			preamble_matches = train_and_equalize_on_preamble( &hrate_b[rx_chan][preamble_start] );
TaskFastIntr("s4285_rx4");

			/* Actions done whatsoever */
			bad_preamble_count  = 0;
			rx_state            = RX_DATA;
			viterbi_decode_reset();
			sync_deinterleaver();
			deinterleaver_reset();

			/* Normal data reception */
			if (do_demod) demodulate_and_equalize( &hrate_b[rx_chan][data_start] );			
TaskFastIntr("s4285_rx5");
		}
	}	// RX_HUNTING
	else
	{	// RX_DATA
T_EN(dn);
		rx_downconvert( in, frate_b[rx_chan], hrate_b[rx_chan], &acc[rx_chan], delta[rx_chan] );
		rx_final_downconvert( &hrate_b[rx_chan][SAMPLE_BLOCK_SIZE], &sync_delta );

T_EX(dn);
TaskFastIntr("s4285_rx6");
T_EN(pr);
		preamble_matches = train_and_equalize_on_preamble( &hrate_b[rx_chan][preamble_start] );

		if (rx_callback) this->rx_callback(rx_callback_arg, &hrate_b[rx_chan][SAMPLE_BLOCK_SIZE], HALF_SAMPLE_BLOCK_SIZE, 1);

		sync_deinterleaver();
T_EX(pr);
TaskFastIntr("s4285_rx7");
T_EN(de);
		if (do_demod) demodulate_and_equalize( &hrate_b[rx_chan][data_start] );
T_EX(de);
TaskFastIntr("s4285_rx8");
T_EN(co);

		m_preamble_errors = PREAMBLE_LENGTH - preamble_matches;
		//printf("s4285 RX_DATA: INDEX %3d MATCHES %3d MSEC %.3f\n",
		//	preamble_start,preamble_matches,m_preamble_errors, (float) t_us / 1000);

		if( ( m_preamble_errors >= 15 ) && ( preamble_check( &hrate_b[rx_chan][preamble_start] ) == 0 ) )
		{
			bad_preamble_count++;
			if( bad_preamble_count >= con_bad_probe_threshold )
			{
			    rx_state = RX_HUNTING;
			}
			/* See if sync position has changed, update if it has */
        	updated_preamble_start = preamble_hunt( hrate_b[rx_chan], &mag[0]);       
			if( preamble_start != updated_preamble_start )
			{
				preamble_start = updated_preamble_start;
			    data_start     = preamble_start + (PREAMBLE_LENGTH*SPS);
			    equalize_reset();
			}
		}	// RX_DATA preamble errors
		else
		{	// RX_DATA normal
			doppler_correct( &hrate_b[rx_chan][preamble_start], &sync_delta );
			report_frequency_error( sync_delta, rx_chan );
			bad_preamble_count = 0;
		}	
T_EX(co);
TaskFastIntr("s4285_rx9");
#if 1
printf("s4285 %7.3f: %6.3fdn %6.3fco %6.3fde %6.3fco\n",
	f_dn + f_pr + f_de + f_co, f_dn, f_pr, f_de, f_co);
#endif

	}

	status_string();
	//printf("s4285: %s\n", m_status_text);
}

void CSt4285::check_fast_intr()
{
	run_us += timer_us() - run_start;
	TaskFastIntr("s4285_fastintr");
	run_start = timer_us();
}

//
// Input samples at 48000
//
void CSt4285::process_rx_block( float *in, int length )
{

	for( int i = 0; i < length; i++ )
	{
		if( m_sample_counter == 0 )
		{
			// Effective Sample rate is 9600
			rx_block_buffer[rx_block_buffer_index++] =  in[i];

			if( rx_block_buffer_index == RX_BLOCK_LENGTH )
			{
				process_rx_block( rx_block_buffer );
				rx_block_buffer_index = 0;
			}
		}
		m_sample_counter = ++m_sample_counter%5;
	}
}
void CSt4285::process_rx_block( short *in, int length, float scale )
{

	for( int i = 0; i < length; i++ )
	{
		rx_block_buffer[rx_block_buffer_index++] = (float) in[i] / scale;

		if( rx_block_buffer_index == RX_BLOCK_LENGTH )
		{
			process_rx_block( rx_block_buffer );
			rx_block_buffer_index = 0;
		}
	}
}
int CSt4285::process_rx_block_tx_loopback( void )
{
	if (tx_sample_rx_count < RX_BLOCK_LENGTH)
		return 0;
	
	for( int i = 0; i < RX_BLOCK_LENGTH; i++ ) {
		rx_block_buffer[i] = tx_samples[tx_sample_rx_ra];
		tx_sample_rx_ra++;
		if (tx_sample_rx_ra == TX_SAMPLE_BLOCK_SIZE)
			tx_sample_rx_ra = 0;
		tx_sample_rx_count--;
	}
	process_rx_block( rx_block_buffer );

	if (tx_sample_rx_count < RX_BLOCK_LENGTH)
		return 0;
	return 1;
}

#endif
