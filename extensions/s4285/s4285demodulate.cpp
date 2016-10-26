/*
 *
 * Demonstration demodulators for use with channel equalizer.
 *
 * Written by C.H Brain G4GUO Aug 2000
 *
 */

// Copyright (c) 2000-2016, C.H Brain, G4GUO


#include "ext.h"

#ifdef EXT_S4285

#include "stdafx.h"
#include <math.h>
#include "st4285.h"

void CSt4285::update_mse_average( FComplex error )
{
	float mse;
	
	mse = (error.re*error.re) + (error.im*error.im);
	
	mse_average -= mse_magnitudes[mse_count];
	mse_magnitudes[mse_count] = mse;
	mse_average += mse;
	mse_count = ++mse_count%MSE_HISTORY_LENGTH;
}
float CSt4285::get_mse_divisor( void )
{
	float val;
	val = (float)(1.0/(mse_average+0.000001));
	
	return(val);
}
/*
 *
 * BPSK demodulator 
 *
 */

FDemodulate CSt4285::demodulate_bpsk( FComplex symbol )
{
	FDemodulate symd;
	FComplex    sym;

	sym.re = cmultRealConj(symbol,scrambler_table[rx_scramble_count]);
	sym.im  = cmultImagConj(symbol,scrambler_table[rx_scramble_count]);

	save_constellation_symbol( sym );

	if( sym.re > 0.0 )
	{
		symd.data           = 0;
		/* Soft decision information */
		sd[soft_index++]    =  sym.re*get_mse_divisor();
		symd.dx_symbol.re = (float)0.01;		
		symd.dx_symbol.im = (float)0.0;		
	}
	else
	{
		symd.data           =  1;
		/* Soft decision information */
		sd[soft_index++]    =  sym.re*get_mse_divisor();
		symd.dx_symbol.re = -(float)0.01;		
		symd.dx_symbol.im =  (float)0.0;		
	}

	sym.re = cmultReal(symd.dx_symbol,scrambler_table[rx_scramble_count]);
	sym.im = cmultImag(symd.dx_symbol,scrambler_table[rx_scramble_count]);

	symd.dx_symbol = sym;

	/* Calculate the error */
	symd.error.re = sym.re - symbol.re;  
	symd.error.im = sym.im - symbol.im;  

	update_mse_average( symd.error );

	/* Save the actual symbol */
	symd.rx_symbol = symbol;

	return symd;	
}

/* 
 *
 * QPSK demodulator 
 *
 */

FDemodulate CSt4285::demodulate_qpsk( FComplex symbol )
{
	FDemodulate symd;
	FComplex    sym;
	float       rabs,iabs;
	float       soft;
	
	sym.re  = cmultRealConj(symbol,scrambler_table[rx_scramble_count]);
	sym.im  = cmultImagConj(symbol,scrambler_table[rx_scramble_count]);

	save_constellation_symbol( sym );

	rabs = (float)fabs( sym.re );
	iabs = (float)fabs( sym.im );
	
	
	if( rabs > iabs )
	{
		/* Real component greater */
		soft =  (rabs - iabs) * get_mse_divisor();
		if( sym.re >= 0.0 )
		{
			symd.data           = 0;
		    sd[soft_index++]    = soft;
		    sd[soft_index++]    = soft;
			symd.dx_symbol.re = (float)0.01;		
			symd.dx_symbol.im = (float)0.00;		
		}
		else
		{
			symd.data           =  3;
		    sd[soft_index++]    = -soft;
		    sd[soft_index++]    = -soft;
			symd.dx_symbol.re = -(float)0.01;		
			symd.dx_symbol.im =  (float)0.00;		
		}
	}
	else
	{
		/* Imaginary component greater */

		soft =  (iabs - rabs) * get_mse_divisor();

		if( sym.im >= 0.0 )
		{
			symd.data           =  1;
		    sd[soft_index++]    =  soft;
		    sd[soft_index++]    = -soft;
			symd.dx_symbol.re =  (float)0.00;		
			symd.dx_symbol.im =  (float)0.01;		
		}
		else
		{
			symd.data           =  2;
		    sd[soft_index++]    = -soft;
		    sd[soft_index++]    =  soft;
			symd.dx_symbol.re =  (float)0.00;		
			symd.dx_symbol.im = -(float)0.01;		
		}
	} 
	/* Calculate the perfect symbol */

	sym.re = cmultReal(symd.dx_symbol,scrambler_table[rx_scramble_count]);
	sym.im = cmultImag(symd.dx_symbol,scrambler_table[rx_scramble_count]);

	symd.dx_symbol =  sym;

	/* Calculate the error */
	symd.error.re = sym.re - symbol.re;  
	symd.error.im = sym.im - symbol.im;  

	update_mse_average( symd.error );

	/* Save the actual symbol */
	symd.rx_symbol = symbol;

	return symd;	
}

