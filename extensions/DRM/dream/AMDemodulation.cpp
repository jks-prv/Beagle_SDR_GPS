/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2001-2005
 *
 * Author(s):
 *	Volker Fischer
 *
 * Description:
 *	Implementation of an analog AM demodulation
 *
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

#include "AMDemodulation.h"


/* Implementation *************************************************************/

CAMDemodulation::CAMDemodulation() :
    cvecBReal(), cvecBImag(), rvecZReal(), rvecZImag(),
    cvecBAMAfterDem(), rvecZAMAfterDem(), rvecInpTmp(),
    cvecHilbert(),
    iHilFiltBlLen(0),
    FftPlansHilFilt(),
    rBPNormBW(0.0),
    rNormCurMixFreqOffs((CReal) 0.0),
    rBPNormCentOffsTot(0.0),
    rvecZAM(), rvecADC(), rvecBDC(), rvecZFM(), rvecAFM(), rvecBFM(),
    iSymbolBlockSize(0),
    bPLLIsEnabled(false), bAutoFreqAcquIsEnabled(false), eDemodType(DT_AM),
    cOldVal(), eNoiRedType(NR_OFF), iNoiRedLevel(-12),
    PLL(), Mixer(), FreqOffsAcq(), AGC(), NoiseReduction(),
    iFreeSymbolCounter(0), iAudSampleRate(0), iSigSampleRate(0), iBandwidth(0),
    iResOutBlockSize(0)
{
}

void CAMDemodulation::ProcessDataInternal(CParameter& Parameters)
{
    int i;

    /* OPH: update free-running symbol counter */
    Parameters.Lock();
    iFreeSymbolCounter++;
    if (iFreeSymbolCounter >= Parameters.CellMappingTable.iNumSymPerFrame)
    {
        iFreeSymbolCounter = 0;
    }
    Parameters.Unlock();


    /* Frequency offset estimation if requested */
    if (FreqOffsAcq.Run(*pvecInputData))
        SetNormCurMixFreqOffs(FreqOffsAcq.GetCurResult());


    /* Band-pass filter and mixer ------------------------------------------- */
    /* Copy CVector data in CMatlibVector */
    for (i = 0; i < iInputBlockSize; i++)
        rvecInpTmp[i] = (*pvecInputData)[i];

    /* Cut out a spectrum part of desired bandwidth */
    cvecHilbert = CComplexVector(
                      FftFilt(cvecBReal, rvecInpTmp, rvecZReal, FftPlansHilFilt),
                      FftFilt(cvecBImag, rvecInpTmp, rvecZImag, FftPlansHilFilt));

    /* Mix it down to zero frequency */
    Mixer.Process(cvecHilbert);


    /* Phase lock loop (PLL) ------------------------------------------------ */
    if (bPLLIsEnabled)
    {
        PLL.Process(rvecInpTmp);

        /* Update mixer frequency with tracking result from PLL. Special case
           here since we change the mixer frequency but do not update the
           band-pass filter. We can do this because the frequency changes by the
           PLL are usually small */
        Mixer.SetMixFreq(PLL.GetCurNormFreqOffs());
        rNormCurMixFreqOffs = PLL.GetCurNormFreqOffs(); /* For GUI */
    }


    /* Analog demodulation -------------------------------------------------- */
    /* Actual demodulation. Reuse temp buffer "rvecInpTmp" for output
       signal */
    switch (eDemodType)
    {
    case DT_AM:
        /* Use envelope of signal and apply low-pass filter */
        rvecInpTmp = FftFilt(cvecBAMAfterDem, Abs(cvecHilbert),
                             rvecZAMAfterDem, FftPlansHilFilt);

        /* Apply DC filter (high-pass filter) */
        rvecInpTmp = Filter(rvecBDC, rvecADC, rvecInpTmp, rvecZAM);
        break;

    case DT_LSB:
    case DT_USB:
    case DT_CW:
        /* Make signal real and compensate for removed spectrum part */
        rvecInpTmp = Real(cvecHilbert) * (CReal) 2.0;
        break;

    case DT_FM:
        /* Get phase of complex signal and apply differentiation */
        for (i = 0; i < iInputBlockSize; i++)
        {
            /* Back-rotate new input sample by old value to get
               differentiation operation, get angle of complex signal and
               amplify result */
            rvecInpTmp[i] = Angle(cvecHilbert[i] * Conj(cOldVal)) *
                            _MAXSHORT / ((CReal) 4.0 * crPi);

            /* Store old value */
            cOldVal = cvecHilbert[i];
        }

        /* Low-pass filter */
        rvecInpTmp = Filter(rvecBFM, rvecAFM, rvecInpTmp, rvecZFM);
    }


    /* Noise reduction -------------------------------------------------- */
    if (eNoiRedType != NR_OFF)
        NoiseReduction.Process(rvecInpTmp);


    /* AGC -------------------------------------------------------------- */
    AGC.Process(rvecInpTmp);


    /* Resampling audio ------------------------------------------------- */
    /* Change type of data (short -> real) */
    for (i = 0; i < iSymbolBlockSize; i++)
        vecTempResBufIn[i] = rvecInpTmp[i];

    /* Resample data */
    ResampleObj.Resample(vecTempResBufIn, vecTempResBufOut);

    /* Write mono signal in both channels (left and right) */
    for (i = 0; i < iResOutBlockSize; i++)
        (*pvecOutputData)[2 * i] = (*pvecOutputData)[2 * i + 1] =
            Real2Sample(vecTempResBufOut[i]);
}

