/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2001
 *
 * Author(s):
 *	Volker Fischer
 *
 * Description:
 *	Frequency synchronization acquisition (FFT-based)
 *
 * The input data is not modified by this module, it is just a measurement
 * of the frequency offset. The data is fed through this module.
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

#include "FreqSyncAcq.h"
#include "../matlib/MatlibSigProToolbox.h"

/* Implementation *************************************************************/
void CFreqSyncAcq::ProcessDataInternal(CParameter& Parameters)
{
	int			i;
	int			iMaxIndex=0;
	CReal		rMaxValue = 0.0;
	int			iNumDetPeaks;
	//int			iDiffTemp;
	//CReal		rLevDiff;
	bool	bNoPeaksLeft;
	CRealVector	vecrPSDPilPoin(3);

	Parameters.Lock(); 

	/* OPH: update free-running symbol counter */
	iFreeSymbolCounter++;
	if (iFreeSymbolCounter >= Parameters.CellMappingTable.iNumSymPerFrame)
	{
		iFreeSymbolCounter = 0;
	}

	if (bAquisition)
	{

		/* Do not transfer any data to the next block if no frequency
		   acquisition was successfully done */
		iOutputBlockSize = 0;

		/* Add new symbol in history (shift register) */
		vecrFFTHistory.AddEnd((*pvecInputData), iInputBlockSize);


		/* Start algorithm when history memory is filled -------------------- */
		/* Wait until history memory is filled for the first FFT operation.
		   ("> 1" since, e.g., if we would have only one buffer, we can start
		   immediately) */
		if (iAquisitionCounter > 1)
		{
			/* Decrease counter */
			iAquisitionCounter--;
		}
		else
		{
			/* Copy vector to matlib vector and calculate real-valued FFT */
			const int iStartIdx = iHistBufSize - iFrAcFFTSize;
			for (i = 0; i < iFrAcFFTSize; i++)
				vecrFFTInput[i] = vecrFFTHistory[i + iStartIdx];

			static CMatlibVector<CReal> vecRet;
			vecRet.Init(vecrFFTInput.GetSize(), VTY_TEMP);

			for (int k = 0; k < vecrFFTInput.GetSize(); k++)
				vecRet[k] = vecrFFTInput[k] * vecrHammingWin[k];

			/* Calculate power spectrum (X = real(F)^2 + imag(F)^2) */
			vecrSqMagFFTOut =
				SqMag(rfft(vecRet, FftPlan));

			/* Calculate moving average for better estimate of PSD */
			vvrPSDMovAv.Add(vecrSqMagFFTOut);

			/* Wait until we have sufficient data for averaging */
			if (iAverageCounter > 1)
			{
				/* Decrease counter */
				iAverageCounter--;
			}
			else
			{
				/* Get PSD estimate */
				const CRealVector vecrPSD(vvrPSDMovAv.GetAverage());


				/* -------------------------------------------------------------
				   Low pass filtering over frequency axis. We do the filtering
				   from both sides, once from right to left and then from left
				   to the right side. Afterwards, these results are averaged
				   This way, the noise floor is estimated */

// TODO: Introduce offset to debar peak at DC frequency (cause by DC offset of
// sound cards). Set "iStartFilt" in Init() routine!
const int iStartFilt = 0; // <- no offset right now

				/* Reset vectors for intermediate filtered result */
				vecrFiltResLR.Reset((CReal) 0.0);
				vecrFiltResRL.Reset((CReal) 0.0);

				/* From the left edge to the right edge */
				vecrFiltResLR[iStartFilt] = vecrPSD[iStartFilt];
				for (i = iStartFilt + 1; i < iHalfBuffer; i++)
				{
					vecrFiltResLR[i] = (vecrFiltResLR[i - 1] - vecrPSD[i]) *
						LAMBDA_FREQ_IIR_FILT + vecrPSD[i];
				}

				/* From the right edge to the left edge */
				vecrFiltResRL[iHalfBuffer - 1] =
					vecrPSD[iHalfBuffer - 1];
				for (i = iHalfBuffer - 2; i >= iStartFilt; i--)
				{
					vecrFiltResRL[i] = (vecrFiltResRL[i + 1] - vecrPSD[i]) *
						LAMBDA_FREQ_IIR_FILT + vecrPSD[i];
				}

				/* Average RL and LR filter outputs */
				vecrFiltRes = (vecrFiltResLR + vecrFiltResRL) / 2;

#ifdef _DEBUG_
#if 0
/* Stores curves for PSD estimation and filtering */
FILE* pFile2 = fopen("test/freqacqFilt.dat", "w");
for (i = 0; i < iHalfBuffer; i++)
	fprintf(pFile2, "%e %e\n", vecrPSD[i], vecrFiltRes[i]);
fclose(pFile2);
#endif
#endif

				/* Equalize PSD by "noise floor estimate" */
				for (i = 0; i < iHalfBuffer; i++)
				{
					/* Make sure we do not devide by zero */
					if (vecrFiltRes[i] != 0.0)
						vecrPSD[i] /= vecrFiltRes[i];
					else
						vecrPSD[i] = 0.0;
				}

				/* Correlate known frequency-pilot structure with equalized
				   power spectrum */
				for (i = 0; i < iSearchWinSize; i++)
				{
					vecrPSDPilCor[i] =
						vecrPSD[i + veciTableFreqPilots[0]] +
						vecrPSD[i + veciTableFreqPilots[1]] +
						vecrPSD[i + veciTableFreqPilots[2]];
				}


				/* Detect peaks --------------------------------------------- */
				/* Get peak indices of detected peaks */
				iNumDetPeaks = 0;
				for (i = iStartDCSearch; i < iEndDCSearch; i++)
				{
					/* Test peaks against a bound */
					if (vecrPSDPilCor[i] > rPeakBoundFiltToSig)
					{
						veciPeakIndex[iNumDetPeaks] = i;
						iNumDetPeaks++;
					}
				}

				/* Check, if at least one peak was detected */
				if (iNumDetPeaks > 0)
				{
					/* ---------------------------------------------------------
					   The following test shall exclude sinusoid interferers in
					   the received spectrum */
					CVector<int> vecbFlagVec(iNumDetPeaks, 1);

					/* Check all detected peaks in the "PSD-domain" if there are
					   at least two peaks with approx the same power at the
					   correct places (positions of the desired pilots) */
					for (i = 0; i < iNumDetPeaks; i++)
					{
						/* Fill the vector with the values at the desired
						   pilot positions */
						vecrPSDPilPoin[0] =
							vecrPSD[veciPeakIndex[i] + veciTableFreqPilots[0]];
						vecrPSDPilPoin[1] =
							vecrPSD[veciPeakIndex[i] + veciTableFreqPilots[1]];
						vecrPSDPilPoin[2] =
							vecrPSD[veciPeakIndex[i] + veciTableFreqPilots[2]];

						/* Sort, to extract the highest and second highest
						   peak */
						vecrPSDPilPoin = Sort(vecrPSDPilPoin);

						/* Debar peak, if it is much higher than second highest
						   peak (most probably a sinusoid interferer). The
						   highest peak is stored at "vecrPSDPilPoin[2]". Also
						   test for lowest peak */
						if ((vecrPSDPilPoin[1] / vecrPSDPilPoin[2] <
							MAX_RAT_PEAKS_AT_PIL_POS_HIGH) &&
							(vecrPSDPilPoin[0] / vecrPSDPilPoin[2] <
							MAX_RAT_PEAKS_AT_PIL_POS_LOW))
						{
							/* Reset "good-flag" */
							vecbFlagVec[i] = 0;
						}
					}


					/* Get maximum ------------------------------------------ */
					/* First, get the first valid peak entry and init the
					   maximum with this value. We also detect, if a peak is
					   left */
					bNoPeaksLeft = true;
					for (i = 0; i < iNumDetPeaks; i++)
					{
						if (vecbFlagVec[i] == 1)
						{
							/* At least one peak is left */
							bNoPeaksLeft = false;

							/* Init max value */
							iMaxIndex = veciPeakIndex[i];
							rMaxValue = vecrPSDPilCor[veciPeakIndex[i]];
						}
					}

					if (bNoPeaksLeft == false)
					{
						/* Actual maximum detection, take the remaining peak
						   which has the highest value */
						for (i = 0; i < iNumDetPeaks; i++)
						{
							if ((vecbFlagVec[i] == 1) &&
								(vecrPSDPilCor[veciPeakIndex[i]] >
								rMaxValue))
							{
								iMaxIndex = veciPeakIndex[i];
								rMaxValue = vecrPSDPilCor[veciPeakIndex[i]];
							}
						}

#ifdef _DEBUG_
/* Stores all important parameters for last shot */
FILE* pFile1 = fopen("test/freqacq.dat", "w");
int iPeakCnt = 0;
for (i = 1; i < iSearchWinSize; i++) {
	_REAL rPM, rFinPM; /* rPM: peak marker, rFinPM: final peak marker */
	if (iPeakCnt < iNumDetPeaks) {
		if (i == veciPeakIndex[iPeakCnt]) {
			rPM = vecrPSDPilCor[i];
			if (vecbFlagVec[iPeakCnt] == 1) rFinPM = vecrPSDPilCor[i]; else rFinPM = 0;
			iPeakCnt++;
		} else {rPM = 0; rFinPM = 0;}
	} else {rPM = 0; rFinPM = 0;}
	fprintf(pFile1, "%e %e %e\n", vecrPSDPilCor[i], rPM, rFinPM);
}
fclose(pFile1);
// close all;load freqacq.dat;load freqacqFilt.dat;subplot(211),semilogy(freqacq(:,1));hold;plot(freqacq(:,2),'*y');plot(freqacq(:,3),'*k');subplot(212),semilogy(freqacqFilt)
// close all;load freqacq.dat;semilogy(freqacq(:,1));hold;plot(freqacq(:,2),'*y');plot(freqacq(:,3),'*k');
#endif


						/* -----------------------------------------------------
						   An acquisition frequency offest estimation was
						   found */
						/* Calculate frequency offset and set global parameter
						   for offset */
						Parameters.rFreqOffsetAcqui =
							(_REAL) iMaxIndex / iFrAcFFTSize;

						/* Reset acquisition flag */
						bAquisition = false;


						/* Send out the data stored for FFT calculation ----- */
						/* This does not work for bandpass filter. TODO: make
						   this possible for bandpass filter, too */
						if (bUseRecFilter == false)
						{
							iOutputBlockSize = iHistBufSize;

							/* Frequency offset correction */
							const _REAL rNormCurFreqOffsFst = (_REAL) 2.0 * crPi *
								(Parameters.rFreqOffsetAcqui - rInternIFNorm);

							for (i = 0; i < iHistBufSize; i++)
							{
								/* Multiply with exp(j omega t) */
								(*pvecOutputData)[i] = vecrFFTHistory[i] *
									_COMPLEX(Cos(i * rNormCurFreqOffsFst),
									Sin(-i * rNormCurFreqOffsFst));
							}

							/* Init "exp-step" for regular frequency shift which
							   is used in tracking mode to get contiuous mixing
							   signal */
							cCurExp =
								_COMPLEX(Cos(iHistBufSize * rNormCurFreqOffsFst),
								Sin(-iHistBufSize * rNormCurFreqOffsFst));
						}
					}
				}
			}
		}
	}
	else
	{
		/* If synchronized DRM input stream is used, overwrite the detected
		   frequency offest estimate by the desired frequency, because we know
		   this value */
		if (bSyncInput)
		{
			Parameters.rFreqOffsetAcqui =
				(_REAL) Parameters.CellMappingTable.iIndexDCFreq / Parameters.CellMappingTable.iFFTSizeN;
		}

		/* Use the same block size as input block size */
		iOutputBlockSize = iInputBlockSize;


		/* Frequency offset correction -------------------------------------- */
		/* Total frequency offset from acquisition and tracking (we calculate
		   the normalized frequency offset) */
		const _REAL rNormCurFreqOffset =
			(_REAL) 2.0 * crPi * (Parameters.rFreqOffsetAcqui +
			Parameters.rFreqOffsetTrack - rInternIFNorm);

		/* New rotation vector for exp() calculation */
		const _COMPLEX cExpStep =
			_COMPLEX(Cos(rNormCurFreqOffset), Sin(rNormCurFreqOffset));

		/* Input data is real, make complex and compensate for frequency
		   offset */
		for (i = 0; i < iOutputBlockSize; i++)
		{
			(*pvecOutputData)[i] = (*pvecInputData)[i] * Conj(cCurExp);

			/* Rotate exp-pointer on step further by complex multiplication with
			   precalculated rotation vector cExpStep. This saves us from
			   calling sin() and cos() functions all the time (iterative
			   calculation of these functions) */
			cCurExp *= cExpStep;
		}


		/* Bandpass filter -------------------------------------------------- */
		if (bUseRecFilter)
			BPFilter.Process(*pvecOutputData);

	}
	Parameters.Unlock(); 
}

