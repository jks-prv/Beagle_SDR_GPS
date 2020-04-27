/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2001
 *
 * Author(s):
 *	Volker Fischer
 *
 * Description:
 *	Tables for MLC
 *
 ******************************************************************************
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
\******************************************************************************/

#if !defined(_TABLE_MLC_H__3B0_CA63_4344_BB2B_23E7912__INCLUDED_)
#define _TABLE_MLC_H__3B0_CA63_4344_BB2B_23E7912__INCLUDED_

#include "../GlobalDefinitions.h"


/* Definitions ****************************************************************/
/* Default number of iterations at application startup */
#define MC_NUM_ITERATIONS				1

/* Generator polynomials used for channel coding (octal form, defined by
   a leading "0"!). We must bit-reverse the octal-forms given in the standard
   since we shift bits from right to the left! */
/* In this implementation we shift bits from right to left, therefore the order
   of the code-words are: [..., b_(0, i), b_(1, i), b(2, i), b(3, i), ...] */
#define MC_NUM_OUTPUT_BITS_PER_STEP		4	/* MC: Multi-level Coder */
const _BYTE byGeneratorMatrix[MC_NUM_OUTPUT_BITS_PER_STEP] = {
    0155,	/* (133) x_{0, i} */
    0117,	/* (171) x_{1, i} */
    0123,	/* (145) x_{2, i} */
    0155	/* (133) x_{3, i} */
};

#define MC_CONSTRAINT_LENGTH			7

/* Since we have a periodical structure in the trellis it
   is enough to build one step. 2^(MC_CONSTRAINT_LENGTH - 1) states have
   to be considered. ("- 1": since one bit is the transition to the next
   state) */
#define MC_NUM_STATES					(1 << (MC_CONSTRAINT_LENGTH - 1))
#define MC_NUM_OUTPUT_COMBINATIONS		(1 << MC_NUM_OUTPUT_BITS_PER_STEP)

/* Maximum number of levels (Its in case of HMmix) */
#define MC_MAX_NUM_LEVELS				6


/* Puncturing --------------------------------------------------------------- */
/* Only these types of patterns are used in DRM */
#define PP_TYPE_0000					0 /* not used, dummy */
#define PP_TYPE_1111					1
#define PP_TYPE_0111					2
#define PP_TYPE_0011					3
#define PP_TYPE_0001					4
#define PP_TYPE_0101					5

