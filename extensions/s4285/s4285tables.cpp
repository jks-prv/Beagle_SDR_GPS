/*
 * 
 * 4285 serial tone modem.
 *
 */

// Copyright (c) 2000-2016, C.H Brain, G4GUO

#include "ext.h"

#ifdef EXT_S4285

#include "stdafx.h"
#include "st4285.h"

#pragma warning( disable : 4305 )

// 80-bit BPSK preamble
// x^5 + x^2 + 1, init 5'b11010
// repeating in the block as 31 + 31 + 18
// see: http://i56578-swl.blogspot.com/2016/05/phase-keyed-signals-sa-and-fake.html
FComplex tx_preamble_lookup[CSt4285::PREAMBLE_LENGTH]= 
{
	{  1.0,0.0 },
	{ -1.0,0.0 },
	{  1.0,0.0 },
	{ -1.0,0.0 },
	{ -1.0,0.0 },
	{  1.0,0.0 },
	{  1.0,0.0 },
	{ -1.0,0.0 },
	{ -1.0,0.0 },
	{ -1.0,0.0 },
	{ -1.0,0.0 },
	{ -1.0,0.0 },
	{  1.0,0.0 },
	{  1.0,0.0 },
	{  1.0,0.0 },
	{ -1.0,0.0 },
	{ -1.0,0.0 },
	{  1.0,0.0 },
	{ -1.0,0.0 },
	{ -1.0,0.0 },
	{ -1.0,0.0 },
	{  1.0,0.0 },
	{ -1.0,0.0 },
	{  1.0,0.0 },
	{ -1.0,0.0 },
	{  1.0,0.0 },
	{  1.0,0.0 },
	{  1.0,0.0 },
	{  1.0,0.0 },
	{ -1.0,0.0 },
	{  1.0,0.0 },
	{  1.0,0.0 },
	{ -1.0,0.0 },
	{  1.0,0.0 },
	{ -1.0,0.0 },
	{ -1.0,0.0 },
	{  1.0,0.0 },
	{  1.0,0.0 },
	{ -1.0,0.0 },
	{ -1.0,0.0 },
	{ -1.0,0.0 },
	{ -1.0,0.0 },
	{ -1.0,0.0 },
	{  1.0,0.0 },
	{  1.0,0.0 },
	{  1.0,0.0 },
	{ -1.0,0.0 },
	{ -1.0,0.0 },
	{  1.0,0.0 },
	{ -1.0,0.0 },
	{ -1.0,0.0 },
	{ -1.0,0.0 },
	{  1.0,0.0 },
	{ -1.0,0.0 },
	{  1.0,0.0 },
	{ -1.0,0.0 },
	{  1.0,0.0 },
	{  1.0,0.0 },
	{  1.0,0.0 },
	{  1.0,0.0 },
	{ -1.0,0.0 },
	{  1.0,0.0 },
	{  1.0,0.0 },
	{ -1.0,0.0 },
	{  1.0,0.0 },
	{ -1.0,0.0 },
	{ -1.0,0.0 },
	{  1.0,0.0 },
	{  1.0,0.0 },
	{ -1.0,0.0 },
	{ -1.0,0.0 },
	{ -1.0,0.0 },
	{ -1.0,0.0 },
	{ -1.0,0.0 },
	{  1.0,0.0 },
	{  1.0,0.0 },
	{  1.0,0.0 },
	{ -1.0,0.0 },
	{ -1.0,0.0 },
	{  1.0,0.0 }
};

FComplex rx_preamble_lookup[CSt4285::PREAMBLE_LENGTH];

FComplex symbol_psk2[2]=
{
	{  1.000000,  0.000000 },	// p0
	{ -1.000000,  0.000000 }	// p4
};
FComplex symbol_psk4[4]=
{
	{  1.000000,  0.000000 },	// p0
	{  0.000000,  1.000000 },	// p2
	{  0.000000, -1.000000 },	// p6
	{ -1.000000,  0.000000 }	// p4
};

/*

8-PSK phases (p0..7)

    Q
    2
  3   1
4       0 I
  5   7
    6

*/

// Q: why is the phase order staggered like this?
FComplex symbol_psk8[8]=
{
	{  0.707107,  0.707107 },	// p1 001
	{  1.000000,  0.000000 },	// p0 000
	{  0.000000,  1.000000 },	// p2 010
	{ -0.707107,  0.707107 },	// p3 011
	{  0.000000, -1.000000 },	// p6 110
	{  0.707107, -0.707107 },	// p7 111
	{ -0.707107, -0.707107 },	// p5 101
	{ -1.000000,  0.000000 }	// p4 100
};

FComplex scrambler_phase[8]=
{
	{  1.000000,  0.000000 },	// p0
	{  0.707107,  0.707107 },	// p1
	{  0.000000,  1.000000 },	// p2
	{ -0.707107,  0.707107 },	// p3
	{ -1.000000,  0.000000 },	// p4
	{ -0.707107, -0.707107 },	// p5
	{  0.000000, -1.000000 },	// p6
	{  0.707107, -0.707107 }	// p7
};

