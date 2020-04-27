/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2001
 *
 * Author(s):
 *	Volker Fischer
 *
 * Description:
 *
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

#include "ChannelCode.h"


/* Implementation *************************************************************/
CVector<int> CChannelCode::GenPuncPatTable(ECodScheme eNewCodingScheme,
        EChanType eNewChannelType,
        int iN1, int iN2,
        int iNewNumOutBitsPartA,
        int iNewNumOutBitsPartB,
        int iPunctPatPartA, int iPunctPatPartB,
        int iLevel)
{
    int				i;
    int				iNumOutBits;
    int				iNumOutBitsWithMemory;
    int				iTailbitPattern;
    int				iTailbitParamL0;
    int				iTailbitParamL1;
    int				iPartAPatLen;
    int				iPartBPatLen;
    int				iPunctCounter;
    CVector<int>	veciPuncPatPartA;
    CVector<int>	veciPuncPatPartB;
    CVector<int>	veciTailBitPat;
    CVector<int>	veciReturn;

    /* Number of bits out is the sum of all protection levels */
    iNumOutBits = iNewNumOutBitsPartA + iNewNumOutBitsPartB;

    /* Number of out bits including the state memory */
    iNumOutBitsWithMemory = iNumOutBits + MC_CONSTRAINT_LENGTH - 1;

    /* Init vector, storing table for puncturing pattern */
    veciReturn.Init(iNumOutBitsWithMemory);


    /* Set tail-bit pattern ------------------------------------------------- */
    /* We have to consider two cases because in HSYM "N1 + N2" is used
       instead of only "N2" to calculate the tailbit pattern */
    switch (eNewCodingScheme)
    {
    case CS_3_HMMIX:
        iTailbitParamL0 = iN1 + iN2;
        iTailbitParamL1 = iN2;
        break;

    case CS_3_HMSYM:
        iTailbitParamL0 = 2 * (iN1 + iN2);
        iTailbitParamL1 = 2 * iN2;
        break;

    default:
        iTailbitParamL0 = 2 * iN2;
        iTailbitParamL1 = 2 * iN2;
    }

    /* Tailbit pattern calculated according DRM-standard. We have to consider
       two cases because in HSYM "N1 + N2" is used instead of only "N2" */
    if (iLevel == 0)
        iTailbitPattern =
            iTailbitParamL0 - 12 - iPuncturingPatterns[iPunctPatPartB][1] *
            (int) ((iTailbitParamL0 - 12) /
                   iPuncturingPatterns[iPunctPatPartB][1]);
    else
        iTailbitPattern =
            iTailbitParamL1 - 12 - iPuncturingPatterns[iPunctPatPartB][1] *
            (int) ((iTailbitParamL1 - 12) /
                   iPuncturingPatterns[iPunctPatPartB][1]);


    /* Set puncturing bit patterns and lengths ------------------------------ */
    /* Lengths */
    iPartAPatLen = iPuncturingPatterns[iPunctPatPartA][0];
    iPartBPatLen = iPuncturingPatterns[iPunctPatPartB][0];

    /* Vector, storing patterns for part A. Patterns begin at [][2 + x] */
    veciPuncPatPartA.Init(iPartAPatLen);
    for (i = 0; i < iPartAPatLen; i++)
        veciPuncPatPartA[i] = iPuncturingPatterns[iPunctPatPartA][2 + i];

    /* Vector, storing patterns for part B. Patterns begin at [][2 + x] */
    veciPuncPatPartB.Init(iPartBPatLen);
    for (i = 0; i < iPartBPatLen; i++)
        veciPuncPatPartB[i] = iPuncturingPatterns[iPunctPatPartB][2 + i];

    /* Vector, storing patterns for tailbit pattern */
    veciTailBitPat.Init(LENGTH_TAIL_BIT_PAT);
    for (i = 0; i < LENGTH_TAIL_BIT_PAT; i++)
        veciTailBitPat[i] = iPunctPatTailbits[iTailbitPattern][i];


    /* Generate actual table for puncturing pattern ------------------------- */
    /* Reset counter for puncturing */
    iPunctCounter = 0;

    for (i = 0; i < iNumOutBitsWithMemory; i++)
    {
        if (i < iNewNumOutBitsPartA)
        {
            /* Puncturing patterns part A */
            /* Get current pattern */
            veciReturn[i] = veciPuncPatPartA[iPunctCounter];

            /* Increment index and take care of wrap around */
            iPunctCounter++;
            if (iPunctCounter == iPartAPatLen)
                iPunctCounter = 0;
        }
        else
        {
            /* In case of FAC do not use special tailbit-pattern! */
            if ((i < iNumOutBits) || (eNewChannelType == CT_FAC))
            {
                /* Puncturing patterns part B */
                /* Reset counter when beginning of part B is reached */
                if (i == iNewNumOutBitsPartA)
                    iPunctCounter = 0;

                /* Get current pattern */
                veciReturn[i] = veciPuncPatPartB[iPunctCounter];

                /* Increment index and take care of wrap around */
                iPunctCounter++;
                if (iPunctCounter == iPartBPatLen)
                    iPunctCounter = 0;
            }
            else
            {
                /* Tailbits */
                /* Check when tailbit pattern starts */
                if (i == iNumOutBits)
                    iPunctCounter = 0;

                /* Set tailbit pattern */
                veciReturn[i] = veciTailBitPat[iPunctCounter];

                /* No test for wrap around needed, since there ist only one
                   cycle of this pattern */
                iPunctCounter++;
            }
        }
    }

    return veciReturn;
}

CChannelCode::CChannelCode()
{
    /* Create table for parity bit */
    for (int j = 0; j < 1 << SIZEOF__BYTE; j++)
    {
        /* XOR all bits in byResult.
           We observe always the LSB by masking using operator "& 1". To get
           access to all bits in "byResult" we shift the current bit so long
           until it reaches the mask (at zero) by using operator ">> i". The
           actual XOR operation is done by "^=" */
        vecbiParity[j] = 0;
        for (int i = 0; i < MC_CONSTRAINT_LENGTH; i++)
            vecbiParity[j] ^= (j >> i) & 1;
    }
}