/* {a, b, c ...}: a = Number of groups, b = Number of "1"s, c = Patterns */
const uint32_t iPuncturingPatterns[13][10] = {
    /*
    B0: 1
    B1: 1
    B2: 1
    B3: 1
    */
    {1, 4,
        PP_TYPE_1111,
        PP_TYPE_0000,
        PP_TYPE_0000,
        PP_TYPE_0000,
        PP_TYPE_0000,
        PP_TYPE_0000,
        PP_TYPE_0000,
        PP_TYPE_0000},

    /*
    B0: 1 1 1
    B1: 1 1 1
    B2: 1 1 1
    B3: 1 0 0
    */
    {3, 10,
     PP_TYPE_1111,
     PP_TYPE_0111,
     PP_TYPE_0111,
     PP_TYPE_0000,
     PP_TYPE_0000,
     PP_TYPE_0000,
     PP_TYPE_0000,
     PP_TYPE_0000},

    /*
    B0: 1
    B1: 1
    B2: 1
    B3: 0
    */
    {1, 3,
     PP_TYPE_0111,
     PP_TYPE_0000,
     PP_TYPE_0000,
     PP_TYPE_0000,
     PP_TYPE_0000,
     PP_TYPE_0000,
     PP_TYPE_0000,
     PP_TYPE_0000},

    /*
    B0: 1 1 1 1
    B1: 1 1 1 1
    B2: 1 1 1 0
    B3: 0 0 0 0
    */
    {4, 11,
     PP_TYPE_0111,
     PP_TYPE_0111,
     PP_TYPE_0111,
     PP_TYPE_0011,
     PP_TYPE_0000,
     PP_TYPE_0000,
     PP_TYPE_0000,
     PP_TYPE_0000},

    /*
    B0: 1
    B1: 1
    B2: 0
    B3: 0
    */
    {1, 2,
     PP_TYPE_0011,
     PP_TYPE_0000,
     PP_TYPE_0000,
     PP_TYPE_0000,
     PP_TYPE_0000,
     PP_TYPE_0000,
     PP_TYPE_0000,
     PP_TYPE_0000},

    /*
    B0: 1 1 1 1
    B1: 1 0 1 0
    B2: 0 1 0 0
    B3: 0 0 0 0
    */
    {4, 7,
     PP_TYPE_0011,
     PP_TYPE_0101,
     PP_TYPE_0011,
     PP_TYPE_0001,
     PP_TYPE_0000,
     PP_TYPE_0000,
     PP_TYPE_0000,
     PP_TYPE_0000},

    /*
    B0: 1 1 1
    B1: 1 0 1
    B2: 0 0 0
    B3: 0 0 0
    */
    {3, 5,
     PP_TYPE_0011,
     PP_TYPE_0001,
     PP_TYPE_0011,
     PP_TYPE_0000,
     PP_TYPE_0000,
     PP_TYPE_0000,
     PP_TYPE_0000,
     PP_TYPE_0000},

    /*
    B0: 1 1
    B1: 1 0
    B2: 0 0
    B3: 0 0
    */
    {2, 3,
     PP_TYPE_0011,
     PP_TYPE_0001,
     PP_TYPE_0000,
     PP_TYPE_0000,
     PP_TYPE_0000,
     PP_TYPE_0000,
     PP_TYPE_0000,
     PP_TYPE_0000},

    /*
    B0: 1 1 1 1 1 1 1 1
    B1: 1 0 0 1 0 0 1 0
    B2: 0 0 0 0 0 0 0 0
    B3: 0 0 0 0 0 0 0 0
    */
    {8, 11,
     PP_TYPE_0011,
     PP_TYPE_0001,
     PP_TYPE_0001,
     PP_TYPE_0011,
     PP_TYPE_0001,
     PP_TYPE_0001,
     PP_TYPE_0011,
     PP_TYPE_0001},

    /*
    B0: 1 1 1
    B1: 1 0 0
    B2: 0 0 0
    B3: 0 0 0
    */
    {3, 4,
     PP_TYPE_0011,
     PP_TYPE_0001,
     PP_TYPE_0001,
     PP_TYPE_0000,
     PP_TYPE_0000,
     PP_TYPE_0000,
     PP_TYPE_0000,
     PP_TYPE_0000},

    /*
    B0: 1 1 1 1
    B1: 1 0 0 0
    B2: 0 0 0 0
    B3: 0 0 0 0
    */
    {4, 5,
     PP_TYPE_0011,
     PP_TYPE_0001,
     PP_TYPE_0001,
     PP_TYPE_0001,
     PP_TYPE_0000,
     PP_TYPE_0000,
     PP_TYPE_0000,
     PP_TYPE_0000},

    /*
    B0: 1 1 1 1 1 1 1
    B1: 1 0 0 0 0 0 0
    B2: 0 0 0 0 0 0 0
    B3: 0 0 0 0 0 0 0
    */
    {7, 8,
     PP_TYPE_0011,
     PP_TYPE_0001,
     PP_TYPE_0001,
     PP_TYPE_0001,
     PP_TYPE_0001,
     PP_TYPE_0001,
     PP_TYPE_0001,
     PP_TYPE_0000},

    /*
    B0: 1 1 1 1 1 1 1 1
    B1: 1 0 0 0 0 0 0 0
    B2: 0 0 0 0 0 0 0 0
    B3: 0 0 0 0 0 0 0 0
    */
    {8, 9,
     PP_TYPE_0011,
     PP_TYPE_0001,
     PP_TYPE_0001,
     PP_TYPE_0001,
     PP_TYPE_0001,
     PP_TYPE_0001,
     PP_TYPE_0001,
     PP_TYPE_0001}
};

