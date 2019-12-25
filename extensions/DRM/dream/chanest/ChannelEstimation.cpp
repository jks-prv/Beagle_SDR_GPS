/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2001-2006
 *
 * Author(s):
 *	Volker Fischer
 *
 * Description:
 *	Channel estimation and equalization
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

#include "ChannelEstimation.h"
#include <limits>
#include "../matlib/MatlibSigProToolbox.h"


/* Implementation *************************************************************/
void CChannelEstimation::ProcessDataInternal(CParameter& Parameters)
{
    int		i, j, k;
    int		iModSymNum;
    _REAL	rCurSNREst;
    _REAL	rOffsPDSEst;

    /* Check if symbol ID index has changed by the synchronization unit. If it
       has changed, reinit this module */
    if ((*pvecInputData).GetExData().bSymbolIDHasChanged)
    {
// FIXME: we loose one OFDM symbol by this call -> slower DRM signal acquisition
        SetInitFlag();
        return;
    }


// TODO: use CFIFO class for "matcHistory"!
    /* Move data in history-buffer (from iLenHistBuff - 1 towards 0) */
    for (j = 0; j < iLenHistBuff - 1; j++)
    {
        for (i = 0; i < iNumCarrier; i++)
            matcHistory[j][i] = matcHistory[j + 1][i];
    }

    /* Write new symbol in memory */
    for (i = 0; i < iNumCarrier; i++)
        matcHistory[iLenHistBuff - 1][i] = (*pvecInputData)[i];

    const int iSymbolCounter = (*pvecInputData).GetExData().iSymbolID;

    Parameters.Lock();

    /* Time interpolation *****************************************************/
#ifdef USE_DD_WIENER_FILT_TIME
    /* For decision directed channel estimation, it is important to know the
       types of all cells, not only the pilot cells. Therefore, we have to take
       care of frame ID here, too */
    if (iSymbolCounter == 0)
    {
        /* Frame ID of this FAC block stands for the new block. We need
           the ID of the old block, therefore we have to subtract "1" */
        iCurrentFrameID = Parameters.iFrameIDReceiv - 1;
        if (iCurrentFrameID < 0)
            iCurrentFrameID = NUM_FRAMES_IN_SUPERFRAME - 1;
    }

    /* Set absolute symbol position */
    const int iCurSymbIDTiInt =
        iCurrentFrameID * iNumSymPerFrame + iSymbolCounter;
#else
    /* For regular channel estimation, only the positions of the pilots are
       important. These positions are the same in each frame, independent of the
       frame number */
    const int iCurSymbIDTiInt = (*pvecInputData).GetExData().iSymbolID;
#endif

    /* Update the pilot memory for rpil tag of RSCI */
    UpdateRSIPilotStore(Parameters, pvecInputData, Parameters.CellMappingTable.matiMapTab[iCurSymbIDTiInt],
                        Parameters.CellMappingTable.matcPilotCells[iCurSymbIDTiInt], iSymbolCounter);


    /* Get symbol-counter for next symbol. Use the count from the frame
       synchronization (in OFDM.cpp). Call estimation routine */
    const _REAL rSNRAftTiInt =
        pTimeInt->Estimate(pvecInputData, veccPilots,
                           Parameters.CellMappingTable.matiMapTab[iCurSymbIDTiInt],
                           Parameters.CellMappingTable.matcPilotCells[iCurSymbIDTiInt],
                           /* The channel estimation is based on the pilots so
                              it needs the SNR on the pilots. Do a correction */
                           rSNREstimate * rSNRTotToPilCorrFact);

    /* Debar initialization of channel estimation in time direction */
    if (iInitCnt > 0)
    {
        iInitCnt--;

        /* Do not put out data in initialization phase */
        iOutputBlockSize = 0;

        /* Do not continue */
        Parameters.Unlock();
        return;
    }
    else
        iOutputBlockSize = iNumCarrier;

    /* Define DC carrier for robustness mode D because there is no pilot */
    if (iDCPos != 0)
        veccPilots[iDCPos] = (CReal) 0.0;


    /* -------------------------------------------------------------------------
       Use time-interpolated channel estimate for timing synchronization
       tracking */
    TimeSyncTrack.Process(Parameters, veccPilots,
                          (*pvecInputData).GetExData().iCurTimeCorr, rLenPDSEst /* out */,
                          rOffsPDSEst /* out */);

    /* Store current delay in history */
    vecrDelayHist.AddEnd(rLenPDSEst);


    /* Frequency-interploation ************************************************/
    switch (TypeIntFreq)
    {
    case FLINEAR:
        /**********************************************************************\
         * Linear interpolation												  *
        \**********************************************************************/
        /* Set first pilot position */
        veccChanEst[0] = veccPilots[0];

        for (j = 0, k = 1; j < iNumCarrier - iScatPilFreqInt;
                j += iScatPilFreqInt, k++)
        {
            /* Set values at second time pilot position in cluster */
            veccChanEst[j + iScatPilFreqInt] = veccPilots[k];

            /* Interpolation cluster */
            for (i = 1; i < iScatPilFreqInt; i++)
            {
                /* E.g.: c(x) = (c_4 - c_0) / 4 * x + c_0 */
                veccChanEst[j + i] =
                    (veccChanEst[j + iScatPilFreqInt] - veccChanEst[j]) /
                    (_REAL) (iScatPilFreqInt) * (_REAL) i + veccChanEst[j];
            }
        }
        break;

    case FDFTFILTER:
        /**********************************************************************\
         * DFT based algorithm												  *
        \**********************************************************************/
        /* Special case with robustness mode D: pilots in all carriers!
           so no processing is required, the channel estimation is
           simply the pilots */
        if (iNumIntpFreqPil == iNumCarrier)
        {
            veccChanEst = veccPilots;
            break;
        }
        /* ---------------------------------------------------------------------
           Put all pilots at the beginning of the vector. The "real" length of
           the input vector is longer than the number of pilots, but we
           calculate the FFT only over "iNumCarrier / iScatPilFreqInt + 1"
           values (this is the number of pilot positions) */
        /* Weighting pilots with window */
        veccPilots *= vecrDFTWindow;

        /* Transform in time-domain */
        veccPilots = Ifft(veccPilots, FftPlanShort);

        /* Set values outside a defined bound to zero, zero padding (noise
           filtering). Copy second half of spectrum at the end of the new vector
           length and zero out samples between the two parts of the spectrum */
        veccIntPil.Merge(
            /* First part of spectrum */
            veccPilots(1, iStartZeroPadding),
            /* Zero padding in the middle, length: Total length minus length of
               the two parts at the beginning and end */
            CComplexVector(Zeros(iLongLenFreq - 2 * iStartZeroPadding),
                           Zeros(iLongLenFreq - 2 * iStartZeroPadding)),
            /* Set the second part of the actual spectrum at the end of the new
               vector */
            veccPilots(iNumIntpFreqPil - iStartZeroPadding + 1,
                       iNumIntpFreqPil));

        /* Transform back in frequency-domain */
        veccIntPil = Fft(veccIntPil, FftPlanLong);

        /* Remove weighting with DFT window by inverse multiplication */
        veccChanEst = veccIntPil(1, iNumCarrier) * vecrDFTwindowInv;
        break;

    case FWIENER:
        /**********************************************************************\
         * Wiener filter													   *
        \**********************************************************************/
        /* Wiener filter update --------------------------------------------- */
        /* Do not update filter in case of simulation */
        if (Parameters.eSimType == CParameter::ST_NONE)
        {
            /* Update Wiener filter each OFDM symbol. Use current estimates */
            UpdateWienerFiltCoef(rSNRAftTiInt, rLenPDSEst / iNumCarrier,
                                 rOffsPDSEst / iNumCarrier);
        }


        /* Actual Wiener interpolation (FIR filtering) ---------------------- */
        /* FIR filter of the pilots with filter taps. We need to filter the
           pilot positions as well to improve the SNR estimation (which
           follows this procedure) */
        for (j = 0; j < iNumCarrier; j++)
        {
// TODO: Do only calculate channel estimation for data cells, not for pilot
// cells (exeption: if we want to use SNR estimation based on pilots, we also
// need Wiener on these cells!)
            /* Convolution */
            veccChanEst[j] = _COMPLEX((_REAL) 0.0, (_REAL) 0.0);

            for (i = 0; i < iLengthWiener; i++)
            {
                veccChanEst[j] +=
                    matcFiltFreq[j][i] * veccPilots[veciPilOffTab[j] + i];
            }
        }
        break;
    }


    /* Equalize the output vector ------------------------------------------- */
    /* Calculate squared magnitude of channel estimation */
    vecrSqMagChanEst = SqMag(veccChanEst);

    /* Write to output vector. Take oldest symbol of history for output. Also,
       ship the channel state at a certain cell */
    for (i = 0; i < iNumCarrier; i++)
    {
        (*pvecOutputData)[i].cSig = matcHistory[0][i] / veccChanEst[i];

#ifdef USE_MAX_LOG_MAP
        /* In case of MAP we need the squared magnitude for the calculation of
           the metric */
        (*pvecOutputData)[i].rChan = vecrSqMagChanEst[i];
#else
        /* In case of hard-desicions, we need the magnitude of the channel for
           the calculation of the metric */
        (*pvecOutputData)[i].rChan = sqrt(vecrSqMagChanEst[i]);
#endif
    }


    /* -------------------------------------------------------------------------
       Calculate symbol ID of the current output block and set parameter */
    (*pvecOutputData).GetExData().iSymbolID =
        (*pvecInputData).GetExData().iSymbolID - iLenHistBuff + 1;


    /* SNR estimation ------------------------------------------------------- */
    /* Modified symbol ID, check range {0, ..., iNumSymPerFrame} */
    iModSymNum = (*pvecOutputData).GetExData().iSymbolID;

    while (iModSymNum < 0)
        iModSymNum += iNumSymPerFrame;

    /* Two different types of SNR estimation are available */
    switch (TypeSNREst)
    {
    case SNR_PIL:
        /* Use estimated channel and compare it to the received pilots. This
           estimation works only if the channel estimation was successful */
        for (i = 0; i < iNumCarrier; i++)
        {
            /* Identify pilot positions. Use MODIFIED "iSymbolID" (See lines
               above) */
            if (_IsScatPil(Parameters.CellMappingTable.matiMapTab[iModSymNum][i]))
            {
                /* We assume that the channel estimation in "veccChanEst" is
                   noise free (e.g., the wiener interpolation does noise
                   reduction). Thus, we have an estimate of the received signal
                   power \hat{r} = s * \hat{h}_{wiener} */
                const _COMPLEX cModChanEst = veccChanEst[i] *
                                             Parameters.CellMappingTable.matcPilotCells[iModSymNum][i];


                /* Calculate and average noise and signal estimates --------- */
                /* The noise estimation is difference between the noise reduced
                   signal and the noisy received signal
                   \tilde{n} = \hat{r} - r */
                IIR1(rNoiseEst, SqMag(matcHistory[0][i] - cModChanEst),
                     rLamSNREstFast);

                /* The received signal power estimation is just \hat{r} */
                IIR1(rSignalEst, SqMag(cModChanEst), rLamSNREstFast);

                /* Calculate final result (signal to noise ratio) */
                rCurSNREst = CalAndBoundSNR(rSignalEst, rNoiseEst);

                /* Average the SNR with a two sided recursion. Apply correction
                   factor, too */
                IIR1TwoSided(rSNREstimate, rCurSNREst / rSNRTotToPilCorrFact,
                             rLamSNREstFast,	rLamSNREstSlow);
            }
        }
        break;

    case SNR_FAC:
        /* SNR estimation based on FAC cells and hard decisions */
        /* SNR estimation with initialization */
        if (iSNREstInitCnt > 0)
        {
            for (i = 0; i < iNumCarrier; i++)
            {
                /* Only use the last frame of the initialization phase for
                   initial SNR estimation to debar initialization phase of
                   synchronization and channel estimation units */
                if (iSNREstInitCnt < iNumSymPerFrame * iNumCarrier)
                {
                    const int iCurCellFlag =
                        Parameters.CellMappingTable.matiMapTab[iModSymNum][i];

                    /* Initial signal estimate. Use channel estimation from all
                       data and pilot cells. Apply averaging */
                    if ((_IsData(iCurCellFlag)) || (_IsPilot(iCurCellFlag)))
                    {
                        /* Signal estimation */
                        rSignalEst += vecrSqMagChanEst[i];
                        iSNREstIniSigAvCnt++;
                    }

                    /* Noise estimation from all data cells from tentative
                       decisions */
                    if (_IsFAC(iCurCellFlag)) /* FAC cell */
                    {
                        rNoiseEst += vecrSqMagChanEst[i] *
                                     SqMag(MinDist4QAM((*pvecOutputData)[i].cSig));

                        iSNREstIniNoiseAvCnt++;
                    }
                }
            }

            iSNREstInitCnt--;
        }
        else
        {
            /* Only right after initialization phase apply initial SNR
               value */
            if (bSNRInitPhase)
            {
                /* Normalize average */
                rSignalEst /= iSNREstIniSigAvCnt;
                rNoiseEst /= iSNREstIniNoiseAvCnt;

                bSNRInitPhase = false;
            }

            /* replace SNR estimate of current symbol */
            rNoiseEstSum -= vecrNoiseEstFACSym[iModSymNum];
            rSignalEstSum -= vecrSignalEstFACSym[iModSymNum];

            vecrNoiseEstFACSym[iModSymNum] = 0.0;
            vecrSignalEstFACSym[iModSymNum] = 0.0;

            for (i = 0; i < iNumCarrier; i++)
            {
                /* Only use FAC cells for this SNR estimation method */
                if (_IsFAC(Parameters.CellMappingTable.matiMapTab[iModSymNum][i]))
                {
                    /* Get tentative decision for this FAC QAM symbol. FAC is
                       always 4-QAM. Calculate all distances to the four
                       possible constellation points of a 4-QAM and use the
                       squared result of the returned distance vector */
                    const CReal rCurErrPow =
                        SqMag(MinDist4QAM((*pvecOutputData)[i].cSig));

                    /* Use decision together with channel estimate to get
                       estimates for signal and noise */
                    vecrNoiseEstFACSym[iModSymNum] += rCurErrPow * vecrSqMagChanEst[i];
                    vecrSignalEstFACSym[iModSymNum] += vecrSqMagChanEst[i];
                }
            }

            rNoiseEstSum += vecrNoiseEstFACSym[iModSymNum];
            rSignalEstSum += vecrSignalEstFACSym[iModSymNum];

            /* Average SNR estimation from whole frame by exponentially forgetting IIR */
            IIR1(rNoiseEst, rNoiseEstSum, rLamSNREstFast);
            IIR1(rSignalEst, rSignalEstSum, rLamSNREstFast);

            /* Calculate final result (signal to noise ratio) */
            rCurSNREst = CalAndBoundSNR(rSignalEst, rNoiseEst);

            /* Consider correction factor for average signal energy */
            rSNREstimate = rCurSNREst * rSNRFACSigCorrFact;
        }
        break;
    }

    /* OPH: WMER, WMM and WMF estimation ------------------------------------ */
    for (i = 0; i < iNumCarrier; i++)
    {
        /* Use MSC cells for this SNR estimation method */
        if (_IsMSC(Parameters.CellMappingTable.matiMapTab[iModSymNum][i]))
        {
            CReal rCurErrPow = 0.0;

            /* Get tentative decision for this MSC QAM symbol and calculate
               squared distance as a measure for the noise. MSC can be 16 or
               64 QAM */
            switch (Parameters.eMSCCodingScheme)
            {
            case CS_3_SM:
            case CS_3_HMSYM:
            case CS_3_HMMIX:
                rCurErrPow = SqMag(MinDist64QAM((*pvecOutputData)[i].cSig));
                break;

            case CS_2_SM:
                rCurErrPow = SqMag(MinDist16QAM((*pvecOutputData)[i].cSig));
                break;

            default:
                break;
            }

            /* Use decision together with channel estimate to get
               estimates for signal and noise (for each carrier) */
            IIR1(vecrNoiseEstMSC[i], rCurErrPow * vecrSqMagChanEst[i],
                 rLamMSCSNREst);

            IIR1(vecrSigEstMSC[i], vecrSqMagChanEst[i],
                 rLamMSCSNREst);

            /* Calculate MER on MSC cells */
            IIR1(rNoiseEstMSCMER, rCurErrPow, rLamSNREstFast);

            /* OPH: Update accumulators for RSCI MERs */
            rNoiseEstMERAcc += rCurErrPow;
            rNoiseEstWMMAcc += rCurErrPow * vecrSqMagChanEst[i];
            rSignalEstWMMAcc += vecrSqMagChanEst[i];
            iCountMERAcc ++;
        }
        else if (_IsFAC(Parameters.CellMappingTable.matiMapTab[iModSymNum][i]))
        {
            /* Update accumulators for RSCI WMF */
            CReal rCurErrPow = SqMag(MinDist4QAM((*pvecOutputData)[i].cSig));
            rNoiseEstWMFAcc += rCurErrPow * vecrSqMagChanEst[i];
            rSignalEstWMFAcc += vecrSqMagChanEst[i];
        }
    }

    /* SNR and Doppler are updated every symbol.
     * TODO is this necessary ? */

    if (bSNRInitPhase == false)
    {
        const _REAL rNomBWSNR = rSNREstimate * rSNRSysToNomBWCorrFact;

        /* Bound the SNR at 0 dB */
        if (rNomBWSNR > 1.0 )
            Parameters.SetSNR(10.0 * log10(rNomBWSNR));
        else
            Parameters.SetSNR(0.0);
    }

    /* Doppler estimation is only implemented in the
     * Wiener time interpolation module */
    if (TypeIntTime == TWIENER)
        Parameters.rSigmaEstimate = TimeWiener.GetSigma();
    else
        Parameters.rSigmaEstimate = -1.0;

    /* After processing last symbol of the frame */
    if (iModSymNum == iNumSymPerFrame - 1)
    {

        /* set minimum and maximum delay from history */
        _REAL rMinDelay = 1000.0;
        _REAL rMaxDelay = 0.0;
        for (int i = 0; i < iLenDelayHist; i++)
        {
            if (rMinDelay > vecrDelayHist[i])
                rMinDelay = vecrDelayHist[i];
            if(rMaxDelay < vecrDelayHist[i])
                rMaxDelay = vecrDelayHist[i];
        }

        /* Return delay in ms */
        _REAL rDelayScale = _REAL(iFFTSizeN) / _REAL(iSampleRate * iNumIntpFreqPil * iScatPilFreqInt) * 1000.0;
        Parameters.rMinDelay = rMinDelay * rDelayScale;
        Parameters.rMaxDelay = rMaxDelay * rDelayScale;

        /* Calculate and generate RSCI measurement values */
        /* rmer (MER for MSC) */
        CReal rMER =
            CalAndBoundSNR(AV_DATA_CELLS_POWER, rNoiseEstMERAcc / iCountMERAcc);

        /* Bound MER at 0 dB */
        Parameters.rMER = (rMER > (_REAL) 1.0 ? (_REAL) 10.0 * log10(rMER) : (_REAL) 0.0);

        /* rwmm (WMER for MSC)*/
        CReal rWMM = AV_DATA_CELLS_POWER *
                     CalAndBoundSNR(rSignalEstWMMAcc, rNoiseEstWMMAcc);

        /* Bound the MER at 0 dB */
        Parameters.rWMERMSC = (rWMM > (_REAL) 1.0 ? (_REAL) 10.0 * log10(rWMM) : (_REAL) 0.0);

        /* rwmf (WMER for FAC) */
        CReal rWMF = AV_DATA_CELLS_POWER *
                     CalAndBoundSNR(rSignalEstWMFAcc, rNoiseEstWMFAcc);

        /* Bound the MER at 0 dB */
        Parameters.rWMERFAC = (rWMF > (_REAL) 1.0 ? (_REAL) 10.0 * log10(rWMF) : (_REAL) 0.0);

        /* Reset all the accumulators and counters */
        rNoiseEstMERAcc = (_REAL) 0.0;
        iCountMERAcc = 0;
        rSignalEstWMMAcc = (_REAL) 0.0;
        rNoiseEstWMMAcc = (_REAL) 0.0;
        rSignalEstWMFAcc = (_REAL) 0.0;
        rNoiseEstWMFAcc = (_REAL) 0.0;

        if (Parameters.bMeasureDelay)
            TimeSyncTrack.CalculateRdel(Parameters);

        if (Parameters.bMeasureDoppler)
            TimeSyncTrack.CalculateRdop(Parameters);

        if (Parameters.bMeasureInterference)
        {
            /* Calculate interference tag */
            CalculateRint(Parameters);
        }
    }

    Parameters.Unlock();

    /* Interferer consideration --------------------------------------------- */
    if (bInterfConsid)
    {
        for (i = 0; i < iNumCarrier; i++)
        {
            /* Weight the channel estimates with the SNR estimate of the current
               carrier to consider the higher noise variance caused by
               interferers */
            (*pvecOutputData)[i].rChan *=
                CalAndBoundSNR(vecrSigEstMSC[i], vecrNoiseEstMSC[i]);
        }
    }
}

