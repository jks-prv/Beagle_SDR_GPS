/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2001
 *
 * Author(s):
 *	Volker Fischer
 *
 * Description:
 *	OFDM modulation;
 *	OFDM demodulation, SNR estimation, PSD estimation
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

#include "OFDM.h"


/* Implementation *************************************************************/
/******************************************************************************\
* OFDM-modulation                                                              *
\******************************************************************************/
void COFDMModulation::ProcessDataInternal(CParameter&)
{
    int	i;

    /* Copy input vector in matlib vector and place bins at the correct
       position */
    for (i = iShiftedKmin; i < iEndIndex; i++)
        veccFFTInput[i] = (*pvecInputData)[i - iShiftedKmin];

    /* Calculate inverse fast Fourier transformation */
    veccFFTOutput = Ifft(veccFFTInput, FftPlan);

    /* Copy complex FFT output in output buffer and scale */
    for (i = 0; i < iDFTSize; i++)
        (*pvecOutputData)[i + iGuardSize] = veccFFTOutput[i] * (CReal) iDFTSize;

    /* Copy data from the end to the guard-interval (Add guard-interval) */
    for (i = 0; i < iGuardSize; i++)
        (*pvecOutputData)[i] = (*pvecOutputData)[iDFTSize + i];


    /* Shift spectrum to desired IF ----------------------------------------- */
    /* Only apply shifting if phase is not zero */
    if (cExpStep != _COMPLEX((_REAL) 1.0, (_REAL) 0.0))
    {
        for (i = 0; i < iOutputBlockSize; i++)
        {
            (*pvecOutputData)[i] = (*pvecOutputData)[i] * Conj(cCurExp);

            /* Rotate exp-pointer on step further by complex multiplication
               with precalculated rotation vector cExpStep. This saves us from
               calling sin() and cos() functions all the time (iterative
               calculation of these functions) */
            cCurExp *= cExpStep;
        }
    }
}

void COFDMModulation::InitInternal(CParameter& Parameters)
{
    Parameters.Lock();
    /* Get global parameters */
    iDFTSize = Parameters.CellMappingTable.iFFTSizeN;
    iGuardSize = Parameters.CellMappingTable.iGuardSize;
    iShiftedKmin = Parameters.CellMappingTable.iShiftedKmin;

    /* Last index */
    iEndIndex = Parameters.CellMappingTable.iShiftedKmax + 1;

    /* Normalized offset correction factor for IF shift. Subtract the
       default IF frequency ("VIRTUAL_INTERMED_FREQ") */
    const _REAL rNormCurFreqOffset = (_REAL) -2.0 * crPi *
                                     (rDefCarOffset - VIRTUAL_INTERMED_FREQ) / Parameters.GetSigSampleRate();

    /* Rotation vector for exp() calculation */
    cExpStep = _COMPLEX(cos(rNormCurFreqOffset), sin(rNormCurFreqOffset));

    /* Start with phase null (can be arbitrarily chosen) */
    cCurExp = (_REAL) 1.0;

    /* Init plans for FFT (faster processing of Fft and Ifft commands) */
    FftPlan.Init(iDFTSize);

    /* Allocate memory for intermediate result of fft. Zero out input vector
       (because only a few bins are used, the rest has to be zero) */
    veccFFTInput.Init(iDFTSize, (CReal) 0.0);
    veccFFTOutput.Init(iDFTSize);

    /* Define block-sizes for input and output */
    iInputBlockSize = Parameters.CellMappingTable.iNumCarrier;
    iOutputBlockSize = Parameters.CellMappingTable.iSymbolBlockSize;
    Parameters.Unlock();
}


