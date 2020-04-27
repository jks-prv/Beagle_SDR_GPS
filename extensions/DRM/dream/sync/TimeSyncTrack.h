/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2001-2006
 *
 * Author(s):
 *	Volker Fischer
 *
 * Description:
 *	See TimeSyncTrack.cpp
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

#if !defined(TIMESYNCTRACK_H__3B0BA6346234634554344_BBE7A0D31912__INCLUDED_)
#define TIMESYNCTRACK_H__3B0BA6346234634554344_BBE7A0D31912__INCLUDED_

#include "../GlobalDefinitions.h"
#include "../Parameter.h"
#include "../util/Vector.h"
#include "../matlib/MatlibSigProToolbox.h"


/* Definitions ****************************************************************/
/* Define target position for first path in guard-interval. The number defines
   the fraction of the guard-interval */
#define TARGET_TI_POS_FRAC_GUARD_INT		9

/* Weights for bound calculation. First parameter is for peak distance and 
   second for distance from minimum value */
#define TETA1_DIST_FROM_MAX_DB				20
#define TETA2_DIST_FROM_MIN_DB				23
#define TETA1_DIST_FROM_MAX_DB_RMD			20 /* Robustness mode D */
#define TETA2_DIST_FROM_MIN_DB_RMD			15 /* Robustness mode D */

/* Control parameters */
#define CONT_PROP_IN_GUARD_INT				((_REAL) 0.06)
#define CONT_PROP_BEFORE_GUARD_INT			((_REAL) 0.08)
#define CONT_PROP_ENERGY_METHOD				((_REAL) 0.02)

/* Time constant for IIR averaging of PDS estimation */
#define TICONST_PDS_EST_TISYNC				((CReal) 0.25) /* sec */

/* Minimum statistic used for estimation of the noise variance where the samples
   with the lowest energy are taken as an estimate for the noise. Since the
   actual energy is most certainly higher than the minimum, we need to
   overestimate the result. Specify the number of samples for minimum search
   and the overestimation factor */
#define NUM_SAM_IR_FOR_MIN_STAT				10
#define OVER_EST_FACT_MIN_STAT				((CReal) 4.0)

/* Parameter of controlling the closed loop for sample rate offset */
#define CONTR_SAMP_OFF_INT_FTI				((_REAL) 0.001)

/* Length of history for sample rate offset estimation using time corrections
   in seconds */
#define HIST_LEN_SAM_OFF_EST_TI_CORR		((CReal) 30.0) /* sec */

/* Length of history used for sample rate offset acquisition estimate */
#define SAM_OFF_EST_TI_CORR_ACQ_LEN			((CReal) 4.0) /* sec */


/* Classes ********************************************************************/
class CTimeSyncTrack
{
public:
	CTimeSyncTrack() : bTiSyncTracking(false), 
		bSamRaOffsAcqu(true), TypeTiSyncTrac(TSENERGY) {}
	virtual ~CTimeSyncTrack() {}

	void Init(CParameter& Parameter, int iNewSymbDelay);

	void Process(CParameter& Parameter, CComplexVector& veccChanEst,
				 int iNewTiCorr, _REAL& rLenPDS, _REAL& rOffsPDS);

	void GetAvPoDeSp(CVector<_REAL>& vecrData, CVector<_REAL>& vecrScale, 
					 _REAL& rLowerBound, _REAL& rHigherBound,
					 _REAL& rStartGuard, _REAL& rEndGuard, _REAL& rPDSBegin,
					 _REAL& rPDSEnd, int iSampleRate);

	void StartTracking() {bTiSyncTracking = true;}
	void StopTracking() {bTiSyncTracking = false;}

	 /* SetInitFlag() is needed for this function. Is done in channel estimation
	    module */
	void StartSaRaOffAcq() {bSamRaOffsAcqu = true;}

	void SetTiSyncTracType(ETypeTiSyncTrac eNewTy);
	ETypeTiSyncTrac GetTiSyncTracType() {return TypeTiSyncTrac;}
 
	/* OPH: calculation of delay and doppler using RSCI method */
	void CalculateRdel(CParameter& Parameter);
	CRealVector& GetRdelThresholds() {return vecrRdelThresholds;}
	void CalculateRdop(CParameter& Parameter);


protected:
	CComplexVector			veccPilots;
	int						iNumIntpFreqPil;
	CFftPlans				FftPlan;
	int						iScatPilFreqInt;
	int						iNumCarrier;
	CRealVector				vecrAvPoDeSp;
	CReal					rLamAvPDS;

	CRealVector				vecrHammingWindow;
	CReal					rConst1;
	CReal					rConst2;
	int						iStPoRot;
	CRealVector				vecrAvPoDeSpRot;
	int						iSymDelay;
	CShiftRegister<int>		vecTiCorrHist;
	CShiftRegister<int>		veciNewMeasHist;
	
	CReal					rFracPartTiCor;
	int						iTargetTimingPos;

	bool				bTiSyncTracking;
	bool				bSamRaOffsAcqu;

	int						iDFTSize;

	CReal					rBoundLower;
	CReal					rBoundHigher;
	CReal					rGuardSizeFFT;

	CReal					rEstPDSEnd; /* Estimated end of PSD */
	CReal					rEstPDSBegin; /* Estimated beginning of PSD */

	CReal					rFracPartContr;

	ETypeTiSyncTrac			TypeTiSyncTrac;

	CShiftRegister<int>		veciSRTiCorrHist;
	int						iLenCorrectionHist;
	long int				iIntegTiCorrections;
	CReal					rSymBloSiIRDomain;
	int						iResOffsetAcquCnt;
	int						iResOffAcqCntMax;
	int						iOldNonZeroDiff;

	CReal GetSamOffHz(int iDiff, int iLen, int iSampleRate);

	/* O. Haffenden variables for rdop and rdel calculation */
	CComplexVector			veccOldImpulseResponse;
	CRealVector				vecrRdelThresholds;
	CRealVector				vecrRdelIntervals;
};


#endif // !defined(TIMESYNCTRACK_H__3B0BA6346234634554344_BBE7A0D31912__INCLUDED_)
