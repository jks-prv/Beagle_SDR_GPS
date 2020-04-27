/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2001
 *
 * Author(s):
 *	Volker Fischer
 *
 * Description:
 *	Frame synchronization (Using time-pilots), frequency sync tracking
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

#include "SyncUsingPil.h"
#include "../matlib/MatlibSigProToolbox.h"

/* Implementation *************************************************************/
void CSyncUsingPil::ProcessDataInternal(CParameter& Parameters)
{
	int i;

	Parameters.Lock(); 

	/**************************************************************************\
	* Frame synchronization detection										   *
	\**************************************************************************/
	bool bSymbolIDHasChanged = false;

	if ((bSyncInput == false) && (bAquisition))
	{
		/* DRM frame synchronization based on time pilots ------------------- */
		/* Calculate correlation of received cells with pilot pairs */
		CReal rResultPilPairCorr = (CReal) 0.0;
		for (i = 0; i < iNumPilPairs; i++)
		{
			/* Actual correlation */
			const CComplex cCorrRes = (*pvecInputData)[vecPilCorr[i].iIdx1] *
				Conj(vecPilCorr[i].cPil1) *
				Conj((*pvecInputData)[vecPilCorr[i].iIdx2]) *
				vecPilCorr[i].cPil2 * cR_HH;

			rResultPilPairCorr += Real(cCorrRes);
		}

		/* Store correlation results in a shift register for finding the peak */
		vecrCorrHistory.AddEnd(rResultPilPairCorr);


		/* Finding beginning of DRM frame in results ------------------------ */
		/* Wait until history is filled completly */
		if (iInitCntFraSy > 0)
			iInitCntFraSy--;
		else
		{
			/* Search for maximum */
			int iMaxIndex = 0;
			CReal rMaxValue = -_MAXREAL;
			for (i = 0; i < iNumSymPerFrame; i++)
			{
				if (vecrCorrHistory[i] > rMaxValue)
				{
					rMaxValue = vecrCorrHistory[i];
					iMaxIndex = i;
				}
			}

			/* For initial frame synchronization, use maximum directly */
			if (bInitFrameSync)
			{
				/* Reset init flag */
				bInitFrameSync = false;

				/* Set symbol ID index according to received data */
				iSymbCntFraSy = iNumSymPerFrame - iMaxIndex - 1;
			}
			else
			{
				/* If maximum is in the middle of the interval
				   (check frame sync) */
				if (iMaxIndex == iMiddleOfInterval)
				{
					if (iSymbCntFraSy == iNumSymPerFrame - iMiddleOfInterval - 1)
					{
						/* Reset flags */
						bBadFrameSync = false;
						bFrameSyncWasOK = true;

						/* Post Message for GUI (Good frame sync) */
						Parameters.ReceiveStatus.FSync.SetStatus(RX_OK);
					}
					else
					{
						if (bBadFrameSync)
						{
							/* Reset symbol ID index according to received
							   data */
							iSymbCntFraSy =
								iNumSymPerFrame - iMiddleOfInterval - 1;

							/* Inform that symbol ID has changed */
							bSymbolIDHasChanged = true;

							/* Reset flag */
							bBadFrameSync = false;

							Parameters.ReceiveStatus.FSync.SetStatus(CRC_ERROR);
						}
						else
						{
							/* One false detected frame sync should not reset
							   the actual frame sync because the measurement
							   could be wrong. Sometimes the frame sync
							   detection gets false results. If the next time
							   the frame sync is still unequal to the
							   measurement, then correct it */
							bBadFrameSync = true;

							if (bFrameSyncWasOK)
							{
								/* Post Message that frame sync was wrong but
								   was not yet corrected (yellow light) */
								Parameters.ReceiveStatus.FSync.SetStatus(DATA_ERROR);
							}
							else
								Parameters.ReceiveStatus.FSync.SetStatus(CRC_ERROR);
						}

						/* Set flag for bad sync */
						bFrameSyncWasOK = false;
					}
				}
			}
		}
	}
	else
	{
		/* Frame synchronization has successfully finished, show always green
		   light */
		Parameters.ReceiveStatus.FSync.SetStatus(RX_OK);
	}

	/* Set current symbol ID and flag in extended data of output vector */
	(*pvecOutputData).GetExData().iSymbolID = iSymbCntFraSy;
	(*pvecOutputData).GetExData().bSymbolIDHasChanged = bSymbolIDHasChanged;

	/* Increase symbol counter and take care of wrap around */
	iSymbCntFraSy++;
	if (iSymbCntFraSy >= iNumSymPerFrame)
		iSymbCntFraSy = 0;


	/**************************************************************************\
	* Using Frequency pilot information										   *
	\**************************************************************************/
	if ((bSyncInput == false) && (bTrackPil))
	{
		CComplex cFreqOffEstVecSym = CComplex((CReal) 0.0, (CReal) 0.0);

		for (i = 0; i < NUM_FREQ_PILOTS; i++)
		{
			/* The old pilots must be rotated due to timing corrections */
			const CComplex cOldFreqPilCorr =
				Rotate(cOldFreqPil[i], iPosFreqPil[i],
				(*pvecInputData).GetExData().iCurTimeCorr);

			/* Calculate the inner product of the sum */
			const CComplex cCurPilMult =
				(*pvecInputData)[iPosFreqPil[i]] * Conj(cOldFreqPilCorr);

			/* Save "old" frequency pilots for next symbol. Special treatment
			   for robustness mode D (carriers 7 and 21) necessary 
			   (See 8.4.2.2) */

// TODO: take care of initialization phase: do not start using estimates until
// the first "old pilots" were stored. Also, an "initial frequecy offset
// estimate" should be made and rFreqOffsetTrack should be set to this value!

			if ((Parameters.GetWaveMode() == RM_ROBUSTNESS_MODE_D) &&
				(i < 2))
			{
				cOldFreqPil[i] = -(*pvecInputData)[iPosFreqPil[i]];
			}
			else
				cOldFreqPil[i] = (*pvecInputData)[iPosFreqPil[i]];

#ifdef USE_SAMOFFS_TRACK_FRE_PIL
			/* Get phase difference for sample rate offset estimation. Average
			   the vector, real and imaginary part separately */
			IIR1(cFreqPilotPhDiff[i], cCurPilMult, rLamSamRaOff);
#endif

			/* Calculate estimation of frequency offset */
			cFreqOffEstVecSym += cCurPilMult;
		}


		/* Frequency offset ------------------------------------------------- */
		/* Correct frequency offset estimation for resample offset corrections.
		   When a sample rate offset correction was applied, the frequency
		   offset is shifted proportional to this correction. The correction
		   is mandatory if large sample rate offsets occur */

		/* Get sample rate offset change */
		const CReal rDiffSamOffset =
			rPrevSamRateOffset - Parameters.rResampleOffset;

		/* Save current resample offset for next symbol */
		rPrevSamRateOffset = Parameters.rResampleOffset;

		/* Correct sample-rate offset correction according to the proportional
		   rule. Use relative DC frequency offset plus relative average offset
		   of frequency pilots to the DC frequency. Normalize this offset so
		   that it can be used as a phase correction for frequency offset
		   estimation  */
		CReal rPhaseCorr = (Parameters.rFreqOffsetAcqui +
			Parameters.rFreqOffsetTrack + rAvFreqPilDistToDC) *
			rDiffSamOffset / Parameters.GetSigSampleRate() / rNormConstFOE;

		/* Actual correction (rotate vector) */
		cFreqOffVec *= CComplex(Cos(rPhaseCorr), Sin(rPhaseCorr));


		/* Average vector, real and imaginary part separately */
		IIR1(cFreqOffVec, cFreqOffEstVecSym, rLamFreqOff);

		/* Calculate argument */
		const CReal rFreqOffsetEst = Angle(cFreqOffVec);

		/* Correct measurement average for actually applied frequency
		   correction */
		cFreqOffVec *= CComplex(Cos(-rFreqOffsetEst), Sin(-rFreqOffsetEst));

#ifndef USE_FRQOFFS_TRACK_GUARDCORR
		/* Integrate the result for controling the frequency offset, normalize
		   estimate */
		Parameters.rFreqOffsetTrack += rFreqOffsetEst * rNormConstFOE;
#endif


#ifdef USE_SAMOFFS_TRACK_FRE_PIL
		/* Sample rate offset ----------------------------------------------- */
		/* Calculate estimation of sample frequency offset. We use the different
		   frequency offset estimations of the frequency pilots. We normalize
		   them with the distance between them and average the result (/ 2.0) */
		CReal rSampFreqOffsetEst =
			((Angle(cFreqPilotPhDiff[1]) - Angle(cFreqPilotPhDiff[0])) /
			(iPosFreqPil[1] - iPosFreqPil[0]) +
			(Angle(cFreqPilotPhDiff[2]) - Angle(cFreqPilotPhDiff[0])) /
			(iPosFreqPil[2] - iPosFreqPil[0])) / (CReal) 2.0;

		/* Integrate the result for controling the resampling */
		Parameters.rResampleOffset +=
			CONTR_SAMP_OFF_INTEGRATION * rSampFreqOffsetEst;
#endif

#ifdef _DEBUG_
/* Save frequency and sample rate tracking */
static FILE* pFile = fopen("test/freqtrack.dat", "w");
fprintf(pFile, "%e %e\n", Parameters.GetSigSampleRate() * Parameters.rFreqOffsetTrack,
	Parameters.rResampleOffset);
fflush(pFile);
#endif
	}


	/* If synchronized DRM input stream is used, overwrite the detected
	   frequency offest estimate by "0", because we know this value */
	if (bSyncInput)
		Parameters.rFreqOffsetTrack = (CReal) 0.0;

	/* Do not ship data before first frame synchronization was done. The flag
	   "bAquisition" must not be set to false since in that case we would run
	   into an infinite loop since we would not ever ship any data. But since
	   the flag is set after this module, we should be fine with that. */
	if ((bInitFrameSync) && (bSyncInput == false))
		iOutputBlockSize = 0;
	else
	{
		iOutputBlockSize = iNumCarrier;

		/* Copy data from input to the output. Data is not modified in this
		   module */
		for (i = 0; i < iOutputBlockSize; i++)
			(*pvecOutputData)[i] = (*pvecInputData)[i];
	}

	Parameters.Unlock(); 
}