void CAMDemodulation::InitInternal(CParameter& Parameters)
{
    /* Get parameters from info class */
    Parameters.Lock();
    iSymbolBlockSize = Parameters.CellMappingTable.iSymbolBlockSize;
    iAudSampleRate = Parameters.GetAudSampleRate();
    iSigSampleRate = Parameters.GetSigSampleRate();
    Parameters.Unlock();

    /* SetFilterBW() can be called before InitInternal() and iSigSampleRate is defined,
       so we need to recalculate the bandwidth here */
    rBPNormBW = (CReal) iBandwidth / iSigSampleRate;

    /* Init temporary vector for filter input and output */
    rvecInpTmp.Init(iSymbolBlockSize);
    cvecHilbert.Init(iSymbolBlockSize);

    /* Init old value needed for differentiation */
    cOldVal = (CReal) 0.0;

    /* Init noise reduction object */
    NoiseReduction.Init(iSigSampleRate, iSymbolBlockSize);

    /* Init frequency offset acquisition (start new acquisition) */
    FreqOffsAcq.Init(iSymbolBlockSize);
    FreqOffsAcq.Start((CReal) 0.0); /* Search entire bandwidth */

    /* Init AGC */
    AGC.Init(iSigSampleRate, iSymbolBlockSize);

    /* Init mixer */
    Mixer.Init(iSymbolBlockSize);

    /* Init PLL */
    PLL.Init(iSymbolBlockSize);

    /* Inits for Hilbert and DC filter -------------------------------------- */
    /* Hilbert filter block length is the same as input block length */
    iHilFiltBlLen = iSymbolBlockSize;

    /* Init state vector for filtering with zeros */
    rvecZReal.Init(iHilFiltBlLen, (CReal) 0.0);
    rvecZImag.Init(iHilFiltBlLen, (CReal) 0.0);
    rvecZAMAfterDem.Init(iHilFiltBlLen, (CReal) 0.0);

    /* "+ 1" because of the Nyquist frequency (filter in frequency domain) */
    cvecBReal.Init(iHilFiltBlLen + 1);
    cvecBImag.Init(iHilFiltBlLen + 1);
    cvecBAMAfterDem.Init(iHilFiltBlLen + 1);

    /* FFT plans are initialized with the long length */
    FftPlansHilFilt.Init(iHilFiltBlLen * 2);

    /* Init DC filter */
    /* IIR filter: H(Z) = (1 - z^{-1}) / (1 - lamDC * z^{-1}) */
    rvecZAM.Init(2, (CReal) 0.0); /* Memory */
    rvecBDC.Init(2);
    rvecADC.Init(2);
    rvecBDC[0] = (CReal) 1.0;
    rvecBDC[1] = (CReal) -1.0;
    rvecADC[0] = (CReal) 1.0;
    rvecADC[1] = -DC_IIR_FILTER_LAMBDA;

    /* Init FM audio filter. This is a butterworth IIR filter with cut-off
       of 3 kHz. It was generated in Matlab with
       [b, a] = butter(4, 3000 / 24000); */
    rvecZFM.Init(5, (CReal) 0.0); /* Memory */
    rvecBFM.Init(5);
    rvecAFM.Init(5);
    rvecBFM[0] = (CReal) 0.00093349861295;
    rvecBFM[1] = (CReal) 0.00373399445182;
    rvecBFM[2] = (CReal) 0.00560099167773;
    rvecBFM[3] = (CReal) 0.00373399445182;
    rvecBFM[4] = (CReal) 0.00093349861295;
    rvecAFM[0] = (CReal) 1.0;
    rvecAFM[1] = (CReal) -2.97684433369673;
    rvecAFM[2] = (CReal) 3.42230952937764;
    rvecAFM[3] = (CReal) -1.78610660021804;
    rvecAFM[4] = (CReal) 0.35557738234441;


    /* Init band-pass filter */
    SetNormCurMixFreqOffs(rNormCurMixFreqOffs);


    /* Define block-sizes for input and output */
    /* The output buffer is a cyclic buffer, we have to specify the total
       buffer size */
    iMaxOutputBlockSize = (int) ((_REAL) iAudSampleRate *
                                 (_REAL) 0.4 /* 400ms */ * 2 /* for stereo */);

    iInputBlockSize = iSymbolBlockSize;

    iResOutBlockSize = (int) ((_REAL) iSymbolBlockSize *
                              iAudSampleRate / iSigSampleRate);

    iOutputBlockSize = 2 * iResOutBlockSize; /* Stereo */


    /* Init audio resampler */
    ResampleObj.Init(iSymbolBlockSize,
        (_REAL) iAudSampleRate / iSigSampleRate);
    vecTempResBufIn.Init(iSymbolBlockSize, (_REAL) 0.0);
    vecTempResBufOut.Init(iResOutBlockSize, (_REAL) 0.0);

    /* OPH: init free-running symbol counter */
    iFreeSymbolCounter = 0;

}

