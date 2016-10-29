/*
 * 
 * This module controls the Stanag 4285 modem parameters.
 *
 */

// Copyright (c) 2000-2016, C.H Brain, G4GUO

#include "ext.h"

#ifdef EXT_S4285

#include "stdafx.h"
#include <stdio.h>
#include "st4285.h"

int CSt4285::get_tx_octets_per_block( void )
{
	return con_tx_octets_per_block;
}
void CSt4285::set_vox_ptt( void )
{
	ptt_type = VOX_PTT;
}
void CSt4285::set_rts_ptt( void )
{
}
void CSt4285::set_dtr_ptt( void )
{
}
void CSt4285::ptt_transmit( void )
{
}
void CSt4285::ptt_receive( void )
{
}
void CSt4285::set_duplex( void )
{
	duplex_mode = DUPLEX_MODE;
}
void CSt4285::set_simplex( void )
{
	duplex_mode = SIMPLEX_MODE;
}
int CSt4285::is_duplex( void )
{
	return ( duplex_mode == DUPLEX_MODE ? 1 : 0 );
}
int CSt4285::is_simplex( void )
{
	return ( duplex_mode == SIMPLEX_MODE ? 1 : 0 );
}
void CSt4285::set_tx_mode( Kmode mode )
{
	switch( (int)mode )
	{
		case B75N:
			interleaver_disable();
			tx_mode             = TX_75_BPS;
			con_tx_octets_per_block = 1;
			con_tx_flush_octets     = 21;
	  		break;
		case B75S:
			interleaver_enable();
			interleaving_tx_600_75_short();
			tx_mode             = TX_75_BPS;
			con_tx_octets_per_block = 1;
			con_tx_flush_octets     = 21;
			break;
		case B75L:
			interleaver_enable();
			interleaving_tx_600_75_long();
			tx_mode             = TX_75_BPS;
			con_tx_octets_per_block = 1;
			con_tx_flush_octets     = 109;
			break;
		case B150N:
			interleaver_disable();
			tx_mode             = TX_150_BPS;
			con_tx_octets_per_block = 2;
			con_tx_flush_octets     = 29;
			break;							
		case B150S:
			interleaver_enable();
			interleaving_tx_600_75_short();
			tx_mode             = TX_150_BPS;
			con_tx_octets_per_block = 2;
			con_tx_flush_octets     = 29;
			break;
		case B150L:
			interleaver_enable();
			interleaving_tx_600_75_long();
			tx_mode             = TX_150_BPS;
			con_tx_octets_per_block = 2;
			con_tx_flush_octets     = 205;
			break;
		case B300N:
			interleaver_disable();
			tx_mode = TX_300_BPS;
			con_tx_octets_per_block = 4;
			con_tx_flush_octets     = 45;
			break;
		case B300S:
			interleaver_enable();
			interleaving_tx_600_75_short();
			tx_mode = TX_300_BPS;
			con_tx_octets_per_block = 4;
			con_tx_flush_octets     = 45;
			break;
		case B300L:
			interleaver_enable();
			interleaving_tx_600_75_long();
			tx_mode = TX_300_BPS;
			con_tx_octets_per_block = 4;
			con_tx_flush_octets     = 397;
			break;
		case B600N:
			interleaver_disable();
			tx_mode = TX_600_BPS;
			con_tx_octets_per_block = 8;
			con_tx_flush_octets     = 77;
			break;
		case B600S:
			interleaver_enable();
			interleaving_tx_600_75_short();
			tx_mode = TX_600_BPS;
			con_tx_octets_per_block = 8;
			con_tx_flush_octets     = 77;
			break;
		case B600L:
			interleaver_enable();
			interleaving_tx_600_75_long();
			tx_mode = TX_600_BPS;
			con_tx_octets_per_block = 8;
			con_tx_flush_octets     = 781;
			break;
		case B600U:
			/* Not supported */
			break;
		case B1200N:
			interleaver_disable();
			tx_mode = TX_1200_BPS;
			con_tx_octets_per_block = 16;
			con_tx_flush_octets     = 141;
			break;
		case B1200S:
			interleaver_enable();
			interleaving_tx_1200_short();
			tx_mode = TX_1200_BPS;
			con_tx_octets_per_block = 16;
			con_tx_flush_octets     = 141;
			break;
		case B1200L:
			interleaver_enable();
			interleaving_tx_1200_long();
			tx_mode = TX_1200_BPS;
			con_tx_octets_per_block = 16;
			con_tx_flush_octets     = 1549;
			break;
		case B1200U:
			tx_mode = TX_1200U_BPS;
			con_tx_octets_per_block = 16;
			con_tx_flush_octets     = 16;
			break;
		case B1800U:
			/* Not supported */
			break;
		case B2400N:
			interleaver_disable();
			tx_mode = TX_2400_BPS;
			con_tx_octets_per_block = 32;
			con_tx_flush_octets     = 269;
			break;
		case B2400S:
			interleaver_enable();
			interleaving_tx_2400_short();
			tx_mode = TX_2400_BPS;
			con_tx_octets_per_block = 32;
			con_tx_flush_octets     = 269;
			break;
		case B2400L:
			interleaver_enable();
			interleaving_tx_2400_long();
			tx_mode = TX_2400_BPS;
			con_tx_octets_per_block = 32;
			con_tx_flush_octets     = 3085;
			break;
		case B2400U:
			tx_mode = TX_2400U_BPS;
			con_tx_octets_per_block = 32;
			con_tx_flush_octets     = 32;
			break;
		case B3600U:
			tx_mode = TX_3600U_BPS;
			con_tx_octets_per_block = 48;
			con_tx_flush_octets     = 48;
			break;
		default:
			break;
	}
}
void CSt4285::set_rx_mode( Kmode mode )
{
	switch( mode )
	{
		case B75N:
			demod = &CSt4285::demodulate_bpsk;
			deinterleaver_disable();
			rx_mode                 = RX_75_BPS;
			con_rx_octets_per_block = 1;
			con_bad_probe_threshold = 1;
	  		break;
		case B75S:
			demod = &CSt4285::demodulate_bpsk;
			deinterleaver_enable();
			interleaving_rx_600_75_short();
			rx_mode             = RX_75_BPS;
			con_rx_octets_per_block = 1;
			con_bad_probe_threshold = 5;
			viterbi_set_alternate_2_path_length();
			break;
		case B75L:
			demod = &CSt4285::demodulate_bpsk;
			deinterleaver_enable();
			interleaving_rx_600_75_long();
			rx_mode             = RX_75_BPS;
			con_rx_octets_per_block = 1;
			con_bad_probe_threshold = 10;
			viterbi_set_normal_path_length();
			break;
		case B150N:
			demod = &CSt4285::demodulate_bpsk;// Not used
			deinterleaver_disable();
			rx_mode             = RX_150_BPS;
			con_rx_octets_per_block = 1;
			break;							
		case B150S:
			demod = &CSt4285::demodulate_bpsk;
			deinterleaver_enable();
			interleaving_rx_600_75_short();
			rx_mode             = RX_150_BPS;
			con_rx_octets_per_block = 2;
			con_bad_probe_threshold = 5;
			viterbi_set_alternate_1_path_length();
			break;
		case B150L:
			demod = &CSt4285::demodulate_bpsk;
			deinterleaver_enable();
			interleaving_rx_600_75_long();
			rx_mode             = RX_150_BPS;
			con_rx_octets_per_block = 2;
			con_bad_probe_threshold = 10;
			viterbi_set_normal_path_length();
			break;
		case B300N:
			demod = &CSt4285::demodulate_bpsk;
			deinterleaver_disable();
			rx_mode = RX_300_BPS;
			con_rx_octets_per_block = 4;
			con_bad_probe_threshold = 1;
			break;
		case B300S:
			demod = &CSt4285::demodulate_bpsk;
			deinterleaver_enable();
			interleaving_rx_600_75_short();
			rx_mode = RX_300_BPS;
			con_rx_octets_per_block = 4;
			con_bad_probe_threshold = 2;
			viterbi_set_normal_path_length();
			break;
		case B300L:
			demod = &CSt4285::demodulate_bpsk;
			deinterleaver_enable();
			interleaving_rx_600_75_long();
			rx_mode = RX_300_BPS;
			con_rx_octets_per_block = 4;
			con_bad_probe_threshold = 10;
			viterbi_set_normal_path_length();
			break;
		case B600N:
			demod = &CSt4285::demodulate_bpsk;
			deinterleaver_disable();
			rx_mode = RX_600_BPS;
			con_rx_octets_per_block = 8;
			con_bad_probe_threshold = 1;
			break;
		case B600S:
			demod = &CSt4285::demodulate_bpsk;
			deinterleaver_enable();
			interleaving_rx_600_75_short();
			rx_mode = RX_600_BPS;
			con_rx_octets_per_block = 8;
			con_bad_probe_threshold = 2;
			viterbi_set_normal_path_length();
			break;
		case B600L:
			demod = &CSt4285::demodulate_bpsk;
			deinterleaver_enable();
			interleaving_rx_600_75_long();
			rx_mode = RX_600_BPS;
			con_rx_octets_per_block = 8;
			con_bad_probe_threshold = 10;
			viterbi_set_normal_path_length();
			break;
		case B600U:
			/* Not supported */
			break;
		case B1200N:
			demod = &CSt4285::demodulate_qpsk;
			deinterleaver_disable();
			rx_mode = RX_1200_BPS;
			con_rx_octets_per_block = 16;
			con_bad_probe_threshold = 1;
			break;
		case B1200S:
			demod = &CSt4285::demodulate_qpsk;
			deinterleaver_enable();
			interleaving_rx_1200_short();
			rx_mode = RX_1200_BPS;
			con_rx_octets_per_block = 16;
			con_bad_probe_threshold = 2;
			viterbi_set_normal_path_length();
			break;
		case B1200L:
			demod = &CSt4285::demodulate_qpsk;
			deinterleaver_enable();
			interleaving_rx_1200_long();
			rx_mode = RX_1200_BPS;
			con_rx_octets_per_block = 16;
			con_bad_probe_threshold = 10;
			viterbi_set_normal_path_length();
			break;
		case B1200U:
			demod = &CSt4285::demodulate_qpsk;
			rx_mode = RX_1200U_BPS;
			con_rx_octets_per_block = 16;
			con_bad_probe_threshold = 1;
			break;
		case B1800U:
			/* Not supported */
			break;
		case B2400N:
			demod = &CSt4285::demodulate_8psk;
			deinterleaver_disable();
			rx_mode = RX_2400_BPS;
			con_rx_octets_per_block = 32;
			con_bad_probe_threshold = 1;
			break;
		case B2400S:
			demod = &CSt4285::demodulate_8psk;
			deinterleaver_enable();
			interleaving_rx_2400_short();
			rx_mode = RX_2400_BPS;
			con_rx_octets_per_block = 32;
			con_bad_probe_threshold = 2;
			viterbi_set_normal_path_length();
			break;
		case B2400L:
			demod = &CSt4285::demodulate_8psk;
			deinterleaver_enable();
			interleaving_rx_2400_long();
			rx_mode = RX_2400_BPS;
			con_rx_octets_per_block = 32;
			con_bad_probe_threshold = 10;
			viterbi_set_normal_path_length();
			break;
		case B2400U:
			demod = &CSt4285::demodulate_8psk;
			rx_mode = RX_2400U_BPS;
			con_rx_octets_per_block = 32;
			con_bad_probe_threshold = 1;
			break;
		case B3600U:
			demod = &CSt4285::demodulate_8psk;
			rx_mode = RX_3600U_BPS;
			con_rx_octets_per_block = 48;
			con_bad_probe_threshold = 1;
			break;
		default:
			break;
	}
	rx_mode_type = mode;
	status_string();
}
Kmode CSt4285::set_rx_mode_text( char *mode )
{

	if( strcmp(mode,"75N") == 0 )
		return B75N;

	if( strcmp(mode,"75S") == 0 )
		return B75S;

	if( strcmp(mode,"75L") == 0 )
		return B75L;

	if( strcmp(mode,"150N") == 0 )
		return B150N;

	if( strcmp(mode,"150S") == 0 )
		return B150S;

	if( strcmp(mode,"150L") == 0 )
		return B150L;

	if( strcmp(mode,"300N") == 0 )
		return B300N;

	if( strcmp(mode,"300S") == 0 )
		return B300S;

	if( strcmp(mode,"300L") == 0 )
		return B300L;

	if( strcmp(mode,"600S") == 0 )
		return B600S;

	if( strcmp(mode,"600L") == 0 )
		return B600L;

	if( strcmp(mode,"600U") == 0 )
		return B600U;

	if( strcmp(mode,"1200N") == 0 )
		return B1200N;

	if( strcmp(mode,"1200S") == 0 )
		return B1200S;

	if( strcmp(mode,"1200L") == 0 )
		return B1200L;

	if( strcmp(mode,"1200U") == 0 )
		return B1200U;

	if( strcmp(mode,"1800U") == 0 )
		return B1800U;

	if( strcmp(mode,"2400N") == 0 )
		return B2400N;

	if( strcmp(mode,"2400S") == 0 )
		return B2400S;

	if( strcmp(mode,"2400L") == 0 )
		return B2400L;

	if( strcmp(mode,"2400U") == 0 )
		return B2400U;

	if( strcmp(mode,"3600U") == 0 )
		return B3600U;
	return B600S;
}
const char* CSt4285::get_rx_mode_text( void )
{
	const char *mode;

	switch( rx_mode_type )
	{
		case B75N:
			mode = "75N";
	  		break;
		case B75S:
			mode = "75S";
			break;
		case B75L:
			mode = "75L";
			break;
		case B150N:
			mode = "150N";
			break;							
		case B150S:
			mode = "150S";
			break;
		case B150L:
			mode = "150L";
			break;
		case B300N:
			mode = "300N";
			break;
		case B300S:
			mode = "300S";
			break;
		case B300L:
			mode = "300L";
			break;
		case B600N:
			mode = "600N";
			break;
		case B600S:
			mode = "600S";
			break;
		case B600L:
			mode = "600L";
			break;
		case B600U:
			mode = "600U";
			/* Not supported */
			break;
		case B1200N:
			mode = "1200N";
			break;
		case B1200S:
			mode = "1200S";
			break;
		case B1200L:
			mode = "1200L";
			break;
		case B1200U:
			mode = "1200U";
			break;
		case B1800U:
			mode = "1800U";
			/* Not supported */
			break;
		case B2400N:
			mode = "2400N";
			break;
		case B2400S:
			mode = "2400S";
			break;
		case B2400L:
			mode = "2400L";
			break;
		case B2400U:
			mode = "2400U";
			break;
		case B3600U:
			mode = "3600U";
			break;
		default:
			mode = "NONE";
			break;
	}
	return mode;
}