void CSyncUsingPil::InitInternal(CParameter& Parameters)
{
	int			i;
	_COMPLEX	cPhaseCorTermDivi;

	Parameters.Lock(); 

	/* Init base class for modifying the pilots (rotation) */
	CPilotModiClass::InitRot(Parameters);

	/* Init internal parameters from global struct */
	iNumCarrier = Parameters.CellMappingTable.iNumCarrier;
	eCurRobMode = Parameters.GetWaveMode();

	/* Check if symbol number per frame has changed. If yes, reset the
	   symbol counter */
	if (iNumSymPerFrame != Parameters.CellMappingTable.iNumSymPerFrame)
	{
		/* Init internal counter for symbol number */
		iSymbCntFraSy = 0;

		/* Refresh parameter */
		iNumSymPerFrame = Parameters.CellMappingTable.iNumSymPerFrame;
	}

	/* Allocate memory for histories. Init history with small values, because
	   we search for maximum! */
	vecrCorrHistory.Init(iNumSymPerFrame, -_MAXREAL);

	/* Set middle of observation interval */
	iMiddleOfInterval = iNumSymPerFrame / 2;


	/* DRM frame synchronization based on time pilots, inits ---------------- */
	/* Allocate memory for storing pilots and indices. Since we do
	   not know the resulting "iNumPilPairs" we allocate memory for the
	   worst case, i.e. "iNumCarrier" */
	vecPilCorr.Init(iNumCarrier);

	/* Store pilots and indices for calculating the correlation. Use only first
	   symbol of "matcPilotCells", because there are the pilots for
	   Frame-synchronization */
	iNumPilPairs = 0;

	for (i = 0; i < iNumCarrier - 1; i++)
	{
		/* Only successive pilots (in frequency direction) are used */
		if (_IsPilot(Parameters.CellMappingTable.matiMapTab[0][i]) &&
			_IsPilot(Parameters.CellMappingTable.matiMapTab[0][i + 1]))
		{
			/* Store indices and complex numbers */
			vecPilCorr[iNumPilPairs].iIdx1 = i;
			vecPilCorr[iNumPilPairs].iIdx2 = i + 1;
			vecPilCorr[iNumPilPairs].cPil1 = Parameters.CellMappingTable.matcPilotCells[0][i];
			vecPilCorr[iNumPilPairs].cPil2 = Parameters.CellMappingTable.matcPilotCells[0][i + 1];

			iNumPilPairs++;
		}
	}

	/* Calculate channel correlation in frequency direction. Use rectangular
	   shaped PDS with the length of the guard-interval */
	const CReal rArgSinc =
		(CReal) Parameters.CellMappingTable.iGuardSize / Parameters.CellMappingTable.iFFTSizeN;
	const CReal rArgExp = crPi * rArgSinc;

	cR_HH = Sinc(rArgSinc) * CComplex(Cos(rArgExp), -Sin(rArgExp));


	/* Frequency offset estimation ------------------------------------------ */
	/* Get position of frequency pilots */
	int iFreqPilCount = 0;
	int iAvPilPos = 0;
	for (i = 0; i < iNumCarrier - 1; i++)
	{
		if (_IsFreqPil(Parameters.CellMappingTable.matiMapTab[0][i]))
		{
			/* For average frequency pilot position to DC carrier */
			iAvPilPos += i + Parameters.CellMappingTable.iCarrierKmin;
			
			iPosFreqPil[iFreqPilCount] = i;
			iFreqPilCount++;
		}
	}

	/* Average distance of the frequency pilots from the DC carrier. Needed for
	   corrections for sample rate offset changes. Normalized to sample rate! */
	rAvFreqPilDistToDC =
		(CReal) iAvPilPos / NUM_FREQ_PILOTS / Parameters.CellMappingTable.iFFTSizeN;

	/* Init memory for "old" frequency pilots */
	for (i = 0; i < NUM_FREQ_PILOTS; i++)
		cOldFreqPil[i] = CComplex((CReal) 0.0, (CReal) 0.0);
	
	/* Nomalization constant for frequency offset estimation */
	rNormConstFOE =
		(CReal) 1.0 / ((CReal) 2.0 * crPi * Parameters.CellMappingTable.iSymbolBlockSize);

	/* Init time constant for IIR filter for frequency offset estimation */
	rLamFreqOff = IIR1Lam(TICONST_FREQ_OFF_EST, (CReal) Parameters.GetSigSampleRate() /
		Parameters.CellMappingTable.iSymbolBlockSize);

	/* Init vector for averaging the frequency offset estimation */
	cFreqOffVec = CComplex((CReal) 0.0, (CReal) 0.0);

	/* Init value for previous estimated sample rate offset with the current
	   setting. This can be non-zero if, e.g., an initial sample rate offset
	   was set by command line arguments */
	rPrevSamRateOffset = Parameters.rResampleOffset;


#ifdef USE_SAMOFFS_TRACK_FRE_PIL
	/* Inits for sample rate offset estimation algorithm -------------------- */
	/* Init memory for actual phase differences */
	for (i = 0; i < NUM_FREQ_PILOTS; i++)
		cFreqPilotPhDiff[i] = CComplex((CReal) 0.0, (CReal) 0.0);

	/* Init time constant for IIR filter for sample rate offset estimation */
	rLamSamRaOff = IIR1Lam(TICONST_SAMRATE_OFF_EST,
		(CReal) Parameters.GetSigSampleRate() / Parameters.CellMappingTable.iSymbolBlockSize);
#endif


	/* Define block-sizes for input and output */
	iInputBlockSize = iNumCarrier;
	iMaxOutputBlockSize = iNumCarrier;

	Parameters.Unlock(); 
}

void CSyncUsingPil::StartAcquisition()
{
	/* Init internal counter for symbol number */
	iSymbCntFraSy = 0;

	/* Reset correlation history */
	vecrCorrHistory.Reset(-_MAXREAL);

	bAquisition = true;

	/* After an initialization the frame sync must be adjusted */
	bBadFrameSync = true;
	bInitFrameSync = true; /* Set flag to show that (re)-init was done */
	bFrameSyncWasOK = false;

	/* Initialize count for filling the history buffer */
	iInitCntFraSy = iNumSymPerFrame;
}
