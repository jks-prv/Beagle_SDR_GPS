/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2001
 *
 * Author(s):
 *	Volker Fischer
 *
 * Description:
 *	See FreqSyncAcq.cpp
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

#if !defined(FREQSYNC_H__3B0BA660EDOINBEROUEBGF4344_BB2B_23E7912__INCLUDED_)
#define FREQSYNC_H__3B0BA660EDOINBEROUEBGF4344_BB2B_23E7912__INCLUDED_

#include "../Parameter.h"
#include "../util/Modul.h"
#include "../matlib/Matlib.h"
#include "../util/Utilities.h"

/* Definitions ****************************************************************/
/* Bound for peak detection between filtered signal (in frequency direction) 
   and the unfiltered signal. Define different bounds for different relative
   search window sizes */
#define PEAK_BOUND_FILT2SIGNAL_1		((CReal) 9)
#define PEAK_BOUND_FILT2SIGNAL_0_042	((CReal) 7)

/* This value MUST BE AT LEAST 2, because otherwise we would get an overrun
   when we try to add a complete symbol to the buffer! */
#ifdef USE_FRQOFFS_TRACK_GUARDCORR
# define NUM_BLOCKS_4_FREQ_ACQU			30 /* Accuracy must be higher */
#else
# define NUM_BLOCKS_4_FREQ_ACQU			6
#endif

/* Number of block used for averaging */
#define NUM_BLOCKS_USED_FOR_AV			3

/* Lambda for IIR filter for estimating noise floor in frequency domain */
#define LAMBDA_FREQ_IIR_FILT			((CReal) 0.87)

/* Ratio between highest and second highest peak at the frequency pilot
   positions in the PSD estimation (after peak detection) */
#define MAX_RAT_PEAKS_AT_PIL_POS_HIGH	((CReal) 0.99)

/* Ratio between highest and lowest peak at frequency pilot positions (see
   above) */
#define MAX_RAT_PEAKS_AT_PIL_POS_LOW	((CReal) 0.8)

/* Number of blocks storing the squared magnitude of FFT used for
   averaging */
#define NUM_FFT_RES_AV_BLOCKS			(NUM_BLOCKS_4_FREQ_ACQU * (NUM_BLOCKS_USED_FOR_AV - 1) + 1)


/* Classes ********************************************************************/
class CFreqSyncAcq : public CReceiverModul<_REAL, _COMPLEX>
{
public:
	CFreqSyncAcq() : 
		veciTableFreqPilots(3), /* 3 frequency pilots */
		bAquisition(false), bSyncInput(false),
		rCenterFreq(0), rWinSize(0),
		bUseRecFilter(false)
		{}
	virtual ~CFreqSyncAcq() {}

	void SetSearchWindow(_REAL rNewCenterFreq, _REAL rNewWinSize);

	void StartAcquisition();
	void StopAcquisition() {bAquisition = false;}
	bool GetAcquisition() {return bAquisition;}

	void SetRecFilter(const bool bNewF) {bUseRecFilter = bNewF;}
	bool GetRecFilter() {return bUseRecFilter;}
	bool GetUnlockedFrameBoundary() {return iFreeSymbolCounter==0;}

	/* To set the module up for synchronized DRM input data stream */
	void SetSyncInput(bool bNewS) {bSyncInput = bNewS;}

protected:
	CVector<int>				veciTableFreqPilots;
	CShiftRegister<_REAL>		vecrFFTHistory;

	CFftPlans					FftPlan;
	CRealVector					vecrFFTInput;
	CRealVector					vecrSqMagFFTOut;
	CRealVector					vecrHammingWin;
	CMovingAv<CRealVector>		vvrPSDMovAv;

	int							iFrAcFFTSize;
	int							iHistBufSize;

	int							iFFTSize;

	bool					bAquisition;

	int							iAquisitionCounter;
	int							iAverageCounter;

	bool					bSyncInput;

	_REAL						rCenterFreq;
	_REAL						rWinSize;
	int							iStartDCSearch;
	int							iEndDCSearch;
	_REAL						rPeakBoundFiltToSig;

	CRealVector					vecrPSDPilCor;
	int							iHalfBuffer;
	int							iSearchWinSize;
	CRealVector					vecrFiltResLR;
	CRealVector					vecrFiltResRL;
	CRealVector					vecrFiltRes;
	CVector<int>				veciPeakIndex;

	_COMPLEX					cCurExp;
	_REAL						rInternIFNorm;

	CDRMBandpassFilt			BPFilter;
	bool					bUseRecFilter;

	/* OPH: counter to count symbols within a frame in order to generate */
	/* RSCI output even when unlocked */
	int							iFreeSymbolCounter;

	virtual void InitInternal(CParameter& Parameters);
	virtual void ProcessDataInternal(CParameter& Parameters);
};


#endif // !defined(FREQSYNC_H__3B0BA660EDOINBEROUEBGF4344_BB2B_23E7912__INCLUDED_)
