/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2001
 *
 * Author(s):
 *	Volker Fischer
 *
 * Description:
 *
 *	Note: We always shift the bits towards the MSB
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

#include "ConvEncoder.h"


/* Implementation *************************************************************/
int CConvEncoder::Encode(CVector<_DECISION>& vecInputData,
                         CVector<_DECISION>& vecOutputData)
{
    /* Set output size to zero, increment it each time a new bit is encoded */
    int iOutputCnt = 0;

    /* Reset counter for puncturing and state-register */
    _BYTE byStateShiftReg = 0;
#ifdef USE_MAX_LOG_MAP
    /* We know the initial state of the shift registers, therefore a very
       high soft information value */
    vecStateMem.Reset(ML_SOFT_INF_MAX_VALUE);
#endif

    for (int i = 0; i < iNumInBitsWithMemory; i++)
    {
        /* Update shift-register (state information) ------------------------ */
        /* Shift bits in state-shift-register */
        byStateShiftReg <<= 1;

        /* Tailbits are calculated in this loop. Check when end of vector is
           reached and no more bits must be added */
        if (i < iNumInBits)
        {
            /* Add new bit at the beginning */
            if (ExtractBit(vecInputData[i]) != 0)
                byStateShiftReg |= 1;

#ifdef USE_MAX_LOG_MAP
            /* Update shift register for soft information. We assume here that
               the decision type is some floating point type -> we use
               fabs() function */
            vecStateMem.AddBegin(fabs(vecInputData[i]));
#endif
        }


        /* Puncturing ------------------------------------------------------- */
        /* Depending on the current puncturing pattern, different numbers of
           output bits are generated. The state shift register "byStateShiftReg"
           is convoluted with the respective patterns for this bit (is done
           inside the convolution function) */
#ifdef USE_MAX_LOG_MAP
        switch (veciTablePuncPat[i])
        {
        case PP_TYPE_0001:
            /* Pattern 0001 */
            vecOutputData[iOutputCnt++] =
                SoftConvolution(byStateShiftReg, vecStateMem, 0);
            break;

        case PP_TYPE_0101:
            /* Pattern 0101 */
            vecOutputData[iOutputCnt++] =
                SoftConvolution(byStateShiftReg, vecStateMem, 0);

            vecOutputData[iOutputCnt++] =
                SoftConvolution(byStateShiftReg, vecStateMem, 2);
            break;

        case PP_TYPE_0011:
            /* Pattern 0011 */
            vecOutputData[iOutputCnt++] =
                SoftConvolution(byStateShiftReg, vecStateMem, 0);

            vecOutputData[iOutputCnt++] =
                SoftConvolution(byStateShiftReg, vecStateMem, 1);
            break;

        case PP_TYPE_0111:
            /* Pattern 0111 */
            vecOutputData[iOutputCnt++] =
                SoftConvolution(byStateShiftReg, vecStateMem, 0);

            vecOutputData[iOutputCnt++] =
                SoftConvolution(byStateShiftReg, vecStateMem, 1);

            vecOutputData[iOutputCnt++] =
                SoftConvolution(byStateShiftReg, vecStateMem, 2);
            break;

        case PP_TYPE_1111:
            /* Pattern 1111 */
            vecOutputData[iOutputCnt++] =
                SoftConvolution(byStateShiftReg, vecStateMem, 0);

            vecOutputData[iOutputCnt++] =
                SoftConvolution(byStateShiftReg, vecStateMem, 1);

            vecOutputData[iOutputCnt++] =
                SoftConvolution(byStateShiftReg, vecStateMem, 2);

            vecOutputData[iOutputCnt++] =
                SoftConvolution(byStateShiftReg, vecStateMem, 3);
            break;
        }
#else
        switch (veciTablePuncPat[i])
        {
        case PP_TYPE_0001:
            /* Pattern 0001 */
            vecOutputData[iOutputCnt++] = Convolution(byStateShiftReg, 0);
            break;

        case PP_TYPE_0101:
            /* Pattern 0101 */
            vecOutputData[iOutputCnt++] = Convolution(byStateShiftReg, 0);
            vecOutputData[iOutputCnt++] = Convolution(byStateShiftReg, 2);
            break;

        case PP_TYPE_0011:
            /* Pattern 0011 */
            vecOutputData[iOutputCnt++] = Convolution(byStateShiftReg, 0);
            vecOutputData[iOutputCnt++] = Convolution(byStateShiftReg, 1);
            break;

        case PP_TYPE_0111:
            /* Pattern 0111 */
            vecOutputData[iOutputCnt++] = Convolution(byStateShiftReg, 0);
            vecOutputData[iOutputCnt++] = Convolution(byStateShiftReg, 1);
            vecOutputData[iOutputCnt++] = Convolution(byStateShiftReg, 2);
            break;

        case PP_TYPE_1111:
            /* Pattern 1111 */
            vecOutputData[iOutputCnt++] = Convolution(byStateShiftReg, 0);
            vecOutputData[iOutputCnt++] = Convolution(byStateShiftReg, 1);
            vecOutputData[iOutputCnt++] = Convolution(byStateShiftReg, 2);
            vecOutputData[iOutputCnt++] = Convolution(byStateShiftReg, 3);
            break;
        }
#endif
    }

    /* Return number of encoded bits */
    return iOutputCnt;
}

