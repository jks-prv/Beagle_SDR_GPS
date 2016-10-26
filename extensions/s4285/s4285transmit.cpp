// Copyright (c) 2000-2016, C.H Brain, G4GUO

#include "ext.h"

#ifdef EXT_S4285

#include "stdafx.h"
#include "st4285.h"
#define _USE_MATH_DEFINES
#include "math.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>

extern FComplex symbol_psk2[2];
extern FComplex symbol_psk4[4];
extern FComplex symbol_psk8[8];
extern float    tx_coffs[CSt4285::TX_FILTER_LENGTH];
extern FComplex tx_preamble_lookup[CSt4285::PREAMBLE_LENGTH]; 

/* Functions */

// symbol is 8-PSK, except BPSK for preamble
void CSt4285::tx_symbol( FComplex symbol )
{
	int             i,k;
	FComplex        output;
	FComplex        Csample;
	float	        sample;
	
	/* Update the filter input buffer */
	
	for( i = 0; i < (TX_FILTER_LENGTH/4)-1; i++)
	{
		tx_buffer[i] = tx_buffer[i+1];
	}
	tx_buffer[i] = symbol;
	
	/* Do the filtering and produce 4 output samples */
	// i.e. 2400 baud symbol rate * 4 samples/symbol = 9600 Hz sample rate
	for( k = 0; k < 4; k++ )
	{
		output.re = tx_buffer[0].re*tx_coffs[k];
		output.im = tx_buffer[0].im*tx_coffs[k];
		
		for( i = 1; i < (TX_FILTER_LENGTH/4); i++)
		{
			output.re += tx_buffer[i].re*tx_coffs[(i*4)+k];
			output.im += tx_buffer[i].im*tx_coffs[(i*4)+k];
		}
		
		// complex samples remain at baseband
		tx_Csamples[tx_sample_wa] = output;

		/* Up convert the samples to passband */
		sample  = (float)cos(tx_acc)*output.re;	
		sample -= (float)sin(tx_acc)*output.im;
		tx_acc += (float)(2*M_PI*(CENTER_FREQUENCY)/sample_rate);	
		if( tx_acc >= 2*M_PI ) tx_acc -= (float)(2*M_PI);
		tx_samples[tx_sample_wa] = sample;
		tx_sample_wa++;
		tx_sample_tx_count++;
		tx_sample_rx_count++;
		if( tx_sample_wa == TX_SAMPLE_BLOCK_SIZE )
		{
			tx_sample_wa = 0; 
		}
	}	
}
void CSt4285::tx_preamble( void )
{
	int i;
	
	for( i = 0 ; i < PREAMBLE_LENGTH ; i++ )
	{
		tx_symbol( tx_preamble_lookup[i]);
	}
}
void CSt4285::tx_probe( void )
{
	int i;
	
	for( i = 0 ; i < PROBE_LENGTH ; i++ )
	{
		tx_symbol( scrambler_table[tx_scramble_count]);
		tx_scramble_count++;
	}
}
static int do_sync = 2;
void CSt4285::tx_frame_state( void )
{
	switch( tx_count )
	{
		case 0:		
			sync_interleaver();
			if (do_sync >= 2) tx_preamble();
			tx_count += PREAMBLE_LENGTH;
			tx_scramble_count = 0;
			break;
		case (PREAMBLE_LENGTH+DATA_LENGTH):
			if (do_sync >= 1) tx_probe();
			tx_count += PROBE_LENGTH;
			break;
		case (PREAMBLE_LENGTH+DATA_LENGTH+PROBE_LENGTH+DATA_LENGTH):
			if (do_sync >= 1) tx_probe();
			tx_count += PROBE_LENGTH;
			break;
		case (PREAMBLE_LENGTH+DATA_LENGTH+PROBE_LENGTH+DATA_LENGTH+DATA_LENGTH+PROBE_LENGTH):
			if (do_sync >= 1) tx_probe();
			tx_count += PROBE_LENGTH;
			break;
		default:
			break;
	}				
	tx_count = ++tx_count%SYMBOLS_PER_FRAME;
}
void CSt4285::tx_bpsk( int bit )
{
	FComplex   symbol;

	tx_frame_state();
	
	// BPSK because the data is a single bit. But the scrambler_table is an
	// 8-PSK symbol, so the result is also 8-PSK.
	symbol.re = cmultReal(symbol_psk2[bit],scrambler_table[tx_scramble_count]);
	symbol.im = cmultImag(symbol_psk2[bit],scrambler_table[tx_scramble_count]);

	tx_scramble_count++;
	
	tx_symbol( symbol );
}
void CSt4285::tx_qpsk( int dibit )
{
	FComplex   symbol;
		
	tx_frame_state();

	symbol.re = cmultReal(symbol_psk4[dibit],scrambler_table[tx_scramble_count]);
	symbol.im = cmultImag(symbol_psk4[dibit],scrambler_table[tx_scramble_count]);

	tx_scramble_count++;
	
	tx_symbol( symbol );
}
void CSt4285::tx_psk8( int tribit )
{
	FComplex   symbol;

	tx_frame_state();
		
	symbol.re = cmultReal(symbol_psk8[tribit],scrambler_table[tx_scramble_count]);
	symbol.im = cmultImag(symbol_psk8[tribit],scrambler_table[tx_scramble_count]);

	tx_scramble_count++;
	
	tx_symbol( symbol );
}