void CAMDemodulation::SetNormCurMixFreqOffs(const CReal rNewNormCurMixFreqOffs)
{
    /* In case CW demodulation is used, set mixer frequency so that the center
       of the band-pass filter is at the selected frequency */
    if (eDemodType == DT_CW)
    {
        rNormCurMixFreqOffs =
            rNewNormCurMixFreqOffs - FREQ_OFFS_CW_DEMOD / iSigSampleRate;
    }
    else
        rNormCurMixFreqOffs = rNewNormCurMixFreqOffs;

    /* Generate filter taps and set mixing frequency */
    SetBPFilter(rBPNormBW, rNormCurMixFreqOffs, eDemodType);
    Mixer.SetMixFreq(rNormCurMixFreqOffs);

    /* Tell the PLL object the new frequency (we do not care here if it is
       enabled or not) */
    PLL.SetRefNormFreq(rNormCurMixFreqOffs);
}

void CAMDemodulation::SetBPFilter(const CReal rNewBPNormBW,
                                  const CReal rNewNormFreqOffset,
                                  const EDemodType eDemodType)
{
    /* Set internal parameter */
    rBPNormBW = rNewBPNormBW;

    /* Actual prototype filter design */
    CRealVector vecrFilter(iHilFiltBlLen);
    vecrFilter = FirLP(rBPNormBW, Nuttallwin(iHilFiltBlLen));

    /* Adjust center of filter for respective demodulation types */
    CReal rBPNormFreqOffset = (CReal) 0.0;
    const CReal rSSBMargin = SSB_DC_MARGIN_HZ / iSigSampleRate;
    switch (eDemodType)
    {
    case DT_AM:
    case DT_FM:
        /* No offset */
        rBPNormFreqOffset = (CReal) 0.0;
        break;

    case DT_LSB:
        /* Shift filter to the left side of the carrier. Add a small offset
          to the filter bandwidht because of the filter slope */
        rBPNormFreqOffset = - rBPNormBW / 2 - rSSBMargin;
        break;

    case DT_USB:
        /* Shift filter to the right side of the carrier. Add a small offset
          to the filter bandwidht because of the filter slope */
        rBPNormFreqOffset = rBPNormBW / 2 + rSSBMargin;
        break;

    case DT_CW:
        /* Shift filter to the right side of the carrier according to the
           special CW demodulation shift */
        rBPNormFreqOffset = FREQ_OFFS_CW_DEMOD / iSigSampleRate;
        break;
    }

    /* Actual band-pass filter offset is the demodulation frequency plus the
       additional offset for the demodulation type */
    rBPNormCentOffsTot = rNewNormFreqOffset + rBPNormFreqOffset;


    /* Set filter coefficients ---------------------------------------------- */
    /* Make sure that the phase in the middle of the filter is always the same
       to avaoid clicks when the filter coefficients are changed */
    const CReal rStartPhase = (CReal) iHilFiltBlLen * crPi * rBPNormCentOffsTot;

    /* Copy actual filter coefficients. It is important to initialize the
       vectors with zeros because we also do a zero-padding */
    CRealVector rvecBReal(2 * iHilFiltBlLen, (CReal) 0.0);
    CRealVector rvecBImag(2 * iHilFiltBlLen, (CReal) 0.0);
    for (int i = 0; i < iHilFiltBlLen; i++)
    {
        rvecBReal[i] = vecrFilter[i] *
                       Cos((CReal) 2.0 * crPi * rBPNormCentOffsTot * i - rStartPhase);

        rvecBImag[i] = vecrFilter[i] *
                       Sin((CReal) 2.0 * crPi * rBPNormCentOffsTot * i - rStartPhase);
    }

    /* Transformation in frequency domain for fft filter */
    cvecBReal = rfft(rvecBReal, FftPlansHilFilt);
    cvecBImag = rfft(rvecBImag, FftPlansHilFilt);

    /* Set filter coefficients for AM filter after demodulation (use same low-
       pass design as for the bandpass filter) */
    CRealVector rvecBAMAfterDem(2 * iHilFiltBlLen, (CReal) 0.0);
    rvecBAMAfterDem.PutIn(1, iHilFiltBlLen, vecrFilter);
    cvecBAMAfterDem = rfft(rvecBAMAfterDem, FftPlansHilFilt);
}