/* OPH: Calculate the values for the rint RSCI tag */
void CChannelEstimation::CalculateRint(CParameter& Parameters)
{
    CReal rMaxNoiseEst = (CReal) 0.0;
    CReal rSumNoiseEst = (CReal) 0.0;
    CReal rSumSigEst  = (CReal) 0.0;
    int iMaxIntCarrier = 0;

    /* Find peak noise and add up noise and signal estimates */
    for (int i = 0; i < iNumCarrier; i++)
    {
        CReal rNoiseEstMSC = vecrNoiseEstMSC[i];
        if (rNoiseEstMSC> rMaxNoiseEst)
        {
            rMaxNoiseEst = rNoiseEstMSC;
            iMaxIntCarrier = i;
        }
        rSumNoiseEst += rNoiseEstMSC;
        rSumSigEst += vecrSigEstMSC[i];
    }

    /* Interference to noise ratio */
    _REAL rINRtmp = CalAndBoundSNR(rMaxNoiseEst, rSumNoiseEst / iNumCarrier);
    Parameters.rINR = (rINRtmp > (_REAL) 1.0 ? (_REAL) 10.0 * log10(rINRtmp) : (_REAL) 0.0);

    /* Interference to (single) carrier ratio */
    _REAL rICRtmp = CalAndBoundSNR(rMaxNoiseEst,
                                   AV_DATA_CELLS_POWER * rSumSigEst/iNumCarrier);


    Parameters.rICR = (rICRtmp > (_REAL) 1.0 ? (_REAL) 10.0 * log10(rICRtmp) : (_REAL) 0.0);

    /* Interferer frequency */
    Parameters.rIntFreq = ((_REAL) iMaxIntCarrier + Parameters.CellMappingTable.iCarrierKmin) /
                             Parameters.CellMappingTable.iFFTSizeN * iSampleRate;
}

