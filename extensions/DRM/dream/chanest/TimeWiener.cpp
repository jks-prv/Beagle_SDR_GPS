/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2001
 *
 * Author(s):
 *	Volker Fischer
 *
 * Description:
 *	Wiener filter in time direction for channel estimation
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

#include "TimeWiener.h"
#include "../matlib/MatlibSigProToolbox.h"

/* Implementation *************************************************************/
_REAL CTimeWiener::Estimate(CVectorEx<_COMPLEX>* pvecInputData,
						    CComplexVector& veccOutputData,
						    CVector<int>& veciMapTab,
						    CVector<_COMPLEX>& veccPilotCells, _REAL rSNR)
{
	int			j, i;
	int			iPiHiIdx;
	int			iTimeDiffNew;
	_COMPLEX	cNewPilot;

	/* Timing correction history -------------------------------------------- */
	/* Shift old vaules and add a "0" at the beginning of the vector */
	vecTiCorrHist.AddBegin(0);

	/* Add new one to all history values except of the current one */
	for (i = 1; i < iLenTiCorrHist; i++)
		vecTiCorrHist[i] += (*pvecInputData).GetExData().iCurTimeCorr;


	/* Update histories for channel estimates at the pilot positions -------- */
	for (i = 0, iPiHiIdx = 0; i < iNumCarrier; i += iScatPilFreqInt, iPiHiIdx++)
	{
		/* Identify and calculate transfer function at the pilot positions */
		if (_IsScatPil(veciMapTab[i]))
		{
			/* Save channel estimates at the pilot positions for each carrier.
			   Move old estimates and put new value. Use reversed order to
			   prepare vector for convolution */
			for (j = iLengthWiener - 1; j > 0; j--)
			{
				matcChanAtPilPos[j][iPiHiIdx] =
					matcChanAtPilPos[j - 1][iPiHiIdx];
			}

			/* Add new channel estimate: h = r / s, h: transfer function of the
			   channel, r: received signal, s: transmitted signal */
			matcChanAtPilPos[0][iPiHiIdx] =
				(*pvecInputData)[i] / veccPilotCells[i];


			/* Estimation of the channel correlation function --------------- */
			/* We calcuate the estimation for one symbol first and average this
			   result */
			for (j = 0; j < iNumTapsSigEst; j++)
			{
				/* Correct pilot information for phase rotation */
				iTimeDiffNew = vecTiCorrHist[iScatPilTimeInt * j];
				cNewPilot = Rotate(matcChanAtPilPos[j][iPiHiIdx], i,
					iTimeDiffNew);

				/* Use IIR filtering for averaging */
				IIR1(veccTiCorrEst[j],
					Conj(matcChanAtPilPos[0][iPiHiIdx]) * cNewPilot,
					rLamTiCorrAv);
			}
		}


		/* Wiener interpolation and filtering ------------------------------- */
		/* This check is for robustness mode D since "iScatPilFreqInt" is "1"
		   in this case it would include the DC carrier in the for-loop */
		if (!_IsDC(veciMapTab[i]))
		{
			/* Read current filter phase from table */
			const int iCurrFiltPhase = matiFiltPhaseTable[iPiHiIdx]
				[(*pvecInputData).GetExData().iSymbolID];


#ifdef USE_DD_WIENER_FILT_TIME
// Get length of current filter, TODO: better solution
			const int iCurFiltLen = vecvecPilIdx[iCurrFiltPhase].Size();
			CComplexVector veccZFHestDD(iCurFiltLen);

			int iPilIdx = 0;
			int iDatIdx = 0;

			for (j = 0; j < iCurFiltLen; j++)
			{
				/* Build vector for filtering. Make sure that pilot cells and
				   DD-data cells are at the correct place */
				CComplex cNewPilot;
				if (vecvecPilIdx[iCurrFiltPhase][j].bIsPilot)
				{
					cNewPilot = matcChanAtPilPos[iPilIdx][iPiHiIdx];
					iPilIdx++;
				}
				else
				{
					cNewPilot = matcChanAtDataPos[iDatIdx][iPiHiIdx];
					iDatIdx++;
				}

				/* We need to correct pilots due to timing corrections */
				/* Calculate timing difference */
				iTimeDiffNew = vecTiCorrHist[
					vecvecPilIdx[iCurrFiltPhase][j].iIdx + iCurrFiltPhase] -
					vecTiCorrHist[iLenHistBuff - 1];

				/* Correct pilot information for phase rotation */
				veccZFHestDD[j] = Rotate(cNewPilot, i, iTimeDiffNew);
			}

			/* Convolution */
			const CComplex cCurChanEst =
				Sum(veccZFHestDD * matrFiltTime[iCurrFiltPhase]);

			/* Received signal must be delayed (we are using a FIFO here) */
			matcRecSigHist[iPiHiIdx].Add((*pvecInputData)[i]);
			matiMapTabHist[iPiHiIdx].Add(veciMapTab[i]);

			const CComplex cCurRecSig = matcRecSigHist[iPiHiIdx].Get();
			const int iCurMapTab = matiMapTabHist[iPiHiIdx].Get();

			/* Make decision and store result in history */
			/* Make sure current cell is a data cell */
			if (!_IsPilot(iCurMapTab))
			{
				/* The dicision directed channel estimation is calculated as
				   follows: h_DD = r / Dec(r / h) */
				CComplex cCurDDChanEst;

				/* Find out which coding scheme is used for this cell */
				if (_IsFAC(iCurMapTab) || (_IsSDC(iCurMapTab) &&
					(eSDCQAMMode == CParameter::CS_1_SM)))
				{
					/* 4-QAM */
					cCurDDChanEst =
						cCurRecSig / Dec4QAM(cCurRecSig / cCurChanEst);
				}
				else if (_IsSDC(iCurMapTab) || (_IsMSC(iCurMapTab) &&
					(eMSCQAMMode == CParameter::CS_2_SM)))
				{
					/* 16-QAM */
					cCurDDChanEst =
						cCurRecSig / Dec16QAM(cCurRecSig / cCurChanEst);
				}
				else
				{
					/* 64-QAM */
					cCurDDChanEst =
						cCurRecSig / Dec64QAM(cCurRecSig / cCurChanEst);
				}

				/* Save channel estimates at the data positions for each
				   carrier. Move old estimates and put new value. Use reversed
				   order */
				for (j = iLenDDHist - 1; j > 0; j--)
				{
					matcChanAtDataPos[j][iPiHiIdx] =
						matcChanAtDataPos[j - 1][iPiHiIdx];
				}

				matcChanAtDataPos[0][iPiHiIdx] = cCurDDChanEst;
			}
#else
			/* Convolution with one phase of the optimal filter */
			/* Init sum */
			_COMPLEX cCurChanEst = _COMPLEX((_REAL) 0.0, (_REAL) 0.0);
			for (j = 0; j < iLengthWiener; j++)
			{
				/* We need to correct pilots due to timing corrections */
				/* Calculate timing difference */
				iTimeDiffNew =
					vecTiCorrHist[j * iScatPilTimeInt + iCurrFiltPhase] -
					vecTiCorrHist[iLenHistBuff - 1];

				/* Correct pilot information for phase rotation */
				cNewPilot =
					Rotate(matcChanAtPilPos[j][iPiHiIdx], i, iTimeDiffNew);

				/* Actual convolution with filter phase */
				cCurChanEst += cNewPilot * matrFiltTime[iCurrFiltPhase][j];
			}
#endif

			/* Copy channel estimation from current symbol in output buffer */
			veccOutputData[iPiHiIdx] = cCurChanEst;
		}
	}


	/* Update sigma estimation ---------------------------------------------- */
	if (bTracking)
	{
		/* Update filter coefficients once in one DRM frame */
		if (iUpCntWienFilt > 0)
		{
			iUpCntWienFilt--;

			/* Average estimated SNR values */
			rAvSNR += rSNR;
			iAvSNRCnt++;
		}
		else
		{
			/* Actual estimation of sigma */
			rSigma = ModLinRegr(veccTiCorrEst);

			/* Use overestimated sigma for filter update */
			const _REAL rSigOverEst = rSigma * SIGMA_OVERESTIMATION_FACT;

			/* Update the wiener filter, use averaged SNR */
			if (rSigOverEst < rSigmaMax)
				rMMSE = UpdateFilterCoef(rAvSNR / iAvSNRCnt, rSigOverEst);
			else
				rMMSE = UpdateFilterCoef(rAvSNR / iAvSNRCnt, rSigmaMax);

			/* If no SNR improvent is achieved by the optimal filter, use
			   SNR estimation for MMSE */
			_REAL rNewSNR = (_REAL) 1.0 / rMMSE;
			if (rNewSNR < rSNR)
				rMMSE = (_REAL) 1.0 / rSNR;

			/* Reset counter and sum (for SNR) */
			iUpCntWienFilt = iNumSymPerFrame;
			iAvSNRCnt = 0;
			rAvSNR = (_REAL) 0.0;
		}
	}

	/* Return the SNR improvement by Wiener interpolation in time direction */
	return (_REAL) 1.0 / rMMSE;
}

