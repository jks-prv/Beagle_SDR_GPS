/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2001
 *
 * Author(s):
 *	Volker Fischer
 *
 * Description:
 *	Linear channel estimation in time direction
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

#include "TimeLinear.h"


/* Implementation *************************************************************/
_REAL CTimeLinear::Estimate(CVectorEx<_COMPLEX>* pvecInputData, 
						    CComplexVector& veccOutputData, 
						    CVector<int>& veciMapTab, 
						    CVector<_COMPLEX>& veccPilotCells, _REAL rSNR)
{
	int			i, j;
	int			iPiHiIndex;
	int			iTimeDiffOld;
	int			iTimeDiffNew;
	_COMPLEX	cOldPilot;
	_COMPLEX	cNewPilot;
	_COMPLEX	cGrad;

	/* Channel estimation buffer -------------------------------------------- */
	/* Move data in channel estimation-buffer 
	   (from iLenHistBuff - 1 towards 0) */
	for (j = 0; j < iLenHistBuff - 1; j++)
	{
		for (i = 0; i < iNumIntpFreqPil; i++)
			matcChanEstHist[j][i] = matcChanEstHist[j + 1][i];
	}

	/* Clear current symbol for new channel estimates */
	for (i = 0; i < iNumIntpFreqPil; i++)
		matcChanEstHist[iLenHistBuff - 1][i] = 
			_COMPLEX((_REAL) 0.0, (_REAL) 0.0);


	/* Timing correction history -------------------------------------------- */
	/* Update timing correction history (shift register) */
	vecTiCorrHist.AddEnd(0);

	/* Add new one to all history values */
	for (i = 0; i < iLenTiCorrHist; i++)
		vecTiCorrHist[i] += (*pvecInputData).GetExData().iCurTimeCorr;


	/* Main loop ------------------------------------------------------------ */
	/* Identify and calculate transfer function at the pilot positions */
	for (i = 0; i < iNumCarrier; i++)
	{
		if (_IsScatPil(veciMapTab[i]))
		{
			/* Pilots are only every "iScatPilFreqInt"'th carrier */
			iPiHiIndex = i / iScatPilFreqInt;

			/* h = r / s, h: transfer function of channel, r: received signal, 
			   s: transmitted signal */
			matcChanEstHist[iLenHistBuff - 1][iPiHiIndex] = 
				(*pvecInputData)[i] / veccPilotCells[i];

			/* Linear interpolate in time direction from this current pilot to 
			   the corresponding pilot in the last symbol of the history */
			for (j = 1; j < iLenHistBuff - 1; j++)
			{
				/* We need to correct pilots due to timing corrections ------ */
				/* Calculate timing difference to old and new pilot */
				iTimeDiffOld = vecTiCorrHist[0]	- vecTiCorrHist[j];
				iTimeDiffNew = 0				- vecTiCorrHist[j];
				
				/* Correct pilot information for phase rotation */
				cOldPilot = Rotate(
					matcChanEstHist[0][iPiHiIndex], i, iTimeDiffOld);
				cNewPilot = Rotate(
					matcChanEstHist[iLenHistBuff - 1][iPiHiIndex], i,
					iTimeDiffNew);


				/* Linear interpolation ------------------------------------- */
				/* First, calculate gradient */
				cGrad = (cNewPilot - cOldPilot) / (_REAL) (iLenHistBuff - 1);

				/* Apply linear interpolation to cells in between */
				matcChanEstHist[j][iPiHiIndex] = cGrad * (_REAL) j + cOldPilot;
			}
		}
	}

	/* Copy channel estimation from current symbol in output buffer */
	for (i = 0; i < iNumIntpFreqPil; i++)
		veccOutputData[i] = matcChanEstHist[0][i];

	/* No SNR improvement by linear interpolation */
	return rSNR;
}

int CTimeLinear::Init(CParameter& Parameter)
{
	/* Init base class, must be at the beginning of this init! */
	CPilotModiClass::InitRot(Parameter);

	/* Get parameters from global struct */
	iNumCarrier = Parameter.CellMappingTable.iNumCarrier;
	iNumIntpFreqPil = Parameter.CellMappingTable.iNumIntpFreqPil;
	iScatPilFreqInt = Parameter.CellMappingTable.iScatPilFreqInt;

	/* Set length of history-buffer according to time-int-index */
	iLenHistBuff = Parameter.CellMappingTable.iScatPilTimeInt + 1;
	
	/* Init timing correction history with zeros */
	iLenTiCorrHist = iLenHistBuff - 1;
	vecTiCorrHist.Init(iLenTiCorrHist, 0);

	/* Allocate memory for channel estimation history and init with ones */
	matcChanEstHist.Init(iLenHistBuff, iNumIntpFreqPil, _COMPLEX(1.0, 0.0));

	/* Return delay of channel estimation in time direction */
	return iLenHistBuff;
}