/* Interface functions ------------------------------------------------------ */
void CAMDemodulation::SetAcqFreq(const CReal rNewNormCenter)
{
    /* Lock resources */
    Lock();
    {
        if (bAutoFreqAcquIsEnabled)
            FreqOffsAcq.Start(rNewNormCenter);
        else
            SetNormCurMixFreqOffs(rNewNormCenter / 2);
    }
    Unlock();
}

void CAMDemodulation::SetDemodType(const EDemodType eNewType)
{
    /* Lock resources */
    Lock();
    {
        /* Set internal demodulation type flag */
        eDemodType = eNewType;

        /* Init band-pass filter according to new demodulation method */
        if (iSigSampleRate != 0)
            SetBPFilter(rBPNormBW, rNormCurMixFreqOffs, eDemodType);
    }
    Unlock();
}

void CAMDemodulation::SetFilterBW(const int iNewBW)
{
    /* Lock resources */
    Lock();
    {
        /* Set internal bandwidth value */
        iBandwidth = iNewBW;

        /* Init band-pass filter according to new bandwidth value */
        if (iSigSampleRate != 0)
            SetBPFilter((CReal) iNewBW / iSigSampleRate, rNormCurMixFreqOffs,
                        eDemodType);
    }
    Unlock();
}

void CAMDemodulation::SetAGCType(const EAmAgcType eNewType)
{
    /* Lock resources */
    Lock();
    {
        AGC.SetType(eNewType);
    }
    Unlock();
}

void CAMDemodulation::SetNoiRedType(const ENoiRedType eNewType)
{
    /* Lock resources */
    Lock();
    {
        eNoiRedType = eNewType;

        switch (eNoiRedType)
        {
        case NR_OFF:
            break;

        case NR_LOW:
            NoiseReduction.SetNoiRedDegree(CNoiseReduction::NR_LOW);
            break;

        case NR_MEDIUM:
            NoiseReduction.SetNoiRedDegree(CNoiseReduction::NR_MEDIUM);
            break;

        case NR_HIGH:
            NoiseReduction.SetNoiRedDegree(CNoiseReduction::NR_HIGH);
            break;

        case NR_SPEEX:
            NoiseReduction.SetNoiRedDegree((CNoiseReduction::ENoiRedDegree)iNoiRedLevel);
            break;
        }
    }
    Unlock();
}

void CAMDemodulation::SetNoiRedLevel(const int iNewLevel)
{
    /* Lock resources */
    Lock();
    {
        iNoiRedLevel = iNewLevel;
        if (eNoiRedType == NR_SPEEX)
            NoiseReduction.SetNoiRedDegree((CNoiseReduction::ENoiRedDegree)iNoiRedLevel);
    }
    Unlock();
}

bool CAMDemodulation::GetPLLPhase(CReal& rPhaseOut)
{
    bool bReturn;

    /* Lock resources */
    Lock();
    {
        /* Phase is only valid if PLL is enabled. Return status */
        rPhaseOut = PLL.GetCurPhase();
        bReturn = bPLLIsEnabled;
    }
    Unlock();

    return bReturn;
}


/******************************************************************************\
* Mixer                                                                        *
\******************************************************************************/
void CMixer::Process(CComplexVector& veccIn /* in/out */)
{
    for (int i = 0; i < iBlockSize; i++)
    {
        veccIn[i] *= cCurExp;

        /* Rotate exp-pointer one step further by complex multiplication
           with precalculated rotation vector cExpStep. This saves us from
           calling sin() and cos() functions all the time (iterative
           calculation of these functions) */
        cCurExp *= cExpStep;
    }
}

void CMixer::Init(const int iNewBlockSize)
{
    /* Set internal parameter */
    iBlockSize = iNewBlockSize;
}

