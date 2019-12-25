/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2001
 *
 * Author(s):
 *	Volker Fischer
 *
 * Description:
 *	Symbol interleaver for MSC-symbols
 *	We have to support long and short symbol interleaving. Long interleaving
 *	spans over iD times iN_MUX interleaver blocks. To create a "block-wise 
 *	cycle-buffer" we shift the indices (stored in a table) each time a complete
 *	block was written. Thus, we dont need to copy data since we only modify
 *	the indices. 
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

#include "SymbolInterleaver.h"
#include "../matlib/MatlibStdToolbox.h"


/* Implementation *************************************************************/
/******************************************************************************\
* Symbol interleaver														   *
\******************************************************************************/
void CSymbInterleaver::ProcessDataInternal(CParameter&)
{
	int i, j;

	/* Write data in interleaver-memory (always index "0") */
	for (i = 0; i < iInputBlockSize; i++)
		matcInterlMemory[veciCurIndex[0]][i] = (*pvecInputData)[i];

	/* Interleave data according to the interleaver table. Use the
	   the interleaver-blocks described in the DRM-standard 
	   (Ro(i) = i (mod D)  -> "i % iD") */
	for (i = 0; i < iInputBlockSize; i++)
		(*pvecOutputData)[i] = 
			matcInterlMemory[veciCurIndex[i % iD]][veciIntTable[i]];

	/* Set new indices. Move blocks (virtually) forward */
	for (j = 0; j < iD; j++)
	{
		veciCurIndex[j]--;

		if (veciCurIndex[j] < 0)
			veciCurIndex[j] = iD - 1;
	}
}

void CSymbInterleaver::InitInternal(CParameter& TransmParam)
{
	int i;

	TransmParam.Lock(); 

	/* Set internal parameters */
	iN_MUX = TransmParam.CellMappingTable.iNumUsefMSCCellsPerFrame;

	/* Allocate memory for table */
	veciIntTable.Init(iN_MUX);

	/* Make interleaver table */
	MakeTable(veciIntTable, iN_MUX, SYMB_INTERL_CONST_T_0);

	/* Set interleaver depth */
	switch (TransmParam.eSymbolInterlMode)
	{
	case CParameter::SI_LONG:
		iD = D_LENGTH_LONG_INTERL;
		break;

	case CParameter::SI_SHORT:
		iD = D_LENGTH_SHORT_INTERL;
		break;
	}

	/* Always allocate memory for long interleaver case (interleaver memory) */
	matcInterlMemory.Init(D_LENGTH_LONG_INTERL, iN_MUX);

	/* Index for addressing the buffers */
	veciCurIndex.Init(D_LENGTH_LONG_INTERL);
	for (i = 0; i < D_LENGTH_LONG_INTERL; i++)
		veciCurIndex[i] = i;

	/* Define block-sizes for input and output */
	iInputBlockSize = iN_MUX;
	iOutputBlockSize = iN_MUX;

	/* Since the MSC logical frames must not end at the end of one symbol
	   (could be somewhere in the middle of the symbol), the output buffer must
	   accept more cells than one logical MSC frame is long */
	iMaxOutputBlockSize = 2 * iN_MUX;

	TransmParam.Unlock(); 
}


/******************************************************************************\
* Symbol deinterleaver														   *
\******************************************************************************/
void CSymbDeinterleaver::ProcessDataInternal(CParameter&)
{
	int i, j;

	/* Deinterleave data according to the deinterleaver table. Use the
	   the interleaver-blocks described in the DRM-standard 
	   (Ro(i) = i (mod D)  -> "i % iD") */
	for (i = 0; i < iInputBlockSize; i++)
		matcDeinterlMemory[veciCurIndex[i % iD]][veciIntTable[i]] = 
			(*pvecInputData)[i];

	/* Read data from current block (index "iD - 1")*/
	for (i = 0; i < iInputBlockSize; i++)
		(*pvecOutputData)[i] = matcDeinterlMemory[veciCurIndex[iD - 1]][i];

	/* Set new indices. Move blocks forward (virtually) */
	for (j = 0; j < iD; j++)
	{
		veciCurIndex[j]--;

		if (veciCurIndex[j] < 0)
			veciCurIndex[j] = iD - 1;
	}

	/* Debar initialization phase */
	if (iInitCnt > 0)
	{
		iInitCnt--;

		/* Do not put out data in initialization phase */
		iOutputBlockSize = 0;
	}
	else
		iOutputBlockSize = iN_MUX; 
}

void CSymbDeinterleaver::InitInternal(CParameter& Parameters)
{
	int i;

	Parameters.Lock(); 

	/* Set internal parameters */
	iN_MUX = Parameters.CellMappingTable.iNumUsefMSCCellsPerFrame;

	/* Allocate memory for table */
	veciIntTable.Init(iN_MUX);

	/* Make interleaver table */
	MakeTable(veciIntTable, iN_MUX, SYMB_INTERL_CONST_T_0);

	/* Set interleaver depth */
	switch (Parameters.eSymbolInterlMode)
	{
	case CParameter::SI_LONG:
		iD = D_LENGTH_LONG_INTERL;
		break;

	case CParameter::SI_SHORT:
		iD = D_LENGTH_SHORT_INTERL;
		break;
	}

	/* Always allocate memory for long interleaver case (interleaver memory) */
	matcDeinterlMemory.Init(D_LENGTH_LONG_INTERL, iN_MUX
#ifdef USE_ERASURE_FOR_FASTER_ACQ
		, CEquSig(ERASURE_TAG_VALUE, ERASURE_TAG_VALUE) /* Init with erasures */
#endif		
		);

	/* Index for addressing the buffers */
	veciCurIndex.Init(D_LENGTH_LONG_INTERL);
	for (i = 0; i < D_LENGTH_LONG_INTERL; i++)
		veciCurIndex[i] = i;

#ifdef USE_ERASURE_FOR_FASTER_ACQ
	if (Parameters.eSimType == CParameter::ST_NONE)
	{
		/* Output right after the first block */
		iInitCnt = (int) Min(1, iD);
	}
	else
#endif
	{
		/* After an initialization we do not put out data befor the number
		   symbols of the interleaver delay have been processed (this is
		   also needed in any case for simulations) */
		iInitCnt = iD - 1;
	}

	/* Define block-sizes for input and output */
	iInputBlockSize = iN_MUX;
	iMaxOutputBlockSize = iN_MUX;

	Parameters.Unlock(); 
}