int CTimeWiener::Init(CParameter& Parameters)
{
	/* Init base class, must be at the beginning of this init! */
	CPilotModiClass::InitRot(Parameters);

	/* Set local parameters */
	const CCellMappingTable& Param = Parameters.CellMappingTable;
	iNumCarrier = Param.iNumCarrier;
	iScatPilTimeInt = Param.iScatPilTimeInt;
	iScatPilFreqInt = Param.iScatPilFreqInt;
	iNumSymPerFrame = Param.iNumSymPerFrame;
	const int iNumIntpFreqPil = Param.iNumIntpFreqPil;

	/* Generate filter phase table for Wiener filter */
	GenFiltPhaseTable(Parameters.CellMappingTable.matiMapTab, iNumCarrier, iNumSymPerFrame, iScatPilTimeInt);

	/* Init length of filter and maximum value of sigma (doppler) */
	switch (Parameters.GetWaveMode())
	{
	case RM_ROBUSTNESS_MODE_A:
		iLengthWiener = LEN_WIENER_FILT_TIME_RMA;
		rSigmaMax = MAX_SIGMA_RMA;
		break;

	case RM_ROBUSTNESS_MODE_B:
		iLengthWiener = LEN_WIENER_FILT_TIME_RMB;
		rSigmaMax = MAX_SIGMA_RMB;
		break;

	case RM_ROBUSTNESS_MODE_C:
		iLengthWiener = LEN_WIENER_FILT_TIME_RMC;
		rSigmaMax = MAX_SIGMA_RMC;
		break;

	case RM_ROBUSTNESS_MODE_D:
		iLengthWiener = LEN_WIENER_FILT_TIME_RMD;
		rSigmaMax = MAX_SIGMA_RMD;
		break;
	
	default:
		break;
	}

	/* Set delay of this channel estimation type. The longer the delay is, the
	   more "acausal" pilots can be used for interpolation. We use the same
	   amount of causal and acausal filter taps here. Make sure that we get
	   R_hp's which have the most energy collected:
	   L = Np * TiPi - TiPi + 1 is the total number of cells which span our
	   interpolation when we set a pilot on the left-most and right-most
	   Ceil(L / 2) is the middle of the range, now we only have to consider
	   half of the TiPi which is Floor(TiPi / 2) */
	const int iSymDelyChanEst = (int) Ceil((CReal) (
		iLengthWiener * iScatPilTimeInt - iScatPilTimeInt + 1) / 2) +
		(int) Floor((CReal) iScatPilTimeInt / 2) - 1;

	/* Set number of phases for wiener filter */
	iNumFiltPhasTi = iScatPilTimeInt;

	/* Set length of history-buffer */
	iLenHistBuff = iSymDelyChanEst + 1;

	/* Duration of useful part plus guard interval */
	rTs = (_REAL) Parameters.CellMappingTable.iSymbolBlockSize / Parameters.GetSigSampleRate();

	/* Total number of interpolated pilots in frequency direction. We have to
	   consider the last pilot at the end ("+ 1") */
	const int iTotNumPiFreqDir = iNumCarrier / iScatPilFreqInt + 1;

	/* Allocate memory for Channel at pilot positions (matrix) and init with ones */
	matcChanAtPilPos.Init(iLengthWiener, iTotNumPiFreqDir, _COMPLEX(1.0, 0.0));

	/* Set number of taps for sigma estimation */
	if (iLengthWiener < NUM_TAPS_USED4SIGMA_EST)
		iNumTapsSigEst = iLengthWiener;
	else
		iNumTapsSigEst = NUM_TAPS_USED4SIGMA_EST;

	/* Init vector for estimation of the correlation function in time direction
	   (IIR average) */
	veccTiCorrEst.Init(iNumTapsSigEst, (CReal) 0.0);

	/* Init time constant for IIR filter for averaging correlation estimation.
	   Consider averaging over frequency axis, too. Pilots in frequency
	   direction are "iScatPilTimeInt * iScatPilFreqInt" apart */
	const int iNumPilOneOFDMSym = iNumIntpFreqPil / iScatPilTimeInt;
	rLamTiCorrAv = IIR1Lam(TICONST_TI_CORREL_EST * iNumPilOneOFDMSym,
		(CReal) Parameters.GetSigSampleRate() / Parameters.CellMappingTable.iSymbolBlockSize);

	/* Init update counter for Wiener filter update. We immediatly use the
	   filtered result although right at the beginning there is no averaging.
	   But sine the estimation usually starts with higher values and goes down
	   to the correct one, this should not be critical */
	iUpCntWienFilt = iNumSymPerFrame;

	/* Init averaging of SNR values */
	rAvSNR = (_REAL) 0.0;
	iAvSNRCnt = 0;


	/* Allocate memory for filter phases (Matrix) */
	matrFiltTime.Init(iNumFiltPhasTi, iLengthWiener);

	/* Length of the timing correction history buffer */
	iLenTiCorrHist = iLengthWiener * iNumFiltPhasTi;

	/* Init timing correction history with zeros */
	vecTiCorrHist.Init(iLenTiCorrHist, 0);


#ifdef USE_DD_WIENER_FILT_TIME
	/* Allocate memory for Channel at data positions and init with ones */
// TODO: we init the vectors longer than needed
	iLenDDHist = iLengthWiener * iScatPilTimeInt;

	matcChanAtDataPos.Init(iLenDDHist, iTotNumPiFreqDir, (CReal) 1.0);

	/* Get modulation alphabet of MSC and SDC (MSC can only be 16- or 64-QAM,
	   for SDC, only 4- or 16-QAM is possible) */
	if (Parameters.eMSCCodingScheme == CParameter::CS_2_SM)
		eMSCQAMMode = CParameter::CS_2_SM;
	else
		eMSCQAMMode = CParameter::CS_3_SM;

	if (Parameters.eSDCCodingScheme == CParameter::CS_1_SM)
		eSDCQAMMode = CParameter::CS_1_SM;
	else
		eSDCQAMMode = CParameter::CS_2_SM;

	/* Init matrix for pilot and data indices */
	vecvecPilIdx.Init(iNumFiltPhasTi);

	/* Allocate memory for received signal history buffer and zero out */
	matcRecSigHist.Init(iTotNumPiFreqDir);
	matiMapTabHist.Init(iTotNumPiFreqDir);

	for (int i = 0; i < iTotNumPiFreqDir; i++)
	{
		matcRecSigHist[i].Init(iLenHistBuff, (CReal) 0.0);
		matiMapTabHist[i].Init(iLenHistBuff, 0);
	}
#endif


	/* Calculate optimal filter --------------------------------------------- */
	_REAL rSNR;

	/* Distinguish between simulation and regular receiver. When we run a
	   simulation, the parameters are taken from simulation init */
	if (Parameters.eSimType == CParameter::ST_NONE)
	{
		/* Init SNR value */
		rSNR = pow((CReal) 10.0, INIT_VALUE_SNR_WIEN_TIME_DB / 10);

		/* Init sigma with a large value. This make the acquisition more
		   robust in case of a large sample frequency offset. But we get more
		   aliasing in the time domain and this could make the timing unit
		   perform worse. Therefore, this is only a trade-off */
		rSigma = rSigmaMax;
	}
	else
	{
		/* Get SNR on the pilot positions */
		rSNR = pow((CReal) 10.0, Parameters.GetSysSNRdBPilPos() / 10);
	
		/* Sigma from channel profiles */
		switch (Parameters.iDRMChannelNum)
		{
		case 1:
		case 2:
			rSigma = LOW_BOUND_SIGMA;
			break;

		case 4:
			rSigma = 1.0 / 2;
			break;

		case 3:
			rSigma = 1.2 / 2;
			break;

		case 5:
			rSigma = 2.0 / 2;
			break;

		case 8:
		case 10:
		case 11:
		case 12:
			rSigma = (_REAL) Parameters.iSpecChDoppler / 2;
			break;

		default: /* Including channel number 6 */
			rSigma = rSigmaMax / 2;
			break;
		}

		/* Reset flag to inhibit parameter adaptation */
		bTracking = false;
	}

	/* Calculate initialization wiener filter taps and init MMSE */
	rMMSE = UpdateFilterCoef(rSNR, rSigma);

	/* Return delay of channel equalization */
	return iLenHistBuff;
}

