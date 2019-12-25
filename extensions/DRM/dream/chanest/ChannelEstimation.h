/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2001
 *
 * Author(s):
 *	Volker Fischer
 *
 * Description:
 *	See ChannelEstimation.cpp
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

#if !defined(CHANEST_H__3B0BA660_CA63_4344_BB2B_23E7A0D31912__INCLUDED_)
#define CHANEST_H__3B0BA660_CA63_4344_BB2B_23E7A0D31912__INCLUDED_

#include "../Parameter.h"
#include "../util/Modul.h"
#include "../OFDMcellmapping/OFDMCellMapping.h"
#include "../OFDMcellmapping/CellMappingTable.h"
#include "../tables/TableQAMMapping.h"
#include "../matlib/Matlib.h"
#include "../sync/TimeSyncTrack.h"
#include "TimeLinear.h"
#include "TimeWiener.h"


/* Definitions ****************************************************************/
#define LEN_WIENER_FILT_FREQ_RMA		6
#define LEN_WIENER_FILT_FREQ_RMB		11
#define LEN_WIENER_FILT_FREQ_RMC		11
#define LEN_WIENER_FILT_FREQ_RMD		13

/* Time constant for IIR averaging of fast signal power estimation */
#define TICONST_SNREST_FAST				((CReal) 30.0) /* sec */

/* Time constant for IIR averaging of slow signal power estimation */
#define TICONST_SNREST_SLOW				((CReal) 100.0) /* sec */

/* Time constant for IIR averaging of MSC signal / noise power estimation */
#define TICONST_SNREST_MSC				((CReal) 1.0) /* sec */

/* Initial value for SNR */
#define INIT_VALUE_SNR_WIEN_FREQ_DB		((_REAL) 30.0) /* dB */

/* SNR estimation initial SNR value */
#define INIT_VALUE_SNR_ESTIM_DB			((_REAL) 20.0) /* dB */

/* Wrap around bound for calculation of group delay. It is wraped by the 2 pi
   periodicity of the angle() function */
#define WRAP_AROUND_BOUND_GRP_DLY		((_REAL) 4.0)

/* Set length of history for delay values for minimum search. Since the
   delay estimation is optimized for channel estimation performance and
   therefore the delay is usually estimated as too long, it is better for the
   log file to use the minimum value in a certain time period for a good
   estimate of the true delay */
#define LEN_HIST_DELAY_LOG_FILE_S		((CReal) 1.0) /* sec */

/* max frame len for FAC SNR estimates for each symbol of frame */
#define MAX_NUM_SYM_PER_FRAME			RMD_NUM_SYM_PER_FRAME

/* Classes ********************************************************************/
class CChannelEstimation : public CReceiverModul<_COMPLEX, CEquSig>
{
public:
    CChannelEstimation() : eDFTWindowingMethod(DFT_WIN_HAMM),
        TypeIntFreq(FWIENER), TypeIntTime(TWIENER),
        TypeSNREst(SNR_FAC), iLenHistBuff(0),
        rNoiseEst(0.0), rSignalEst(0.0),
        rNoiseEstWMMAcc(0.0), rSignalEstWMMAcc(0.0), rNoiseEstWMFAcc(0.0),
        rSignalEstWMFAcc(0.0), rNoiseEstMERAcc(0.0),iCountMERAcc(0),
        bInterfConsid(false) {}

    virtual ~CChannelEstimation() {}

    void GetTransferFunction(CVector<_REAL>& vecrData,
                             CVector<_REAL>& vecrGrpDly,	CVector<_REAL>& vecrScale);
    void GetAvPoDeSp(CVector<_REAL>& vecrData, CVector<_REAL>& vecrScale,
                     _REAL& rLowerBound, _REAL& rHigherBound,
                     _REAL& rStartGuard, _REAL& rEndGuard, _REAL& rPDSBegin,
                     _REAL& rPDSEnd);
    void GetSNRProfile(CVector<_REAL>& vecrData, CVector<_REAL>& vecrScale);

    CTimeLinear* GetTimeLinear() {
        return &TimeLinear;
    }
    CTimeWiener* GetTimeWiener() {
        return &TimeWiener;
    }
    CTimeSyncTrack* GetTimeSyncTrack() {
        return &TimeSyncTrack;
    }

    /* Set (get) frequency and time interpolation algorithm */
    void SetFreqInt(ETypeIntFreq eNewTy) {
        TypeIntFreq = eNewTy;
    }
    ETypeIntFreq GetFrequencyInterpolationAlgorithm() {
        return TypeIntFreq;
    }
    void SetTimeInt(ETypeIntTime eNewTy) {
        TypeIntTime = eNewTy;
        SetInitFlag();
    }
    ETypeIntTime GetTimeInterpolationAlgorithm() const {
        return TypeIntTime;
    }

    void SetIntCons(const bool bNewIntCons) {
        bInterfConsid = bNewIntCons;
    }
    bool GetIntCons() {
        return bInterfConsid;
    }


