/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2001-2006
 *
 * Author(s):
 *	Volker Fischer
 *
 * Description:
 *	Time synchronization tracking using information of scattered pilots
 *
 *	Algorithm proposed by Baoguo Yang in "Timing Recovery for OFDM
 *	Transmission", IEEE November 2000
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

#include "TimeSyncTrack.h"
#include "../matlib/MatlibSigProToolbox.h"


/* Implementation *************************************************************/
void CTimeSyncTrack::Process(CParameter& Parameters,
		 CComplexVector& veccChanEst, int iNewTiCorr,
		 _REAL& rLenPDS, _REAL& rOffsPDS)
{
	int			i, j;
	int			iIntShiftVal;
	int			iFirstPathDelay = 0;
	CReal		rPeakBound;
	CReal		rCurEnergy;
	CReal		rWinEnergy;
	CReal		rMaxWinEnergy;
	bool	bDelayFound = false;
	bool	bPDSResultFound;

	/* Rotate the averaged PDP to follow the time shifts -------------------- */
	/* Update timing correction history (shift register) */
	vecTiCorrHist.AddEnd(iNewTiCorr);

	/* Calculate the actual shift of the timing correction. Since we do the
	   timing correction at the sound card sample rate (48 kHz) and the
	   estimated impulse response has a different sample rate (since the
	   spectrum is only one little part of the sound card frequency range)
	   we have to correct the timing correction by a certain bandwidth factor */
	const CReal rActShiftTiCor = rFracPartTiCor -
		(_REAL) vecTiCorrHist[0] * iNumCarrier / iDFTSize;

	/* Integer part of shift */
	const int iIntPartTiCorr = (int) Round(rActShiftTiCor);

	/* Extract the fractional part since we can only correct integer timing
	   shifts */
	rFracPartTiCor = rActShiftTiCor - iIntPartTiCorr;

	/* Shift the values in the vector storing the averaged impulse response. We
	   have to consider two cases for shifting (left and right shift) */
	if (rActShiftTiCor < 0)
		iIntShiftVal = iIntPartTiCorr + iNumIntpFreqPil;
	else
		iIntShiftVal = iIntPartTiCorr;

	/* OPH: copy latest impulse response into old impulse response store
	   (for rdop calculation) */
	veccOldImpulseResponse = veccPilots;

	/* If new correction is out of range, do not apply rotation */
	if ((iIntShiftVal > 0) && (iIntShiftVal < iNumIntpFreqPil))
	{
		/* Actual rotation of vector */
		vecrAvPoDeSp.Merge(vecrAvPoDeSp(iIntShiftVal + 1, iNumIntpFreqPil),
			vecrAvPoDeSp(1, iIntShiftVal));

		/* OPH: rotate old impulse response vector */
		veccOldImpulseResponse.Merge(veccOldImpulseResponse(iIntShiftVal + 1,
			iNumIntpFreqPil), veccOldImpulseResponse(1, iIntShiftVal));
	}


	/* New estimate for impulse response ------------------------------------ */
	/* Apply hamming window, Eq (15) */
	veccPilots = veccChanEst * vecrHammingWindow;

	/* Transform in time-domain to get an estimate for the delay power profile,
	   Eq (15) */
	veccPilots = Ifft(veccPilots, FftPlan);

	/* Average result, Eq (16) (Should be a moving average function, for
	   simplicity we have chosen an IIR filter here) */
	IIR1(vecrAvPoDeSp, SqMag(veccPilots), rLamAvPDS);

	/* Rotate the averaged result vector to put the earlier peaks
	   (which can also detected in a certain amount) at the beginning of
	   the vector */
	vecrAvPoDeSpRot.Merge(vecrAvPoDeSp(iStPoRot, iNumIntpFreqPil),
		vecrAvPoDeSp(1, iStPoRot - 1));


	/* Different timing algorithms ------------------------------------------ */
	switch (TypeTiSyncTrac)
	{
	case TSFIRSTPEAK:
		/* Detect first peak algorithm proposed by Baoguo Yang */
		/* Lower and higher bound */
		rBoundHigher = Max(vecrAvPoDeSpRot) * rConst1;
		rBoundLower = Min(vecrAvPoDeSpRot) * rConst2;

		/* Calculate the peak bound, Eq (19) */
		rPeakBound = Max(rBoundHigher, rBoundLower);

		/* Get final estimate, Eq (18) */
		for (i = 0; i < iNumIntpFreqPil - 1; i++)
		{
			/* We are only interested in the first peak */
			if (bDelayFound == false)
			{
				if ((vecrAvPoDeSpRot[i] > vecrAvPoDeSpRot[i + 1]) &&
					(vecrAvPoDeSpRot[i] > rPeakBound))
				{
					/* The first peak was found, store index */
					iFirstPathDelay = i;

					/* Set the flag */
					bDelayFound = true;
				}
			}
		}
		break;

	case TSENERGY:
		/* Determin position of window with maximum energy in guard-interval.
		   A window with the size of the guard-interval is moved over the entire
		   profile and the energy inside this window is calculated. The window
		   position which maximises this energy is taken as the new timing
		   position */
		rMaxWinEnergy = (CReal) 0.0;
		for (i = 0; i < iNumIntpFreqPil - 1 - rGuardSizeFFT; i++)
		{
			rWinEnergy = (CReal) 0.0;

			/* Energy IN the guard-interval */
			for (j = 0; j < rGuardSizeFFT; j++)
				rWinEnergy += vecrAvPoDeSpRot[i + j];

			/* Get maximum */
			if (rWinEnergy > rMaxWinEnergy)
			{
				rMaxWinEnergy = rWinEnergy;
				iFirstPathDelay = i;
			}
		}

		/* We always have a valid measurement, set flag */
		bDelayFound = true;
		break;
	}


	/* Only apply timing correction if search was successful and tracking is
	   activated */
	if ((bDelayFound) && (bTiSyncTracking))
	{
		CReal rPropGain = 0.0; /* quiet compiler */
		/* Consider the rotation introduced for earlier peaks in path delay.
		   Since the "iStPoRot" is the position of the beginning of the block
		   at the end for cutting out, "iNumIntpFreqPil" must be substracted.
		   (Actually, a part of the following line should be look like this:
		   "iStPoRot - 1 - iNumIntpFreqPil + 1" but the "- 1 + 1" compensate
		   each other) */
		iFirstPathDelay += iStPoRot - iNumIntpFreqPil - iTargetTimingPos - 1;


		/* Correct timing offset -------------------------------------------- */
		/* Final offset is target position in comparision to the estimated first
		   path delay. Since we have a delay from the channel estimation, the
		   previous correction is subtracted "- vecrNewMeasHist[0]". If the
		   "real" correction arrives after the delay, this correction is
		   compensated. The length of the history buffer (vecrNewMeasHist) must
		   be equal to the delay of the channel estimation.
		   The corrections must be quantized to the upsampled output sample
		   rate ("* iDFTSize / iNumCarrier") */
		const CReal rTiOffset = (CReal) -iFirstPathDelay *
			iDFTSize / iNumCarrier - veciNewMeasHist[0];

		/* Different controlling parameters for different types of tracking */
		switch (TypeTiSyncTrac)
		{
		case TSFIRSTPEAK:
			/* Adapt the linear control parameter to the region, where the peak
			   was found. The region left of the desired timing position is
			   critical, because we immediately get ISI if a peak appers here.
			   Therefore we apply fast correction here. At the other positions,
			   we smooth the controlling to improve the immunity against false
			   peaks */
			if (rTiOffset > 0)
				rPropGain = CONT_PROP_BEFORE_GUARD_INT;
			else
				rPropGain = CONT_PROP_IN_GUARD_INT;
			break;

		case TSENERGY:
			rPropGain = CONT_PROP_ENERGY_METHOD;
			break;
		}

		/* In case of sample rate offset acquisition phase, use faster timing
		   corrections */
		if (bSamRaOffsAcqu)
			rPropGain *= 2;

		/* Apply proportional control and fix result to sample grid */
		const CReal rCurCorrValue = rTiOffset * rPropGain + rFracPartContr;
		const int iContrTiOffs = (int) Fix(rCurCorrValue);

		/* Calculate new fractional part of controlling */
		rFracPartContr = rCurCorrValue - iContrTiOffs;

		/* Manage correction history */
		veciNewMeasHist.AddEnd(0);
		for (i = 0; i < iSymDelay - 1; i++)
			veciNewMeasHist[i] += iContrTiOffs;

		/* Apply correction */
		Parameters.iTimingOffsTrack = -iContrTiOffs;
	}


	/* Sample rate offset estimation ---------------------------------------- */
	/* This sample rate offset estimation is based on the movement of the
	   estimated PDS with time. The movement per symbol (or a number of symbols)
	   is proportional to the sample rate offset. It has to be considered the
	   PDS shiftings of the timing correction unit ("rActShiftTiCor" can be used
	   for that). The procedere is to detect the maximum peak in the PDS and use
	   this as a reference, assuming tha delay of this peak is not changing. The
	   problem is when another peak get higher than this due to fading. In this
	   case we must adjust the history to this new peak (the new reference) */
	int		iMaxInd;
	CReal	rMax;

	/* Find index of maximum peak in PDS estimation. This is our reference
	   for this estimation method */
	Max(rMax, iMaxInd, vecrAvPoDeSpRot);

	/* Integration of timing corrections
	   FIXME: Check for overrun of "iIntegTiCorrections" variable! */
	iIntegTiCorrections += (long int) iIntPartTiCorr;

	/* We need to consider the timing corrections done by the timing unit. What
	   we want to estimate is only the real movement of the detected maximum
	   peak */
	const int iCurRes = iIntegTiCorrections + iMaxInd;
	veciSRTiCorrHist.AddEnd(iCurRes);

	/* We assumed that the detected peak is always the same peak in the actual
	   PDS. But due to fading the maximum can change to a different peak. In
	   this case the estimation would be wrong. We try to detect the detection
	   of a different peak by defining a maximum sample rate change. The sample
	   rate offset is very likely to be very constant since usually crystal
	   oscialltors are used. Thus, if a larger change of sample rate offset
	   happens, we assume that the maximum peak has changed */
	const int iNewDiff = veciSRTiCorrHist[iLenCorrectionHist - 2] - iCurRes;

	/* If change is larger than 2, it is most likely that a new peak was chosen
	   by the maximum function. Also, if the sign has changed of the difference
	   (and it is not zero), we also say that a new peak was selected */
	if ((abs(iNewDiff) > 2) ||
		((Sign(iOldNonZeroDiff) != Sign(iNewDiff)) && (iNewDiff != 0)))
	{
		/* Correct the complete history to the new reference peak. Reference
		   peak was already added, therefore do not use last element */
		for (i = 0; i < iLenCorrectionHist - 1; i++)
			veciSRTiCorrHist[i] -= iNewDiff;
	}

	/* Store old difference if it is not zero */
	if (iNewDiff != 0)
		iOldNonZeroDiff = iNewDiff;


	/* Check, if we are in acquisition phase */
	if (iResOffsetAcquCnt > 0)
	{
		/* Acquisition phase */
		iResOffsetAcquCnt--;
	}
	else
	{
		/* Apply the result from acquisition only once */
		if (bSamRaOffsAcqu)
		{
			/* End of acquisition phase */
			bSamRaOffsAcqu = false;

			/* Set sample rate offset to initial estimate. We consider the
			   initialization phase of channel estimation by "iSymDelay" */
			CReal rInitSamOffset = GetSamOffHz(iCurRes - veciSRTiCorrHist[
				iLenCorrectionHist - (iResOffAcqCntMax - iSymDelay)],
				iResOffAcqCntMax - iSymDelay - 1, Parameters.GetSigSampleRate());

#ifndef USE_SAMOFFS_TRACK_FRE_PIL
			/* Apply initial sample rate offset estimation */
			Parameters.rResampleOffset -= rInitSamOffset;
#endif

			/* Reset estimation history (init with zeros) since the sample
			   rate offset was changed */
			veciSRTiCorrHist.Init(iLenCorrectionHist, 0);
			iIntegTiCorrections = 0;
		}
		else
		{
			/* Tracking phase */
			/* Get actual sample rate offset in Hertz */
			const CReal rSamOffset = GetSamOffHz(iCurRes - veciSRTiCorrHist[0],
				iLenCorrectionHist - 1, Parameters.GetSigSampleRate());

#ifndef USE_SAMOFFS_TRACK_FRE_PIL
			/* Apply result from sample rate offset estimation */
			Parameters.rResampleOffset -= CONTR_SAMP_OFF_INT_FTI * rSamOffset;
#endif
		}
	}


	/* Delay spread length estimation --------------------------------------- */
	/* Estimate the noise energy using the minimum statistic. We assume that
	   the noise variance is equal on all samples of the impulse response.
	   Therefore we subtract the variance on each sample. The total estimated
	   signal energy is the total energy minus the noise energy */
	/* Calculate total energy */
	const CReal rTotEgy = Sum(vecrAvPoDeSpRot);

	/* Sort the values of the PDS to get the smallest values */
	CRealVector rSortAvPoDeSpRot(Sort(vecrAvPoDeSpRot));

	/* Average the result of smallest values and overestimate result */
	const CReal rSigmaNoise =
		Sum(rSortAvPoDeSpRot(1, NUM_SAM_IR_FOR_MIN_STAT - 1)) /
		NUM_SAM_IR_FOR_MIN_STAT * OVER_EST_FACT_MIN_STAT;

	/* Calculate signal energy by subtracting the noise energy from total
	   energy (energy cannot by negative -> bound at zero) */
	const CReal rSigEnergyBound =
		Max(rTotEgy - rSigmaNoise * iNumIntpFreqPil, (CReal) 0.0);

	/* From left to the right -> search for end of PDS */
	rEstPDSEnd = (CReal) (iNumIntpFreqPil - 1);
	rCurEnergy = (CReal) 0.0;
	bPDSResultFound = false;
	for (i = 0; i < iNumIntpFreqPil; i++)
	{
		if (bPDSResultFound == false)
		{
			if (rCurEnergy > rSigEnergyBound)
			{
				/* Delay index */
				rEstPDSEnd = (CReal) i;

				bPDSResultFound = true;
			}

			/* Accumulate signal energy, subtract noise on each sample */
			rCurEnergy += vecrAvPoDeSpRot[i] - rSigmaNoise;
		}
	}

	/* From right to the left -> search for beginning of PDS */
	rEstPDSBegin = (CReal) 0.0;
	rCurEnergy = (CReal) 0.0;
	bPDSResultFound = false;
	for (i = iNumIntpFreqPil - 1; i >= 0; i--)
	{
		if (bPDSResultFound == false)
		{
			if (rCurEnergy > rSigEnergyBound)
			{
				/* Delay index */
				rEstPDSBegin = (CReal) i;

				bPDSResultFound = true;
			}

			/* Accumulate signal energy, subtract noise on each sample */
			rCurEnergy += vecrAvPoDeSpRot[i] - rSigmaNoise;
		}
	}

	/* If the signal energy is too low it can happen that the estimated
	   beginning of the impulse response is before the end -> correct */
	if (rEstPDSBegin > rEstPDSEnd)
	{
		/* Set beginning and end to their maximum (minimum) value */
		rEstPDSBegin = (CReal) 0.0;
		rEstPDSEnd = (CReal) (iNumIntpFreqPil - 1);
	}

	/* Correct estimates of begin and end of PDS by the rotation */
	const CReal rPDSLenCorrection = iNumIntpFreqPil - iStPoRot + 1;
	rEstPDSBegin -= rPDSLenCorrection;
	rEstPDSEnd -= rPDSLenCorrection;


	/* write PIR estimate into parameter struct for RSI output - use the function provided for plotting */
	CVector<_REAL> vecrScale;
	_REAL rLowerBound=0, rHigherBound=0, rStartGuard=0, rEndGuard=0, rPDSBegin=0, rPDSEnd=0;

	//Parameters.Lock();
	GetAvPoDeSp(Parameters.vecrPIR, vecrScale, rLowerBound, rHigherBound, rStartGuard, rEndGuard, rPDSBegin, rPDSEnd, Parameters.GetSigSampleRate());
	Parameters.rPIRStart = vecrScale[0];
	Parameters.rPIREnd = vecrScale[vecrScale.Size()-1];
	//Parameters.Unlock();


	/* Set return parameters */
	rLenPDS = rEstPDSEnd - rEstPDSBegin;
	rOffsPDS = rEstPDSBegin;
}

