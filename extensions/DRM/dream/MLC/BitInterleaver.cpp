/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2001
 *
 * Author(s):
 *	Volker Fischer
 *
 * Description:
 *
 * The two parts with different protection levels shall not overlap in the
 * interleaving process. Therefore the interleaved lower protected part shall
 * be appended to the interleaved higher protected part.
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

#include "BitInterleaver.h"


/* Implementation *************************************************************/
/******************************************************************************\
* Bit interleaver															   *
\******************************************************************************/
void CBitInterleaver::Interleave(CVector<_DECISION>& InputData)
{
    int i;

    /* Block 1 -------------------------------------------------------------- */
    /* Interleave data according the interleaver table */
    for (i = 0; i < ix_in1; i++)
        vecInterlMemory1[i] = InputData[veciIntTable1[i]];

    /* Copy result in input-vector */
    for (i = 0; i < ix_in1; i++)
        InputData[i] = vecInterlMemory1[i];

    /* Block 2 -------------------------------------------------------------- */
    /* Interleave data according the interleaver table */
    for (i = 0; i < ix_in2; i++)
        vecInterlMemory2[i] = InputData[veciIntTable2[i] + ix_in1];

    /* Copy result in input-vector */
    for (i = 0; i < ix_in2; i++)
        InputData[i + ix_in1] = vecInterlMemory2[i];
}

void CBitInterleaver::Init(int iNewx_in1, int iNewx_in2, int it_0)
{
    /* Set internal parameters */
    ix_in1 = iNewx_in1;
    ix_in2 = iNewx_in2;

    /* ix_in1 can be 0 but ix_in2 is always greater than "0" */
    if (ix_in1 > 0)
    {
        /* Allocate memory for table */
        veciIntTable1.Init(ix_in1);

        /* Make interleaver table */
        MakeTable(veciIntTable1, ix_in1, it_0);

        /* Allocate memory for interleaver */
        vecInterlMemory1.Init(ix_in1);
    }

    /* Allocate memory for table */
    veciIntTable2.Init(ix_in2);

    /* Make interleaver table */
    MakeTable(veciIntTable2, ix_in2, it_0);

    /* Allocate memory for interleaver */
    vecInterlMemory2.Init(ix_in2);
}


/******************************************************************************\
* Bit deinterleaver															   *
\******************************************************************************/
void CBitDeinterleaver::Deinterleave(CVector<CDistance>& vecInput)
{
    int i;

    /* Block 1 -------------------------------------------------------------- */
    /* Deinterleave data according the deinterleaver table */
    for (i = 0; i < ix_in1; i++)
        vecDeinterlMemory1[veciIntTable1[i]] = vecInput[i];

    /* Copy result in input-vector */
    for (i = 0; i < ix_in1; i++)
        vecInput[i] = vecDeinterlMemory1[i];

    /* Block 2 -------------------------------------------------------------- */
    /* Deinterleave data according the deinterleaver table */
    for (i = 0; i < ix_in2; i++)
        vecDeinterlMemory2[veciIntTable2[i]] = vecInput[i + ix_in1];

    /* Copy result in input-vector */
    for (i = 0; i < ix_in2; i++)
        vecInput[i + ix_in1] = vecDeinterlMemory2[i];
}

void CBitDeinterleaver::Init(int iNewx_in1, int iNewx_in2, int it_0)
{
    /* Set internal parameters */
    ix_in1 = iNewx_in1;
    ix_in2 = iNewx_in2;

    /* ix_in1 can be 0 but ix_in2 is always greater than "0" */
    if (ix_in1 > 0)
    {
        /* Allocate memory for table */
        veciIntTable1.Init(ix_in1);

        /* Make interleaver table */
        MakeTable(veciIntTable1, ix_in1, it_0);

        /* Allocate memory for interleaver */
        vecDeinterlMemory1.Init(ix_in1);
    }

    /* Allocate memory for table */
    veciIntTable2.Init(ix_in2);

    /* Make interleaver table */
    MakeTable(veciIntTable2, ix_in2, it_0);

    /* Allocate memory for interleaver */
    vecDeinterlMemory2.Init(ix_in2);
}