/*
 * 
 * 8PSK demodulator 
 *
 */

FDemodulate CSt4285::demodulate_8psk( FComplex symbol )
{
	FDemodulate symd;
	float       val;
	float       rabs,iabs;
	float       soft;
	int         angle;
	FComplex    sym;


	sym.re  = cmultRealConj(symbol,scrambler_table[rx_scramble_count]);
	sym.im  = cmultImagConj(symbol,scrambler_table[rx_scramble_count]);

	save_constellation_symbol( sym );
	
	rabs = (float)fabs(sym.re);
	iabs = (float)fabs(sym.im);

	val = iabs/rabs;
	
	if( val <= 0.41421356237 ) /* 22.5 deg */
	{
		soft  = (float)0.38268343237*rabs - (float)0.92387953251*iabs; /* Metric */		
		angle = 0;	
	}
	else
	{
		if( val <=1 ) /* 45 deg */
		{
				soft  = (float)0.92387953251*iabs - (float)0.38268343237*rabs; /* Metric */		
				angle = 45;		
		}
		else
		{
			if( val <= 2.4142135624 ) /* 67.5 deg */
			{
				soft  =  (float)(0.92387953251*rabs - 0.38268343237*iabs); /* Metric */		
				angle = 45;	
			}
			else
			{
				soft  = (float)(0.38268343237*iabs - 0.92387953251*rabs); /* Metric */		
				angle = 90;	
			}
		}
	}
	soft = soft*get_mse_divisor();		
	
	/* Find region */

	if( sym.re >= 0.0 )
	{
		if( sym.im < 0.0 )
		{
			angle = 360 - angle;
		}
	}
	else
	{
		if( sym.im >= 0.0 )
		{
			angle = 180 - angle;
		}
		else
		{
			angle = 180 + angle;
		}
	}
	
	switch( angle )
	{
		case 0:
		case 360:
			symd.data           =  1;
			sd[soft_index++]    =  soft;
			sd[soft_index++]    =  soft;
			sd[soft_index++]    = -soft;
			sd[soft_index++]    =  0;
			symd.dx_symbol.re =  (float)0.01000000;
			symd.dx_symbol.im =  (float)0.00000000;
			break;
		case 45:
			symd.data           =  0;
			sd[soft_index++]    =  soft;
			sd[soft_index++]    =  soft;
			sd[soft_index++]    =  soft;
			sd[soft_index++]    =  0;
			symd.dx_symbol.re =  (float)0.00707107;
			symd.dx_symbol.im =  (float)0.00707107;
			break;
		case 90:
			symd.data           =  2;
			sd[soft_index++]    =  soft;
			sd[soft_index++]    = -soft;
			sd[soft_index++]    =  soft;
			sd[soft_index++]    =  0;
			symd.dx_symbol.re =  (float)0.00000000;
			symd.dx_symbol.im =  (float)0.01000000;
			break;
		case 135:
			symd.data           =  3;
			sd[soft_index++]    =  soft;
			sd[soft_index++]    = -soft;
			sd[soft_index++]    = -soft;
			sd[soft_index++]    =  0;
			symd.dx_symbol.re = -(float)0.00707107;
			symd.dx_symbol.im =  (float)0.00707107;
			break;
		case 180:
			symd.data           =  7; 
			sd[soft_index++]    = -soft;
			sd[soft_index++]    = -soft;
			sd[soft_index++]    = -soft;
			sd[soft_index++]    =  0;
			symd.dx_symbol.re = -(float)0.01000000;
			symd.dx_symbol.im =  (float)0.00000000;
			break;
		case 225:
			symd.data           =  6;
			sd[soft_index++]    = -soft;
			sd[soft_index++]    = -soft;
			sd[soft_index++]    =  soft;
			sd[soft_index++]    =  0;
			symd.dx_symbol.re = -(float)0.00707107;
			symd.dx_symbol.im = -(float)0.00707107;
			break;
		case 270:
			symd.data		    =  4;
			sd[soft_index++]    = -soft;
			sd[soft_index++]    =  soft;
			sd[soft_index++]    =  soft;
			sd[soft_index++]    =  0;
			symd.dx_symbol.re =  (float)0.00000000;
			symd.dx_symbol.im = -(float)0.01000000;
			break;
		case 315:
			symd.data      		=  5;
			sd[soft_index++]    = -soft;
			sd[soft_index++]    =  soft;
			sd[soft_index++]    = -soft;
			sd[soft_index++]    =  0;
			symd.dx_symbol.re =  (float)0.00707107;
			symd.dx_symbol.im = -(float)0.00707107;
			break;
		default:
			break;
	}

	sym.re = cmultReal(symd.dx_symbol,scrambler_table[rx_scramble_count]);
	sym.im = cmultImag(symd.dx_symbol,scrambler_table[rx_scramble_count]);

	symd.dx_symbol = sym;

	/* Calculate the error */
	symd.error.re = sym.re - symbol.re;  
	symd.error.im = sym.im - symbol.im;  

	update_mse_average( symd.error );

	/* Save the actual symbol */
	symd.rx_symbol = symbol;

	return symd;	
}