/******************************************************************************\
* OFDM-demodulation                                                            *
\******************************************************************************/
void COFDMDemodulation::ProcessDataInternal(CParameter&)
{
    int i;

    /* Copy data from CVector in CMatlib vector */
    for (i = 0; i < iInputBlockSize; i++)
        veccFFTInput[i] = (*pvecInputData)[i];

    /* Calculate Fourier transformation (actual OFDM demodulation) */
    veccFFTOutput = Fft(veccFFTInput, FftPlan);

    /* Use only useful carriers and normalize with the block-size ("N"). Check
       if spectrum can be cut in one step or two steps */
    if (iShiftedKmin < 0)
    {
        /* Spectrum must be cut in two steps since some parts are on the left
           side of the DC frequency */
        for (i = iShiftedKmin; i < 0; i++)
        {
            (*pvecOutputData)[i - iShiftedKmin] =
                veccFFTOutput[iDFTSize + i] / (CReal) iDFTSize;
        }

        for (i = 0; i < iShiftedKmax + 1; i++)
        {
            (*pvecOutputData)[i - iShiftedKmin] =
                veccFFTOutput[i] / (CReal) iDFTSize;
        }
    }
    else
    {
        /* DRM spectrum is completely on the right side of the DC carrier and
           can be cut in one step */
        for (i = iShiftedKmin; i < iShiftedKmax + 1; i++)
        {
            (*pvecOutputData)[i - iShiftedKmin] =
                veccFFTOutput[i] / (CReal) iDFTSize;
        }
    }


    /* Save averaged spectrum for plotting ---------------------------------- */
    /* Average power (using power of this tap) (first order IIR filter) */
    for (i = 0; i < iLenPowSpec; i++)
        IIR1(vecrPowSpec[i], SqMag(veccFFTOutput[i]), rLamPSD);
}

void COFDMDemodulation::InitInternal(CParameter& Parameters)
{
    Parameters.Lock();
    iSampleRate = Parameters.GetSigSampleRate();
    iDFTSize = Parameters.CellMappingTable.iFFTSizeN;
    iGuardSize = Parameters.CellMappingTable.iGuardSize;
    iShiftedKmin = Parameters.CellMappingTable.iShiftedKmin;
    iShiftedKmax = Parameters.CellMappingTable.iShiftedKmax;

    /* Init plans for FFT (faster processing of Fft and Ifft commands) */
    FftPlan.Init(iDFTSize);

    /* Allocate memory for intermediate result of fftw */
    veccFFTInput.Init(iDFTSize);
    veccFFTOutput.Init(iDFTSize);

    /* Vector for power density spectrum of input signal */
    iLenPowSpec = iDFTSize / 2;
    vecrPowSpec.Init(iLenPowSpec, (_REAL) 0.0);
    rLamPSD = IIR1Lam(TICONST_PSD_EST_OFDM, (CReal) iSampleRate /
                      Parameters.CellMappingTable.iSymbolBlockSize); /* Lambda for IIR filter */


    /* Define block-sizes for input and output */
    iInputBlockSize = iDFTSize;
    iOutputBlockSize = Parameters.CellMappingTable.iNumCarrier;
    Parameters.Unlock();
}

void COFDMDemodulation::GetPowDenSpec(CVector<_REAL>& vecrData,
                                      CVector<_REAL>& vecrScale)
{
    /* Init output vectors */
    vecrData.Init(iLenPowSpec, (_REAL) 0.0);
    vecrScale.Init(iLenPowSpec, (_REAL) 0.0);

    /* Do copying of data only if vector is of non-zero length which means that
       the module was already initialized */
    if (iLenPowSpec != 0)
    {
        /* Lock resources */
        Lock();

        /* Init the constants for scale and normalization */
        const _REAL rNormData =
            (_REAL) iDFTSize * iDFTSize * _MAXSHORT * _MAXSHORT;

        const _REAL rFactorScale = _REAL(iSampleRate) / iLenPowSpec / 2000;

        /* Apply the normalization (due to the FFT) */
        for (int i = 0; i < iLenPowSpec; i++)
        {
            const _REAL rNormPowSpec = vecrPowSpec[i] / rNormData;

            if (rNormPowSpec > 0)
                vecrData[i] = (_REAL) 10.0 * log10(rNormPowSpec);
            else
                vecrData[i] = RET_VAL_LOG_0;

            vecrScale[i] = (_REAL) i * rFactorScale;
        }

        /* Release resources */
        Unlock();
    }
}