void CTimeSyncTrack::Init(CParameter& Parameters, int iNewSymbDelay)
{
	iNumCarrier = Parameters.CellMappingTable.iNumCarrier;
	iScatPilFreqInt = Parameters.CellMappingTable.iScatPilFreqInt;
	iNumIntpFreqPil = Parameters.CellMappingTable.iNumIntpFreqPil;
	iDFTSize = Parameters.CellMappingTable.iFFTSizeN;

	/* Timing correction history */
	iSymDelay = iNewSymbDelay;
	vecTiCorrHist.Init(iSymDelay, 0);

	/* History for new measurements (corrections) */
	veciNewMeasHist.Init(iSymDelay - 1, 0);

	/* Init vector for received data at pilot positions */
	veccPilots.Init(iNumIntpFreqPil);

	/* Vector for averaged power delay spread estimation */
	vecrAvPoDeSp.Init(iNumIntpFreqPil, (CReal) 0.0);

	/* Lambda for IIR filter for averaging the PDS */
	rLamAvPDS = IIR1Lam(TICONST_PDS_EST_TISYNC, (CReal) Parameters.GetSigSampleRate() /
		Parameters.CellMappingTable.iSymbolBlockSize);

	/* Vector for rotated result */
	vecrAvPoDeSpRot.Init(iNumIntpFreqPil);

	/* Length of guard-interval with respect to FFT-size! */
	rGuardSizeFFT = (CReal) iNumCarrier *
		Parameters.CellMappingTable.RatioTgTu.iEnum / Parameters.CellMappingTable.RatioTgTu.iDenom;

	/* Get the hamming window taps. The window is to reduce the leakage effect
	   of a DFT transformation */
	vecrHammingWindow.Init(iNumIntpFreqPil);
	vecrHammingWindow = Hamming(iNumIntpFreqPil);

	/* Weights for peak bound calculation, in Eq. (19), special values for
	   robustness mode D! */
	if (Parameters.GetWaveMode() == RM_ROBUSTNESS_MODE_D)
	{
		rConst1 = pow((_REAL) 10.0, (_REAL) -TETA1_DIST_FROM_MAX_DB_RMD / 10);
		rConst2 = pow((_REAL) 10.0, (_REAL) TETA2_DIST_FROM_MIN_DB_RMD / 10);
	}
	else
	{
		rConst1 = pow((_REAL) 10.0, (_REAL) -TETA1_DIST_FROM_MAX_DB / 10);
		rConst2 = pow((_REAL) 10.0, (_REAL) TETA2_DIST_FROM_MIN_DB / 10);
	}

	/* Define start point for rotation of detection vector for acausal taps.
	   Per definition is this point somewhere in the region after the
	   actual guard-interval window */
	if ((int) rGuardSizeFFT > iNumIntpFreqPil)
		iStPoRot = iNumIntpFreqPil;
	else
	{
		/* "+ 1" because of "Matlab indices" used in ".Merge()" function */
		iStPoRot = (int) (rGuardSizeFFT +
			Ceil((iNumIntpFreqPil - rGuardSizeFFT) / 2) + 1);
	}

	/* Init fractional part of timing correction to zero and fractional part
	   of controlling */
	rFracPartTiCor = (CReal) 0.0;
	rFracPartContr = (CReal) 0.0;

	/* Inits for the time synchronization tracking type */
	SetTiSyncTracType(TypeTiSyncTrac);

	/* Init begin and end of PDS estimation with zero and the length of guard-
	   interval respectively */
	rEstPDSBegin = (CReal) 0.0;
	rEstPDSEnd = rGuardSizeFFT;

	/* Init plans for FFT (faster processing of Fft and Ifft commands) */
	FftPlan.Init(iNumIntpFreqPil);


	/* Inits for sample rate offset estimation ------------------------------ */
	/* Calculate number of symbols for a given time span as defined for the
	   length of the sample rate offset estimation history size */
	iLenCorrectionHist = (int) ((_REAL) Parameters.GetSigSampleRate() *
		HIST_LEN_SAM_OFF_EST_TI_CORR / Parameters.CellMappingTable.iSymbolBlockSize);

	/* Init count for acquisition */
	iResOffAcqCntMax = (int) ((_REAL) Parameters.GetSigSampleRate() *
		SAM_OFF_EST_TI_CORR_ACQ_LEN / Parameters.CellMappingTable.iSymbolBlockSize);

	/* Init sample rate offset estimation acquisition count */
	iResOffsetAcquCnt = iResOffAcqCntMax;

	veciSRTiCorrHist.Init(iLenCorrectionHist, 0); /* Init with zeros */
	iIntegTiCorrections = 0;

	/* Symbol block size converted in domain of estimated PDS */
	rSymBloSiIRDomain =
		(CReal) Parameters.CellMappingTable.iSymbolBlockSize * iNumCarrier / iDFTSize;

	/* Init variable for storing the old difference of maximum position */
	iOldNonZeroDiff = 0;

	/* (O.Haffenden) Vector for power delay spread estimation for previous
	   symbol (used for RSCI rdop calculation) */
	veccOldImpulseResponse.Init(iNumIntpFreqPil, (CReal) 0.0);
	vecrRdelThresholds.Init(3);
	vecrRdelThresholds[0] = (CReal) 90.0;
	vecrRdelThresholds[1] = (CReal) 95.0;
	vecrRdelThresholds[2] = (CReal) 99.0;
	vecrRdelIntervals.Init(3);
}