void CMixer::SetMixFreq(const CReal rNewNormMixFreq)
{
    /* Set mixing constant */
    cExpStep = CComplex(Cos((CReal) 2.0 * crPi * rNewNormMixFreq),
                        -Sin((CReal) 2.0 * crPi * rNewNormMixFreq));
}


/******************************************************************************\
* Phase lock loop (PLL)                                                        *
\******************************************************************************/
void CPLL::Process(CRealVector& vecrIn /* in/out */)
{
    /* Mix it down to zero frequency */
    cvecLow = vecrIn;
    Mixer.Process(cvecLow);

    /* Complex loop filter */
    rvecRealTmp = Filter(rvecB, rvecA, Real(cvecLow), rvecZReal);
    rvecImagTmp = Filter(rvecB, rvecA, Imag(cvecLow), rvecZImag);

    /* Calculate current phase for GUI */
    rCurPhase = Angle(CComplex(Mean(rvecRealTmp), Mean(rvecImagTmp)));

    /* Average over entire block (only real part) */
    const CReal rOffsEst = Mean(rvecRealTmp);

    /* Close loop */
    rNormCurFreqOffsetAdd = PLL_LOOP_GAIN * rOffsEst;

    /* Update mixer */
    Mixer.SetMixFreq(rNormCurFreqOffset + rNormCurFreqOffsetAdd);
}

void CPLL::Init(const int iNewBlockSize)
{
    /* Set internal parameter */
    iBlockSize = iNewBlockSize;

    /* Init mixer object */
    Mixer.Init(iBlockSize);

    /* Init buffers */
    cvecLow.Init(iBlockSize);
    rvecRealTmp.Init(iBlockSize);
    rvecImagTmp.Init(iBlockSize);

    /* Init loop filter (low-pass filter design) */
    /* IIR filter: H(Z) = (-(1 - lam) * z^{-1}) / (1 - lam * z^{-1}) */
    rvecZReal.Init(2, (CReal) 0.0); /* Memory */
    rvecZImag.Init(2, (CReal) 0.0);
    rvecB.Init(2);
    rvecA.Init(2);
    rvecB[0] = (CReal) 0.0;
    rvecB[1] = -((CReal) 1.0 - PLL_LOOP_FILTER_LAMBDA);
    rvecA[0] = (CReal) 1.0;
    rvecA[1] = -PLL_LOOP_FILTER_LAMBDA;
}

void CPLL::SetRefNormFreq(const CReal rNewNormFreq)
{
    /* Store new reference frequency */
    rNormCurFreqOffset = rNewNormFreq;

    /* Reset offset and phase */
    rNormCurFreqOffsetAdd = (CReal) 0.0;
    rCurPhase = (CReal) 0.0;
}


/******************************************************************************\
* Automatic gain control (AGC)                                                 *
\******************************************************************************/
void CAGC::Process(CRealVector& vecrIn)
{
    if (eType == AT_NO_AGC)
    {
        /* No modification of the signal (except of an amplitude
           correction factor) */
        vecrIn *= AM_AMPL_CORR_FACTOR;
    }
    else
    {
        for (int i = 0; i < iBlockSize; i++)
        {
            /* Two sided one-pole recursion for average amplitude
               estimation */
            IIR1TwoSided(rAvAmplEst, Abs(vecrIn[i]), rAttack, rDecay);

            /* Lower bound for estimated average amplitude */
            if (rAvAmplEst < LOWER_BOUND_AMP_LEVEL)
                rAvAmplEst = LOWER_BOUND_AMP_LEVEL;

            /* Normalize to average amplitude and then amplify to the
               desired level */
            vecrIn[i] *= DES_AV_AMPL_AM_SIGNAL / rAvAmplEst;
        }
    }
}

void CAGC::Init(int iNewSampleRate, int iNewBlockSize)
{
    /* Set internal parameter */
    iSampleRate = iNewSampleRate;
    iBlockSize = iNewBlockSize;

    /* Init filters */
    SetType(eType);

    /* Init average amplitude estimation with desired amplitude */
    rAvAmplEst = DES_AV_AMPL_AM_SIGNAL / AM_AMPL_CORR_FACTOR;
}

