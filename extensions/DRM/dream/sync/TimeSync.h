/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2001
 *
 * Author(s):
 *	Volker Fischer
 *
 * Description:
 *	See TimeSync.cpp
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

#if !defined(TIMESYNC_H__3B0BEVJBN872345NBEROUEBGF4344_BB27912__INCLUDED_)
#define TIMESYNC_H__3B0BEVJBN872345NBEROUEBGF4344_BB27912__INCLUDED_

#include "../Parameter.h"
#include "../util/Modul.h"
#include "../util/Vector.h"
#include "../matlib/Matlib.h"
#include "TimeSyncFilter.h"


/* Definitions ****************************************************************/
/* Use 5 or 10 kHz bandwidth for guard-interval correlation. 10 kHz bandwidth
   should be chosen when time domain freuqency offset estimation is used */
#define USE_10_KHZ_HILBFILT

#define LAMBDA_LOW_PASS_START			((CReal) 0.99)
#define TIMING_BOUND_ABS				150

/* Non-linear correction of the timing if variation is too big */
#define NUM_SYM_BEFORE_RESET			5

/* Definitions for robustness mode detection */
#define NUM_BLOCKS_FOR_RM_CORR			16
#define THRESHOLD_RELI_MEASURE			((CReal) 8.0)

/* The guard-interval correlation is only updated every "STEP_SIZE_GUARD_CORR"
   samples to save computations */
#define STEP_SIZE_GUARD_CORR			4

/* "GRDCRR_DEC_FACT": Downsampling factor. We only use approx. 6 [12] kHz for
   correlation, therefore we can use a decimation of 8 [4]
   (i.e., 48 kHz / 8 [4] = 6 [12] kHz). Must be 8 [4] since all symbol and
   guard-interval lengths at 48000 for all robustness modes are dividable
   by 8 [4] */
#ifdef USE_10_KHZ_HILBFILT
# define GRDCRR_DEC_FACT				4
# define NUM_TAPS_HILB_FILT_24			NUM_TAPS_HILB_FILT_10_24
# define NUM_TAPS_HILB_FILT_48			NUM_TAPS_HILB_FILT_10_48
# define NUM_TAPS_HILB_FILT_96			NUM_TAPS_HILB_FILT_10_96
# define NUM_TAPS_HILB_FILT_192			NUM_TAPS_HILB_FILT_10_192
# define NUM_TAPS_HILB_FILT_384			NUM_TAPS_HILB_FILT_10_384
# define HILB_FILT_BNDWIDTH				HILB_FILT_BNDWIDTH_10
#else
# define GRDCRR_DEC_FACT				8
# define NUM_TAPS_HILB_FILT_24			NUM_TAPS_HILB_FILT_5_24
# define NUM_TAPS_HILB_FILT_48			NUM_TAPS_HILB_FILT_5_48
# define NUM_TAPS_HILB_FILT_96			NUM_TAPS_HILB_FILT_5_96
# define NUM_TAPS_HILB_FILT_192			NUM_TAPS_HILB_FILT_5_192
# define NUM_TAPS_HILB_FILT_384			NUM_TAPS_HILB_FILT_5_384
# define HILB_FILT_BNDWIDTH				HILB_FILT_BNDWIDTH_5
#endif

#ifdef USE_FRQOFFS_TRACK_GUARDCORR
/* Time constant for IIR averaging of frequency offset estimation */
# define TICONST_FREQ_OFF_EST_GUCORR	((CReal) 1.0) /* sec */
#endif


/* Classes ********************************************************************/
class CTimeSync : public CReceiverModul<_COMPLEX, _COMPLEX>
{
public:
	CTimeSync();
	virtual ~CTimeSync() {}

	/* To set the module up for synchronized DRM input data stream */
	void SetSyncInput(const bool bNewS) {bSyncInput = bNewS;}

	void StartAcquisition();
	void StopTimingAcqu() {bTimingAcqu = false;}
	void StopRMDetAcqu() {bRobModAcqu = false;}

protected:
	int							iSampleRate;
	int							iGrdcrrDecFact;
	int							iNumTapsHilbFilt;
	const float*				fHilLPProt;
	int							iCorrCounter;
	int							iAveCorr;
	int							iStepSizeGuardCorr;

	CShiftRegister<_COMPLEX>	HistoryBuf;
	CShiftRegister<_COMPLEX>	HistoryBufCorr;
	CShiftRegister<_REAL>		pMaxDetBuffer;
	CRealVector					vecrHistoryFilt;
	CMovingAv<CReal>			vecrGuardEnMovAv;

	CRealVector					vecCorrAvBuf;
	int							iCorrAvInd;

	int							iMaxDetBufSize;
	int							iCenterOfMaxDetBuf;

	int							iMovAvBufSize;
	int							iTotalBufferSize;
	int							iSymbolBlockSize;
	int							iDecSymBS;
	int							iGuardSize;
	int							iTimeSyncPos;
	int							iDFTSize;
	CReal						rStartIndex;

	int							iCenterOfBuf;

	bool					bSyncInput;

	bool					bInitTimingAcqu;
	bool					bTimingAcqu;
	bool					bRobModAcqu;
	bool					bAcqWasActive;

	int							iTiSyncInitCnt;
	int							iRobModInitCnt;

	int							iSelectedMode;

	CComplexVector				cvecZ;
	CComplexVector				cvecB;
	CVector<_COMPLEX>			cvecOutTmpInterm;

	CReal						rLambdaCoAv;


	/* Intermediate correlation results and robustness mode detection */
	CComplexVector				veccIntermCorrRes[NUM_ROBUSTNESS_MODES];
	CRealVector					vecrIntermPowRes[NUM_ROBUSTNESS_MODES];
	CVector<int>				iLengthIntermCRes;
	CVector<int>				iPosInIntermCResBuf;
	CVector<int>				iLengthOverlap;
	CVector<int>				iLenUsefPart;
	CVector<int>				iLenGuardInt;

	CComplexVector				cGuardCorr;
	CComplexVector				cGuardCorrBlock;
	CRealVector					rGuardPow;
	CRealVector					rGuardPowBlock;

	CRealVector					vecrRMCorrBuffer[NUM_ROBUSTNESS_MODES];
	CRealVector					vecrCos[NUM_ROBUSTNESS_MODES];
	int							iRMCorrBufSize;

#ifdef USE_FRQOFFS_TRACK_GUARDCORR
	CComplex					cFreqOffAv;
	CReal						rLamFreqOff;
	CReal						rNormConstFOE;
#endif

	int			GetIndFromRMode(ERobMode eNewMode);
	ERobMode	GetRModeFromInd(int iNewInd);
	void		SetFilterTaps(CReal rNewOffsetNorm);

	virtual void InitInternal(CParameter& Parameters);
	virtual void ProcessDataInternal(CParameter& Parameters);
};


#endif // !defined(TIMESYNC_H__3B0BEVJBN872345NBEROUEBGF4344_BB27912__INCLUDED_)