/******************************************************************************\
* OFDM-demodulation only for simulation purposes, with guard interval removal  *
\******************************************************************************/
void COFDMDemodSimulation::ProcessDataInternal(CParameter&)
{
    int i, j;
    int iEndPointGuardRemov;

    /* Guard-interval removal parameter */
    iEndPointGuardRemov = iSymbolBlockSize - iGuardSize + iStartPointGuardRemov;


    /* Regular signal *********************************************************/
    /* Convert input vector in fft-vector type and cut out the guard-interval */
    for (i = iStartPointGuardRemov; i < iEndPointGuardRemov; i++)
        veccFFTInput[i - iStartPointGuardRemov] = (*pvecInputData)[i].tOut;

    /* Actual OFDM demodulation */
    /* Calculate Fourier transformation (actual OFDM demodulation) */
    veccFFTOutput = Fft(veccFFTInput, FftPlan);

    /* Use only useful carriers and normalize with the block-size ("N") */
    for (i = iShiftedKmin; i < iShiftedKmax + 1; i++)
    {
        (*pvecOutputData2)[i - iShiftedKmin].tOut =
            veccFFTOutput[i] / (CReal) iDFTSize;
    }

    /* We need the same information for channel estimation evaluation, too */
    for (i = 0; i < iNumCarrier; i++)
        (*pvecOutputData)[i] = (*pvecOutputData2)[i].tOut;


    /* Channel-in signal ******************************************************/
    /* Convert input vector in fft-vector type and cut out the guard-interval
       We have to cut out the FFT window at the correct position, because the
       channel estimation has information only about the original pilots
       which are not phase shifted due to a timing-offset. To be able to
       compare reference signal and channel estimation output we have to use
       the synchronous reference signal for input */
    for (i = iGuardSize; i < iSymbolBlockSize; i++)
        veccFFTInput[i - iGuardSize] = (*pvecInputData)[i].tIn;

    /* Actual OFDM demodulation */
    /* Calculate Fourier transformation (actual OFDM demodulation) */
    veccFFTOutput = Fft(veccFFTInput, FftPlan);

    /* Use only useful carriers and normalize with the block-size ("N") */
    for (i = iShiftedKmin; i < iShiftedKmax + 1; i++)
    {
        (*pvecOutputData2)[i - iShiftedKmin].tIn =
            veccFFTOutput[i] / (CReal) iDFTSize;
    }


    /* Reference signal *******************************************************/
    /* Convert input vector in fft-vector type and cut out the guard-interval */
    for (i = iStartPointGuardRemov; i < iEndPointGuardRemov; i++)
        veccFFTInput[i - iStartPointGuardRemov] = (*pvecInputData)[i].tRef;

    /* Actual OFDM demodulation */
    /* Calculate Fourier transformation (actual OFDM demodulation) */
    veccFFTOutput = Fft(veccFFTInput, FftPlan);

    /* Use only useful carriers and normalize with the block-size ("N") */
    for (i = iShiftedKmin; i < iShiftedKmax + 1; i++)
    {
        (*pvecOutputData2)[i - iShiftedKmin].tRef =
            veccFFTOutput[i] / (CReal) iDFTSize;
    }


    /* Channel tap gains ******************************************************/
    /* Loop over all taps */
    for (j = 0; j < iNumTapsChan; j++)
    {
        /* Convert input vector in fft-vector type and cut out the
           guard-interval */
        for (i = iStartPointGuardRemov; i < iEndPointGuardRemov; i++)
            veccFFTInput[i - iStartPointGuardRemov] =
                (*pvecInputData)[i].veccTap[j];

        /* Actual OFDM demodulation */
        /* Calculate Fourier transformation (actual OFDM demodulation) */
        veccFFTOutput = Fft(veccFFTInput, FftPlan);

        /* Use only useful carriers and normalize with the block-size ("N") */
        for (i = 0; i < iNumCarrier; i++)
        {
            (*pvecOutputData2)[i].veccTap[j] =
                veccFFTOutput[i] / (CReal) iDFTSize;
        }

        /* Store the end of the vector, too */
        for (i = 0; i < iNumCarrier; i++)
        {
            (*pvecOutputData2)[i].veccTapBackw[j] =
                veccFFTOutput[iDFTSize - i - 1] / (CReal) iDFTSize;
        }
    }


    /* Take care of symbol IDs ---------------------------------------------- */
    iSymbolCounterTiSy++;
    if (iSymbolCounterTiSy == iNumSymPerFrame)
        iSymbolCounterTiSy = 0;

    /* Set current symbol ID in extended data of output vector */
    (*pvecOutputData).GetExData().iSymbolID = iSymbolCounterTiSy;
    (*pvecOutputData2).GetExData().iSymbolID = iSymbolCounterTiSy;

    /* No timing corrections, timing is constant in this case */
    (*pvecOutputData).GetExData().iCurTimeCorr = 0;
    (*pvecOutputData2).GetExData().iCurTimeCorr = 0;

    /* Symbol ID index is always ok */
    (*pvecOutputData).GetExData().bSymbolIDHasChanged = false;
    (*pvecOutputData2).GetExData().bSymbolIDHasChanged = false;
}

