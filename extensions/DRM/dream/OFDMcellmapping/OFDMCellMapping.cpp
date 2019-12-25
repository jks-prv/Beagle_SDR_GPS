/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2001
 *
 * Author(s):
 *	Volker Fischer
 *
 * Description:
 *	Mapping of the symbols on the carriers
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

#include "OFDMCellMapping.h"


/* Implementation *************************************************************/
/******************************************************************************\
* OFDM cells mapping														   *
\******************************************************************************/
void COFDMCellMapping::ProcessDataInternal(CParameter& TransmParam)
{
	const CCellMappingTable& Param = TransmParam.CellMappingTable;

	/* Mapping of the data and pilot cells on the OFDM symbol --------------- */
	/* Set absolute symbol position */
	int iSymbolCounterAbs =
		TransmParam.iFrameIDTransm * iNumSymPerFrame + iSymbolCounter;

	/* Init temporary counter */
	int iDummyCellCounter = 0;
	int iMSCCounter = 0;
	int iFACCounter = 0;
	int iSDCCounter = 0;
	for (int iCar = 0; iCar < iNumCarrier; iCar++)
	{
		/* MSC */
		if (_IsMSC(Param.matiMapTab[iSymbolCounterAbs][iCar]))
		{
			if (iMSCCounter >= Param.veciNumMSCSym[iSymbolCounterAbs])
			{
				/* Insert dummy cells */
				(*pvecOutputData)[iCar] = pcDummyCells[iDummyCellCounter];

				iDummyCellCounter++;
			}
			else
				(*pvecOutputData)[iCar] = (*pvecInputData)[iMSCCounter];
				
			iMSCCounter++;
		}

		/* FAC */
		if (_IsFAC(Param.matiMapTab[iSymbolCounterAbs][iCar]))
		{
			(*pvecOutputData)[iCar] = (*pvecInputData2)[iFACCounter];
				
			iFACCounter++;
		}

		/* SDC */
		if (_IsSDC(Param.matiMapTab[iSymbolCounterAbs][iCar]))
		{
			(*pvecOutputData)[iCar] = (*pvecInputData3)[iSDCCounter];
				
			iSDCCounter++;
		}

		/* Pilots */
		if (_IsPilot(Param.matiMapTab[iSymbolCounterAbs][iCar]))
			(*pvecOutputData)[iCar] = Param.matcPilotCells[iSymbolCounterAbs][iCar];

		/* DC carrier */
		if (_IsDC(Param.matiMapTab[iSymbolCounterAbs][iCar]))
			(*pvecOutputData)[iCar] = _COMPLEX((_REAL) 0.0, (_REAL) 0.0);
	}

	/* Increase symbol-counter and wrap if needed */
	iSymbolCounter++;
	if (iSymbolCounter == iNumSymPerFrame)
	{
		iSymbolCounter = 0;

		/* Increase frame-counter (ID) (Used also in FAC.cpp) */
		TransmParam.iFrameIDTransm++;
		if (TransmParam.iFrameIDTransm == NUM_FRAMES_IN_SUPERFRAME)
			TransmParam.iFrameIDTransm = 0;
	}

	/* Set absolute symbol position (for updated relative positions) */
	iSymbolCounterAbs =
		TransmParam.iFrameIDTransm * iNumSymPerFrame + iSymbolCounter;

	/* Set input block-sizes for next symbol */
	iInputBlockSize = Param.veciNumMSCSym[iSymbolCounterAbs];
	iInputBlockSize2 = Param.veciNumFACSym[iSymbolCounterAbs];
	iInputBlockSize3 = Param.veciNumSDCSym[iSymbolCounterAbs];
}

