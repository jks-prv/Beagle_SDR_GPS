/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2001
 *
 * Author(s):
 *	Volker Fischer
 *
 * Description:
 *	See TimeWiener.cpp
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

#if !defined(TIMEWIENER_LINEAR_H__3B0BA660_C345345_4344_D31912__INCLUDED_)
#define TIMEWIENER_LINEAR_H__3B0BA660_C345345_4344_D31912__INCLUDED_

#include "../GlobalDefinitions.h"
#include "../Parameter.h"
#include "../util/Vector.h"
#include "../OFDMcellmapping/OFDMCellMapping.h"
#include "../matlib/Matlib.h"
#include "ChanEstTime.h"
#ifdef USE_DD_WIENER_FILT_TIME
# include "../tables/TableQAMMapping.h"
#endif


/* Definitions ****************************************************************/
/* Number of taps we want to use for sigma estimation */
#define NUM_TAPS_USED4SIGMA_EST			3

/* Lengths of wiener filter for wiener filtering in time direction */
#define LEN_WIENER_FILT_TIME_RMA		5
#define LEN_WIENER_FILT_TIME_RMB		7
#define LEN_WIENER_FILT_TIME_RMC		9
#define LEN_WIENER_FILT_TIME_RMD		9

/* Maximum values for doppler for a specific robustness mode.
   Parameters found by looking at resulting filter coefficients. The values
   "rSigma" are set to the maximum possible doppler frequency which can be
   interpolated by the pilot frequency grid. Since we have a Gaussian
   power spectral density, the power is never exactely zero. Therefore we
   determine the point where the PDS has fallen below a 50 dB limit */
#define MAX_SIGMA_RMA					((_REAL) 1.6 /* Hz */ / 2)
#define MAX_SIGMA_RMB					((_REAL) 2.7 /* Hz */ / 2)
#define MAX_SIGMA_RMC					((_REAL) 5.7 /* Hz */ / 2)
#define MAX_SIGMA_RMD					((_REAL) 4.5 /* Hz */ / 2)

/* Define a lower bound for the doppler */
#define LOW_BOUND_SIGMA					((_REAL) 0.1 /* Hz */ / 2)

/* Initial value for SNR */
#define INIT_VALUE_SNR_WIEN_TIME_DB		((_REAL) 25.0) /* dB */

/* Time constant for IIR averaging of time correlation estimation */
#define TICONST_TI_CORREL_EST			((CReal) 60.0) /* sec */

/* Overestimation factor for sigma estimation.
   We overestimate the sigma since the channel estimation result is much worse
   if we use a sigma which is small than the real one compared to a value
   which is bigger than the real one. Since we can only estimate on sigma for
   all paths, it can happen that a path with a small gain but a high doppler
   does not contribute enough on the global sigma estimation. Therefore the
   overestimation */
#define SIGMA_OVERESTIMATION_FACT		((_REAL) 3.0)


/* Classes ********************************************************************/
class CTimeWiener : public CChanEstTime
{
public:
	CTimeWiener() : bTracking(false) {}
	virtual ~CTimeWiener() {}

	virtual int Init(CParameter& Parameters);
	virtual _REAL Estimate(CVectorEx<_COMPLEX>* pvecInputData,
						   CComplexVector& veccOutputData,
						   CVector<int>& veciMapTab,
						   CVector<_COMPLEX>& veccPilotCells, _REAL rSNR);

	_REAL GetSigma() {return rSigma * 2;}

	void StartTracking() {bTracking = true;}
	void StopTracking() {bTracking = false;}
	
protected:
	CReal TimeOptimalFilter(CRealVector& vecrTaps, const int iTimeInt,
							const int iDiff, const CReal rNewSNR,
							const CReal rNewSigma, const CReal rTs,
							const int iLength);
	void GenFiltPhaseTable(const CMatrix<int>& matiMapTab, const int iNumCarrier,
						   const int iNumSymPerFrame,
						   const int iScatPilTimeInt);
	_REAL UpdateFilterCoef(const _REAL rNewSNR, const _REAL rNewSigma);
	CReal ModLinRegr(const CComplexVector& veccCorrEst);


#ifdef USE_DD_WIENER_FILT_TIME
	/* Decision directed Wiener */
	class CDDPilIdx
	{
	public:
		CDDPilIdx() : iIdx(0), bIsPilot(false) {}
		CDDPilIdx(int iNI, bool bNP) : iIdx(iNI), bIsPilot(bNP) {}
		int			iIdx;
		bool	bIsPilot;
	};

	CReal TimeOptimalFiltDD(CRealVector& vecrTaps, const int iTimeInt,
							const int iDiff, const CReal rNewSNR,
							const CReal rNewSigma, const CReal rTs,
							const int iLength, const int iFiltPhase);

	CMatrix<_COMPLEX>				matcChanAtDataPos;
	int								iLenDDHist;
	CParameter::ECodScheme			eMSCQAMMode, eSDCQAMMode;
	CVector<CVector<CDDPilIdx> >	vecvecPilIdx;
	CVector<CFIFO<CComplex> >		matcRecSigHist;
	CVector<CFIFO<int> >			matiMapTabHist;
#endif


	CMatrix<int>		matiFiltPhaseTable;

	int					iNumCarrier;

	int					iLengthWiener;
	int					iNumFiltPhasTi;
	CRealMatrix			matrFiltTime;
	
	CMatrix<_COMPLEX>	matcChanAtPilPos;

	CComplexVector		veccTiCorrEst;
	CReal				rLamTiCorrAv;

	int					iScatPilFreqInt; /* Frequency interpolation */
	int					iScatPilTimeInt; /* Time interpolation */
	int					iNumSymPerFrame;

	int					iLenHistBuff;

	CShiftRegister<int>	vecTiCorrHist;
	int					iLenTiCorrHist;
	int					iNumTapsSigEst;
	int					iUpCntWienFilt;

	_REAL				rTs;
	_REAL				rSigma;
	_REAL				rSigmaMax;

	_REAL				rMMSE;
	_REAL				rAvSNR;
	int					iAvSNRCnt;

	bool			bTracking;
};


#endif // !defined(TIMEWIENER_LINEAR_H__3B0BA660_C345345_4344_D31912__INCLUDED_)