void COFDMDemodSimulation::InitInternal(CParameter& Parameters)
{
    Parameters.Lock();
    /* Set internal parameters */
    iDFTSize = Parameters.CellMappingTable.iFFTSizeN;
    iGuardSize = Parameters.CellMappingTable.iGuardSize;
    iNumCarrier = Parameters.CellMappingTable.iNumCarrier;
    iShiftedKmin = Parameters.CellMappingTable.iShiftedKmin;
    iShiftedKmax = Parameters.CellMappingTable.iShiftedKmax;
    iSymbolBlockSize = Parameters.CellMappingTable.iSymbolBlockSize;
    iNumSymPerFrame = Parameters.CellMappingTable.iNumSymPerFrame;

    iNumTapsChan = Parameters.iNumTaps;

    /* Init plans for FFT (faster processing of Fft and Ifft commands) */
    FftPlan.Init(iDFTSize);

    /* Allocate memory for intermediate result of fftw, init input signal with
       zeros because imaginary part is not written */
    veccFFTInput.Init(iDFTSize, (CReal) 0.0);
    veccFFTOutput.Init(iDFTSize);

    /* Init internal counter for symbol number. Set it to this value to get
       a "0" for the first time */
    iSymbolCounterTiSy = iNumSymPerFrame - 1;

    /* Set guard-interval removal start index. Adapt this parameter to the
       channel which was chosen. Place the delay spread centered in the
       middle of the guard-interval */
    iStartPointGuardRemov =
        (iGuardSize + Parameters.iPathDelay[iNumTapsChan - 1]) / 2;

    /* Check the case if impulse response is longer than guard-interval */
    if (iStartPointGuardRemov > iGuardSize)
        iStartPointGuardRemov = iGuardSize;

    /* Set start point of useful part extraction in global struct */
    Parameters.iOffUsfExtr = iGuardSize - iStartPointGuardRemov;

    /* Define block-sizes for input and output */
    iInputBlockSize = iSymbolBlockSize;
    iOutputBlockSize = iNumCarrier;
    iOutputBlockSize2 = iNumCarrier;

    /* We need to store as many symbols in output buffer as long the channel
       estimation delay is */
    iMaxOutputBlockSize2 = iNumCarrier * Parameters.iChanEstDelay;
    Parameters.Unlock();
}
