/*  -*-  mode: c; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; coding: utf-8  -*-  */
/************************************************************************************
 **                                                                                 **
 **                               mcHF QRP Transceiver                              **
 **                             K Atanassov - M0NKA 2014                            **
 **                                                                                 **
 **---------------------------------------------------------------------------------**
 **                                                                                 **
 **  File name:                                                                     **
 **  Description:                                                                   **
 **  Last Modified:                                                                 **
 **  Licence:		GNU GPLv3                                                          **
 ************************************************************************************/

// ----------------------------------------------------------------------------
// re-implemented from:
// https://www.wrbishop.com/ham/arduino-iambic-keyer-and-side-tone-generator/
// by Steven T. Elliott
// ----------------------------------------------------------------------------

// Common
#include "uhsdr_cw_decoder.h"
#include "uhsdr_cw_gen.h"

// States
#define CW_IDLE             0
#define CW_DIT_CHECK        1
#define CW_DAH_CHECK        3
#define CW_KEY_DOWN         4
#define CW_KEY_UP           5
#define CW_PAUSE            6
#define CW_WAIT             7

#define CW_DIT_L            0x01
#define CW_DAH_L            0x02
#define CW_DIT_PROC         0x04
#define CW_END_PROC         0x10

#define CW_IAMBIC_A         0x00
#define CW_IAMBIC_B         0x10

#define CW_SMOOTH_LEN       2	// with sm_table size of 128 -> 2 => ~5.3ms for edges, ~ 9 steps of 0.66 ms,
// 1 => 2.7ms , 5 steps of 0.66ms required.
// 3 => 8ms, 13 steps
#define CW_SMOOTH_STEPS		9	// 1 step = 0.6ms; 13 for 8ms, 9 for 5.4 ms, for internal keyer

#define CW_SPACE_CHAR		1

// The vertical listing permits easier direct comparison of code vs. character in
// editors by placing both in vertically split window
const uint32_t cw_char_codes[] =
{
		CW_SPACE_CHAR,    // 		-> ' '
		2,    // . 		-> 'E'
		3,    // - 		-> 'T'
		10,   // .. 	-> 'I'
		11,   // .- 	-> 'A'
		14,   // -. 	-> 'N'
		15,   // -- 	-> 'M'
		42,   // ...	-> 'S'
		43,   // ..-	-> 'U'
		46,   // .-.	-> 'R'
		47,   // .--	-> 'W'
		58,	  // -..	-> 'D'
		59,   // -.-	-> 'K'
		62,   // --.	-> 'G'
		63,   // ---	-> 'O'
		170,  // ....	-> 'H'
		171,  // ...-	-> 'V'
		174,  // ..-.	-> 'F'
		186,  // .-..	-> 'L'
		190,  // .--.	-> 'P'
		191,  // .---	-> 'J'
		234,  // -...	-> 'B'
		235,  // -..-	-> 'X'
		238,  // -.-.	-> 'C'
		239,  // -.--	-> 'Y'
		250,  // --..	-> 'Z'
		251,  // --.-	-> 'Q'
		682,  // .....	-> '5'
		683,  // ....-	-> '4'
		687,  // ...--	-> '3'
		703,  // ..---  -> '2'
		767,  // .----  -> '1'
		938,  // -....  -> '6'
		939,  // -...-  -> '='
		942,  // -..-.  -> '/'
		1002, // --...  -> '7'
		1018, // ---..  -> '8'
		1022, // ----.  -> '9'
		1023, // -----  -> '0'
		2810, // ..--.. -> '?'
		2990, // .-..-. -> '_' / '"'
		3003, // .-.-.- -> '.'
		3054, // .--.-. -> '@'
		3070, // .----. -> '\''
		3755, // -....- -> '-'
		4015, // --..-- -> ','
		4074  // ---... -> ':'
};

#define CW_CHAR_CODES (sizeof(cw_char_codes)/sizeof(*cw_char_codes))
const char cw_char_chars[CW_CHAR_CODES] =
{
		' ',
		'E',
		'T',
		'I',
		'A',
		'N',
		'M',
		'S',
		'U',
		'R',
		'W',
		'D',
		'K',
		'G',
		'O',
		'H',
		'V',
		'F',
		'L',
		'P',
		'J',
		'B',
		'X',
		'C',
		'Y',
		'Z',
		'Q',
		'5',
		'4',
		'3',
		'2',
		'1',
		'6',
		'=',
		'/',
		'7',
		'8',
		'9',
		'0',
		'?',
		'"',
		'.',
		'@',
		'\'',
		'-',
		',',
		':'
};

const uint32_t cw_sign_codes[] =
{
		187, //   <AA>
		750, //   <AR>
		746, //   <AS>
		61114, // <CL>
		955, //   <CT>
		43690, // <HH>
		958, //   <KN>
		3775, //  <NJ>
		2747, //  <SK>
		686 //    <SN>
};

#define CW_SIGN_CODES (sizeof(cw_sign_codes)/sizeof(*cw_sign_codes))
const char* cw_sign_chars[CW_SIGN_CODES] =
{
		"AA",
		"AR",
		"AS",
		"CL",
		"CT",
		"HH",
		"KN",
		"NJ",
		"SK",
		"SN"
};

const char cw_sign_onechar[CW_SIGN_CODES] =
{
		'^', // AA
		'+', // AR
		'&', // AS
		'{', // CL
		'}', // CT
		0x7f, // HH
		'(', // KN
		'%', // NJ
		'>', // SK
		'~' // SN
};

//------------------------------------------------------------------
//
// The Character Identification Function applies dot/dash pattern
// recognition to identify the received character.
//
// The function returns the ASCII code for the character received,
// or 0xff if pattern was not recognized.
//
//------------------------------------------------------------------
uint8_t CwGen_CharacterIdFunc(uint32_t code)
{
	uint8_t out = 0xff; // 0xff selected to indicate ERROR
	// Should never happen - Empty, spike suppression or similar
	if (code == 0)
	{
		out = 0xfe;
	}

	for (int i = 0; i<CW_CHAR_CODES; i++)
	{
		if (cw_char_codes[i] == code) {
			out = cw_char_chars[i];
			break;
		}
	}

	if (out == 0xff)
	{
		for (int i = 0; i<CW_SIGN_CODES; i++)
		{
			if (cw_sign_codes[i] == code) {
				out = cw_sign_onechar[i];

				break;
			}
		}
	}

	return out;
}