void CTimeWiener::GenFiltPhaseTable(const CMatrix<int>& matiMapTab,
									const int iNumCarrier,
									const int iNumSymPerFrame,
									const int iScatPilTimeInt)
{
	/* Init matrix */
	matiFiltPhaseTable.Init(iNumCarrier, iNumSymPerFrame);

	/* Get the index of first symbol in a super-frame on where the first cell
	   (carrier-index = 0) is a pilot. This is needed for determining the
	   correct filter phase for the convolution */
	int iFirstSymWithPilot = 0;
	while (!_IsScatPil(matiMapTab[iFirstSymWithPilot][0]))
		iFirstSymWithPilot++;

	for (int i = 0; i < iNumCarrier; i++)
	{
		for (int j = 0; j < iNumSymPerFrame; j++)
		{
			/* Calculate filter phases for Wiener filter for each OFDM cell in
			   a DRM frame */
			matiFiltPhaseTable[i][j] = (iScatPilTimeInt -
				(iNumSymPerFrame - j + iFirstSymWithPilot + i) %
				iScatPilTimeInt) % iScatPilTimeInt;
		}
	}
}

_REAL CTimeWiener::UpdateFilterCoef(const _REAL rNewSNR, const _REAL rNewSigma)
{
	/* Calculate MMSE for wiener filtering for all phases and average */
	_REAL rMMSE = (_REAL) 0.0;

	/* One filter for all possible filter phases */
	for (int j = 0; j < iNumFiltPhasTi; j++)
	{
		/* We have to define the dependency between the difference between the
		   current pilot to the observed symbol in the history buffer and the
		   indizes of the FiltTime array. Definition:
		   Largest distance = index zero, index increases to smaller
		   distances */
		const int iCurrDiffPhase = -(iLenHistBuff - j - 1);

		/* Calculate filter phase and average MMSE */
#ifdef USE_DD_WIENER_FILT_TIME
		rMMSE += TimeOptimalFiltDD(matrFiltTime[j], iScatPilTimeInt,
			iCurrDiffPhase,	rNewSNR, rNewSigma, rTs, iLengthWiener, j);
#else
		rMMSE += TimeOptimalFilter(matrFiltTime[j], iScatPilTimeInt,
			iCurrDiffPhase,	rNewSNR, rNewSigma, rTs, iLengthWiener);
#endif
	}

	/* Normalize averaged MMSE */
	rMMSE /= iNumFiltPhasTi;

	return rMMSE;
}