void *CSt4285::allocate_context( void )
{
	start();
	return(NULL);
}

void CSt4285::process_samples( float *in, unsigned int length )
{
	process_rx_block( in, length );
}

unsigned int CSt4285::get_data( int *data, int &length )
{
	for( int i = 0; i < output_offset ; i++ )
	{
		data[i] = output_data[i];
	}
	length        = output_offset;
	output_offset = 0;
	return TYPE_BITSTREAM_DATA;
}

void CSt4285::get_set_params( const char *cmd, char *resp )
{
	char params[10][15];
	unsigned int i,j, index;
	int ival;

	// Extract the params

	for( i = 0, index = 0, j = 0; i < strlen(cmd); i++ )
	{
		if( cmd[i] == ',' )
		{
			params[index++][j] = 0x00;
			j = 0;
		}
		else
		{
			params[index][j++] = cmd[i];
		}
	}
	params[index][j] = 0x00;
	//
	// Do the request 
	//
	if( strcmp(params[0],"GET") == 0 )
	{
		if( strcmp(params[1],"DCD") == 0 )
		{
			if( rx_state == RX_HUNTING )
			{
				sprintf(resp,"");
			}
			else
			{
				sprintf(resp,"DCD");
			}
		}
		if( strcmp(params[1],"FERROR") == 0 )
		{
			sprintf(resp,"%s",frequency_error);
		}
		if( strcmp(params[1],"MODE") == 0 )
		{
			sprintf(resp,"%s",get_rx_mode_text());
		}
		if( strcmp(params[1],"CONFIDENCE") == 0 )
		{
			ival = (preamble_matches*100)/80;
			if( ival > 50 )
				ival = (ival - 50)*2;
			else
				ival = 0;

			sprintf(resp,"%d",ival);
		}

		if( strcmp(params[1],"STATUS") == 0 )
		{
			get_status_text( resp );
		}
		if( strcmp(params[1],"LOTONE") == 0 )
		{
			sprintf( resp,"%d",CENTER_FREQUENCY-1500);
		}
		if( strcmp(params[1],"MIDTONE") == 0 )
		{
			sprintf( resp,"%d",CENTER_FREQUENCY);
		}
		if( strcmp(params[1],"HITONE") == 0 )
		{
			sprintf( resp,"%d",CENTER_FREQUENCY+1500);
		}
		if( strcmp(params[1],"MULTITONES") == 0 )
		{
			sprintf( resp,"0");
		}
	}
	if( strcmp(params[0],"SET") == 0 )
	{
		if( strcmp(params[1],"MODE") == 0 )
		{
			set_rx_mode( set_rx_mode_text( params[2] ));
		}
	}
}
void CSt4285::status_string( void )
{
	char temp[80];

	if( rx_state == RX_HUNTING )
		sprintf(temp,"FALSE");
	else
		sprintf(temp,"TRUE Pream errs: %d ", m_preamble_errors );

	static int seq;
	sprintf( m_status_text, "S4285 %s ch%d Ferr: %+7.4f Hz MVC: %4.1f DCD: %s ",
		get_rx_mode_text(),
		rx_chan,
		m_frequency_error, 
		m_viterbi_confidence,
		temp );
	
	m_status_update = true;
}