CReal CTimeSyncTrack::GetSamOffHz(int iDiff, int iLen, int iSampleRate)
{
	/* Calculate actual sample rate offset in Hertz */
	const CReal rCurSampOffsNorm = (CReal) iDiff / iLen / rSymBloSiIRDomain;

	return (CReal) iSampleRate * ((CReal) 1.0 - (CReal) 1.0 / ((CReal) 1.0 + rCurSampOffsNorm));
}

void CTimeSyncTrack::SetTiSyncTracType(ETypeTiSyncTrac eNewTy)
{
	TypeTiSyncTrac = eNewTy;

	switch (TypeTiSyncTrac)
	{
	case TSFIRSTPEAK:
		/* Define target position for first path. Should be close to zero but
		   not exactely zero because even small estimation errors would lead to
		   ISI. The target timing position must be at least 2 samples away from
		   the guard-interval border */
		iTargetTimingPos = (int) (rGuardSizeFFT / TARGET_TI_POS_FRAC_GUARD_INT);
		if (iTargetTimingPos < 2)
			iTargetTimingPos = 2;
		break;

	case TSENERGY:
		/* No target timing position needed */
		iTargetTimingPos = 0;
		break;
	}
}

void CTimeSyncTrack::GetAvPoDeSp(CVector<_REAL>& vecrData,
								 CVector<_REAL>& vecrScale,
								_REAL& rLowerBound, _REAL& rHigherBound,
								_REAL& rStartGuard, _REAL& rEndGuard,
								_REAL& rPDSBegin, _REAL& rPDSEnd, int iSampleRate)
{
	int		i;
	int		iHalfSpec;
	_REAL	rScaleIncr;
	_REAL	rScaleAbs;

	/* Init output vectors */
	vecrData.Init(iNumIntpFreqPil, (_REAL) 0.0);
	vecrScale.Init(iNumIntpFreqPil, (_REAL) 0.0);
	rHigherBound = (_REAL) 0.0;
	rLowerBound = (_REAL) 0.0;
	rStartGuard = (_REAL) 0.0;
	rEndGuard = (_REAL) 0.0;
	rPDSBegin = (_REAL) 0.0;
	rPDSEnd = (_REAL) 0.0;

	/* Do copying of data only if vector is of non-zero length which means that
	   the module was already initialized */
	if (iNumIntpFreqPil != 0)
	{
		/* With this setting we only define the position of the guard-interval
		   in the plot. With this setting we position it centered */
		iHalfSpec = (int) ((iNumIntpFreqPil - rGuardSizeFFT) / 2);

		/* Init scale (in "ms") */
		rScaleIncr = (_REAL) iDFTSize /
			(iSampleRate * iNumIntpFreqPil) * 1000 / iScatPilFreqInt;

		/* Let the target timing position be the "0" time */
		rScaleAbs = -(iHalfSpec + iTargetTimingPos) * rScaleIncr;

		/* Copy first part of data in output vector */
		for (i = 0; i < iHalfSpec; i++)
		{
			const _REAL rCurPDSVal =
				vecrAvPoDeSp[iNumIntpFreqPil - iHalfSpec + i];

			if (rCurPDSVal > 0)
				vecrData[i] = (_REAL) 10.0 * log10(rCurPDSVal);
			else
				vecrData[i] = RET_VAL_LOG_0;

			/* Scale */
			vecrScale[i] = rScaleAbs;
			rScaleAbs += rScaleIncr;
		}

		/* Save scale point because this is the start point of guard-interval */
		rStartGuard = rScaleAbs;

		/* Copy second part of data in output vector */
		for (i = iHalfSpec; i < iNumIntpFreqPil; i++)
		{
			const _REAL rCurPDSVal = vecrAvPoDeSp[i - iHalfSpec];

			if (rCurPDSVal > 0)
				vecrData[i] = (_REAL) 10.0 * log10(rCurPDSVal);
			else
				vecrData[i] = RET_VAL_LOG_0;

			/* Scale */
			vecrScale[i] = rScaleAbs;
			rScaleAbs += rScaleIncr;
		}

		/* Return bounds */
		switch (TypeTiSyncTrac)
		{
		case TSFIRSTPEAK:
			if (rBoundHigher > 0)
				rHigherBound = (_REAL) 10.0 * log10(rBoundHigher);
			else
				rHigherBound = RET_VAL_LOG_0;

			if (rBoundLower > 0)
				rLowerBound = (_REAL) 10.0 * log10(rBoundLower);
			else
				rLowerBound = RET_VAL_LOG_0;
			break;

		case TSENERGY:
			/* No bounds needed for energy type, set both values to "defined
			   infinity value", so it does not show up in the plot */
			rHigherBound = RET_VAL_LOG_0;
			rLowerBound = RET_VAL_LOG_0;
			break;
		}

		/* End point of guard interval */
		rEndGuard = rScaleIncr * (rGuardSizeFFT - iTargetTimingPos);

		/* Estmiated begin and end of estimated PDS */
		rPDSBegin = rScaleIncr * (rEstPDSBegin - iTargetTimingPos);
		rPDSEnd = rScaleIncr * (rEstPDSEnd - iTargetTimingPos);
	}
}