CReal CTimeWiener::TimeOptimalFilter(CRealVector& vecrTaps, const int iTimeInt,
									 const int iDiff, const CReal rNewSNR,
									 const CReal rNewSigma, const CReal rTs,
									 const int iLength)
{
	int i;

	CRealVector vecrRpp(iLength);
	CRealVector vecrRhp(iLength);

	/* Factor for the argument of the exponetial function to generate the
	   correlation function */
	const CReal rFactorArgExp =
		(CReal) -2.0 * crPi * crPi * rTs * rTs * rNewSigma * rNewSigma;

	/* Doppler-spectrum for short-wave channel is Gaussian
	   (Calculation of R_hp!) */
	for (i = 0; i < iLength; i++)
	{
		const int iCurPos = i * iTimeInt + iDiff;

		vecrRhp[i] = exp(rFactorArgExp * iCurPos * iCurPos);
	}

	/* Doppler-spectrum for short-wave channel is Gaussian
	   (Calculation of R_pp!) */
	for (i = 0; i < iLength; i++)
	{
		const int iCurPos = i * iTimeInt;

		vecrRpp[i] = exp(rFactorArgExp * iCurPos * iCurPos);
	}

	/* Add SNR at first tap */
	vecrRpp[0] += (CReal) 1.0 / rNewSNR;

	/* Call levinson algorithm to solve matrix system for optimal solution */
	vecrTaps = Levinson(vecrRpp, vecrRhp);

	/* Return MMSE for the current wiener filter */
	return (CReal) 1.0 - Sum(vecrRhp * vecrTaps);
}

