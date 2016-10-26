//
// Root Kalman Equalizer.
//

// Copyright (c) 2000-2016, C.H Brain, G4GUO

#include "ext.h"

#ifdef EXT_S4285

#include "stdafx.h"
#include <stdio.h>
#include <math.h>
#include "st4285.h"

/*
 *
 * Diagnostic routine
 *
 *
 */
void CSt4285::kalman_reset_coffs( void )
{
	int i;
	
	for( i=0; i < KN_44 ; i++ )
	{
		c[i].re = 0.0;
		c[i].im = 0.0;
	}
}
void CSt4285::kalman_reset_ud( void )
{
	int i,j;

	for( j = 0 ; j < KN_44 ; j++ )
	{
		for( i = 0; i < j ; i++ )

		{
			u[i][j].re = 0.0;
			u[i][j].im = 0.0;
		}
		d[j]         = 1.0;
	}
}
/*
 *
 *
 * Reset Kalman variables, to ensure stability 
 *
 *
 */
void CSt4285::kalman_reset( void )
{
	kalman_reset_ud();
	kalman_reset_coffs();
}

/*
 *
 *
 * Initialise this module
 *
 *
 */
void CSt4285::kalman_init( void )
{
	q      = (float)0.008;
	E      = (float)0.01;

	kalman_reset_ud();
	kalman_reset_coffs();
}

/*
 *
 *
 * Modified Root Kalman gain Vector estimator
 *
 *
 */
void CSt4285::kalman_calculate( FComplex *x )
{
	int        i,j;
	FComplex   B0;
	float      hq;
	float      B;
	float      ht;

   	f[0].re =  x[0].re;               // 6.2
	f[0].im = -x[0].im;

	for( j = 1; j < KN_44 ; j++)              // 6.3
	{
		f[j].re  = cmultRealConj(u[0][j],x[0]) + x[j].re; 
		f[j].im  = cmultImagConj(u[0][j],x[0]) - x[j].im;
		for( i = 1 ; i < j ; i++ )
		{			
			f[j].re += cmultRealConj(u[i][j],x[i]);
			f[j].im += cmultImagConj(u[i][j],x[i]); 
		}
	}

	for( j = 0; j < KN_44 ; j++)                // 6.4
	{
		g[j].re = d[j]*f[j].re;
		g[j].im = d[j]*f[j].im;
	}
    a[0] = E + cmultRealConj(g[0],f[0]); 	 // 6.5

	for( j = 1; j < KN_44 ; j++ ) // 6.6
	{
		a[j] = a[j-1] + cmultRealConj(g[j],f[j]);
	}
	hq  = 1 + q;                              // 6.7
	ht  = a[KN_44-1]*q;

	y = (float)1.0/(a[0]+ht);                       // 6.19

	d[0] = d[0] * hq * ( E + ht ) * y;       // 6.20

	// 6.10 - 6.16 (Calculate recursively)

	for( j = 1; j < KN_44 ; j++ )
	{
		B = a[j-1] + ht;                 // 6.21

		h[j].re = -f[j].re*y;        // 6.11
		h[j].im = -f[j].im*y;

		y = (float)1.0/(a[j]+ht);               // 6.22

		d[j] = d[j]*hq*B*y;              // 6.13

 		for( i = 0; i < j ; i++ )
		{
			B0           =  u[i][j];
			u[i][j].re =  B0.re + cmultRealConj(h[j],g[i]); // 6.15
			u[i][j].im =  B0.im + cmultImagConj(h[j],g[i]);

			g[i].re +=  cmultRealConj(g[j],B0);               // 6.16
			g[i].im +=  cmultImagConj(g[j],B0);
		}
	}
}

/*
 *
 * Update the filter coefficients using the Kalman gain vector and 
 * the error
 *
 */
void CSt4285::kalman_update( FComplex *data, FComplex error )
{
	int i;
	//
   	// Calculate the new Kalman gain vector 
	//
	kalman_calculate( data );
	//
	// Update the filter coefficients using the gain vector
	// and the error.
	//
	error.re *= y;
	error.im *= y;
 
	for( i = 0; i < KN_44 ; i++ )
	{
		c[i].re  += cmultReal(error,g[i]);
		c[i].im  += cmultImag(error,g[i]);
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
//
//
// Equalizer routines
//
//
//
/////////////////////////////////////////////////////////////////////////////////////////////////////


FComplex CSt4285::equalize( FComplex *in )
{
	int      i;
	float    ii,qq,iq,qi;
	FComplex symbol;
	
	for( i = 0 ; i < FF_EQ_LENGTH ; i++ )
	{
		d_eq[i] = in[i];
	}
	/* Calculate the symbol */ 

	ii = d_eq[0].re*c[0].re;
	qq = d_eq[0].im*c[0].im;
	iq = d_eq[0].re*c[0].im;
	qi = d_eq[0].im*c[0].re;

	for( i=1; i < (FF_EQ_LENGTH+FB_EQ_LENGTH); i++ )
	{
		ii += d_eq[i].re*c[i].re;
		qq += d_eq[i].im*c[i].im;
		iq += d_eq[i].re*c[i].im;
		qi += d_eq[i].im*c[i].re;
	}

	symbol.re = ii - qq;
	symbol.im = iq + qi;
		
	return symbol;
}
void CSt4285::update_fb( FComplex in )
{
	int i;

	for( i = (FF_EQ_LENGTH+FB_EQ_LENGTH-1) ; i > FF_EQ_LENGTH ; i-- )
	{
		d_eq[i] = d_eq[i-1];
	}
	d_eq[FF_EQ_LENGTH] =   in; /* Update */
}
/*
 * Initialize the equalizer.
 */
void CSt4285::equalize_init( void )
{
	kalman_init();
}
void CSt4285::equalize_reset( void )
{
	kalman_reset();
}
/*
 *
 * Train the equalizer using known symbols
 *
 */
FComplex CSt4285::equalize_train( FComplex *in, FComplex train )
{
	FComplex error;
	FComplex symbol;
		
	symbol = equalize( in );	

	/* Calculate error */

	error.re = train.re - symbol.re;
	error.im = train.im - symbol.im;

	/* Update the coefficients */
	kalman_update( d_eq, error );         

	/* Update the FB data */
	
	update_fb( train );

	return( symbol );
}
/*
 *
 * Equalize the data and train on the decision
 *
 */
FDemodulate CSt4285::equalize_data( FComplex *in )
{
	FComplex    symbol;
	FDemodulate decision;

	symbol = equalize( in );

	/* Decode */
	decision = (*this.*demod)( symbol  );

	update_mse_average( decision.error );
		
	/* Update the coefficients */
	kalman_update( d_eq, decision.error );
	
	/* Update the FB data */
	update_fb( decision.dx_symbol );

	return decision;
}

#endif