void COFDMCellMapping::InitInternal(CParameter& TransmParam)
{
	const CCellMappingTable& Param = TransmParam.CellMappingTable;

	iNumSymPerFrame = Param.iNumSymPerFrame;
	iNumCarrier = Param.iNumCarrier;

	/* Init symbol-counter */
	iSymbolCounter = 0;

	/* Init frame ID */
	TransmParam.iFrameIDTransm = 0;

	/* Choose right dummy cells for MSC QAM scheme */
	switch (TransmParam.eMSCCodingScheme)
	{
	case CS_1_SM: /* no 4QAM in MSC */
		break;

	case CS_2_SM:
		pcDummyCells = (_COMPLEX*) &cDummyCells16QAM[0];
		break;

	case CS_3_SM:
	case CS_3_HMSYM:
	case CS_3_HMMIX:
		pcDummyCells = (_COMPLEX*) &cDummyCells64QAM[0];
		break;
	}

	/* Define block-sizes for input and output of the module ---------------- */
	iInputBlockSize = Param.veciNumMSCSym[0]; /* MSC */
	iInputBlockSize2 = Param.veciNumFACSym[0]; /* FAC */
	iInputBlockSize3 = Param.veciNumSDCSym[0]; /* SDC */
	iOutputBlockSize = Param.iNumCarrier; /* Output */
}


/******************************************************************************\
* OFDM cells demapping														   *
\******************************************************************************/
void COFDMCellDemapping::ProcessDataInternal(CParameter& Parameters)
{
	const CCellMappingTable& Param = Parameters.CellMappingTable;

	/* Set absolute symbol position */
	const int iSymbolCounterAbs =
		iCurrentFrameID * iNumSymPerFrame + iSymbolCounter;

	/* Set output block-sizes for this symbol */
	iOutputBlockSize = Param.veciNumMSCSym[iSymbolCounterAbs];
	iOutputBlockSize2 = Param.veciNumFACSym[iSymbolCounterAbs];
	iOutputBlockSize3 = Param.veciNumSDCSym[iSymbolCounterAbs];

	/* Demap data from the cells */
	int iMSCCounter = 0;
	int iFACCounter = 0;
	int iSDCCounter = 0;
	for (int iCar = 0; iCar < iNumCarrier; iCar++)
	{
		/* MSC */
		if (_IsMSC(Param.matiMapTab[iSymbolCounterAbs][iCar]))
		{
			/* Ignore dummy cells */
			if (iMSCCounter < Param.veciNumMSCSym[iSymbolCounterAbs])
			{
				(*pvecOutputData)[iMSCCounter] = (*pvecInputData)[iCar];

				iMSCCounter++; /* Local counter */
				iMSCCellCounter++; /* Super-frame counter */
			}
		}

		/* FAC */
		if (_IsFAC(Param.matiMapTab[iSymbolCounterAbs][iCar]))
		{
			(*pvecOutputData2)[iFACCounter] = (*pvecInputData)[iCar];

			iFACCounter++; /* Local counter */
			iFACCellCounter++; /* Super-frame counter */
		}

		/* SDC */
		if (_IsSDC(Param.matiMapTab[iSymbolCounterAbs][iCar]))
		{
			(*pvecOutputData3)[iSDCCounter] = (*pvecInputData)[iCar];

			iSDCCounter++; /* Local counter */
			iSDCCellCounter++; /* Super-frame counter */
		}
	}

	/* Get symbol-counter for next symbol and adjust frame-ID. Use the extended
	   data, shipped with the input vector */
	int iNewSymbolCounter = (*pvecInputData).GetExData().iSymbolID + 1;

	/* Check range (iSymbolCounter must be in {0, ... , iNumSymPerFrame - 1} */
	while (iNewSymbolCounter >= iNumSymPerFrame)
		iNewSymbolCounter -= iNumSymPerFrame;
	while (iNewSymbolCounter < 0)
		iNewSymbolCounter += iNumSymPerFrame;

	/* Increment internal symbol counter and take care of wrap around */
	iSymbolCounter++;
	if (iSymbolCounter == iNumSymPerFrame)
		iSymbolCounter = 0;

	/* Check if symbol counter has changed (e.g. due to frame synchronization
	   unit). Reset all buffers in that case to avoid buffer overflow */
	if (iSymbolCounter != iNewSymbolCounter)
	{
		/* Init symbol counter with new value and reset all output buffers */
		iSymbolCounter = iNewSymbolCounter;

		SetBufReset1();
		SetBufReset2();
		SetBufReset3();
		iMSCCellCounter = 0;
		iFACCellCounter = 0;
		iSDCCellCounter = 0;
	}

	/* If frame bound is reached, update frame ID from FAC stream */
	if (iSymbolCounter == 0)
	{
		/* Check, if number of FAC cells is correct. If not, reset
		   output cyclic-buffer. An incorrect number of FAC cells can be if
		   the "iSymbolCounterAbs" was changed, e.g. by the synchronization
		   units */
		if (iFACCellCounter != NUM_FAC_CELLS)
			SetBufReset2(); /* FAC: buffer number 2 */

		/* Reset FAC cell counter */
		iFACCellCounter = 0;

		/* Frame ID of this FAC block stands for the "current" block. We need
		   the ID of the next block, therefore we have to add "1" */
		int iNewFrameID = Parameters.iFrameIDReceiv + 1;
		if (iNewFrameID == NUM_FRAMES_IN_SUPERFRAME)
			iNewFrameID = 0;

		/* Increment internal frame ID and take care of wrap around */
		iCurrentFrameID++;
		if (iCurrentFrameID == NUM_FRAMES_IN_SUPERFRAME)
			iCurrentFrameID = 0;

		/* Check if frame ID has changed, if yes, reset output buffers to avoid
		   buffer overflows */
		if (iCurrentFrameID != iNewFrameID)
		{
			iCurrentFrameID = iNewFrameID;

			/* Only SDC and MSC depend on frame ID */
			SetBufReset1(); /* MSC: buffer number 1 */
			SetBufReset3(); /* SDC: buffer number 3 */
			iMSCCellCounter = 0;
			iSDCCellCounter = 0;
		}

		if (iCurrentFrameID == 0)
		{
			/* Super-frame bound reached, test cell-counters (same as with the
			   FAC cells, see above) */
			if (iMSCCellCounter != iNumUsefMSCCellsPerFrame *
				NUM_FRAMES_IN_SUPERFRAME)
			{
				SetBufReset1(); /* MSC: buffer number 1 */
			}
			if (iSDCCellCounter != iNumSDCCellsPerSFrame)
				SetBufReset3(); /* SDC: buffer number 3 */

			/* Reset counters */
			iMSCCellCounter = 0;
			iSDCCellCounter = 0;
		}
	}
}