    /* Which SNR estimation algorithm */
    void SetSNREst(ETypeSNREst eNewTy) {
        TypeSNREst = eNewTy;
        SetInitFlag();
    }
    ETypeSNREst GetSNREst() {
        return TypeSNREst;
    }

    void StartSaRaOffAcq() {
        TimeSyncTrack.StartSaRaOffAcq();
        SetInitFlag();
    }

protected:
    enum EDFTWinType {DFT_WIN_RECT, DFT_WIN_HAMM, DFT_WIN_HANN};
    EDFTWinType				eDFTWindowingMethod;

    int					iSampleRate;
    int					iNumSymPerFrame;

    CChanEstTime*			pTimeInt;

    CTimeLinear				TimeLinear;
    CTimeWiener				TimeWiener;

    CTimeSyncTrack			TimeSyncTrack;

    ETypeIntFreq			TypeIntFreq;
    ETypeIntTime			TypeIntTime;
    ETypeSNREst				TypeSNREst;

    int						iNumCarrier;

    CMatrix<_COMPLEX>		matcHistory;

    int						iLenHistBuff;

    int						iScatPilFreqInt; /* Frequency interpolation */
    int						iScatPilTimeInt; /* Time interpolation */

    CComplexVector			veccChanEst;
    CRealVector				vecrSqMagChanEst;

    int						iFFTSizeN;

    CReal					rGuardSizeFFT;

    CRealVector				vecrDFTWindow;
    CRealVector				vecrDFTwindowInv;

    int						iLongLenFreq;
    CComplexVector			veccPilots;
    CComplexVector			veccIntPil;
    CFftPlans				FftPlanShort;
    CFftPlans				FftPlanLong;

    int						iNumIntpFreqPil;

    CReal					rLamSNREstFast;
    CReal					rLamSNREstSlow;
    CReal					rLamMSCSNREst;

    _REAL					rNoiseEst;
    _REAL					rNoiseEstMSCMER;
    _REAL					rSignalEst;
    CVector<_REAL>			vecrNoiseEstMSC;
    CVector<_REAL>			vecrSigEstMSC;
    _REAL					rSNREstimate;
    _REAL					rNoiseEstSum;
    _REAL					rSignalEstSum;
    CRealVector				vecrNoiseEstFACSym;
    CRealVector				vecrSignalEstFACSym;
    _REAL					rSNRChanEstCorrFact;
    _REAL					rSNRFACSigCorrFact;
    _REAL					rSNRTotToPilCorrFact;
    _REAL					rSNRSysToNomBWCorrFact;

    /* OPH: Accumulators for calculating the RSCI MER, WMF, and WMM (these are averages, not filtered values) */
    _REAL					rNoiseEstWMMAcc;
    _REAL					rSignalEstWMMAcc;
    _REAL					rNoiseEstWMFAcc;
    _REAL					rSignalEstWMFAcc;
    _REAL					rNoiseEstMERAcc;
    int						iCountMERAcc;

    bool				bInterfConsid;

    /* Needed for GetDelay() */
    _REAL					rLenPDSEst;
    CShiftRegister<CReal>	vecrDelayHist;
    int						iLenDelayHist;

    int						iStartZeroPadding;

    int						iInitCnt;
    int						iSNREstIniSigAvCnt;
    int						iSNREstIniNoiseAvCnt;
    int						iSNREstInitCnt;
    bool				bSNRInitPhase;
    _REAL CalAndBoundSNR(const _REAL rSignalEst, const _REAL rNoiseEst);

    /* OPH: RSCI interference tag calculation */
    void CalculateRint(CParameter& Parameters);
    void UpdateRSIPilotStore(CParameter& Parameters, CVectorEx<_COMPLEX>* pvecInputData,
                             CVector<int>& veciMapTab, CVector<_COMPLEX>& veccPilotCells, const int iSymbolCounter);

    CMatrix<_COMPLEX>	matcRSIPilotStore;
    int iTimeDiffAccuRSI; /* Accumulator for time differences for RSI pilot output */

    /* Wiener interpolation in frequency direction */
    void UpdateWienerFiltCoef(CReal rNewSNR, CReal rRatPDSLen,
                              CReal rRatPDSOffs);

    CComplexVector FreqOptimalFilter(int iFreqInt, int iDiff, CReal rSNR,
                                     CReal rRatPDSLen, CReal rRatPDSOffs,
                                     int iLength);
    CMatrix<_COMPLEX>		matcFiltFreq;
    int						iLengthWiener;
    CVector<int>			veciPilOffTab;

    int						iDCPos;
    int						iPilOffset;
    int						iNumWienerFilt;
    CComplexMatrix			matcWienerFilter;

#ifdef USE_DD_WIENER_FILT_TIME
    int						iCurrentFrameID;
#endif

    virtual void InitInternal(CParameter& Parameters);
    virtual void ProcessDataInternal(CParameter& Parameters);
};


#endif // !defined(CHANEST_H__3B0BA660_CA63_4344_BB2B_23E7A0D31912__INCLUDED_)