void CSt4285::demodulate_and_equalize( FComplex *in )
{
	int i, symbol_offset, data_offset, dt;
	unsigned char data[DATA_AND_PROBE_SYMBOLS_PER_FRAME*4];
	float a,b;

	rx_scramble_count	= 0;
	symbol_offset		= 0;
	data_offset			= 0;
	soft_index			= 0;
	
TaskFastIntr("s4285_de0");
	switch(rx_mode&0x00F0)
	{
		case RX_75_BPS:
		case RX_150_BPS:
		case RX_300_BPS:
		case RX_600_BPS:
			for( i = 0; i < DATA_LENGTH ; i++ )
			{
				equalize_data( &in[symbol_offset] );
TaskFastIntr("s4285_de0_d0");
				rx_scramble_count++;
				symbol_offset +=2;
			}
			for( i = 0; i < PROBE_LENGTH ; i++ )
			{
				equalize_train( &in[symbol_offset], scrambler_train_table[rx_scramble_count] );
TaskFastIntr("s4285_de0_p0");
				rx_scramble_count++;
				symbol_offset +=2;
			}
			for( i = 0; i < DATA_LENGTH ; i++ )
			{
				equalize_data( &in[symbol_offset] );
TaskFastIntr("s4285_de0_d1");
				rx_scramble_count++;
				symbol_offset +=2;
			}
			for( i = 0; i < PROBE_LENGTH ; i++ )
			{
				equalize_train( &in[symbol_offset], scrambler_train_table[rx_scramble_count] );
TaskFastIntr("s4285_de0_p1");
				rx_scramble_count++;
				symbol_offset +=2;
			}
			for( i = 0; i < DATA_LENGTH ; i++ )
			{
				equalize_data( &in[symbol_offset] );
TaskFastIntr("s4285_de0_d2");
				rx_scramble_count++;
				symbol_offset +=2;
			}
			for( i = 0; i < PROBE_LENGTH ; i++ )
			{
				equalize_train( &in[symbol_offset], scrambler_train_table[rx_scramble_count] );
TaskFastIntr("s4285_de0_p2");
				rx_scramble_count++;
				symbol_offset +=2;
			}
			for( i = 0; i < DATA_LENGTH ; i++ )
			{
				equalize_data( &in[symbol_offset] );
TaskFastIntr("s4285_de0_d3");
				rx_scramble_count++;
				symbol_offset +=2;
			}
			break;
		case RX_1200_BPS:
			for( i = 0; i < DATA_LENGTH ; i++ )
			{
				equalize_data( &in[symbol_offset] );
				rx_scramble_count++;
				symbol_offset +=2;
			}
			for( i = 0; i < PROBE_LENGTH ; i++ )
			{
				equalize_train( &in[symbol_offset], scrambler_train_table[rx_scramble_count] );
				rx_scramble_count++;
				symbol_offset +=2;
			}
			for( i = 0; i < DATA_LENGTH ; i++ )
			{
				equalize_data( &in[symbol_offset] );
				rx_scramble_count++;
				symbol_offset +=2;
			}
			for( i = 0; i < PROBE_LENGTH ; i++ )
			{
				equalize_train( &in[symbol_offset], scrambler_train_table[rx_scramble_count] );
				rx_scramble_count++;
				symbol_offset +=2;
			}
			for( i = 0; i < DATA_LENGTH ; i++ )
			{
				equalize_data( &in[symbol_offset] );
				rx_scramble_count++;
				symbol_offset +=2;
			}
			for( i = 0; i < PROBE_LENGTH ; i++ )
			{
				equalize_train( &in[symbol_offset], scrambler_train_table[rx_scramble_count] );
				rx_scramble_count++;
				symbol_offset +=2;
			}
			for( i = 0; i < DATA_LENGTH ; i++ )
			{
				equalize_data( &in[symbol_offset] );
				rx_scramble_count++;
				symbol_offset +=2;
			}
			break;
		case RX_2400_BPS:
			for( i = 0; i < DATA_LENGTH ; i++ )
			{
				equalize_data( &in[symbol_offset] );
				rx_scramble_count++;
				symbol_offset +=2;
			}
			for( i = 0; i < PROBE_LENGTH ; i++ )
			{
				equalize_train( &in[symbol_offset], scrambler_train_table[rx_scramble_count] );
				rx_scramble_count++;
				symbol_offset +=2;
			}

			for( i = 0; i < DATA_LENGTH ; i++ )
			{
				equalize_data( &in[symbol_offset] );
				rx_scramble_count++;
				symbol_offset +=2;
			}

			for( i = 0; i < PROBE_LENGTH ; i++ )
			{
				equalize_train( &in[symbol_offset], scrambler_train_table[rx_scramble_count] );
				rx_scramble_count++;
				symbol_offset +=2;
			}

			for( i = 0; i < DATA_LENGTH ; i++ )
			{
				equalize_data( &in[symbol_offset] );
				rx_scramble_count++;
				symbol_offset +=2;
			}
			for( i = 0; i < PROBE_LENGTH ; i++ )
			{
				equalize_train( &in[symbol_offset], scrambler_train_table[rx_scramble_count] );
				rx_scramble_count++;
				symbol_offset +=2;
			}
			for( i = 0; i < DATA_LENGTH ; i++ )
			{
				equalize_data( &in[symbol_offset] );
				rx_scramble_count++;
				symbol_offset +=2;
			}
			break;
		case RX_1200U_BPS:
			for( i = 0; i < DATA_LENGTH ; i++ )
			{
				data[data_offset++] = equalize_data( &in[symbol_offset] ).data;
				rx_scramble_count++;
				symbol_offset +=2;
			}
			for( i = 0; i < PROBE_LENGTH ; i++ )
			{
				equalize_train( &in[symbol_offset], scrambler_train_table[rx_scramble_count] );
				rx_scramble_count++;
				symbol_offset +=2;
			}
			for( i = 0; i < DATA_LENGTH ; i++ )
			{
				data[data_offset++] = equalize_data( &in[symbol_offset] ).data;
				rx_scramble_count++;
				symbol_offset +=2;
			}
			for( i = 0; i < PROBE_LENGTH ; i++ )
			{
				equalize_train( &in[symbol_offset], scrambler_train_table[rx_scramble_count] );
				rx_scramble_count++;
				symbol_offset +=2;
			}
			for( i = 0; i < DATA_LENGTH ; i++ )
			{
				data[data_offset++] = equalize_data( &in[symbol_offset] ).data;
				rx_scramble_count++;
				symbol_offset +=2;
			}
			for( i = 0; i < PROBE_LENGTH ; i++ )
			{
				equalize_train( &in[symbol_offset], scrambler_train_table[rx_scramble_count] );
				rx_scramble_count++;
				symbol_offset +=2;
			}
			for( i = 0; i < DATA_LENGTH ; i++ )
			{
				data[data_offset++] = equalize_data( &in[symbol_offset] ).data;
				rx_scramble_count++;
				symbol_offset +=2;
			}
			break;
		case RX_2400U_BPS:
			for( i = 0; i < DATA_LENGTH ; i++ )
			{
				dt = equalize_data( &in[symbol_offset] ).data;
				data[data_offset++] = dt&2?1:0;
				data[data_offset++] = dt&1?1:0;
				rx_scramble_count++;
				symbol_offset +=2;
			}
			for( i = 0; i < PROBE_LENGTH ; i++ )
			{
				equalize_train( &in[symbol_offset], scrambler_train_table[rx_scramble_count] );
				rx_scramble_count++;
				symbol_offset +=2;
			}
			for( i = 0; i < DATA_LENGTH ; i++ )
			{
				dt = equalize_data( &in[symbol_offset] ).data;
				data[data_offset++] = dt&2?1:0;
				data[data_offset++] = dt&1?1:0;
				rx_scramble_count++;
				symbol_offset +=2;
			}
			for( i = 0; i < PROBE_LENGTH ; i++ )
			{
				equalize_train( &in[symbol_offset], scrambler_train_table[rx_scramble_count] );
				rx_scramble_count++;
				symbol_offset +=2;
			}
			for( i = 0; i < DATA_LENGTH ; i++ )
			{
				dt = equalize_data( &in[symbol_offset] ).data;
				data[data_offset++] = dt&2?1:0;
				data[data_offset++] = dt&1?1:0;
				rx_scramble_count++;
				symbol_offset +=2;
			}
			for( i = 0; i < PROBE_LENGTH ; i++ )
			{
				equalize_train( &in[symbol_offset], scrambler_train_table[rx_scramble_count] );
				rx_scramble_count++;
				symbol_offset +=2;
			}
			for( i = 0; i < DATA_LENGTH ; i++ )
			{
				dt = equalize_data( &in[symbol_offset] ).data;
				data[data_offset++] = dt&2?1:0;
				data[data_offset++] = dt&1?1:0;
				rx_scramble_count++;
				symbol_offset +=2;
			}
			break;
		case RX_3600U_BPS:
			for( i = 0; i < DATA_LENGTH ; i++ )
			{
				dt = equalize_data( &in[symbol_offset] ).data;
				data[data_offset++] = dt&4?1:0;
				data[data_offset++] = dt&2?1:0;
				data[data_offset++] = dt&1?1:0;
				rx_scramble_count++;
				symbol_offset +=2;
			}
			for( i = 0; i < PROBE_LENGTH ; i++ )
			{
				equalize_train( &in[symbol_offset], scrambler_train_table[rx_scramble_count] );
				rx_scramble_count++;
				symbol_offset +=2;
			}
			for( i = 0; i < DATA_LENGTH ; i++ )
			{
				dt = equalize_data( &in[symbol_offset] ).data;
				data[data_offset++] = dt&4?1:0;
				data[data_offset++] = dt&2?1:0;
				data[data_offset++] = dt&1?1:0;
				rx_scramble_count++;
				symbol_offset +=2;
			}
			for( i = 0; i < PROBE_LENGTH ; i++ )
			{
				equalize_train( &in[symbol_offset], scrambler_train_table[rx_scramble_count] );
				rx_scramble_count++;
				symbol_offset +=2;
			}
			for( i = 0; i < DATA_LENGTH ; i++ )
			{
				dt = equalize_data( &in[symbol_offset] ).data;
				data[data_offset++] = dt&4?1:0;
				data[data_offset++] = dt&2?1:0;
				data[data_offset++] = dt&1?1:0;
				rx_scramble_count++;
				symbol_offset +=2;
			}
			for( i = 0; i < PROBE_LENGTH ; i++ )
			{
				equalize_train( &in[symbol_offset], scrambler_train_table[rx_scramble_count] );
				rx_scramble_count++;
				symbol_offset +=2;
			}
			for( i = 0; i < DATA_LENGTH ; i++ )
			{
				dt = equalize_data( &in[symbol_offset] ).data;
				data[data_offset++] = dt&4?1:0;
				data[data_offset++] = dt&2?1:0;
				data[data_offset++] = dt&1?1:0;
				rx_scramble_count++;
				symbol_offset +=2;
			}
		    break;
		default:
			break;
	}
	
	/* De-interleave if required */	
	
TaskFastIntr("s4285_de1");
int do_viterbi = 0;
if (do_viterbi) {
	if( (rx_mode&0x00F0) <= RX_2400_BPS )
	{ 
		for( i = 0 ; i < soft_index; i++ ) sd[i] = deinterleave( sd[i] ); 
	}

	/* Error correct and pack for output */

TaskFastIntr("s4285_de2");
	switch( rx_mode&0x00F0)
	{
		case RX_75_BPS:
		    /* 75 Baud */		    
			for( i = 0; i<DATA_LENGTH/4; i++ ) 
			{
				a = (sd[i*16]+sd[(i*16)+2]+sd[(i*16)+4]+sd[(i*16)+6]+sd[(i*16)+8]+sd[(i*16)+10]+sd[(i*16)+12]+sd[(i*16)+14])/8;
				b = (sd[(i*16)+1]+sd[(i*16)+3]+sd[(i*16)+5]+sd[(i*16)+7]+sd[(i*16)+9]+sd[(i*16)+11]+sd[(i*16)+13]+sd[(i*16)+15])/8;
				data[data_offset++] = viterbi_decode( a, b );
			}
			break;
		case RX_150_BPS:
			/* 150 Baud */
			for( i = 0; i<DATA_LENGTH/2; i++ )
			{
				a = (sd[i*8]+sd[(i*8)+2]+sd[(i*8)+4]+sd[(i*8)+6])/4;
				b = (sd[(i*8)+1]+sd[(i*8)+3]+sd[(i*8)+5]+sd[(i*8)+7])/4;
				data[data_offset++] = viterbi_decode( a, b );
			}
			break;
		case RX_300_BPS:
			/* 300 Baud */
			for( i = 0; i<DATA_LENGTH; i++ )
			{
				a = (sd[i*4]+sd[(i*4)+2])/2;
				b = (sd[(i*4)+1]+sd[(i*4)+3])/2;
				data[data_offset++] = viterbi_decode( a, b );
			}
			break;
		case RX_600_BPS:
			/* 600 Baud */
			for( i = 0; i<DATA_LENGTH*2; i++ )
			{
				data[data_offset++] = viterbi_decode( sd[i*2],sd[(i*2)+1]);
TaskFastIntr("s4285_de2_vd");
			}
			break;
		case RX_1200_BPS:
		    /* 1200 Baud */
			for( i = 0; i<DATA_LENGTH*4; i++ )
			{
				data[data_offset++] = viterbi_decode( sd[i*2],sd[(i*2)+1]);
			}
			break;
		case RX_2400_BPS:
		    /* 2400 */
			for( i = 0; i<DATA_LENGTH*8; i++ )
			{
				data[data_offset++] = viterbi_decode( sd[(i*2)],sd[(i*2)+1]);
			}
			break;
		case RX_1200U_BPS:
			break;
		case RX_2400U_BPS:
			break;
		case RX_3600U_BPS:
		    break;
		default:
			break;
	}
}

TaskFastIntr("s4285_de3");
	// Output data
	for( i = 0; i < data_offset; i++ )
	{
		output_data[output_offset] = data[i];
		output_offset++;
		if (output_offset == OUTPUT_DATA_LENGTH) {
			//printf("s4285 WARNING: output_data overflow\n");
			output_offset = 0;
		}
	}
TaskFastIntr("s4285_de4");
}

#endif
