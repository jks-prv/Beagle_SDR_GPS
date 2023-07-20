/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2001
 *
 * Author(s):
 *	Volker Fischer
 *
 * Description:
 *	See AMDemodulation.cpp
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

#if !defined(AMDEMOD_H__3B0BEVJBN8LKH2934BGF4344_BB27912__INCLUDED_)
#define AMDEMOD_H__3B0BEVJBN8LKH2934BGF4344_BB27912__INCLUDED_

#include "Parameter.h"
#include "util/Modul.h"
#include "util/Vector.h"
#include "matlib/MatlibSigProToolbox.h"
#include "resample/AudioResample.h"
#ifdef HAVE_SPEEX
# include <speex/speex_preprocess.h>
#endif


/* Definitions ****************************************************************/
/* Set value for desired amplitude for AM signal, controlled by the AGC. Maximum
   value is the range for short variables (16 bit) -> 32768 */
#define DES_AV_AMPL_AM_SIGNAL				((CReal) 8000.0)

/* Lower bound for estimated average amplitude. That is needed, since we
   devide by this estimate so it must not be zero */
#define LOWER_BOUND_AMP_LEVEL				((CReal) 10.0)

/* Amplitude correction factor for demodulation */
#define AM_AMPL_CORR_FACTOR					((CReal) 5.0)

/* Lambda for IIR filter for DC-filter */
#define DC_IIR_FILTER_LAMBDA				((CReal) 0.999)

/* Margin at the DC frequency in case of SSB demodulation */
#define SSB_DC_MARGIN_HZ					((CReal) 100.0) /* Hz */

/* Frequency offset for CW demodulation */
#define FREQ_OFFS_CW_DEMOD					((CReal) 1000.0) /* Hz */


/* Parameters for frequency offset estimation algorithm --------------------- */
/* Set the number of blocks used for carrier frequency acquisition */
#define NUM_BLOCKS_CARR_ACQUISITION			20

/* Percentage of aquisition search window half size relative to the useful
   spectrum bandwidth */
#define PERC_SEARCH_WIN_HALF_SIZE			((CReal) 5.0 /* % */ / 100)


/* Parameters for noise reduction algorithm --------------------------------- */
/* Length of minimum statistic estimation history */
#define MIN_STAT_HIST_LENGTH_SEC			((CReal) 1.5) /* sec */

/* Minimum statistic weightning factors. With this value the degree of noise
   reduction can be adjusted. We use three settings here */
#define MIN_STAT_WEIGHT_FACTOR_LOW			((CReal) 0.4)
#define MIN_STAT_WEIGHT_FACTOR_MED			((CReal) 1.0)
#define MIN_STAT_WEIGHT_FACTOR_HIGH			((CReal) 2.0)

/* Time constant for IIR averaging of PSD estimation */
#define TICONST_PSD_EST_SIG_NOISE_RED		((CReal) 1.0) /* sec */


/* Parameters for PLL ------------------------------------------------------- */
#define PLL_LOOP_GAIN						((CReal) 0.0000001)

/* Lambda for IIR filter for loop filter */
#define PLL_LOOP_FILTER_LAMBDA				((CReal) 0.99)


/* Classes ********************************************************************/
/* Noise reduction ---------------------------------------------------------- */
class CNoiseReduction
{
public:
    CNoiseReduction() : eNoiRedDegree(NR_MEDIUM)
#ifdef HAVE_SPEEX
    , preprocess_state(nullptr), speex_data(nullptr)
    , supression_level(0), sample_rate(0)
#endif
    {}
    virtual ~CNoiseReduction();

    enum ENoiRedDegree {NR_LOW=1, NR_MEDIUM=2, NR_HIGH=3};

    void Init(int iSampleRate, int iNewBlockLen);
    void Process(CRealVector& vecrIn /* in/out */);
    void SetNoiRedDegree(const ENoiRedDegree eNND);

protected:
    void UpdateNoiseEst(CRealVector& vecrNoisePSD,
                        const CRealVector& vecrSqMagSigFreq, const ENoiRedDegree eNoiRedDegree);
    CRealVector OptimalFilter(const CComplexVector& vecrSigFreq,
                              const CRealVector& vecrSqMagSigFreq, const CRealVector& vecrNoisePSD);