void CAGC::SetType(const EAmAgcType eNewType)
{
    /* Set internal parameter */
    eType = eNewType;

    /*          Slow     Medium   Fast    */
    /* Attack: 0.025 s, 0.015 s, 0.005 s  */
    /* Decay : 4.000 s, 2.000 s, 0.200 s  */
    switch (eType)
    {
    case AT_SLOW:
        rAttack = IIR1Lam(0.025, iSampleRate);
        rDecay = IIR1Lam(4.000, iSampleRate);
        break;

    case AT_MEDIUM:
        rAttack = IIR1Lam(0.015, iSampleRate);
        rDecay = IIR1Lam(2.000, iSampleRate);
        break;

    case AT_FAST:
        rAttack = IIR1Lam(0.005, iSampleRate);
        rDecay = IIR1Lam(0.200, iSampleRate);
        break;

    case AT_NO_AGC:
        break;
    }
}


/******************************************************************************\
* Frequency offset acquisition                                                 *
\******************************************************************************/
bool CFreqOffsAcq::Run(const CVector<_REAL>& vecrInpData)
{
    /* Init return flag */
    bool bNewAcqResAvailable = false;

    /* Only do new acquisition if requested */
    if (bAcquisition)
    {
        /* Add new symbol in history (shift register) */
        vecrFFTHistory.AddEnd(vecrInpData, iBlockSize);

        if (iAquisitionCounter > 0)
        {
            /* Decrease counter */
            iAquisitionCounter--;
        }
        else
        {
            /* Copy vector to matlib vector and calculate real-valued FFT */
            for (int i = 0; i < iTotalBufferSize; i++)
                vecrFFTInput[i] = vecrFFTHistory[i];

            /* Calculate power spectrum (X = real(F) ^ 2 + imag(F) ^ 2) */
            vecrPSD = SqMag(rfft(vecrFFTInput, FftPlanAcq));

            /* Calculate frequency from maximum peak in spectrum */
            CReal rMaxPeak; /* Not needed, dummy */
            int iIndMaxPeak;
            Max(rMaxPeak, iIndMaxPeak /* out */,
                vecrPSD(iSearchWinStart + 1, iSearchWinEnd));

            /* Calculate estimated relative frequency offset */
            rCurNormFreqOffset =
                (CReal) (iIndMaxPeak + iSearchWinStart) / iHalfBuffer / 2;

            /* Reset acquisition flag and Set return flag to show that new
               result is available*/
            bAcquisition = false;
            bNewAcqResAvailable = true;
        }
    }

    return bNewAcqResAvailable;
}

void CFreqOffsAcq::Init(const int iNewBlockSize)
{
    /* Set internal parameter */
    iBlockSize = iNewBlockSize;

    /* Total buffer size */
    iTotalBufferSize = NUM_BLOCKS_CARR_ACQUISITION * iBlockSize;

    /* Length of the half of the spectrum of real input signal (the other half
       is the same because of the real input signal). We have to consider the
       Nyquist frequency ("iTotalBufferSize" is always even!) */
    iHalfBuffer = iTotalBufferSize / 2 + 1;

    /* Allocate memory for FFT-histories and init with zeros */
    vecrFFTHistory.Init(iTotalBufferSize, (CReal) 0.0);
    vecrFFTInput.Init(iTotalBufferSize);
    vecrPSD.Init(iHalfBuffer);

    /* Init plans for FFT (faster processing of Fft and Ifft commands) */
    FftPlanAcq.Init(iTotalBufferSize);
}

void CFreqOffsAcq::Start(const CReal rNewNormCenter)
{
    /* Search window indices for aquisition. If input parameter is
       zero, we use the entire bandwidth for acquisition (per definition) */
    if (rNewNormCenter == (CReal) 0.0)
    {
        iSearchWinStart = 1; /* Do not use DC */
        iSearchWinEnd = iHalfBuffer;
    }
    else
    {
        const int iWinCenter = (int) (rNewNormCenter * iHalfBuffer);
        const int iHalfWidth = (int) (PERC_SEARCH_WIN_HALF_SIZE * iHalfBuffer);

        iSearchWinStart = iWinCenter - iHalfWidth;
        iSearchWinEnd = iWinCenter + iHalfWidth;

        /* Check the values that they are within the valid range */
        if (iSearchWinStart < 1) /* Do not use DC */
            iSearchWinStart = 1;

        if (iSearchWinEnd > iHalfBuffer)
            iSearchWinEnd = iHalfBuffer;
    }

    /* Set flag for aquisition */
    bAcquisition = true;
    iAquisitionCounter = NUM_BLOCKS_CARR_ACQUISITION;
}


/******************************************************************************\
* Noise reduction                                                              *
\******************************************************************************/
/*
	The original Dream noise reduction algorithm is based on optimal filters, whereas the
	PDS of the noise is estimated with a minimum statistic.
	We use an overlap and add method to avoid clicks caused by fast changing
	optimal filters between successive blocks.

	[Ref] A. Engel: "Transformationsbasierte Systeme zur einkanaligen
		Stoerunterdrueckung bei Sprachsignalen", PhD Thesis, Christian-
		Albrechts-Universitaet zu Kiel, 1998

	Robert Turnbull added the capability to use the one from the SPEEX codec
*/