/* Puncturing patterns for tailbits */
#define LENGTH_TAIL_BIT_PAT				6
const uint32_t iPunctPatTailbits[12][LENGTH_TAIL_BIT_PAT] = {
    /*
    B0: 1 1 1 1 1 1
    B1: 1 1 1 1 1 1
    B2: 0 0 0 0 0 0
    B3: 0 0 0 0 0 0
    */
    {PP_TYPE_0011,
        PP_TYPE_0011,
        PP_TYPE_0011,
        PP_TYPE_0011,
        PP_TYPE_0011,
        PP_TYPE_0011},

    /*
    B0: 1 1 1 1 1 1
    B1: 1 1 1 1 1 1
    B2: 1 0 0 0 0 0
    B3: 0 0 0 0 0 0
    */
    {PP_TYPE_0111,
     PP_TYPE_0011,
     PP_TYPE_0011,
     PP_TYPE_0011,
     PP_TYPE_0011,
     PP_TYPE_0011},

    /*
    B0: 1 1 1 1 1 1
    B1: 1 1 1 1 1 1
    B2: 1 0 0 1 0 0
    B3: 0 0 0 0 0 0
    */
    {PP_TYPE_0111,
     PP_TYPE_0011,
     PP_TYPE_0011,
     PP_TYPE_0111,
     PP_TYPE_0011,
     PP_TYPE_0011},

    /*
    B0: 1 1 1 1 1 1
    B1: 1 1 1 1 1 1
    B2: 1 1 0 1 0 0
    B3: 0 0 0 0 0 0
    */
    {PP_TYPE_0111,
     PP_TYPE_0111,
     PP_TYPE_0011,
     PP_TYPE_0111,
     PP_TYPE_0011,
     PP_TYPE_0011},

    /*
    B0: 1 1 1 1 1 1
    B1: 1 1 1 1 1 1
    B2: 1 1 0 1 1 0
    B3: 0 0 0 0 0 0
    */
    {PP_TYPE_0111,
     PP_TYPE_0111,
     PP_TYPE_0011,
     PP_TYPE_0111,
     PP_TYPE_0111,
     PP_TYPE_0011},

    /*
    B0: 1 1 1 1 1 1
    B1: 1 1 1 1 1 1
    B2: 1 1 1 1 1 0
    B3: 0 0 0 0 0 0
    */
    {PP_TYPE_0111,
     PP_TYPE_0111,
     PP_TYPE_0111,
     PP_TYPE_0111,
     PP_TYPE_0111,
     PP_TYPE_0011},

    /*
    B0: 1 1 1 1 1 1
    B1: 1 1 1 1 1 1
    B2: 1 1 1 1 1 1
    B3: 0 0 0 0 0 0
    */
    {PP_TYPE_0111,
     PP_TYPE_0111,
     PP_TYPE_0111,
     PP_TYPE_0111,
     PP_TYPE_0111,
     PP_TYPE_0111},

    /*
    B0: 1 1 1 1 1 1
    B1: 1 1 1 1 1 1
    B2: 1 1 1 1 1 1
    B3: 1 0 0 0 0 0
    */
    {PP_TYPE_1111,
     PP_TYPE_0111,
     PP_TYPE_0111,
     PP_TYPE_0111,
     PP_TYPE_0111,
     PP_TYPE_0111},

    /*
    B0: 1 1 1 1 1 1
    B1: 1 1 1 1 1 1
    B2: 1 1 1 1 1 1
    B3: 1 0 0 1 0 0
    */
    {PP_TYPE_1111,
     PP_TYPE_0111,
     PP_TYPE_0111,
     PP_TYPE_1111,
     PP_TYPE_0111,
     PP_TYPE_0111},

    /*
    B0: 1 1 1 1 1 1
    B1: 1 1 1 1 1 1
    B2: 1 1 1 1 1 1
    B3: 1 1 0 1 0 0
    */
    {PP_TYPE_1111,
     PP_TYPE_1111,
     PP_TYPE_0111,
     PP_TYPE_1111,
     PP_TYPE_0111,
     PP_TYPE_0111},

    /*
    B0: 1 1 1 1 1 1
    B1: 1 1 1 1 1 1
    B2: 1 1 1 1 1 1
    B3: 1 1 0 1 0 1
    */
    {PP_TYPE_1111,
     PP_TYPE_1111,
     PP_TYPE_0111,
     PP_TYPE_1111,
     PP_TYPE_0111,
     PP_TYPE_1111},

    /*
    B0: 1 1 1 1 1 1
    B1: 1 1 1 1 1 1
    B2: 1 1 1 1 1 1
    B3: 1 1 1 1 0 1
    */
    {PP_TYPE_1111,
     PP_TYPE_1111,
     PP_TYPE_1111,
     PP_TYPE_1111,
     PP_TYPE_0111,
     PP_TYPE_1111},
};