void CChannelEstimation::InitInternal(CParameter& Parameters)
{
    Parameters.Lock();
    /* Get parameters from global struct */
    iScatPilTimeInt = Parameters.CellMappingTable.iScatPilTimeInt;
    iScatPilFreqInt = Parameters.CellMappingTable.iScatPilFreqInt;
    iNumIntpFreqPil = Parameters.CellMappingTable.iNumIntpFreqPil;
    iNumCarrier = Parameters.CellMappingTable.iNumCarrier;
    iFFTSizeN = Parameters.CellMappingTable.iFFTSizeN;
    iNumSymPerFrame = Parameters.CellMappingTable.iNumSymPerFrame;
    iSampleRate = Parameters.GetSigSampleRate();

    /* Length of guard-interval with respect to FFT-size! */
    rGuardSizeFFT = (_REAL) iNumCarrier *
                    Parameters.CellMappingTable.RatioTgTu.iEnum / Parameters.CellMappingTable.RatioTgTu.iDenom;

    /* If robustness mode D is active, get DC position. This position cannot
       be "0" since in mode D no 5 kHz mode is defined (see DRM-standard).
       Therefore we can also use this variable to get information whether
       mode D is active or not (by simply write: "if (iDCPos != 0)") */
    if (Parameters.GetWaveMode() == RM_ROBUSTNESS_MODE_D)
    {
        /* Identify DC carrier position */
        for (int i = 0; i < iNumCarrier; i++)
        {
            if (_IsDC(Parameters.CellMappingTable.matiMapTab[0][i]))
                iDCPos = i;
        }
    }
    else
        iDCPos = 0;

    /* FFT must be longer than "iNumCarrier" because of zero padding effect (
       not in robustness mode D! -> "iLongLenFreq = iNumCarrier") */
    iLongLenFreq = iNumCarrier + iScatPilFreqInt - 1;

    /* Init vector for received data at pilot positions */
    veccPilots.Init(iNumIntpFreqPil);

    /* Init vector for interpolated pilots */
    veccIntPil.Init(iLongLenFreq);

    /* Init plans for FFT (faster processing of Fft and Ifft commands) */
    FftPlanShort.Init(iNumIntpFreqPil);
    FftPlanLong.Init(iLongLenFreq);

    /* Choose time interpolation method and set pointer to correcponding
       object */
    switch (TypeIntTime)
    {
    case TLINEAR:
        pTimeInt = &TimeLinear;
        break;

    case TWIENER:
        pTimeInt = &TimeWiener;
        break;
    }

#ifdef USE_DD_WIENER_FILT_TIME
    /* Reset frame ID counter */
    iCurrentFrameID = 0;
#endif

    /* Init time interpolation interface and set delay for interpolation */
    iLenHistBuff = pTimeInt->Init(Parameters);

    /* Init time synchronization tracking unit */
    TimeSyncTrack.Init(Parameters, iLenHistBuff);

    /* Set channel estimation delay in global struct. This is needed for
       simulation */
    Parameters.iChanEstDelay = iLenHistBuff;


    /* Init window for DFT operation for frequency interpolation ------------ */
    /* Simplified version of "Low-complexity Channel Estimator Based on
       Windowed DFT and Scalar Wiener Fitler for OFDM Systems" by Baoguo Yang,
       Zhigang Cao and K. B. Letaief */
    /* Init memory */
    vecrDFTWindow.Init(iNumIntpFreqPil);
    vecrDFTwindowInv.Init(iNumCarrier);

    /* Set window coefficients. Do not use the edges of the windows */
    const _REAL rWinExpFact = (_REAL) 0.0828; /* Value taken from paper */

    const int iOvlSamOneSideShort =
        (int) Ceil(rWinExpFact * iNumIntpFreqPil / 2);
    const int iOvlSamOneSideLong = iOvlSamOneSideShort * iScatPilFreqInt;

    const int iExpWinLenShort = iNumIntpFreqPil + 2 * iOvlSamOneSideShort;
    const int iExpWinLenLong = iNumCarrier + 2 * iOvlSamOneSideLong;

    CRealVector vecrExpWinShort(iExpWinLenShort);
    CRealVector vecrExpWinLong(iExpWinLenLong);

    switch (eDFTWindowingMethod)
    {
    case DFT_WIN_RECT:
        vecrDFTWindow = Ones(iNumIntpFreqPil);
        vecrDFTwindowInv = Ones(iNumCarrier);
        break;

    case DFT_WIN_HAMM:
        vecrExpWinShort = Hamming(iExpWinLenShort);
        vecrDFTWindow = vecrExpWinShort(iOvlSamOneSideShort + 1,
                                        iExpWinLenShort - iOvlSamOneSideShort);

        vecrExpWinLong = Hamming(iExpWinLenLong);
        vecrDFTwindowInv = (CReal) 1.0 / vecrExpWinLong(iOvlSamOneSideLong + 1,
                           iExpWinLenLong - iOvlSamOneSideLong);
        break;

    case DFT_WIN_HANN:
        vecrExpWinShort = Hann(iExpWinLenShort);
        vecrDFTWindow = vecrExpWinShort(iOvlSamOneSideShort + 1,
                                        iExpWinLenShort - iOvlSamOneSideShort);

        vecrExpWinLong = Hann(iExpWinLenLong);
        vecrDFTwindowInv = (CReal) 1.0 / vecrExpWinLong(iOvlSamOneSideLong + 1,
                           iExpWinLenLong - iOvlSamOneSideLong);
        break;
    }

    /* Set start index for zero padding in time domain for DFT method */
    iStartZeroPadding = (int) rGuardSizeFFT;
    if (iStartZeroPadding > iNumIntpFreqPil)
        iStartZeroPadding = iNumIntpFreqPil;

    /* Allocate memory for channel estimation */
    veccChanEst.Init(iNumCarrier);
    vecrSqMagChanEst.Init(iNumCarrier);

    /* Allocate memory for history buffer (Matrix) and zero out */
    matcHistory.Init(iLenHistBuff, iNumCarrier,
                     _COMPLEX((_REAL) 0.0, (_REAL) 0.0));

    /* After an initialization we do not put out data before the number symbols
       of the channel estimation delay have been processed */
    iInitCnt = iLenHistBuff - 1;

    /* SNR correction factor for the different SNR estimation types. For the
       FAC method, the average signal power has to be considered. For the pilot
       based method, only the SNR on the pilots are evaluated. Therefore, to get
       the total SNR, a correction has to be applied */
    rSNRFACSigCorrFact = Parameters.CellMappingTable.rAvPowPerSymbol / CReal(iNumCarrier);
    rSNRTotToPilCorrFact = Parameters.CellMappingTable.rAvScatPilPow *
                           (_REAL) iNumCarrier / Parameters.CellMappingTable.rAvPowPerSymbol;

    /* Correction factor for transforming the estimated system SNR in the SNR
       where the noise bandwidth is according to the nominal DRM bandwidth */
    rSNRSysToNomBWCorrFact = Parameters.GetSysToNomBWCorrFact();

    /* Inits for SNR estimation (noise and signal averages) */
    rSignalEst = (_REAL) 0.0;
    rNoiseEst = (_REAL) 0.0;
    rNoiseEstMSCMER = (_REAL) 0.0;
    rSNREstimate = (_REAL) pow((_REAL) 10.0, INIT_VALUE_SNR_ESTIM_DB / 10);
    vecrNoiseEstMSC.Init(iNumCarrier, (_REAL) 0.0);
    vecrSigEstMSC.Init(iNumCarrier, (_REAL) 0.0);

    /* For SNR estimation initialization */
    iSNREstIniSigAvCnt = 0;
    iSNREstIniNoiseAvCnt = 0;

    /* for FAC SNR estimation */
    vecrNoiseEstFACSym.Init(iNumSymPerFrame, (_REAL) 0.0);
    vecrSignalEstFACSym.Init(iNumSymPerFrame, (_REAL) 0.0);
    rNoiseEstSum = 0.0;
    rSignalEstSum = 0.0;

    /* We only have an initialization phase for SNR estimation method based on
       the tentative decisions of FAC cells */
    if (TypeSNREst == SNR_FAC)
        bSNRInitPhase = true;
    else
        bSNRInitPhase = false;

    /* 5 DRM frames to start initial SNR estimation after initialization phase
       of other units */
    iSNREstInitCnt = 5 * iNumSymPerFrame;

    /* Lambda for IIR filter */
    rLamSNREstFast = IIR1Lam(TICONST_SNREST_FAST, (CReal) iSampleRate /
                             Parameters.CellMappingTable.iSymbolBlockSize);
    rLamSNREstSlow = IIR1Lam(TICONST_SNREST_SLOW, (CReal) iSampleRate /
                             Parameters.CellMappingTable.iSymbolBlockSize);
    rLamMSCSNREst = IIR1Lam(TICONST_SNREST_MSC, (CReal) iSampleRate /
                            Parameters.CellMappingTable.iSymbolBlockSize);

    /* Init delay spread length estimation (index) */
    rLenPDSEst = (_REAL) 0.0;

    /* Init history for delay values */
    /* Duration of OFDM symbol */
    const _REAL rTs = (CReal) (Parameters.CellMappingTable.iFFTSizeN +
                               Parameters.CellMappingTable.iGuardSize) / iSampleRate;

    iLenDelayHist = (int) (LEN_HIST_DELAY_LOG_FILE_S / rTs);
    vecrDelayHist.Init(iLenDelayHist, (CReal) 0.0);


    /* Inits for Wiener interpolation in frequency direction ---------------- */
    /* Length of wiener filter */
    switch (Parameters.GetWaveMode())
    {
    case RM_ROBUSTNESS_MODE_A:
        iLengthWiener = LEN_WIENER_FILT_FREQ_RMA;
        break;

    case RM_ROBUSTNESS_MODE_B:
        iLengthWiener = LEN_WIENER_FILT_FREQ_RMB;
        break;

    case RM_ROBUSTNESS_MODE_C:
        iLengthWiener = LEN_WIENER_FILT_FREQ_RMC;
        break;

    case RM_ROBUSTNESS_MODE_D:
        iLengthWiener = LEN_WIENER_FILT_FREQ_RMD;
        break;

    default:
        break;
    }


    /* Inits for wiener filter ---------------------------------------------- */
    /* In frequency direction we can use pilots from both sides for
       interpolation */
    iPilOffset = iLengthWiener / 2;

    /* Allocate memory */
    matcFiltFreq.Init(iNumCarrier, iLengthWiener);

    /* Pilot offset table */
    veciPilOffTab.Init(iNumCarrier);

    /* Number of different wiener filters */
    iNumWienerFilt = (iLengthWiener - 1) * iScatPilFreqInt + 1;

    /* Allocate temporary matlib vector for filter coefficients */
    matcWienerFilter.Init(iNumWienerFilt, iLengthWiener);

    /* Distinguish between simulation and regular receiver. When we run a
       simulation, the parameters are taken from simulation init */
    if (Parameters.eSimType == CParameter::ST_NONE)
    {
        /* Initial Wiener filter. Use initial SNR definition and assume that the
           PDS ranges from the beginning of the guard-intervall to the end */
        UpdateWienerFiltCoef(
            pow((CReal) 10.0, INIT_VALUE_SNR_WIEN_FREQ_DB / 10),
            (CReal) Parameters.CellMappingTable.RatioTgTu.iEnum /
            Parameters.CellMappingTable.RatioTgTu.iDenom, (CReal) 0.0);
    }
    else
    {
        /* Get simulation SNR on the pilot positions and set PDS to entire
           guard-interval length */
        UpdateWienerFiltCoef(
            pow((CReal) 10.0, Parameters.GetSysSNRdBPilPos() / 10),
            (CReal) Parameters.CellMappingTable.RatioTgTu.iEnum /
            Parameters.CellMappingTable.RatioTgTu.iDenom, (CReal) 0.0);
    }


    /* Define block-sizes for input and output */
    iInputBlockSize = iNumCarrier;
    iMaxOutputBlockSize = iNumCarrier;


    /* inits for RSCI pilot store */
    matcRSIPilotStore.Init(iNumSymPerFrame / iScatPilTimeInt, iNumCarrier/iScatPilFreqInt + 1,
                           _COMPLEX(_REAL(0.0),_REAL(0.0)));
    iTimeDiffAccuRSI = 0;

    Parameters.Unlock();
}

