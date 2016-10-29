//
// Modified convolutional interleaver
//

// Copyright (c) 2000-2016, C.H Brain, G4GUO

#include "stdafx.h"
#include "math.h"
#include "st4285.h"

#include "ext.h"

#ifdef EXT_S4285

/* Modified interleaver mapping */
int rmod[CSt4285::INTERLEAVER_NR_ROWS]=
{
	0,9,18,27,4,13,22,31,8,17,26,3,12,21,30,7,16,25,2,11,20,29,6,15,24,1,10,19,28,5,14,23
};

//
// Configure once only
//

void CSt4285::interleaver_configure( void  )
{
	int       n;
	int nr_rows;
	int row_inc;
	int array_size;
	//
	// Calculate the size of array needed
	//
	nr_rows   = 32;
	row_inc   =  1;
	array_size = 0;

	for( n = 0; n < nr_rows; n++)
 	{
 		array_size += (n*row_inc)+1;
 	}
 	//printf("Array Size %d\n",array_size);
}
//
// Reset the interleaver
//
void CSt4285::interleaver_reset( void )
{
	int n;
	
	for( n = 0; n < tx_array_size ; n++) in[n]  = 0;

	iip[0] = iop[0] = 0;

	for( n = 1; n < INTERLEAVER_NR_ROWS ; n++)
	{
		iip[n] = iop[n-1]+1; 
		iop[n] = iip[n]+(n*tx_row_inc); 
	}
	in_row   = 0;
}
void CSt4285::deinterleaver_reset( void  )
{
	int n;
	
	for( n = 0; n < rx_array_size ; n++) de[n] = 0.0;

	dip[0] = 0;
	dop[0] = ((INTERLEAVER_NR_ROWS-1)*rx_row_inc);

	for( n = 1; n < INTERLEAVER_NR_ROWS ; n++)
	{
		dip[n] = dop[n-1]+1; 
		dop[n] = dip[n]+((INTERLEAVER_NR_ROWS-n-1)*rx_row_inc); 
	}
	de_row   = 0;
}
//
// Set Interleaver parameters
//
void CSt4285::interleaver_enable( void )
{
	interleaver_status = INTERLEAVER_ENABLED;
}
void CSt4285::interleaver_disable( void )
{
	interleaver_status = INTERLEAVER_DISABLED;
}
void CSt4285::deinterleaver_enable( void )
{
	deinterleaver_status = INTERLEAVER_ENABLED;
}
void CSt4285::deinterleaver_disable( void )
{
	deinterleaver_status = INTERLEAVER_DISABLED;
}
void CSt4285::interleaving_tx_2400_short( void )
{
	tx_row_inc    = 4;
	tx_array_size = 2016;
	interleaver_reset();
}
void CSt4285::interleaving_rx_2400_short( void )
{
	rx_row_inc    = 4;
	rx_array_size = 2016;
	deinterleaver_reset();
}
void CSt4285::interleaving_tx_2400_long( void )
{
	tx_row_inc    = 48;
	tx_array_size = 23840;
	interleaver_reset();
}
void CSt4285::interleaving_rx_2400_long( void )
{
	rx_row_inc    = 48;
	rx_array_size = 23840;
	deinterleaver_reset();
}
void CSt4285::interleaving_tx_1200_short( void )
{
	tx_row_inc    = 2;
	tx_array_size = 1024;
	interleaver_reset();
}
void CSt4285::interleaving_rx_1200_short( void )
{
	rx_row_inc    = 2;
	rx_array_size = 1024;
	deinterleaver_reset();
}
void CSt4285::interleaving_tx_1200_long( void )
{
	tx_row_inc    = 24;
	tx_array_size = 11936;
	interleaver_reset();
}
void CSt4285::interleaving_rx_1200_long( void )
{
	rx_row_inc    = 24;
	rx_array_size = 11936;
	deinterleaver_reset();
}
void CSt4285::interleaving_tx_600_75_short( void )
{
	tx_row_inc    = 1;
	tx_array_size = 528;
	interleaver_reset();
}
void CSt4285::interleaving_rx_600_75_short( void )
{
	rx_row_inc    = 1;
	rx_array_size = 528;
	deinterleaver_reset();
}
void CSt4285::interleaving_tx_600_75_long( void )
{
	tx_row_inc    = 12;
	tx_array_size = 5984;
	interleaver_reset();
}
void CSt4285::interleaving_rx_600_75_long( void )
{
	rx_row_inc    = 12;
	rx_array_size = 5984;
	deinterleaver_reset();
}
void CSt4285::sync_deinterleaver( void )
{
	//de_row = 0;
}
int CSt4285::interleave( int input )
{
	int n;
	
	if( interleaver_status == INTERLEAVER_ENABLED )
	{
		in[iip[rmod[in_row]]] = input;
        input          = in[iop[in_row]];
		in_row++;

		if( in_row == INTERLEAVER_NR_ROWS )
		{
			for( n = 0; n < INTERLEAVER_NR_ROWS ; n++ )
			{
				iip[n] = (iip[n]+tx_array_size-1)%tx_array_size;
				iop[n] = (iop[n]+tx_array_size-1)%tx_array_size;
			}
			in_row   = 0;
		}
	}
	return( input );
}
void CSt4285::sync_interleaver( void )
{
	//in_row = 0;
}
float CSt4285::deinterleave( float input )
{
	int n;

	if( deinterleaver_status == INTERLEAVER_ENABLED )
	{
		de[dip[de_row]] = input;
        input          = de[dop[rmod[de_row]]];
		de_row++;

		if( de_row == INTERLEAVER_NR_ROWS )
		{
			for( n = 0; n < INTERLEAVER_NR_ROWS ; n++ )
			{
				dip[n] = (dip[n]+rx_array_size-1)%rx_array_size;
				dop[n] = (dop[n]+rx_array_size-1)%rx_array_size;
			}
			de_row   = 0;
		}
	}
	return( input );
}

#endif