CNoiseReduction::~CNoiseReduction()
{
#ifdef HAVE_SPEEX
    if (preprocess_state)
        speex_preprocess_state_destroy(preprocess_state);
    if (speex_data)
        free(speex_data);
#endif
}

void CNoiseReduction::SetNoiRedDegree(const ENoiRedDegree eNND)
{
    eNoiRedDegree = eNND;
#ifdef HAVE_SPEEX
    supression_level = (spx_int32_t)eNND;
#endif
}

void CNoiseReduction::Process(CRealVector& vecrIn)
{
#ifdef HAVE_SPEEX
    if ((int)eNoiRedDegree <= 0) {
        int i;
        CReal* vectorData = &vecrIn[0];
        int vectorSz = vecrIn.GetSize();
        if (sample_rate)
        {
            if (preprocess_state)
                speex_preprocess_state_destroy(preprocess_state);
            if (speex_data)
                free(speex_data);
            preprocess_state = speex_preprocess_state_init(vectorSz, sample_rate);
            speex_data = (spx_int16_t*)malloc(vectorSz*sizeof(spx_int16_t));
            supression_level = (spx_int32_t)eNoiRedDegree;
            sample_rate = 0;
        }
        if (supression_level)
        {
            speex_preprocess_ctl(preprocess_state, SPEEX_PREPROCESS_SET_NOISE_SUPPRESS, &supression_level);
            supression_level = 0;
        }
        for (i=0; i<vectorSz; i++)
        {
            speex_data[i] = (spx_int16_t)vectorData[i];
        }
        speex_preprocess(preprocess_state, speex_data, nullptr);
        for (i=0; i<vectorSz; i++)
        {
            vectorData[i] = (CReal)speex_data[i];
        }
    } else {
#endif
        /* Regular block (updates the noise estimate) --------------------------- */
        /* Update history of input signal */
        vecrLongSignal.Merge(vecrOldSignal, vecrIn);

        /* Update signal PSD estimation */
        veccSigFreq = rfft(vecrLongSignal, FftPlan);
        vecrSqMagSigFreq = SqMag(veccSigFreq);

        /* Update minimum statistic for noise PSD estimation. This update is made
           only once a regular (non-shited) block */
        UpdateNoiseEst(vecrNoisePSD, vecrSqMagSigFreq, eNoiRedDegree);

        /* Actual noise reducation filtering based on the noise PSD estimation and
           the current squared magnitude of the input signal */
        vecrFiltResult = OptimalFilter(veccSigFreq, vecrSqMagSigFreq, vecrNoisePSD);

        /* Apply windowing */
        vecrFiltResult *= vecrTriangWin;

        /* Build output signal vector with old half and new half */
        vecrOutSig1.Merge(vecrOldOutSignal, vecrFiltResult(1, iHalfBlockLen));

        /* Save second half of output signal for next block (for overlap and add) */
        vecrOldOutSignal = vecrFiltResult(iHalfBlockLen + 1, iBlockLen);


        /* "Half-shifted" block for overlap and add ----------------------------- */
        /* Build input vector for filtering the "half-shifted" blocks. It is the
           second half of the very old signal plus the complete old signal and the
           first half of the current signal */
        vecrLongSignal.Merge(vecrVeryOldSignal(iHalfBlockLen + 1, iBlockLen),
                             vecrOldSignal, vecrIn(1, iHalfBlockLen));

        /* Store old input signal blocks */
        vecrVeryOldSignal = vecrOldSignal;
        vecrOldSignal = vecrIn;

        /* Update signal PSD estimation for "half-shifted" block and calculate
           optimal filter */
        veccSigFreq = rfft(vecrLongSignal, FftPlan);
        vecrSqMagSigFreq = SqMag(veccSigFreq);

        vecrFiltResult = OptimalFilter(veccSigFreq, vecrSqMagSigFreq, vecrNoisePSD);

        /* Apply windowing */
        vecrFiltResult *= vecrTriangWin;

        /* Overlap and add operation */
        vecrIn = vecrFiltResult + vecrOutSig1;
#ifdef HAVE_SPEEX
    }
#endif
}

