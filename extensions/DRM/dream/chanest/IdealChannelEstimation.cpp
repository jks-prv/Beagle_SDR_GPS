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

#include "IdealChannelEstimation.h"
#include "../matlib/MatlibSigProToolbox.h"

/* Implementation *************************************************************/
void CIdealChanEst::ProcessDataInternal(CParameter& Parameters)
{
	int i, j;

	/* Calculation of channel tranfer function ------------------------------ */
	/*	pvecInputData.cSig		equalized signal \hat{s}(t)
		pvecInputData2.tOut		received signal r(t)
		pvecInputData2.tIn		transmitted signal s(t)
		pvecInputData2.tRef		received signal without noise (channel
									reference signal) */
	for (i = 0; i < iNumCarrier; i++)
	{
		/* Extract channel estimate from equalized signal */
		veccEstChan[i] = (*pvecInputData2)[i].tOut / (*pvecInputData)[i].cSig;

		/* Calculate reference signal for channel from fading taps. We have
		   to do it this way to avoid errors from ICI
		   (inter-carrier-interfearence). Consider offset from guard-interval
		   removal (additional phase rotation) */
		veccRefChan[i] = (_REAL) 0.0;
		for (j = 0; j < Parameters.iNumTaps; j++)
			veccRefChan[i] += Rotate((*pvecInputData2)[0].veccTap[j], i,
				Parameters.iPathDelay[j] + Parameters.iOffUsfExtr);

		/* Normalize result */
		veccRefChan[i] *= Parameters.rGainCorr;
	}

	/* Debar DC carriers, set them to zero */
	for (i = 0; i < iNumDCCarriers; i++)
	{
		veccEstChan[i + iStartDCCar] = _COMPLEX((_REAL) 0.0, (_REAL) 0.0);
		veccRefChan[i + iStartDCCar] = _COMPLEX((_REAL) 0.0, (_REAL) 0.0);
	}

	/* Start evaluation results after exceeding the start count */
	if (iStartCnt > 0)
		iStartCnt--;
	else
	{
		/* MSE for all carriers */
		for (i = 0; i < iNumCarrier; i++)
			vecrMSEAverage[i] += SqMag(veccEstChan[i] - veccRefChan[i]);

		/* New values have been added, increase counter for final result
		   calculation */
		lAvCnt++;
	}


	/* Equalize the output vector ------------------------------------------- */
	/* Write to output vector. Also, ship the channel state at a certain cell */
	for (i = 0; i < iNumCarrier; i++)
	{
#ifdef USE_MAX_LOG_MAP
		/* In case of MAP we need the squared magnitude for the calculation of
		   the metric */
		(*pvecOutputData)[i].rChan = SqMag(veccRefChan[i]);
#else
		/* In case of hard-desicions, we need the magnitude of the channel for
		   the calculation of the metric */
		(*pvecOutputData)[i].rChan = abs(veccRefChan[i]);
#endif

		/* If we do not want to get the actual channel estimate but the
		   ideal equalized signal, we should do it this way to be ICI free */
		(*pvecOutputData)[i].cSig = (*pvecInputData2)[i].tOut *
			(*pvecInputData2)[i].tIn / (*pvecInputData2)[i].tRef;
	}

	/* Set symbol number for output vector */
	(*pvecOutputData).GetExData().iSymbolID = 
		(*pvecInputData).GetExData().iSymbolID;
}

void CIdealChanEst::InitInternal(CParameter& Parameters)
{
	/* Init base class for modifying the pilots (rotation) */
	CPilotModiClass::InitRot(Parameters);

	/* Get local parameters */
	iNumCarrier = Parameters.CellMappingTable.iNumCarrier;
	iNumSymPerFrame = Parameters.CellMappingTable.iNumSymPerFrame;
	iDFTSize = Parameters.CellMappingTable.iFFTSizeN;

	/* Parameters for debaring the DC carriers from evaluation. First check if
	   we have only useful part on the right side of the DC carrier */
	if (Parameters.CellMappingTable.iCarrierKmin > 0)
	{
		/* In this case, no DC carriers are in the useful spectrum */
		iNumDCCarriers = 0;
		iStartDCCar = 0;
	}
	else
	{
		if (Parameters.GetWaveMode() == RM_ROBUSTNESS_MODE_A)
		{
			iNumDCCarriers = 3;
			iStartDCCar = abs(Parameters.CellMappingTable.iCarrierKmin) - 1;
		}
		else
		{
			iNumDCCarriers = 1;
			iStartDCCar = abs(Parameters.CellMappingTable.iCarrierKmin);
		}
	}

	/* Init average counter */
	lAvCnt = 0;

	/* Init start count (debar initialization of channel estimation) */
	iStartCnt = 20;

	/* Additional delay from long interleaving has to be considered */
	if (Parameters.GetInterleaverDepth() == CParameter::SI_LONG)
		iStartCnt += Parameters.CellMappingTable.iNumSymPerFrame * D_LENGTH_LONG_INTERL;


	/* Allocate memory for intermedia results */
	veccEstChan.Init(iNumCarrier);
	veccRefChan.Init(iNumCarrier);
	vecrMSEAverage.Init(iNumCarrier, (_REAL) 0.0); /* Reset average with zeros */

	/* Define block-sizes for inputs and output */
	iInputBlockSize = iNumCarrier;
	iInputBlockSize2 = iNumCarrier;
	iOutputBlockSize = iNumCarrier;
}

void CIdealChanEst::GetResults(CVector<_REAL>& vecrResults)
{
	vecrResults.Init(iNumCarrier, (_REAL) 0.0);

	/* Copy data in return vector */
	for (int i = 0; i < iNumCarrier; i++)
		vecrResults[i] = vecrMSEAverage[i] / lAvCnt;
}