CComplexVector CChannelEstimation::FreqOptimalFilter(int iFreqInt, int iDiff,
        CReal rSNR,
        CReal rRatPDSLen,
        CReal rRatPDSOffs,
        int iLength)
{
    /*
    	We assume that the power delay spread is a rectangle function in the time
    	domain (sinc-function in the frequency domain). Length and position of this
    	window are adapted according to the current estimated PDS.
    */
    int				i;
    CRealVector		vecrRpp(iLength);
    CRealVector		vecrRhp(iLength);
    CRealVector		vecrH(iLength);
    CComplexVector	veccH(iLength);

    /* Calculation of R_hp, this is the SHIFTED correlation function */
    for (i = 0; i < iLength; i++)
    {
        const int iCurPos = i * iFreqInt - iDiff;

        vecrRhp[i] = Sinc((CReal) iCurPos * rRatPDSLen);
    }

    /* Calculation of R_pp */
    for (i = 0; i < iLength; i++)
    {
        const int iCurPos = i * iFreqInt;

        vecrRpp[i] = Sinc((CReal) iCurPos * rRatPDSLen);
    }

    /* Add SNR at first tap */
    vecrRpp[0] += (CReal) 1.0 / rSNR;

    /* Call levinson algorithm to solve matrix system for optimal solution */
    vecrH = Levinson(vecrRpp, vecrRhp);

    /* Correct the optimal filter coefficients. Shift the rectangular
       function in the time domain to the correct position (determined by
       the "rRatPDSOffs") by multiplying in the frequency domain
       with exp(j w T) */
    for (i = 0; i < iLength; i++)
    {
        const int iCurPos = i * iFreqInt - iDiff;

        const CReal rArgExp =
            (CReal) crPi * iCurPos * (rRatPDSLen + rRatPDSOffs * 2);

        veccH[i] = vecrH[i] * CComplex(Cos(rArgExp), Sin(rArgExp));
    }

    return veccH;
}