CRealVector CNoiseReduction::OptimalFilter(const CComplexVector& veccSigFreq,
        const CRealVector& vecrSqMagSigFreq,
        const CRealVector& vecrNoisePSD)
{
    CRealVector vecrReturn(iBlockLenLong);

    /* Calculate optimal filter coefficients in the frequency domain:
       G_opt = max(1 - S_nn(n) / S_xx(n), 0) */
    veccOptFilt = Max(Zeros(iFreqBlLen), Ones(iFreqBlLen) -
                      vecrNoisePSD / vecrSqMagSigFreq);

    /* Constrain the optimal filter in time domain to avoid aliasing */
    vecrOptFiltTime = rifft(veccOptFilt, FftPlan);
    vecrOptFiltTime.Merge(vecrOptFiltTime(1, iBlockLen), Zeros(iBlockLen));
    veccOptFilt = rfft(vecrOptFiltTime, FftPlan);

    /* Actual filtering in frequency domain */
    vecrReturn = rifft(veccSigFreq * veccOptFilt, FftPlan);

    /* Cut out correct samples (to get from cyclic convolution to linear
       convolution) */
    return vecrReturn(iBlockLen + 1, iBlockLenLong);
}

void CNoiseReduction::UpdateNoiseEst(CRealVector& vecrNoisePSD,
                                     const CRealVector& vecrSqMagSigFreq,
                                     const ENoiRedDegree eNoiRedDegree)
{
    /*
    	Implements a mimium statistic proposed by R. Martin
    */
    /* Set weighting factor for minimum statistic */
    CReal rWeiFact = MIN_STAT_WEIGHT_FACTOR_MED;
    switch (eNoiRedDegree)
    {
    case NR_LOW:
        rWeiFact = MIN_STAT_WEIGHT_FACTOR_LOW;
        break;

    case NR_MEDIUM:
        rWeiFact = MIN_STAT_WEIGHT_FACTOR_MED;
        break;

    case NR_HIGH:
        rWeiFact = MIN_STAT_WEIGHT_FACTOR_HIGH;
        break;
    }

    /* Update signal PSD estimation (first order IIR filter) */
    IIR1(vecrSigPSD, vecrSqMagSigFreq, rLamPSD);

    for (int i = 0; i < iFreqBlLen; i++)
    {
// TODO: Update of minimum statistic can be done much more efficient
        /* Update history */
        matrMinimumStatHist[i].Merge(
            matrMinimumStatHist[i](2, iMinStatHistLen), vecrSigPSD[i]);

        /* Minimum values in history are taken for noise estimation */
        vecrNoisePSD[i] = Min(matrMinimumStatHist[i]) * rWeiFact;
    }
}

void CNoiseReduction::Init(int iSampleRate, int iNewBlockLen)
{
    iBlockLen = iNewBlockLen;
    iHalfBlockLen = iBlockLen / 2;
    iBlockLenLong = 2 * iBlockLen;

    /* Block length of signal in frequency domain. "+ 1" because of the Nyquist
       frequency */
    iFreqBlLen = iBlockLen + 1;

    /* Length of the minimum statistic history */
    iMinStatHistLen = (int) (MIN_STAT_HIST_LENGTH_SEC * (CReal) iSampleRate / iBlockLen);

    /* Lambda for IIR filter */
    rLamPSD = IIR1Lam(TICONST_PSD_EST_SIG_NOISE_RED,
                      (CReal) iSampleRate / iBlockLen);

    /* Init vectors storing time series signals */
    vecrOldSignal.Init(iBlockLen, (CReal) 0.0);
    vecrVeryOldSignal.Init(iBlockLen, (CReal) 0.0);
    vecrFiltResult.Init(iBlockLen);
    vecrOutSig1.Init(iBlockLen);
    vecrLongSignal.Init(iBlockLenLong);
    vecrOptFiltTime.Init(iBlockLenLong);
    vecrOldOutSignal.Init(iHalfBlockLen, (CReal) 0.0);

    /* Init plans for FFT (faster processing of Fft and Ifft commands). FFT
       plans are initialized with the long length */
    FftPlan.Init(iBlockLenLong);

    /* Init vectors storing data in frequency domain */
    veccOptFilt.Init(iFreqBlLen);

    /* Init signal and noise PDS estimation vectors */
    veccSigFreq.Init(iFreqBlLen);
    vecrSqMagSigFreq.Init(iFreqBlLen);
    vecrSigPSD.Init(iFreqBlLen, (CReal) 0.0);
    vecrNoisePSD.Init(iFreqBlLen, (CReal) 0.0);

    matrMinimumStatHist.Init(iFreqBlLen, iMinStatHistLen, (CReal) 0.0);

    /* Init window for overlap and add */
    vecrTriangWin.Init(iBlockLen);
    vecrTriangWin = Triang(iBlockLen);

#ifdef HAVE_SPEEX
    sample_rate = iSampleRate;
#endif
}