void COFDMCellDemapping::InitInternal(CParameter& Parameters)
{
	const CCellMappingTable& Param = Parameters.CellMappingTable;

	iNumSymPerFrame = Param.iNumSymPerFrame;
	iNumCarrier = Param.iNumCarrier;
	iNumUsefMSCCellsPerFrame = Param.iNumUsefMSCCellsPerFrame;
	iNumSDCCellsPerSFrame = Param.iNumSDCCellsPerSFrame;

	/* Init symbol-counter and internal frame counter */
	iSymbolCounter = 0;
	iCurrentFrameID = 0;

	/* Init cell-counter */
	iSDCCellCounter = 0;
	iMSCCellCounter = 0;
	iFACCellCounter = 0;

	/* Define block-sizes for input and output of the module ---------------- */
	/* Input */
	iInputBlockSize = iNumCarrier;

	/* Define space for output cyclic buffers. We must consider enough headroom
	   otherwise the buffers could overrun */
	/* FAC, one block is exactly finished when last symbol with FAC cells is
	   processed */
	iMaxOutputBlockSize2 = NUM_FAC_BITS_PER_BLOCK;
	/* SDC, one block is exactly finished when last symbol with SDC cells is
	   processed */
	iMaxOutputBlockSize3 = Param.iNumSDCCellsPerSFrame;
	/* MSC, since the MSC logical frames must not end at the end of one symbol
	   (could be somewhere in the middle of the symbol), the output buffer must
	   accept more cells than one logical MSC frame is long. The worst case is
	   that the block ends right at the beginning of one symbol; in this case we
	   have an overhang of approximately one symbol of MSC cells */
	iMaxOutputBlockSize = Param.iNumUsefMSCCellsPerFrame + Param.iMaxNumMSCSym;
}