    int				iBlockLen;
    int				iHalfBlockLen;
    int				iBlockLenLong;
    int				iFreqBlLen;
    int				iMinStatHistLen;
    CReal			rLamPSD;
    CRealMatrix		matrMinimumStatHist;
    CComplexVector	veccSigFreq;
    CRealVector		vecrSqMagSigFreq;
    CRealVector		vecrSigPSD;
    CRealVector		vecrNoisePSD;
    CFftPlans		FftPlan;
    CComplexVector	veccOptFilt;
    CRealVector		vecrOldSignal;
    CRealVector		vecrVeryOldSignal;
    CRealVector		vecrLongSignal;
    CRealVector		vecrOldOutSignal;
    CRealVector		vecrOutSig1;
    CRealVector		vecrTriangWin;
    CRealVector		vecrOptFiltTime;
    CRealVector		vecrFiltResult;

    ENoiRedDegree	eNoiRedDegree;
#ifdef HAVE_SPEEX
    SpeexPreprocessState *preprocess_state;
    spx_int16_t*	speex_data;
    spx_int32_t		supression_level;
    int				sample_rate;
#endif
};


/* Frequency offset acquisition --------------------------------------------- */
class CFreqOffsAcq
{
public:
    CFreqOffsAcq() : bAcquisition(false), rCurNormFreqOffset((CReal) 0.0) {}
    void Init(const int iNewBlockSize);
    bool Run(const CVector<_REAL>& vecrInpData);

    void Start(const CReal rNewNormCenter);
    CReal GetCurResult() {
        return rCurNormFreqOffset;
    }

protected:
    int						iBlockSize;
    CFftPlans				FftPlanAcq;
    CRealVector				vecrFFTInput;
    int						iTotalBufferSize;
    int						iHalfBuffer;
    int						iAquisitionCounter;
    CShiftRegister<_REAL>	vecrFFTHistory;
    CRealVector				vecrPSD;
    int						iSearchWinStart;
    int						iSearchWinEnd;
    bool				bAcquisition;

    CReal					rNormCenter;
    CReal					rCurNormFreqOffset;
};


/* Automatic gain control --------------------------------------------------- */
class CAGC
{
public:

    CAGC() : eType(AT_MEDIUM) {}
    void Init(int iSampleRate, int iNewBlockSize);
    void Process(CRealVector& vecrIn /* in/out */);

    void SetType(const EAmAgcType eNewType);
    EAmAgcType GetType() {
        return eType;
    }

protected:
    int		iSampleRate;
    int		iBlockSize;
    EAmAgcType	eType;
    CReal	rAttack, rDecay;
    CReal	rAvAmplEst;
};


/* Mixer -------------------------------------------------------------------- */
class CMixer
{
public:
    CMixer() : cCurExp((CReal) 1.0), cExpStep((CReal) 1.0) {}
    void Init(const int iNewBlockSize);
    void Process(CComplexVector& veccIn /* in/out */);

    void SetMixFreq(const CReal rNewNormMixFreq);

protected:
    int			iBlockSize;
    CComplex	cCurExp, cExpStep;
};


/* Phase lock loop ---------------------------------------------------------- */
class CPLL
{
public:
    CPLL() : rCurPhase((CReal) 0.0) {}
    void Init(const int iNewBlockSize);
    void Process(CRealVector& vecrIn /* in/out */);

    void SetRefNormFreq(const CReal rNewNormFreq);
    CReal GetCurPhase() {
        return rCurPhase;
    }
    CReal GetCurNormFreqOffs()
    {
        return rNormCurFreqOffset + rNormCurFreqOffsetAdd;
    }

protected:
    CMixer			Mixer;
    int				iBlockSize;
    CRealVector		rvecRealTmp;
    CRealVector		rvecImagTmp;
    CComplexVector	cvecLow;
    CReal			rNormCurFreqOffset;
    CReal			rNormCurFreqOffsetAdd;
    CReal			rCurPhase;

    CRealVector		rvecZReal;
    CRealVector		rvecZImag;
    CRealVector		rvecA;
    CRealVector		rvecB;
};