#ifdef USE_DD_WIENER_FILT_TIME
CReal CTimeWiener::TimeOptimalFiltDD(CRealVector& vecrTaps, const int iTimeInt,
									 const int iDiff, const CReal rNewSNR,
									 const CReal rNewSigma, const CReal rTs,
									 const int iLength, const int iFiltPhase)
{
	int i, j;

	/* Calculate number of pilot and DD cells. We use all cells up to the cell
	   to be interpolated as pilot cells and after that only the regular
	   pilot cells */
	const int iTotNumCellsInRange = (iLength - 1) * iTimeInt + 1;

	vecvecPilIdx[iFiltPhase].Init(0);
	for (i = 0; i < iTotNumCellsInRange; i++)
	{
		if (i < -iDiff + 1)
		{
			/* Use only real pilots */
			if (i % iTimeInt == 0)
				vecvecPilIdx[iFiltPhase].Add(CDDPilIdx(i, true));
		}
		else
		{
			if (i % iTimeInt == 0) /* Pilot or data cell? */
				vecvecPilIdx[iFiltPhase].Add(CDDPilIdx(i, true));
			else
				vecvecPilIdx[iFiltPhase].Add(CDDPilIdx(i, false));
		}
	}

	const int iNewLength = vecvecPilIdx[iFiltPhase].Size();

	CComplexMatrix matcRpp(iNewLength, iNewLength);
	CComplexVector veccRhp(iNewLength);

	/* Factor for the argument of the exponetial function to generate the
	   correlation function */
	const CReal rFactorArgExp =
		(CReal) -2.0 * crPi * crPi * rTs * rTs * rNewSigma * rNewSigma;

	/* Doppler-spectrum for short-wave channel is Gaussian
	   (Calculation of R_hp!) */
	for (i = 0; i < iNewLength; i++)
	{
		const int iCurPos = vecvecPilIdx[iFiltPhase][i].iIdx + iDiff;

		veccRhp[i] = exp(rFactorArgExp * iCurPos * iCurPos);
	}

	/* Doppler-spectrum for short-wave channel is Gaussian
	   (Calculation of R_pp!) */
	for (i = 0; i < iNewLength; i++)
	{
		for (j = 0; j < iNewLength; j++)
		{
			const int iCurPos = vecvecPilIdx[iFiltPhase][i].iIdx -
				vecvecPilIdx[iFiltPhase][j].iIdx;

			matcRpp[i][j] = exp(rFactorArgExp * iCurPos * iCurPos);

			/* Add SNR (dependent on pilot or DD cell) */
			if (i == j)
			{
				if (vecvecPilIdx[iFiltPhase][j].bIsPilot)
					matcRpp[i][j] += (CReal) 1.0 / rNewSNR;
				else
				{
					/* DD cell has lower SNR */
// TODO consider boosted pilots, consider decision errors!
					matcRpp[i][j] += (CReal) 1.0 / rNewSNR *
						AV_PILOT_POWER / AV_DATA_CELLS_POWER;
				}
			}
		}
	}

	/* Call levinson algorithm to solve matrix system for optimal solution */
// make sure size is correct FIXME: better solution!
	vecrTaps.Init(iNewLength);

	/* Calculate optimal filter (Since our correlations are real, the filter
	   taps must be real, too */
	vecrTaps = Real(Inv(matcRpp) * veccRhp);

	/* Return MMSE for the current wiener filter */
	return (CReal) 1.0 - Sum(Real(veccRhp * vecrTaps));
}
#endif