/* Code rate combinations --------------------------------------------------- */
/* row-index: protection level */
/* {a, b, c}: a = R_0, b = R_1, c = RY_Icm */
const int iCodRateCombMSC16SM[2][3] = {
    {2, 7, 3},
    {4, 9, 4}
};

/* {a, b, c, d}: a = R_0, b = R_1, c = R_2, d = RY_Icm */
const int iCodRateCombMSC64SM[4][4] = {
    {0,  4,  9,  4},
    {2,  7, 10, 15},
    {4,  9, 11,  8},
    {7, 10, 12, 45}
};

/* {a, b, c, d}: a = R_0, b = R_1, c = R_2, d = RY_Icm */
const int iCodRateCombMSC64HMsym[4][4] = {
    {4, 1,  6, 10},
    {5, 3,  8, 11},
    {6, 5, 11, 56},
    {7, 7, 12,  9}
};

/* {a, b, c, d, e, f, g}: a = R_0Re, b = R_0Im, c = R_1Re, d = R_1Im,
   e = R_2Re, f = R_2Im, g = RY_Icm */
const int iCodRateCombMSC64HMmix[4][7] = {
    {4, 0, 1,  4,  6,  9,  20},
    {5, 2, 3,  7,  8, 10, 165},
    {6, 4, 5,  9, 11, 11,  56},
    {7, 7, 7, 10, 12, 12,  45}
};

/* {a, b}: a = R_0, b = R_1 */
const int iCodRateCombSDC16SM[2] = {
    2, 7
};

/* {a}: a = R_0 */
const int iCodRateCombSDC4SM = {
    4
};

/* {a}: a = R_0 */
const int iCodRateCombFDC4SM = {
    6
};


/* Interleaver sequence ----------------------------------------------------- */
/* The different coding modes in DRM use bit-interleavers in certain paths. We
   define the following std::vectors to store the position and type of the
   interleaver as described in the DRM-standard

   Interleaver modules have to be initialized in the way:
   [0]: t_0 = 13;
   [1]: t_0 = 21;
   "-1": no interleaver in this level */
const int iInterlSequ4SM[MC_MAX_NUM_LEVELS] =     { 1, -1, -1, -1, -1, -1};
const int iInterlSequ16SM[MC_MAX_NUM_LEVELS] =    { 0,  1, -1, -1, -1, -1};
const int iInterlSequ64SM[MC_MAX_NUM_LEVELS] =    {-1,  0,  1, -1, -1, -1};
const int iInterlSequ64HMsym[MC_MAX_NUM_LEVELS] = {-1,  0,  1, -1, -1, -1};
const int iInterlSequ64HMmix[MC_MAX_NUM_LEVELS] = {-1, -1,  0,  0,  1,  1};


#endif // !defined(_TABLE_MLC_H__3B0_CA63_4344_BB2B_23E7912__INCLUDED_)