/* AM demodulation module --------------------------------------------------- */
class CAMDemodulation : public CReceiverModul<_REAL, _SAMPLE>
{
public:
    CAMDemodulation();
    virtual ~CAMDemodulation() {}

    void SetAcqFreq(const CReal rNewNormCenter);

    void EnableAutoFreqAcq(const bool bNewEn)
    {
        bAutoFreqAcquIsEnabled = bNewEn;
    }
    bool AutoFreqAcqEnabled() {
        return bAutoFreqAcquIsEnabled;
    }

    void EnablePLL(const bool bNewEn) {
        bPLLIsEnabled = bNewEn;
    }
    bool PLLEnabled() {
        return bPLLIsEnabled;
    }

    void SetDemodType(const EDemodType eNewType);
    EDemodType GetDemodType() {
        return eDemodType;
    }

    void SetFilterBW(const int iNewBW);
    int GetFilterBW() {
        return (int) (rBPNormBW * iSigSampleRate);
    }

    void SetAGCType(const EAmAgcType eNewType);
    EAmAgcType GetAGCType() {
        return AGC.GetType();
    }

    void SetNoiRedType(const ENoiRedType eNewType);
    ENoiRedType GetNoiRedType() {
        return eNoiRedType;
    }

    void SetNoiRedLevel(const int iNewLevel);
    int GetNoiRedLevel() {
        return iNoiRedLevel;
    }

    bool haveSpeex() {
#ifdef HAVE_SPEEX
        return true;
#else
        return false;
#endif
    }

    void GetBWParameters(CReal& rCenterFreq, CReal& rBW)
    {
        rCenterFreq = rBPNormCentOffsTot;
        rBW = rBPNormBW;
    }

    bool GetPLLPhase(CReal& rPhaseOut);
    CReal GetCurMixFreqOffs() const
    {
        return rNormCurMixFreqOffs * iSigSampleRate;
    }

    bool GetFrameBoundary() {
        return iFreeSymbolCounter==0;
    }


protected:
    void SetBPFilter(const CReal rNewBPNormBW, const CReal rNewNormFreqOffset,
                     const EDemodType eDemodType);
    void SetNormCurMixFreqOffs(const CReal rNewNormCurMixFreqOffs);

    CComplexVector				cvecBReal;
    CComplexVector				cvecBImag;
    CRealVector					rvecZReal;
    CRealVector					rvecZImag;
    CComplexVector				cvecBAMAfterDem;
    CRealVector					rvecZAMAfterDem;

    CRealVector					rvecInpTmp;
    CComplexVector				cvecHilbert;
    int							iHilFiltBlLen;
    CFftPlans					FftPlansHilFilt;

    CReal						rBPNormBW;
    CReal						rNormCurMixFreqOffs;
    CReal						rBPNormCentOffsTot;

    CRealVector					rvecZAM;
    CRealVector					rvecADC;
    CRealVector					rvecBDC;
    CRealVector					rvecZFM;
    CRealVector					rvecAFM;
    CRealVector					rvecBFM;

    int							iSymbolBlockSize;

    bool					bPLLIsEnabled;
    bool					bAutoFreqAcquIsEnabled;

    EDemodType					eDemodType;

    CComplex					cOldVal;

    CVector<_REAL>				vecTempResBufIn;
    CVector<_REAL>				vecTempResBufOut;

    ENoiRedType					eNoiRedType;
    int							iNoiRedLevel;

    /* Objects */
    CPLL						PLL;
    CMixer						Mixer;
    CFreqOffsAcq				FreqOffsAcq;
    CAGC						AGC;
    CNoiseReduction				NoiseReduction;
    CAudioResample				ResampleObj;

    /* OPH: counter to count symbols within a frame in order to generate */
    /* RSCI output */
    int							iFreeSymbolCounter;
    int							iAudSampleRate;
    int							iSigSampleRate;
    int							iBandwidth;
    int							iResOutBlockSize;


    virtual void InitInternal(CParameter& Parameters);
    virtual void ProcessDataInternal(CParameter& Parameters);
};


#endif // !defined(AMDEMOD_H__3B0BEVJBN8LKH2934BGF4344_BB27912__INCLUDED_)