/* 0.2 ALPHA root raised cosine filter */

float rx_coffs[CSt4285::RX_FILTER_SIZE]=
{
	-0.00177868490585,
	-0.0103341998675,
	-0.00112763430547,
	 0.00228213234741,
	 0.00668936140064,
	 0.00638440522604,
	 0.000235942310842,
	-0.00864959993006,
	-0.0136725348567,
	-0.00910469000385,
	 0.0048760975508,
	 0.0203811326578,
	 0.0253860155435,
	 0.0117128495191,
	-0.0174006222407,
	-0.0455661423886,
	-0.0498811611031,
	-0.0135906928703,
	 0.0624522097793,
	 0.156547755698,
	 0.234221010827,
	 0.264278252776,
	 0.234221010827,
	 0.156547755698,
	 0.0624522097793,
	-0.0135906928703,
	-0.0498811611031,
	-0.0455661423886,
	-0.0174006222407,
	 0.0117128495191,
	 0.0253860155435,
	 0.0203811326578,
	 0.0048760975508,
	-0.00910469000385,
	-0.0136725348567,
	-0.00864959993006,
	 0.000235942310842,
	 0.00638440522604,
	 0.00668936140064,
	 0.00228213234741,
	-0.00112763430547,
	-0.0103341998675,
	-0.00177868490585
};

/* Tables used on transmit */

/* 0.2 ALPHA */

#define S4285T00 -0.00037431768913
#define S4285T01 -0.0210997775123
#define S4285T02 -0.00582761531675
#define S4285T03  0.00202742566687
#define S4285T04  0.0109706415249
#define S4285T05  0.0132101078099
#define S4285T06  0.00458353148865
#define S4285T07 -0.0111409532827
#define S4285T08 -0.0230555976595
#define S4285T09 -0.0199138939836
#define S4285T10  0.000877916565828
#define S4285T11  0.0285627169488
#define S4285T12  0.0433970433042
#define S4285T13  0.0290784514096
#define S4285T14 -0.0139534870617
#define S4285T15 -0.0636703946324
#define S4285T16 -0.0853111124469
#define S4285T17 -0.0505247884959
#define S4285T18  0.0437692486652
#define S4285T19  0.168727502656
#define S4285T20  0.274923748625
#define S4285T21  0.316544418236
#define S4285T22  0.274923748625
#define S4285T23  0.168727502656
#define S4285T24  0.0437692486652
#define S4285T25 -0.0505247884959
#define S4285T26 -0.0853111124469
#define S4285T27 -0.0636703946324
#define S4285T28 -0.0139534870617
#define S4285T29  0.0290784514096
#define S4285T30  0.0433970433042
#define S4285T31  0.0285627169488
#define S4285T32  0.000877916565828
#define S4285T33 -0.0199138939836
#define S4285T34 -0.0230555976595
#define S4285T35 -0.0111409532827
#define S4285T36  0.00458353148865
#define S4285T37  0.0132101078099
#define S4285T38  0.0109706415249
#define S4285T39  0.00202742566687
#define S4285T40 -0.00582761531675
#define S4285T41 -0.0210997775123
#define S4285T42 -0.00037431768913