/* OPH: Calculate the delay according to the rdel tag of RSCI */
void CTimeSyncTrack::CalculateRdel(CParameter& Parameters)
{
	/* Define the intervals in ascending order of threshold percentage */
	CReal rTotEgy = Sum(vecrAvPoDeSpRot);

	CReal rIntervalAccum= (CReal) 0.0; 
	const int ciNumDelayIntervals = vecrRdelThresholds.GetSize();
	CRealVector vecrIntervalStart, vecrIntervalEnd;
	vecrRdelIntervals.Init(ciNumDelayIntervals);
	vecrIntervalStart.Init(ciNumDelayIntervals);
	vecrIntervalEnd.Init(ciNumDelayIntervals);

	int i = 0;
	int j;

	for (j = ciNumDelayIntervals - 1; j >= 0; j--)
	{
		CReal rIntervalThresh = rTotEgy *
			((CReal) 1.0 - (vecrRdelThresholds[j] / (CReal) 100.0)) *
			(CReal) 0.5;

		for (; rIntervalAccum < rIntervalThresh && i < iNumIntpFreqPil; i++)
		{
			rIntervalAccum += vecrAvPoDeSpRot[i];
		}
		vecrIntervalStart[j] = i;
	}

	i = iNumIntpFreqPil - 1;
	rIntervalAccum = (CReal) 0.0;

	for (j = ciNumDelayIntervals - 1; j >= 0; j--)
	{
		_REAL rIntervalThresh = rTotEgy *
			((CReal) 1.0 - (vecrRdelThresholds[j] / (CReal) 100.0)) *
			(CReal) 0.5;

		for (; rIntervalAccum < rIntervalThresh && i >= 0; i--)
		{
			rIntervalAccum += vecrAvPoDeSpRot[i];
		}
		vecrIntervalEnd[j] = i;
	}

	for (j = 0; j < ciNumDelayIntervals; j++)
	{
		CReal rInterval =
			((_REAL) (vecrIntervalEnd[j] - vecrIntervalStart[j])) *
			Parameters.CellMappingTable.iFFTSizeN / (Parameters.GetSigSampleRate() *
			Parameters.CellMappingTable.iNumIntpFreqPil * Parameters.CellMappingTable.iScatPilFreqInt) * 1000;

		/* Clip the delay interval values for display purposes */
		if (rInterval < (CReal) -9.9)
			rInterval = (CReal) -9.9;

		if (rInterval > (CReal) 9.9)
			rInterval = (CReal) 9.9;

		vecrRdelIntervals[j] = rInterval;
	}

	Parameters.vecrRdelThresholds = vecrRdelThresholds;
	Parameters.vecrRdelIntervals = vecrRdelIntervals;
}

void CTimeSyncTrack::CalculateRdop(CParameter& Parameters)
{
	/* Initialise accumulators for sum of squares and sum of squared
	   differences */
	CReal rSumSqDiff = (CReal) 0.0;
	CReal rSumSqChan = (CReal) 0.0;

	/* Now do the calculation */
	for (int i = 0; i < veccPilots.GetSize(); i++)
	{
		rSumSqDiff += SqMag(veccPilots[i] - veccOldImpulseResponse[i]);
		rSumSqChan += SqMag(veccPilots[i]);
	}

	CReal rTs = (_REAL) Parameters.CellMappingTable.iSymbolBlockSize / Parameters.GetSigSampleRate();

	Parameters.rRdop = Sqrt(rSumSqDiff / rSumSqChan) / (crPi * rTs);
}
