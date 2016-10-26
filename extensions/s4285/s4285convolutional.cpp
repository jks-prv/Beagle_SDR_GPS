/*
 * Convolutional encoder/Decoder for MIL-STANAG modems
 *
 */

// Copyright (c) 2000-2016, C.H Brain, G4GUO

#include "ext.h"

#ifdef EXT_S4285

#include "stdafx.h"
#include "stdio.h"
#include "st4285.h"

#define BF(v,w,x,y,z) \
	tempa = (acm[w]*(float)0.999) + (metric[x]*(float)0.001); \
	tempb = (acm[y]*(float)0.999) + (metric[z]*(float)0.001); \
	if (tempa > tempb) { \
		tm[v] = tempa; \
		path[v][hp] = w; \
	} else { \
		tm[v] = tempb; \
		path[v][hp] = y; \
	}

void CSt4285::parity( int state, int *bit1, int *bit2 )
{
	int count;
	
	count = 0;
	if(state&C10MASK) count++;
	if(state&C11MASK) count++;
	if(state&C13MASK) count++;
	if(state&C14MASK) count++;
	if(state&C16MASK) count++;
	
	*bit1 = count&1;

	count = 0;
	if(state&C20MASK) count++;
	if(state&C23MASK) count++;
	if(state&C24MASK) count++;
	if(state&C25MASK) count++;
	if(state&C26MASK) count++;
	
	*bit2 = count&1;
		                                                      
}

/* 
 *
 * This generates the inline code that calculates the cost etc for the Viterbi Decoder.
 *
 * It is not needed in the final modem.
 *
 */ 

void CSt4285::code_generate(void)
{
	int i;
	int last0;
	int last1;
	int dibit0;
	int dibit1;
	int temp1;
	int temp2;
	
	for(i=0; i< NR_CONVOLUTIONAL_STATES; i++ )
	{
		last0 = (i<<1)&0x3F;
		last1 = ((i<<1)+1)&0x3F;

		parity((i<<1),&temp1,&temp2);
		dibit0 = (temp1<<1)+temp2;

		parity((i<<1)+1,&temp1,&temp2);
		dibit1 = (temp1<<1)+temp2;
		
		printf("\tBF(%d,%d,%d,%d,%d)\n",i,last0,dibit0,last1,dibit1);
	}
}

void CSt4285::convolutional_init( void )
{
	int i;

	/* Build the encoding Tables */	
	for( i=0; i< 128; i++)
	{
		parity( i, &parity_lookup[i].bit1, &parity_lookup[i].bit2 );
	}
	path_length = NORMAL_PATH_LENGTH;
}
void CSt4285::convolutional_encode_reset( void )
{
	encode_state = 0;
}
void CSt4285::viterbi_set_normal_path_length( void  )
{
	path_length = NORMAL_PATH_LENGTH;
}
void CSt4285::viterbi_set_alternate_1_path_length( void  )
{
	path_length = ALTERNATE_1_PATH_LENGTH;
}
void CSt4285::viterbi_set_alternate_2_path_length( void  )
{
	path_length = ALTERNATE_2_PATH_LENGTH;
}
void CSt4285::viterbi_decode_reset( void )
{
	int i,j;
	
	for( i = 0 ; i < NR_CONVOLUTIONAL_STATES ; i++)
	{
		acm[i] = 0.0;
		for( j = 0 ; j < NORMAL_PATH_LENGTH ; j++ )
		{
			path[i][j] = 0;
		}	
	} 
	acm[0] = (float)0.1;
}
void CSt4285::convolutional_encode( int in, int *bit1, int *bit2 )
{
	encode_state = encode_state>>1;
	
	if(in) encode_state |= 0x40;
	
	*bit1 = parity_lookup[encode_state].bit1;
	*bit2 = parity_lookup[encode_state].bit2;
}

/*
 *
 * Positive metric = logic 0
 * Negative metric = logic 1
 *
 */