CReal CTimeWiener::ModLinRegr(const CComplexVector& veccCorrEst)
{
	/* Modified linear regression to estimate the "sigma" of the Gaussian
	   correlation function */
	/* Get vector length */
	const int iVecLen = veccCorrEst.GetSize();

	/* Init vectors and variables */
	CRealVector Tau(iVecLen);
	CRealVector Z(iVecLen);
	CRealVector W(iVecLen);

	/* Generate the tau vector */
	for (int i = 0; i < iVecLen; i++)
		Tau[i] = (CReal) (i * iScatPilTimeInt);

	/* Linearize acf equation:  y = a * exp(-b * x ^ 2)
	   z = ln(y); w = x ^ 2
	   -> z = a0 + a1 * w */
	Z = Log(Abs(veccCorrEst));
	W = Tau * Tau;

	/* Apply linear regression */
	const CReal A1 = LinRegr(W, Z);

	/* Final sigma calculation from estimation and assumed Gaussian model */
	const CReal rSigmaRet = (CReal) 0.5 / crPi * Sqrt((CReal) -2.0 * A1) / rTs;

	/* Bound estimated sigma value */
	if (rSigmaRet > rSigmaMax)
		return rSigmaMax;
	else if (rSigmaRet < LOW_BOUND_SIGMA)
		return LOW_BOUND_SIGMA;
	else
		return rSigmaRet;
}