void CChannelEstimation::UpdateWienerFiltCoef(CReal rNewSNR, CReal rRatPDSLen,
        CReal rRatPDSOffs)
{
    int	j, i;
    int	iDiff;
    int	iCurPil;

    /* Calculate all possible wiener filters */
    for (j = 0; j < iNumWienerFilt; j++)
    {
        matcWienerFilter[j] = FreqOptimalFilter(iScatPilFreqInt, j, rNewSNR,
                                                rRatPDSLen, rRatPDSOffs, iLengthWiener);
    }


#if 0
    /* Save filter coefficients */
    static FILE* pFile = fopen("test/wienerfreq.dat", "w");
    for (j = 0; j < iNumWienerFilt; j++)
    {
        for (i = 0; i < iLengthWiener; i++)
        {
            fprintf(pFile, "%e %e\n", Real(matcWienerFilter[j][i]),
                    Imag(matcWienerFilter[j][i]));
        }
    }
    fflush(pFile);
#endif


    /* Set matrix with filter taps, one filter for each carrier */
    for (j = 0; j < iNumCarrier; j++)
    {
        /* We define the current pilot position as the last pilot which the
           index "j" has passed */
        iCurPil = (int) (j / iScatPilFreqInt);

        /* Consider special cases at the edges of the DRM spectrum */
        if (iCurPil < iPilOffset)
        {
            /* Special case: left edge */
            veciPilOffTab[j] = 0;
        }
        else if (iCurPil - iPilOffset > iNumIntpFreqPil - iLengthWiener)
        {
            /* Special case: right edge */
            veciPilOffTab[j] = iNumIntpFreqPil - iLengthWiener;
        }
        else
        {
            /* In the middle */
            veciPilOffTab[j] = iCurPil - iPilOffset;
        }

        /* Special case for robustness mode D, since the DC carrier is not used
           as a pilot and therefore we use the same method for the edges of the
           spectrum also in the middle of robustness mode D */
        if (iDCPos != 0)
        {
            if ((iDCPos - iCurPil < iLengthWiener) && (iDCPos - iCurPil > 0))
            {
                /* Left side of DC carrier */
                veciPilOffTab[j] = iDCPos - iLengthWiener;
            }

            if ((iCurPil - iDCPos < iLengthWiener) && (iCurPil - iDCPos > 0))
            {
                /* Right side of DC carrier */
                veciPilOffTab[j] = iDCPos + 1;
            }
        }

        /* Difference between the position of the first pilot (for filtering)
           and the position of the observed carrier */
        iDiff = j - veciPilOffTab[j] * iScatPilFreqInt;

        /* Copy correct filter in matrix */
        for (i = 0; i < iLengthWiener; i++)
            matcFiltFreq[j][i] = matcWienerFilter[iDiff][i];
    }
}