bool  CSt4285::get_status_text( char *text )
{
	bool status;

	status = false;

	if( m_status_update == true )
	{
		strcpy( text, m_status_text );
		m_status_update = false;
		status = true;
	}
	return status;
}

void CSt4285::save_constellation_symbol( FComplex symbol )
{
	if( m_constellation_offset < MAX_NR_CONSTELLATION_SYMBOLS - 1 )
	{
		m_constellation_symbols[m_constellation_offset].re = symbol.re *100;
		m_constellation_symbols[m_constellation_offset].im = symbol.im *100;
		m_constellation_offset++;
	}
}

//
// Public Interface
//
//
// Input routines
//
void CSt4285::input( float *in, int length )
{
	process_rx_block( in, length );
}
void CSt4285::input( float *real, float *imag, int length )
{

}
void CSt4285::input( int *in, int length )
{

}
//
// Control routines
//
bool CSt4285::control( void *in, int type )
{
	return true;
}
bool CSt4285::control( void *in, void *out, int type )
{
	get_set_params((const char *)in, (char *)out );
	return true;
}

void CSt4285::setSampleRate( float srate )
{
	printf("s4285 sample_rate = %f\n", srate);
	panic("don't use");
	sample_rate = srate;
}

//
// Rx Output
//
unsigned int CSt4285::getRxOutput( void *out, int &length, int type )
{
	switch( type )
	{
		case TYPE_BITSTREAM_DATA:
			get_data( (int *)out, length );
			break;
		default:
			break;
	}
	return type;
}
//
// Tx Output
//
unsigned int CSt4285::getTxOutput( void *out, int length, int type, float scale )
{
	switch( type )
	{
		case TYPE_IQ_F32_DATA:
			tx_output_iq( (FComplex *)out, length, scale );
			break;
		case TYPE_REAL_S16_DATA:
			tx_output_real( (short *)out, length, scale );
			break;
		default:
			break;
	}
	return type;
}

void CSt4285::registerRxCallback( callbackRxOutput_t func, int arg )
{
	rx_callback = func;
	rx_callback_arg = arg;
}

void CSt4285::registerTxCallback( callbackTxInput_t func )
{
	tx_callback = func;
}

#endif
