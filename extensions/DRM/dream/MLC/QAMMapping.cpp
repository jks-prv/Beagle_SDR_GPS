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

#include "QAMMapping.h"


/* Implementation *************************************************************/
void CQAMMapping::Map(CVector<_DECISION>& vecInputData1,
                      CVector<_DECISION>& vecInputData2,
                      CVector<_DECISION>& vecInputData3,
                      CVector<_DECISION>& vecInputData4,
                      CVector<_DECISION>& vecInputData5,
                      CVector<_DECISION>& vecInputData6,
                      CVector<_COMPLEX>* pcOutputData)
{
    /*
    	We always use "& 1" when we combine binary values with logical operators
    	for safety reasons.
    */
    int i;
    int iIndexReal;
    int iIndexImag;

    switch (eMapType)
    {
    case CS_1_SM:
        /* 4QAM ------------------------------------------------------------- */
        /* Mapping according DRM-standard:
           {i_0  q_0} = (y'_0  y'_1) = (y_0,0  y_0,1) */
        for (i = 0; i < iOutputBlockSize; i++)
        {
            (*pcOutputData)[i] = _COMPLEX(
                                     /* Odd entries (second column in "rTableQAM4") */
                                     rTableQAM4[ExtractBit(vecInputData1[2 * i]) & 1][0],
                                     /* Even entries in input-vector */
                                     rTableQAM4[ExtractBit(vecInputData1[2 * i + 1]) & 1][1]);
        }
        break;

    case CS_2_SM:
        /* 16QAM ------------------------------------------------------------ */
        /* Mapping according DRM-standard:
           {i_0  i_1  q_0  q_1} = (y_0,0  y_1,0  y_0,1  y_1,1) */
        for (i = 0; i < iOutputBlockSize; i++)
        {
            const int i2i = 2 * i;
            const int i2ip1 = 2 * i + 1;

            /* Filling indices [y_0,0, y_1,0]. Incoming bits are shifted to
               their desired positions in the integer variables "iIndexImag"
               and "iIndexReal" and combined */
            iIndexReal = ((ExtractBit(vecInputData1[i2i]) & 1) << 1) |
                         (ExtractBit(vecInputData2[i2i]) & 1);
            iIndexImag = ((ExtractBit(vecInputData1[i2ip1]) & 1) << 1) |
                         (ExtractBit(vecInputData2[i2ip1]) & 1);

            (*pcOutputData)[i] =
                /* Odd entries (second column in "rTableQAM16") */
                _COMPLEX(rTableQAM16[iIndexReal][0],
                         /* Even entries in input-vector */
                         rTableQAM16[iIndexImag][1]);
        }
        break;

    case CS_3_SM:
        /* 64QAM SM --------------------------------------------------------- */
        /* Mapping according DRM-standard: {i_0  i_1  i_2  q_0  q_1  q_2} =
           (y_0,0  y_1,0  y_2,0  y_0,1  y_1,1  y_2,1) */
        for (i = 0; i < iOutputBlockSize; i++)
        {
            const int i2i = 2 * i;
            const int i2ip1 = 2 * i + 1;

            /* Filling indices [y_0,0, y_1,0, y_2,0]. Incoming bits
               are shifted to their desired positions in the integer variables
               "iIndexImag" and "iIndexReal" and combined */
            iIndexReal = ((ExtractBit(vecInputData1[i2i]) & 1) << 2) |
                         ((ExtractBit(vecInputData2[i2i]) & 1) << 1) |
                         (ExtractBit(vecInputData3[i2i]) & 1);
            iIndexImag = ((ExtractBit(vecInputData1[i2ip1]) & 1) << 2) |
                         ((ExtractBit(vecInputData2[i2ip1]) & 1) << 1) |
                         (ExtractBit(vecInputData3[i2ip1]) & 1);

            (*pcOutputData)[i] =
                /* Odd entries (second column in "rTableQAM64SM") */
                _COMPLEX(rTableQAM64SM[iIndexReal][0],
                         /* Even entries in input-vector */
                         rTableQAM64SM[iIndexImag][1]);
        }
        break;

    case CS_3_HMSYM:
        /* 64QAM HMsym ------------------------------------------------------ */
        /* Mapping according DRM-standard: {i_0  i_1  i_2  q_0  q_1  q_2} =
           (y_0,0  y_1,0  y_2,0  y_0,1  y_1,1  y_2,1) */
        for (i = 0; i < iOutputBlockSize; i++)
        {
            const int i2i = 2 * i;
            const int i2ip1 = 2 * i + 1;

            /* Filling indices [y_0,0, y_1,0, y_2,0]. Incoming bits
               are shifted to their desired positions in the integer variables
               "iIndexImag" and "iIndexReal" and combined */
            iIndexReal = ((ExtractBit(vecInputData1[i2i]) & 1) << 2) |
                         ((ExtractBit(vecInputData2[i2i]) & 1) << 1) |
                         (ExtractBit(vecInputData3[i2i]) & 1);
            iIndexImag = ((ExtractBit(vecInputData1[i2ip1]) & 1) << 2) |
                         ((ExtractBit(vecInputData2[i2ip1]) & 1) << 1) |
                         (ExtractBit(vecInputData3[i2ip1]) & 1);

            (*pcOutputData)[i] =
                /* Odd entries (second column in "rTableQAM64HMsym") */
                _COMPLEX(rTableQAM64HMsym[iIndexReal][0],
                         /* Even entries in input-vector */
                         rTableQAM64HMsym[iIndexImag][1]);
        }
        break;

    case CS_3_HMMIX:
        /* 64QAM HMmix------------------------------------------------------- */
        /* Mapping according DRM-standard: {i_0  i_1  i_2  q_0  q_1  q_2} =
           (y_0,0Re  y_1,0Re  y_2,0Re  y_0,0Im  y_1,0Im  y_2,0Im) */
        for (i = 0; i < iOutputBlockSize; i++)
        {
            /* Filling indices [y_0,0, y_1,0, y_2,0] (Re, Im). Incoming bits
               are shifted to their desired positions in the integer variables
               "iIndexImag" and "iIndexReal" and combined */
            iIndexReal = ((ExtractBit(vecInputData1[i]) & 1) << 2) |
                         ((ExtractBit(vecInputData3[i]) & 1) << 1) |
                         (ExtractBit(vecInputData5[i]) & 1);
            iIndexImag = ((ExtractBit(vecInputData2[i]) & 1) << 2) |
                         ((ExtractBit(vecInputData4[i]) & 1) << 1) |
                         (ExtractBit(vecInputData6[i]) & 1);

            (*pcOutputData)[i] =
                /* Odd entries (second column in "rTableQAM64HMmix") */
                _COMPLEX(rTableQAM64HMmix[iIndexReal][0],
                         /* Even entries in input-vector */
                         rTableQAM64HMmix[iIndexImag][1]);
        }
        break;
    }
}

void CQAMMapping::Init(int iNewOutputBlockSize, ECodScheme eNewCodingScheme)
{
    /* Set the two internal parameters */
    iOutputBlockSize = iNewOutputBlockSize;
    eMapType = eNewCodingScheme;
}
