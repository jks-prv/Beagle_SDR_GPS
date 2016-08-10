#include "stdafx.h"
/*
 *
 * Receive module for serial tone HF voice modem.
 *
 *
 */

// Copyright (c) 2000-2016, C.H Brain, G4GUO

#include <stdio.h>
#define _USE_MATH_DEFINES
#include <math.h>
#include "st4285.h"


extern FComplex rx_preamble_lookup[CSt4285::PREAMBLE_LENGTH];
extern float rx_coffs[CSt4285::RX_FILTER_SIZE];

#define LAMBDA 0.02
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
		real += in[i*2].re*rx_preamble_lookup[i].re;
		imag += in[i*2].im*rx_preamble_lookup[i].re;
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
	float real,imag;
	float val_a;
	float val_b;
	float val_c;

	/* Break the preamble into two 31 chip sequences */
	/* Compare aligned and non aligned magnitudes    */
	/* make decision.                                */
	
	real = in[0].re*rx_preamble_lookup[0].re;
	imag = in[0].im*rx_preamble_lookup[0].re;
	
	for( i = 1; i < 31 ; i++ )
	{
		real += in[i*2].re*rx_preamble_lookup[i].re;
		imag += in[i*2].im*rx_preamble_lookup[i].re;
	}
	val_a = (real*real)+(imag*imag);

	real = in[31].re*rx_preamble_lookup[0].re;
	imag = in[31].im*rx_preamble_lookup[0].re;
	
	for( i = 1; i < 31 ; i++ )
	{
		real += in[(i*2)+31].re*rx_preamble_lookup[i].re;
		imag += in[(i*2)+31].im*rx_preamble_lookup[i].re;
	}
	val_b = ((real*real)+(imag*imag))*10;

	real = in[62].re*rx_preamble_lookup[0].re;
	imag = in[62].im*rx_preamble_lookup[0].re;
	
	for( i = 1; i < 31 ; i++ )
	{
		real += in[(i*2)+62].re*rx_preamble_lookup[i].re;
		imag += in[(i*2)+62].im*rx_preamble_lookup[i].re;
	}
	val_c = (real*real)+(imag*imag);

	if( ( val_a > val_b ) && ( val_c > val_b ) )
	{
		return 1;
	}
	else
		return 0;
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
				
	h = (LAMBDA*mag) + (1.0-LAMBDA)*hold;
	
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
float CSt4285::initial_doppler_correct( FComplex *in , float *delta )
{
	int i;
	float real,imag,error;	

	real = cmultRealConj(in[0],in[62]);	
	imag = cmultImagConj(in[0],in[62]);	

	for( i = 2 ; i < (PREAMBLE_LENGTH-31)*2 ; i+=2 )
	{
		real += cmultRealConj(in[i],in[i+62]);	
		imag += cmultImagConj(in[i],in[i+62]);	
	}
	if( real == 0.0 ) real = (float)0.0000000001;/* No divide by zero */
	error  = (float)(atan2(imag,real)*0.008064516129);
	*delta = -error*2.0f;
	
	return error;
}
float CSt4285::doppler_correct( FComplex *in , float *delta )
{
	int i;
	float real,imag,error;	

	real = cmultRealConj(in[0],in[62]);	
	imag = cmultImagConj(in[0],in[62]);	

	for( i = 2 ; i < (PREAMBLE_LENGTH-31)*2 ; i+=2 )
	{
		real += cmultRealConj(in[i],in[i+62]);	
		imag += cmultImagConj(in[i],in[i+62]);	
	}

	error   = (float)(atan2(imag,real)*0.008064516129);
	*delta -= (float)(error*0.1);
	
	return error;
}
int CSt4285::train_and_equalize_on_preamble( FComplex *in )
{
	int       i;
	int       count;
	FComplex  symbol;
    	
	for( i = 0, count = 0 ; i < PREAMBLE_LENGTH; i++ )
	{
		symbol = equalize_train( &in[(i*2)], rx_preamble_lookup[i] );	
		if(symbol.re*rx_preamble_lookup[i].re > 0) count++;	
	}	
	return count;
}
void CSt4285::rx_downconvert( float *in, FComplex *outa, FComplex *outb, float *acc, float delta )
{
	int i;
	/* Update Received Data, in place of circular addressing  */
	for( i = 0; i < RX_FILTER_SIZE; i++ )
	{
		outa[i] =  outa[i+SAMPLE_BLOCK_SIZE];
	}
	for( i = 0; i < SAMPLE_BLOCK_SIZE; i++ )
	{
		/* Update with new samples */
		outa[i+RX_FILTER_SIZE].re =  (float)cos(*acc)*in[i];
		outa[i+RX_FILTER_SIZE].im = -(float)sin(*acc)*in[i];
		*acc         +=  delta;
		if( *acc >= 2*M_PI ) *acc -= (float)(2*M_PI);			
	}
	/* Update Received Data, in place of circular addressing  */
	for( i = 0; i < SAMPLE_BLOCK_SIZE ; i++ )
	{
		outb[i] = outb[i+HALF_SAMPLE_BLOCK_SIZE];
	}
	/* Decimate by two and filter */
	for( i = 0; i < HALF_SAMPLE_BLOCK_SIZE; i++ )
	{
		/* Run the channel filter  */
		outb[i+SAMPLE_BLOCK_SIZE] = agc( rx_filter( &outa[i*2] ));
	}
}
/*
 *
 * This is a final post mix downconvert.
 * It is a bit of a kludge.
 *
 */