// Called at user data rate (75, 150, 300, 600, 1200, 2400, 3600 bps).
// Ultimately calls tx_symbol() at 1200 baud after FEC.
// And with tx_symbol() calls from the 50% additional overhead of preamble and probes
// gives final 2400 baud symbol rate (aka line rate, manipulation speed, etc.)
// i.e. preamble @ 80b + 3 * probe @ 16b = 128b (50%), 4 * data @ 32b = 128b (50%)
void CSt4285::tx_octet( unsigned char octet )
{
	int bits[8];
	int bit1,bit2,bit3,bit4;
	int i,j;
	
	bits[0] = octet&0x01?1:0;
	bits[1] = octet&0x02?1:0;
	bits[2] = octet&0x04?1:0;
	bits[3] = octet&0x08?1:0;
	bits[4] = octet&0x10?1:0;
	bits[5] = octet&0x20?1:0;
	bits[6] = octet&0x40?1:0;
	bits[7] = octet&0x80?1:0;
	
	switch( tx_mode&0x000F)
	{
		case TX_75_BPS:
		    /* 75 Baud */
			for( i = 0; i < 8; i++ )
			{
				convolutional_encode( bits[i],&bit1,&bit2);
				for( j = 0 ; j < 8 ; j++ )
				{
	        		tx_bpsk( interleave( bit1 ));
	        		tx_bpsk( interleave( bit2 ));
				}
			}
			break;
		case TX_150_BPS:
			/* 150 Baud */
			for( i = 0; i < 8; i++ )
			{
				convolutional_encode( bits[i],&bit1,&bit2);
		
				for( j = 0 ; j < 4 ; j++ )
				{
	        		tx_bpsk( interleave( bit1 ));
	        		tx_bpsk( interleave( bit2 ));
				}
			}
			break;
		case TX_300_BPS:
			/* 300 Baud */
			for( i = 0; i < 8; i++ )
			{
				convolutional_encode( bits[i],&bit1,&bit2 );
		
				for( j = 0 ; j < 2 ; j++ )
				{
	        		tx_bpsk( interleave( bit1 ));
	        		tx_bpsk( interleave( bit2 ));
				}
			}
			break;
		case TX_600_BPS:
			/* 600 Baud */
			for( i = 0; i < 8; i++ )
			{
				convolutional_encode( bits[i],&bit1,&bit2 );
	        	tx_bpsk( interleave( bit1 ));
	        	tx_bpsk( interleave( bit2 ));
			}			
			break;
		case TX_1200_BPS:
		    /* 1200 Baud */
			for( i = 0; i < 8; i++ )
			{
				convolutional_encode( bits[i],&bit1,&bit2 );
	        	bit1 = interleave( bit1 );
	        	bit2 = interleave( bit2 );
	        	bit1 = (bit1<<1)+bit2;
	        	tx_qpsk( bit1 );
			}
			break;
		case TX_2400_BPS:
		    /* 2400 */
			for( i = 0; i < 4; i++ )
			{
				convolutional_encode( bits[i*2],&bit1,&bit2 );
				convolutional_encode( bits[(i*2)+1],&bit3,&bit4 );
	        	bit1 = interleave( bit1 );
	        	bit2 = interleave( bit2 );
	        	bit3 = interleave( bit3 );
	        	bit4 = interleave( bit4 );

	        	bit1 = (bit1<<2)+(bit2<<1)+bit3; /* bit 4 thrown away */
	        	tx_psk8( bit1 );
			}
			break;
		case TX_1200U_BPS:
			/* 1200 Baud Uncoded */
			for( i = 0; i < 8; i++ )
			{
	        	tx_bpsk( bits[i] );
			}			
			break;
		case TX_2400U_BPS:
			for( i = 0; i < 4; i++ )
			{
				bit1 = (bits[(i*2)]<<1)+bits[(i*2)+1];
	        	tx_qpsk( bit1 );
			}			
			break;
		case TX_3600U_BPS:
			for( i = 0; i < 8; i++ )
			{
				tx_tribit   = tx_tribit<<1;
				tx_tribit  += bits[i];
				tx_bitcount = ++tx_bitcount%3;
				if( tx_bitcount == 0 ) tx_psk8( tx_tribit&7 );				
			}			
			break;
		default:
			break;
	}			
}
void CSt4285::tx_output_iq( FComplex *data, int length, float scale )
{
	int i;
	FComplex *dp = data;
	
	//printf("s4285 V1 tx_output want %d\n", length);
	while (tx_sample_tx_count < length)
		tx_octet(this->tx_callback());
	
	for (i=0; i < length; i++) {
		dp->re = scale * tx_Csamples[tx_sample_tx_ra].re;
		dp->im = scale * tx_Csamples[tx_sample_tx_ra].im;
		tx_sample_tx_ra++;
		dp++;
		if (tx_sample_tx_ra == TX_SAMPLE_BLOCK_SIZE)
			tx_sample_tx_ra = 0;
		tx_sample_tx_count--;
	}
}
void CSt4285::tx_output_real( short *data, int length, float scale )
{
	int i;
	short *dp = data;
	
	//printf("s4285 tx_output want %d\n", length);
	while (tx_sample_tx_count < length)
		tx_octet(this->tx_callback());
	
	for (i=0; i < length; i++) {
		*dp++ = roundf(scale * tx_samples[tx_sample_tx_ra++]);
		if (tx_sample_tx_ra == TX_SAMPLE_BLOCK_SIZE)
			tx_sample_tx_ra = 0;
		tx_sample_tx_count--;
	}
}
/*
 *
 *
 * Publically visible routines.
 *
 *
 */
void CSt4285::tx_frame( unsigned char *data, int length )
{
//	write_input_queue( data, length );
}
void CSt4285::transmit_callback( void )
{
//	int i, length;
//	unsigned char data[100];

//	length = read_input_queue( data, get_tx_octets_per_block());

//	for( i=0; i< length; i++ )	tx_octet( ct, data[i] );
}

#endif