void CFreqSyncAcq::InitInternal(CParameter& Parameters)
{
	Parameters.Lock(); 

	/* Get sample rate */
	const int iSampleRate = Parameters.GetSigSampleRate();

	/* Needed for calculating offset in Hertz in case of synchronized input
	   (for simulation) */
	iFFTSize = Parameters.CellMappingTable.iFFTSizeN;

	/* We using parameters from robustness mode B as pattern for the desired
	   frequency pilot positions */
	veciTableFreqPilots[0] =
		iTableFreqPilRobModB[0][0] * NUM_BLOCKS_4_FREQ_ACQU;
	veciTableFreqPilots[1] =
		iTableFreqPilRobModB[1][0] * NUM_BLOCKS_4_FREQ_ACQU;
	veciTableFreqPilots[2] =
		iTableFreqPilRobModB[2][0] * NUM_BLOCKS_4_FREQ_ACQU;

	/* Size of FFT */
	iFrAcFFTSize = ADJ_FOR_SRATE(RMB_FFT_SIZE_N, iSampleRate) * NUM_BLOCKS_4_FREQ_ACQU;


	/* -------------------------------------------------------------------------
	   Set start- and endpoint of search window for DC carrier after the
	   correlation with the known pilot structure */
	/* Normalize the desired position and window size which are in Hertz */
	const _REAL rNormDesPos   = rCenterFreq < 0.0 ? 0.5 : rCenterFreq / iSampleRate * 2;
	const _REAL rNormHalfWinSize = rWinSize < 0.0 ? 0.5 : rWinSize    / iSampleRate;

	/* Length of the half of the spectrum of real input signal (the other half
	   is the same because of the real input signal). We have to consider the
	   Nyquist frequency ("iFrAcFFTSize" is always even!) */
	iHalfBuffer = iFrAcFFTSize / 2 + 1;

	/* Search window is smaller than haft-buffer size because of correlation
	   with pilot positions */
	iSearchWinSize = iHalfBuffer - veciTableFreqPilots[2];

	/* Calculate actual indices of start and end of search window */
	iStartDCSearch =
		(int) Floor((rNormDesPos - rNormHalfWinSize) * iHalfBuffer);
	iEndDCSearch = (int) Ceil((rNormDesPos + rNormHalfWinSize) * iHalfBuffer);

	/* Check range. If out of range -> correct */
	if (!((iStartDCSearch > 0) && (iStartDCSearch < iSearchWinSize)))
		iStartDCSearch = 0;

	if (!((iEndDCSearch > 0) && (iEndDCSearch < iSearchWinSize)))
		iEndDCSearch = iSearchWinSize;

	/* Set bound for ratio between filtered signal to signal. Use a lower bound
	   if the search window is smaller */
	if (((_REAL) iEndDCSearch - iStartDCSearch) / iHalfBuffer < (_REAL) 0.042)
		rPeakBoundFiltToSig = PEAK_BOUND_FILT2SIGNAL_0_042;
	else
		rPeakBoundFiltToSig = PEAK_BOUND_FILT2SIGNAL_1;


	/* Init vectors and FFT-plan -------------------------------------------- */
	/* Allocate memory for FFT-histories and init with zeros */
	iHistBufSize = iFrAcFFTSize * NUM_BLOCKS_USED_FOR_AV;
	vecrFFTHistory.Init(iHistBufSize, (_REAL) 0.0);
	vecrFFTInput.Init(iFrAcFFTSize);
	vecrSqMagFFTOut.Init(iHalfBuffer);

	/* Allocate memory for PSD after pilot correlation */
	vecrPSDPilCor.Init(iHalfBuffer);

	/* Init vectors for filtering in frequency direction */
	vecrFiltResLR.Init(iHalfBuffer);
	vecrFiltResRL.Init(iHalfBuffer);
	vecrFiltRes.Init(iHalfBuffer);

	/* Index memory for detected peaks (assume worst case with the size) */
	veciPeakIndex.Init(iHalfBuffer);

	/* Init plans for FFT (faster processing of Fft and Ifft commands) */
	FftPlan.Init(iFrAcFFTSize);

	/* Init Hamming window */
	vecrHammingWin.Init(iFrAcFFTSize);
	vecrHammingWin = Hamming(iFrAcFFTSize);

	/* Init moving average class for SqMag FFT results */
	vvrPSDMovAv.InitVec(NUM_FFT_RES_AV_BLOCKS, iHalfBuffer);


	/* Frequency correction */
	/* Start with phase null (arbitrary) */
	cCurExp = (_REAL) 1.0;
	rInternIFNorm = (_REAL) Parameters.CellMappingTable.iIndexDCFreq / iFFTSize;


	/* Init bandpass filter object */
	BPFilter.Init(iSampleRate, Parameters.CellMappingTable.iSymbolBlockSize, VIRTUAL_INTERMED_FREQ,
		Parameters.GetSpectrumOccup(), CDRMBandpassFilt::FT_RECEIVER);


	/* Define block-sizes for input (The output block size is set inside
	   the processing routine, therefore only a maximum block size is set
	   here) */
	iInputBlockSize = Parameters.CellMappingTable.iSymbolBlockSize;

	/* We have to consider that the next module can take up to two symbols per
	   step. This can be satisfied be multiplying with "3". We also want to ship
	   the whole FFT buffer after finishing the frequency acquisition so that
	   these samples can be reused for synchronization and do not get lost */
	iMaxOutputBlockSize = 3 * Parameters.CellMappingTable.iSymbolBlockSize + iHistBufSize;

	/* OPH: init free-running symbol counter */
	iFreeSymbolCounter = 0;

	Parameters.Unlock(); 
}

void CFreqSyncAcq::SetSearchWindow(_REAL rNewCenterFreq, _REAL rNewWinSize)
{
	/* Set internal parameters */
	rCenterFreq = rNewCenterFreq;
	rWinSize = rNewWinSize;

	/* Set flag to initialize the module to the new parameters */
	SetInitFlag();
}

void CFreqSyncAcq::StartAcquisition()
{
	/* Set flag so that the actual acquisition routine is entered */
	bAquisition = true;

	/* Reset (or init) counters */
	iAquisitionCounter = NUM_BLOCKS_4_FREQ_ACQU;
	iAverageCounter = NUM_FFT_RES_AV_BLOCKS;

	/* Reset FFT-history */
	vecrFFTHistory.Reset((_REAL) 0.0);
}