void CSt4285::rx_final_downconvert( FComplex *in, float delta )
{
	int i;
	FComplex temp,osc;
	
	for( i = 0; i < HALF_SAMPLE_BLOCK_SIZE; i++ )
	{
		/* Update with new samples */
		osc.re =  (float)cos(accumulator);
		osc.im =  -(float)sin(accumulator);
		temp.re = cmultReal(osc,in[i]);
		temp.im = cmultImag(osc,in[i]);
		in[i] = temp;		
		accumulator       +=  delta;
		if( accumulator >= 2*M_PI ) accumulator -= (float)(2*M_PI);			
	}
}
void CSt4285::process_rx_block( float *in )
{
	int   		 i;
	int          start[RX_CHANNELS];
	float        mag[RX_CHANNELS], max_mag;
			
	if( rx_state == RX_HUNTING )
	{
		delta[LOW_RX_CHAN]   = LO_FREQ;
		delta[MID_RX_CHAN]   = CENTER_FREQ;
		delta[HIH_RX_CHAN]   = HI_FREQ;
		max_mag              = 0;
		m_viterbi_confidence = 0;
		m_frequency_error    = 0;

		/* Check all three channels */
		
		u4_t t_start = timer_us();
		for( i = 0; i < RX_CHANNELS; i++ )
		{
			rx_downconvert( in, in_a[i], in_b[i], &acc[i], delta[i] );
	        start[i] = preamble_hunt( in_b[i], &mag[i] );       
	       	
			if( mag[i] > max_mag )
			{
				preamble_start = start[i];
				data_start     = preamble_start + (PREAMBLE_LENGTH*2);
				rx_chan        = i;
				max_mag        = mag[i];
			}
		}	     
		printf("s4285: %c0/%c1/%c2 %3d/%3d/%3d %f/%f/%f\n",
			(rx_chan==0)?'*':' ', (rx_chan==1)?'*':' ', (rx_chan==2)?'*':' ',
			start[0], start[1], start[2], mag[0], mag[1], mag[2]
		);
//return;
//NextTask("s4285_rx1");
		
		/* Train on the probe sequence of the best channel */		
		equalize_reset();
		preamble_matches = train_and_equalize_on_preamble( &in_b[rx_chan][preamble_start] );

		m_preamble_errors = PREAMBLE_LENGTH - preamble_matches;
		u4_t t_us = timer_us() - t_start;
		printf("s4285 RX_HUNTING: INDEX %3d MATCHES %3d ERRORS %3d MSEC %.3f\n",
			preamble_start,preamble_matches,m_preamble_errors, (float) t_us / 1000);
		
		if( ( m_preamble_errors <= 15 ) && ( preamble_check( &in_b[rx_chan][preamble_start] ) != 0 ) )
		{
			printf("s4285 RX_HUNTING: preamble okay!\n");
		    /* Find frequency error */
			initial_doppler_correct( &in_b[rx_chan][preamble_start], &sync_delta );
			report_frequency_error( sync_delta, rx_chan );

			/* correct entire buffered samples */
			rx_final_downconvert( &in_b[rx_chan][0], sync_delta );
			rx_final_downconvert( &in_b[rx_chan][HALF_SAMPLE_BLOCK_SIZE], sync_delta );
			rx_final_downconvert( &in_b[rx_chan][SAMPLE_BLOCK_SIZE], sync_delta );

			/* re-equalize */
			equalize_reset();

			/* Train and equalize on the frequency corrected preamble */
			preamble_matches = train_and_equalize_on_preamble( &in_b[rx_chan][preamble_start] );

			/* Actions done whatsoever */
			bad_preamble_count  = 0;
			rx_state            = RX_DATA;
			viterbi_decode_reset();
			sync_deinterleaver();
			deinterleaver_reset();

			/* Normal data reception */
			demodulate_and_equalize( &in_b[rx_chan][data_start] );			
		}
	}
	else
	{
		rx_downconvert( in, in_a[rx_chan], in_b[rx_chan], &acc[rx_chan], delta[rx_chan] );
		rx_final_downconvert( &in_b[rx_chan][SAMPLE_BLOCK_SIZE], sync_delta );
		preamble_matches = train_and_equalize_on_preamble( &in_b[rx_chan][preamble_start] );
		sync_deinterleaver();
		demodulate_and_equalize( &in_b[rx_chan][data_start] );

		printf("s4285 RX_DATA: INDEX %3d MATCHES %3d    ",preamble_start,preamble_matches);
		m_preamble_errors = PREAMBLE_LENGTH - preamble_matches;

		if( ( m_preamble_errors >= 15 ) && ( preamble_check( &in_b[rx_chan][preamble_start] ) == 0 ) )
		{
			bad_preamble_count++;
			if( bad_preamble_count >= con_bad_probe_threshold )
			{
			    rx_state = RX_HUNTING;
			}
			/* See if sync position has changed, update if it has */
        	start[0] = preamble_hunt( in_b[rx_chan], &mag[0]);       
			if( preamble_start != start[0] )
			{
				preamble_start = start[0];
			    data_start     = preamble_start + (PREAMBLE_LENGTH*2);
			    equalize_reset();
			}
		}
		else
		{
			doppler_correct( &in_b[rx_chan][preamble_start], &sync_delta );
			report_frequency_error( sync_delta, rx_chan );
			bad_preamble_count = 0;
		}	
	}
	status_string();
	//printf("s4285: %s\n", m_status_text);
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
int CSt4285::process_rx_block( void )
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