_REAL CChannelEstimation::CalAndBoundSNR(const _REAL rSignalEst, const _REAL rNoiseEst)
{
    _REAL rReturn;
    const _REAL epsilon = numeric_limits<_REAL>::epsilon();

    /* "rNoiseEst" must not be near zero */

    if ((rNoiseEst < -epsilon) || (rNoiseEst > epsilon))
        rReturn = rSignalEst / rNoiseEst;
    else
        rReturn = (_REAL) 1.0;

    /* Bound the SNR at 0 dB */
    if (rReturn < (_REAL) 1.0)
        rReturn = (_REAL) 1.0;

    return rReturn;
}


#if 0
_REAL CChannelEstimation::GetSNREstdB() const
{
    if (bSNRInitPhase)
        return -1.0;

    const _REAL rNomBWSNR = rSNREstimate * rSNRSysToNomBWCorrFact;

    /* Bound the SNR at 0 dB */
    if ((rNomBWSNR > (_REAL) 1.0) && (bSNRInitPhase == false))
        return (_REAL) 10.0 * log10(rNomBWSNR);
    else
        return (_REAL) 0.0;
}

_REAL CChannelEstimation::GetMSCMEREstdB()
{
    /* Calculate final result (signal to noise ratio) and consider correction
       factor for average signal energy */
    const _REAL rCurMSCMEREst = rSNRSysToNomBWCorrFact *
                                CalAndBoundSNR(AV_DATA_CELLS_POWER, rNoiseEstMSCMER);

    /* Bound the MCS MER at 0 dB */
    if (rCurMSCMEREst > (_REAL) 1.0)
        return (_REAL) 10.0 * log10(rCurMSCMEREst);
    else
        return (_REAL) 0.0;
}