int CSt4285::viterbi_decode( float metric1, float metric2 )
{
	float tempa,tempb;
	float metricX0,metricX1,metric0X,metric1X;
	float metric[4];
	float tm[NR_CONVOLUTIONAL_STATES];
	float max_value;
	int   state;
	int   i,p;
	
	/* Calculate the metric values */
	metricX0 = metricX1 = metric0X = metric1X = 0;
	
	if( metric1 >= 0 )
		metric0X  =    metric1;
	else
		metric1X  =   -metric1;

	if( metric2 >= 0 )
		metricX0  =     metric2;
	else
		metricX1  =   - metric2;

	metric[0] = metric0X + metricX0;
	metric[1] = metric0X + metricX1;
	metric[2] = metric1X + metricX0;
	metric[3] = metric1X + metricX1;


	BF(0,0,0,1,3)
	BF(1,2,2,3,1)
	BF(2,4,0,5,3)
	BF(3,6,2,7,1)
	BF(4,8,3,9,0)
	BF(5,10,1,11,2)
	BF(6,12,3,13,0)
	BF(7,14,1,15,2)
	BF(8,16,3,17,0)
	BF(9,18,1,19,2)
	BF(10,20,3,21,0)
	BF(11,22,1,23,2)
	BF(12,24,0,25,3)
	BF(13,26,2,27,1)
	BF(14,28,0,29,3)
	BF(15,30,2,31,1)
	BF(16,32,1,33,2)
	BF(17,34,3,35,0)
	BF(18,36,1,37,2)
	BF(19,38,3,39,0)
	BF(20,40,2,41,1)
	BF(21,42,0,43,3)
	BF(22,44,2,45,1)
	BF(23,46,0,47,3)
	BF(24,48,2,49,1)
	BF(25,50,0,51,3)
	BF(26,52,2,53,1)
	BF(27,54,0,55,3)
	BF(28,56,1,57,2)
	BF(29,58,3,59,0)
	BF(30,60,1,61,2)
	BF(31,62,3,63,0)
	BF(32,0,3,1,0)
	BF(33,2,1,3,2)
	BF(34,4,3,5,0)
	BF(35,6,1,7,2)
	BF(36,8,0,9,3)
	BF(37,10,2,11,1)
	BF(38,12,0,13,3)
	BF(39,14,2,15,1)
	BF(40,16,0,17,3)
	BF(41,18,2,19,1)
	BF(42,20,0,21,3)
	BF(43,22,2,23,1)
	BF(44,24,3,25,0)
	BF(45,26,1,27,2)
	BF(46,28,3,29,0)
	BF(47,30,1,31,2)
	BF(48,32,2,33,1)
	BF(49,34,0,35,3)
	BF(50,36,2,37,1)
	BF(51,38,0,39,3)
	BF(52,40,1,41,2)
	BF(53,42,3,43,0)
	BF(54,44,1,45,2)
	BF(55,46,3,47,0)
	BF(56,48,1,49,2)
	BF(57,50,3,51,0)
	BF(58,52,1,53,2)
	BF(59,54,3,55,0)
	BF(60,56,2,57,1)
	BF(61,58,0,59,3)
	BF(62,60,2,61,1)
	BF(63,62,0,63,3)

	max_value = 0;
	state     = 0;
	/* Find the best path */
	
	for( i=0; i< NR_CONVOLUTIONAL_STATES; i++ )
	{
		if( tm[i] > max_value )
		{
			max_value = tm[i];
			state = i;
		}
		acm[i] = tm[i];
	}

	m_viterbi_confidence = acm[state];

	/* Trace back the path to find the output */	
    p = hp;

    for( i = 0 ; i < path_length-1; i++ )
    {
		state = path[state][p];
        if( p == 0 )
            p = path_length-1;
        else
            p--;
    }

	hp = ++hp%path_length;
		
	return (state&0x20? 1:0);
}

#endif