float tx_coffs[CSt4285::TX_FILTER_LENGTH]=
{
	(S4285T00+S4285T01+S4285T02+S4285T03)/4,
	(S4285T00+S4285T01+S4285T02)/4,
	(S4285T00+S4285T01)/4,
	(S4285T00)/4,
	(S4285T04+S4285T05+S4285T06+S4285T07)/4,
	(S4285T03+S4285T04+S4285T05+S4285T06)/4,
	(S4285T02+S4285T03+S4285T04+S4285T05)/4,
	(S4285T01+S4285T02+S4285T03+S4285T04)/4,
	(S4285T08+S4285T09+S4285T10+S4285T11)/4,
	(S4285T07+S4285T08+S4285T09+S4285T10)/4,
	(S4285T06+S4285T07+S4285T08+S4285T09)/4,
	(S4285T05+S4285T06+S4285T07+S4285T08)/4,
	(S4285T12+S4285T13+S4285T14+S4285T15)/4,
	(S4285T11+S4285T12+S4285T13+S4285T14)/4,
	(S4285T10+S4285T11+S4285T12+S4285T13)/4,
	(S4285T09+S4285T10+S4285T11+S4285T12)/4,
	(S4285T16+S4285T17+S4285T18+S4285T19)/4,
	(S4285T15+S4285T16+S4285T17+S4285T18)/4,
	(S4285T14+S4285T15+S4285T16+S4285T17)/4,
	(S4285T13+S4285T14+S4285T15+S4285T16)/4,
	(S4285T20+S4285T21+S4285T22+S4285T23)/4,
	(S4285T19+S4285T20+S4285T21+S4285T22)/4,
	(S4285T18+S4285T19+S4285T20+S4285T21)/4,
	(S4285T17+S4285T18+S4285T19+S4285T20)/4,
	(S4285T24+S4285T25+S4285T26+S4285T27)/4,
	(S4285T23+S4285T24+S4285T25+S4285T26)/4,
	(S4285T22+S4285T23+S4285T24+S4285T25)/4,
	(S4285T21+S4285T22+S4285T23+S4285T24)/4,
	(S4285T28+S4285T29+S4285T30+S4285T31)/4,
	(S4285T27+S4285T28+S4285T29+S4285T30)/4,
	(S4285T26+S4285T27+S4285T28+S4285T29)/4,
	(S4285T25+S4285T26+S4285T27+S4285T28)/4,
	(S4285T32+S4285T33+S4285T34+S4285T35)/4,
	(S4285T31+S4285T32+S4285T33+S4285T34)/4,
	(S4285T30+S4285T31+S4285T32+S4285T33)/4,
	(S4285T29+S4285T30+S4285T31+S4285T32)/4,
	(S4285T36+S4285T37+S4285T38+S4285T39)/4,
	(S4285T35+S4285T36+S4285T37+S4285T38)/4,
	(S4285T34+S4285T35+S4285T36+S4285T37)/4,
	(S4285T33+S4285T34+S4285T35+S4285T36)/4,
	(S4285T40+S4285T41+S4285T42)/4,
	(S4285T39+S4285T40+S4285T41+S4285T42)/4,
	(S4285T38+S4285T39+S4285T40+S4285T41)/4,
	(S4285T37+S4285T38+S4285T39+S4285T40)/4,
};

#pragma warning( default : 4305 )

/* 
 * 
 * By pre-computing the scrambler values it should speed thing up a bit.
 *
 */

// x^9 + x^4 + 1, init 9'b111111111
// see: http://i56578-swl.blogspot.com/2016/02/ms188-110c-appendix-cd-scrambler_19.html
void CSt4285::iterate_data_scrambler_sequence( int *state )
{
	int    temp;
	temp = 0;
	
	if( (*state)&0x0001 ) temp++;
	if( (*state)&0x0010 ) temp++;
	
	*state = (*state)>>1;
	
	if( temp&1 ) *state |= 0x0100;
}
void CSt4285::build_data_scrambler_array( void )
{
	int i;
	int state;
	
	state = 0x1FF;
	
	for( i = 0; i < SCRAMBLER_TABLE_LENGTH ; i++ )
	{
		scrambler_sequence[i] = state&0x07;
		iterate_data_scrambler_sequence( &state );	// shift 3 times because the symbol is 8-PSK (tri-bit)
		iterate_data_scrambler_sequence( &state );
		iterate_data_scrambler_sequence( &state );		
	}	
}
		
void CSt4285::build_scrambler_table( void )
{
	int i;

	build_data_scrambler_array();
	
	for( i = 0; i < SCRAMBLER_TABLE_LENGTH; i++ )
	{
		scrambler_table[i]            = scrambler_phase[scrambler_sequence[i]];
		scrambler_train_table[i].re   = scrambler_table[i].re*(float)0.01;
		scrambler_train_table[i].im   = scrambler_table[i].im*(float)0.01;
	}
	for( i = 0; i < PREAMBLE_LENGTH; i++ )
	{
		rx_preamble_lookup[i].re = tx_preamble_lookup[i].re*(float)0.01;
		rx_preamble_lookup[i].im = tx_preamble_lookup[i].im*(float)0.01;
	}
}
void CSt4285::start( void )
{
	rx_scramble_count     = 0;
	rx_block_buffer_index = 0;
	output_offset         = 0;
	rx_state              = RX_HUNTING;
	mse_count             = 0;
	hp                    = 0;
	frequency_error[0]    = 0;
	sample_rate			  = DEFAULT_SAMPLE_RATE;

	m_status_text[0]      = 0;
	rx_scramble_count     = 0;
	tx_scramble_count     = 0;
	tx_count              = 0;
	m_frequency_error     = 0;
	tx_sample_wa		  = 0;
	tx_sample_tx_ra		  = 0;
	tx_sample_rx_ra		  = 0;
	tx_sample_tx_count	  = 0;
	tx_sample_rx_count	  = 0;

	acc[0]                = 0;
	acc[1]                = 0;
	acc[2]                = 0;

	build_scrambler_table();
	convolutional_init();
	convolutional_encode_reset();
	viterbi_decode_reset();
	equalize_init();

	// AGC
	hold = 1.0;
	con_bad_probe_threshold = 5;

	// default parameters
	set_tx_mode( B600L );
	//set_tx_mode( B150L );

	set_rx_mode( B600L );
	//set_rx_mode( B150L );
}

#endif