_REAL CChannelEstimation::GetMSCWMEREstdB()
{
    _REAL rAvNoiseEstMSC = (_REAL) 0.0;
    _REAL rAvSigEstMSC = (_REAL) 0.0;

    /* Lock resources */
    Lock();
    {
        /* Average results from all carriers */
        for (int i = 0; i < iNumCarrier; i++)
        {
            rAvNoiseEstMSC += vecrNoiseEstMSC[i];
            rAvSigEstMSC += vecrSigEstMSC[i];
        }
    }
    Unlock();

    /* Calculate final result (signal to noise ratio) and consider correction
       factor for average signal energy */
    const _REAL rCurMSCWMEREst = rSNRSysToNomBWCorrFact *
                                 CalAndBoundSNR(rAvSigEstMSC, rAvNoiseEstMSC);

    /* Bound the MCS MER at 0 dB */
    if (rCurMSCWMEREst > (_REAL) 1.0)
        return (_REAL) 10.0 * log10(rCurMSCWMEREst);
    else
        return (_REAL) 0.0;
}

_REAL CChannelEstimation::GetSigma()
{
    /* Doppler estimation is only implemented in the Wiener time interpolation
       module */
    if (TypeIntTime == TWIENER)
        return TimeWiener.GetSigma();
    else
        return -1.0;
}

_REAL CChannelEstimation::GetDelay() const
{
    /* Delay in ms */
    return rLenPDSEst * iFFTSizeN /
           (iSampleRate * iNumIntpFreqPil * iScatPilFreqInt) * 1000;
}

_REAL CChannelEstimation::GetMinDelay()
{
    /* Lock because of vector "vecrDelayHist" access */
    //Lock();

    /* Return minimum delay in history */
    _REAL rMinDelay = 1000.0;
    for (int i = 0; i < iLenDelayHist; i++)
    {
        if (rMinDelay > vecrDelayHist[i])
            rMinDelay = vecrDelayHist[i];
    }

    //Unlock();

    /* Return delay in ms */
    return rMinDelay * iFFTSizeN /
           (iSampleRate * iNumIntpFreqPil * iScatPilFreqInt) * 1000;
}
#endif

void CChannelEstimation::GetTransferFunction(CVector<_REAL>& vecrData,
        CVector<_REAL>& vecrGrpDly,
        CVector<_REAL>& vecrScale)
{
    /* Init output vectors */
    vecrData.Init(iNumCarrier, (_REAL) 0.0);
    vecrGrpDly.Init(iNumCarrier, (_REAL) 0.0);
    vecrScale.Init(iNumCarrier, (_REAL) 0.0);

    /* Do copying of data only if vector is of non-zero length which means that
       the module was already initialized */
    if (iNumCarrier != 0)
    {
        _REAL rDiffPhase, rOldPhase;

        /* Lock resources */
        Lock();

        /* TODO - decide if this allows the if(i==0) test to be removed */
        rOldPhase = Angle(veccChanEst[0]);

        /* Init constants for normalization */
        const _REAL rTu = (CReal) iFFTSizeN / iSampleRate;
        const _REAL rNormData = (_REAL) _MAXSHORT * _MAXSHORT;

        /* Copy data in output vector and set scale
           (carrier index as x-scale) */
        for (int i = 0; i < iNumCarrier; i++)
        {
            /* Transfer function */
            const _REAL rNormSqMagChanEst = SqMag(veccChanEst[i]) / rNormData;

            if (rNormSqMagChanEst > 0)
                vecrData[i] = (_REAL) 10.0 * Log10(rNormSqMagChanEst);
            else
                vecrData[i] = RET_VAL_LOG_0;

            /* Group delay */
            if (i == 0)
            {
                /* At position 0 we cannot calculate a derivation -> use
                   the same value as position 0 */
                rDiffPhase = Angle(veccChanEst[1]) - Angle(veccChanEst[0]);
            }
            else
                rDiffPhase = Angle(veccChanEst[i]) - rOldPhase;

            /* Take care of wrap around of angle() function */
            if (rDiffPhase > WRAP_AROUND_BOUND_GRP_DLY)
                rDiffPhase -= 2.0 * crPi;
            if (rDiffPhase < -WRAP_AROUND_BOUND_GRP_DLY)
                rDiffPhase += 2.0 * crPi;

            /* Apply normalization */
            vecrGrpDly[i] = rDiffPhase * rTu * 1000.0 /* ms */;

            /* Store old phase */
            rOldPhase = Angle(veccChanEst[i]);

            /* Scale (carrier index) */
            vecrScale[i] = i;
        }

        /* Release resources */
        Unlock();
    }
}