#ifdef USE_MAX_LOG_MAP
_DECISION CConvEncoder::SoftConvolution(const _BYTE byNewStateShiftReg,
                                        CShiftRegister<_DECISION>& vecStateMem,
                                        const int iGenPolyn)
{
    _DECISION decSoftOut;

    /* Search for minimum norm value of input soft-informations.
       Here we implement the convolution of the soft information independent of
       the poylnoms stored in "byGeneratorMatrix[]"! When changing the polynoms,
       it has to be changed here, too */
    switch (iGenPolyn)
    {
    case 0:
    case 3:
        /* oct: 0155 -> 1101101 */
        decSoftOut = Min(Min(Min(Min(vecStateMem[0], vecStateMem[2]),
                                 vecStateMem[3]), vecStateMem[5]), vecStateMem[6]);
        break;

    case 1:
        /* oct: 0117 -> 1001111 */
        decSoftOut = Min(Min(Min(Min(vecStateMem[0], vecStateMem[1]),
                                 vecStateMem[2]), vecStateMem[3]), vecStateMem[6]);
        break;

    case 2:
        /* oct: 0123 -> 1010011 */
        decSoftOut = Min(Min(Min(vecStateMem[0], vecStateMem[1]),
                             vecStateMem[4]), vecStateMem[6]);
        break;
    }

    /* Hard decision defines the sign, the norm is defined by the minimum of
       input norms of soft informations using max-log approximation */
    if (Convolution(byNewStateShiftReg, iGenPolyn) == 0)
        return -decSoftOut;
    else
        return decSoftOut;
}
#endif

void CConvEncoder::Init(ECodScheme eNewCodingScheme,
                        EChanType eNewChannelType, int iN1,
                        int iN2, int iNewNumInBitsPartA,
                        int iNewNumInBitsPartB, int iPunctPatPartA,
                        int iPunctPatPartB, int iLevel)
{
    /* Number of bits out is the sum of all protection levels */
    iNumInBits = iNewNumInBitsPartA + iNewNumInBitsPartB;

    /* Number of out bits including the state memory */
    iNumInBitsWithMemory = iNumInBits + MC_CONSTRAINT_LENGTH - 1;

    /* Init vector, storing table for puncturing pattern and generate pattern */
    veciTablePuncPat.Init(iNumInBitsWithMemory);

    veciTablePuncPat = GenPuncPatTable(eNewCodingScheme, eNewChannelType, iN1,
                                       iN2, iNewNumInBitsPartA, iNewNumInBitsPartB, iPunctPatPartA,
                                       iPunctPatPartB, iLevel);

#ifdef USE_MAX_LOG_MAP
    vecStateMem.Init(MC_CONSTRAINT_LENGTH);
#endif
}