void CChannelEstimation::GetSNRProfile(CVector<_REAL>& vecrData,
                                       CVector<_REAL>& vecrScale)
{
    int i;

    /* Init output vectors */
    vecrData.Init(iNumCarrier, (_REAL) 0.0);
    vecrScale.Init(iNumCarrier, (_REAL) 0.0);

    /* Do copying of data only if vector is of non-zero length which means that
       the module was already initialized */
    if (iNumCarrier != 0)
    {
        /* Lock resources */
        Lock();

        /* We want to suppress the carriers on which no SNR measurement can be
           performed (DC carrier, frequency pilots carriers) */
        int iNewNumCar = 0;
        for (i = 0; i < iNumCarrier; i++)
        {
            if (vecrSigEstMSC[i] != (_REAL) 0.0)
                iNewNumCar++;
        }

        /* Init output vectors for new size */
        vecrData.Init(iNewNumCar, (_REAL) 0.0);
        vecrScale.Init(iNewNumCar, (_REAL) 0.0);

        /* Copy data in output vector and set scale
           (carrier index as x-scale) */
        int iCurOutIndx = 0;
        for (i = 0; i < iNumCarrier; i++)
        {
            /* Suppress carriers where no SNR measurement is possible */
            if (vecrSigEstMSC[i] != (_REAL) 0.0)
            {
                /* Calculate final result (signal to noise ratio). Consider
                   correction factor for average signal energy */
                const _REAL rNomBWSNR =
                    CalAndBoundSNR(vecrSigEstMSC[i], vecrNoiseEstMSC[i]) *
                    rSNRFACSigCorrFact * rSNRSysToNomBWCorrFact;

                /* Bound the SNR at 0 dB */
                if ((rNomBWSNR > (_REAL) 1.0) && (bSNRInitPhase == false))
                    vecrData[iCurOutIndx] = (_REAL) 10.0 * Log10(rNomBWSNR);
                else
                    vecrData[iCurOutIndx] = (_REAL) 0.0;

                /* Scale (carrier index) */
                vecrScale[iCurOutIndx] = i;

                iCurOutIndx++;
            }
        }

        /* Release resources */
        Unlock();
    }
}

void CChannelEstimation::GetAvPoDeSp(CVector<_REAL>& vecrData,
                                     CVector<_REAL>& vecrScale,
                                     _REAL& rLowerBound, _REAL& rHigherBound,
                                     _REAL& rStartGuard, _REAL& rEndGuard,
                                     _REAL& rPDSBegin, _REAL& rPDSEnd)
{
    /* Lock resources */
    Lock();

    TimeSyncTrack.GetAvPoDeSp(vecrData, vecrScale, rLowerBound,
                              rHigherBound, rStartGuard, rEndGuard, rPDSBegin, rPDSEnd, iSampleRate);

    /* Release resources */
    Unlock();
}

void CChannelEstimation::UpdateRSIPilotStore(CParameter& Parameters, CVectorEx<_COMPLEX>* pvecInputData,
        CVector<int>& veciMapTab, CVector<_COMPLEX>& veccPilotCells, const int iSymbolCounter)
{
    int			i;
    int			iPiHiIdx;
    //int			iTimeDiffNew;
    _COMPLEX	cNewPilot;

    /* Clear time diff accumulator at start of the frame */
    if (iSymbolCounter == 0)
    {
        iTimeDiffAccuRSI = 0;
    }
    else
    {
        /* Add the latest timing step onto the reference value */
        iTimeDiffAccuRSI += (*pvecInputData).GetExData().iCurTimeCorr;
    }


    /* calculate the spacing between scattered pilots in a given symbol */
    int iScatPilFreqSpacing = iScatPilFreqInt * iScatPilTimeInt;

    /* Data is stored in the array with one row per pilot repetition, and in freq order in each row */
    /* Each row has one element per pilot-bearing carrier */
    /* This avoids having a jagged array with different lengths in different rows */
    int iRow = iSymbolCounter / Parameters.CellMappingTable.iScatPilTimeInt;

    int iScatPilFreqInt = Parameters.CellMappingTable.iScatPilFreqInt;


    /* Find the first pilot */
    int iFirstPilotCarrier = 0;

    while (!_IsScatPil(veciMapTab[iFirstPilotCarrier]) )
        iFirstPilotCarrier += iScatPilFreqInt;


    /* Fill in the matrix for channel estimates at the pilot positions -------- */
    /* Step by the pilot spacing in a given symbol */
    for (i = iFirstPilotCarrier, iPiHiIdx = iFirstPilotCarrier/iScatPilFreqInt;
            i < iNumCarrier; i += iScatPilFreqSpacing, iPiHiIdx+=iScatPilTimeInt)
    {
        /* Identify and calculate transfer function at the pilot positions */
        if (_IsScatPil(veciMapTab[i])) /* This will almost always be true */
        {
            /* Add new channel estimate: h = r / s, h: transfer function of the
               channel, r: received signal, s: transmitted signal */
            cNewPilot = (*pvecInputData)[i] / veccPilotCells[i];

            /* We need to correct pilots due to timing corrections */
            /* Calculate timing difference: use the one you want to correct (the current one = 0) - the reference one */
            cNewPilot =	pTimeInt->Rotate(cNewPilot, i, -iTimeDiffAccuRSI);

        }
        else /* it must be the DC carrier in mode D. Set to complex zero */
        {
            cNewPilot = _COMPLEX(_REAL(0.0),_REAL(0.0));
        }

        /* Store it in the matrix */
        matcRSIPilotStore[iRow][iPiHiIdx] = cNewPilot;
    }

    /* Is it the last symbol of the frame? If so, transfer to the CParam object for output via RSI */
    if (iSymbolCounter == Parameters.CellMappingTable.iNumSymPerFrame - 1)
    {
        /* copy into CParam object */
        Parameters.matcReceivedPilotValues.Init(iNumSymPerFrame / iScatPilTimeInt, iNumCarrier/iScatPilFreqInt + 1,
                _COMPLEX(_REAL(0.0),_REAL(0.0)));
        Parameters.matcReceivedPilotValues = matcRSIPilotStore;
        /* copy it a row at a time (vector provides an assignment operator)
        for (i=0; i<iNumSymPerFrame / iScatPilTimeInt; i++)
        	Parameters.matcReceivedPilotValues[i] = matcRSIPilotStore[i];
        */


        /* clear the local copy */
        matcRSIPilotStore.Init(iNumSymPerFrame / iScatPilTimeInt, iNumCarrier/iScatPilFreqInt + 1,
                               _COMPLEX(_REAL(0.0),_REAL(0.0)));

    }
}
