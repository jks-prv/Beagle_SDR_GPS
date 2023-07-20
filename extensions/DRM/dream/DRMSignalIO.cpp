/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2001
 *
 * Author(s):
 *	Volker Fischer, Cesco (HB9TLK)
 *
 * Description:
 *	Transmit and receive data
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

#include "DRMSignalIO.h"
#include "UpsampleFilter.h"
#include <iostream>
#include "sound/sound.h"
#include "sound/AudioFileIn.h"
#include "util/FileTyper.h"
#include "matlib/MatlibSigProToolbox.h"


const static int SineTable[] = { 0, 1, 0, -1, 0 };


/* Implementation *************************************************************/
/******************************************************************************\
* Transmitter                                                                  *
\******************************************************************************/
CTransmitData::CTransmitData() : pFileTransmitter(nullptr),
    pSound(nullptr),
    eOutputFormat(OF_REAL_VAL), rDefCarOffset((_REAL) VIRTUAL_INTERMED_FREQ),
    strOutFileName("test/TransmittedData.txt"), bUseSoundcard(true),
    bAmplified(false), bHighQualityIQ(false)
{

}

void CTransmitData::Stop()
{
    if(pSound!=nullptr) pSound->Close();
}

void CTransmitData::Enumerate(std::vector<std::string>& names, std::vector<std::string>& descriptions)
{
    if(pSound==nullptr) pSound = new CSoundOut;
    pSound->Enumerate(names, descriptions);
}

void CTransmitData::SetSoundInterface(string device)
{
    soundDevice = device;
    if(pSound != nullptr) {
        delete pSound;
        pSound = nullptr;
    }
    pSound = new CSoundOut();
    pSound->SetDev(device);
}

void CTransmitData::ProcessDataInternal(CParameter&)
{
    int i;

    /* Apply bandpass filter */
    BPFilter.Process(*pvecInputData);

    /* Convert vector type. Fill vector with symbols (collect them) */
    const int iNs2 = iInputBlockSize * 2;
    for (i = 0; i < iNs2; i += 2)
    {
        const int iCurIndex = iBlockCnt * iNs2 + i;
        _COMPLEX cInputData = (*pvecInputData)[i / 2];

        if (bHighQualityIQ && eOutputFormat != OF_REAL_VAL)
            HilbertFilt(cInputData);

        /* Imaginary, real */
        const _SAMPLE sCurOutReal =
            Real2Sample(cInputData.real() * rNormFactor);
        const _SAMPLE sCurOutImag =
            Real2Sample(cInputData.imag() * rNormFactor);

        /* Envelope, phase */
        const _SAMPLE sCurOutEnv =
            Real2Sample(Abs(cInputData) * (_REAL) 256.0);
        const _SAMPLE sCurOutPhase = /* 2^15 / pi / 2 -> approx. 5000 */
            Real2Sample(Angle(cInputData) * (_REAL) 5000.0);

        switch (eOutputFormat)
        {
        case OF_REAL_VAL:
            /* Use real valued signal as output for both sound card channels */
            vecsDataOut[iCurIndex] = vecsDataOut[iCurIndex + 1] = sCurOutReal;
            break;

        case OF_IQ_POS:
            /* Send inphase and quadrature (I / Q) signal to stereo sound card
               output. I: left channel, Q: right channel */
            vecsDataOut[iCurIndex] = sCurOutReal;
            vecsDataOut[iCurIndex + 1] = sCurOutImag;
            break;

        case OF_IQ_NEG:
            /* Send inphase and quadrature (I / Q) signal to stereo sound card
               output. I: right channel, Q: left channel */
            vecsDataOut[iCurIndex] = sCurOutImag;
            vecsDataOut[iCurIndex + 1] = sCurOutReal;
            break;

        case OF_EP:
            /* Send envelope and phase signal to stereo sound card
               output. Envelope: left channel, Phase: right channel */
            vecsDataOut[iCurIndex] = sCurOutEnv;
            vecsDataOut[iCurIndex + 1] = sCurOutPhase;
            break;
        }
    }

    iBlockCnt++;
    if (iBlockCnt == iNumBlocks)
        FlushData();
}

void CTransmitData::FlushData()
{
    int i;

    /* Zero the remain of the buffer, if incomplete */
    if (iBlockCnt != iNumBlocks)
    {
        const int iSize = vecsDataOut.Size();
        const int iStart = iSize * iBlockCnt / iNumBlocks;
        for (i = iStart; i < iSize; i++)
            vecsDataOut[i] = 0;
    }

    iBlockCnt = 0;

    if (bUseSoundcard)
    {
        /* Write data to sound card. Must be a blocking function */

        pSound->Write(vecsDataOut);
    }
    else
    {
        /* Write data to file */
        for (i = 0; i < iBigBlockSize; i++)
        {
#ifdef FILE_DRM_USING_RAW_DATA
            const short sOut = vecsDataOut[i];

            /* Write 2 bytes, 1 piece */
            fwrite((const void*) &sOut, size_t(2), size_t(1),
                   pFileTransmitter);
#else
            /* This can be read with Matlab "load" command */
            fprintf(pFileTransmitter, "%d\n", vecsDataOut[i]);
#endif
        }

        /* Flush the file buffer */
        fflush(pFileTransmitter);
    }
}

void CTransmitData::InitInternal(CParameter& Parameters)
{
    /*
    	float*	pCurFilt;
    	int		iNumTapsTransmFilt;
    	CReal	rNormCurFreqOffset;
    */
    /* Get signal sample rate */
    iSampleRate = Parameters.GetSigSampleRate();
    /* Define symbol block-size */
    const int iSymbolBlockSize = Parameters.CellMappingTable.iSymbolBlockSize;

    /* Init vector for storing a complete DRM frame number of OFDM symbols */
    iBlockCnt = 0;
    Parameters.Lock();
    iNumBlocks = Parameters.CellMappingTable.iNumSymPerFrame;
    ESpecOcc eSpecOcc = Parameters.GetSpectrumOccup();
    Parameters.Unlock();
    iBigBlockSize = iSymbolBlockSize * 2 /* Stereo */ * iNumBlocks;

    /* Init I/Q history */
    vecrReHist.Init(NUM_TAPS_IQ_INPUT_FILT_HQ, (_REAL) 0.0);

    vecsDataOut.Init(iBigBlockSize);

    if (pFileTransmitter != nullptr)
    {
        fclose(pFileTransmitter);
    }

    if (bUseSoundcard)
    {
        /* Init sound interface */
        if(pSound!=nullptr) pSound->Init(iSampleRate, iBigBlockSize, true);
    }
    else
    {

        /* Open file for writing data for transmitting */
#ifdef FILE_DRM_USING_RAW_DATA
        pFileTransmitter = fopen(strOutFileName.c_str(), "wb");
#else
        pFileTransmitter = fopen(strOutFileName.c_str(), "w");
#endif

        /* Check for error */
        if (pFileTransmitter == nullptr)
            throw CGenErr("The file " + strOutFileName + " cannot be created.");
    }


    /* Init bandpass filter object */
    BPFilter.Init(iSampleRate, iSymbolBlockSize, rDefCarOffset, eSpecOcc, CDRMBandpassFilt::FT_TRANSMITTER);

    /* All robustness modes and spectrum occupancies should have the same output
       power. Calculate the normalization factor based on the average power of
       symbol (the number 3000 was obtained through output tests) */
    rNormFactor = (CReal) 3000.0 / Sqrt(Parameters.CellMappingTable.rAvPowPerSymbol);

    /* Apply amplification factor, 4.0 = +12dB
       (the maximum without clipping, obtained through output tests) */
    rNormFactor *= bAmplified ? 4.0 : 1.0;

    /* Define block-size for input */
    iInputBlockSize = iSymbolBlockSize;
}

CTransmitData::~CTransmitData()
{
    /* Close file */
    if (pFileTransmitter != nullptr)
        fclose(pFileTransmitter);
}

void CTransmitData::HilbertFilt(_COMPLEX& vecData)
{
    int i;

    /* Move old data */
    for (i = 0; i < NUM_TAPS_IQ_INPUT_FILT_HQ - 1; i++)
        vecrReHist[i] = vecrReHist[i + 1];

    vecrReHist[NUM_TAPS_IQ_INPUT_FILT_HQ - 1] = vecData.real();

    /* Filter */
    _REAL rSum = (_REAL) 0.0;
    for (i = 1; i < NUM_TAPS_IQ_INPUT_FILT_HQ; i += 2)
        rSum += fHilFiltIQ_HQ[i] * vecrReHist[i];

    vecData = _COMPLEX(vecrReHist[IQ_INP_HIL_FILT_DELAY_HQ], -rSum);
}


/******************************************************************************\
* Receive data from the sound card                                             *
\******************************************************************************/

//inline _REAL sample2real(_SAMPLE s) { return _REAL(s)/32768.0; }
inline _REAL sample2real(_SAMPLE s) {
    return _REAL(s);
}

void CReceiveData::Stop()
{
    if(pSound!=nullptr) pSound->Close();
}

void CReceiveData::Enumerate(std::vector<std::string>& names, std::vector<std::string>& descriptions)
{
    if(pSound==nullptr) pSound = new CSoundIn;
    pSound->Enumerate(names, descriptions);
}

void
CReceiveData::SetSoundInterface(string device)
{
    soundDevice = device;
    if(pSound != nullptr) {
        pSound->Close();
        delete pSound;
        pSound = nullptr;
    }
    FileTyper::type t = FileTyper::resolve(device);
    if(t != FileTyper::unrecognised) {
        CAudioFileIn* pAudioFileIn = new CAudioFileIn();
        pAudioFileIn->SetFileName(device, t);
        int sr = pAudioFileIn->GetSampleRate();
        if(iSampleRate!=sr) {
            // TODO
            //cerr << "file sample rate is " << sr << endl;
            iSampleRate = sr;
        }
        pSound = pAudioFileIn;
    }
    else {
        pSound = new CSoundIn();
        pSound->SetDev(device);
    }
}

void CReceiveData::ProcessDataInternal(CParameter& Parameters)
{
    int i;

    /* OPH: update free-running symbol counter */
    Parameters.Lock();

    iFreeSymbolCounter++;
    if (iFreeSymbolCounter >= Parameters.CellMappingTable.iNumSymPerFrame * 2) /* x2 because iOutputBlockSize=iSymbolBlockSize/2 */
    {
        iFreeSymbolCounter = 0;
        /* calculate the PSD once per frame for the RSI output */

        if (Parameters.bMeasurePSD)
            PutPSD(Parameters);

    }
    Parameters.Unlock();


    /* Get data from sound interface. The read function must be a
       blocking function! */
    bool bBad = true;

    if (pSound != nullptr)
    {
        bBad = pSound->Read(vecsSoundBuffer);
    }

    Parameters.Lock();
    Parameters.ReceiveStatus.InterfaceI.SetStatus(bBad ? CRC_ERROR : RX_OK); /* Red light */
    Parameters.Unlock();

    if(bBad)
        return;

    /* Upscale if ratio greater than one */
    if (iUpscaleRatio > 1)
    {
        /* The actual upscaling, currently only 2X is supported */
        InterpFIR_2X(2, &vecsSoundBuffer[0], vecf_ZL, vecf_YL, vecf_B);
        InterpFIR_2X(2, &vecsSoundBuffer[1], vecf_ZR, vecf_YR, vecf_B);

        /* Write data to output buffer. Do not set the switch command inside
           the for-loop for efficiency reasons */
        switch (eInChanSelection)
        {
        case CS_LEFT_CHAN:
            for (i = 0; i < iOutputBlockSize; i++)
                (*pvecOutputData)[i] = _REAL(vecf_YL[unsigned(i)]);
            break;

        case CS_RIGHT_CHAN:
            for (i = 0; i < iOutputBlockSize; i++)
                (*pvecOutputData)[i] = _REAL(vecf_YR[unsigned(i)]);
            break;

        case CS_MIX_CHAN:
            for (i = 0; i < iOutputBlockSize; i++)
            {
                /* Mix left and right channel together */
                (*pvecOutputData)[i] = _REAL(vecf_YL[unsigned(i)] + vecf_YR[unsigned(i)]) / 2.0;
            }
            break;

        case CS_SUB_CHAN:
            for (i = 0; i < iOutputBlockSize; i++)
            {
                /* Subtract right channel from left */
                (*pvecOutputData)[i] = (vecf_YL[unsigned(i)] - vecf_YR[unsigned(i)]) / 2;
            }
            break;

        /* I / Q input */
        case CS_IQ_POS:
            for (i = 0; i < iOutputBlockSize; i++)
            {
                (*pvecOutputData)[i] =
                    HilbertFilt(vecf_YL[unsigned(i)], vecf_YR[unsigned(i)]);
            }
            break;

        case CS_IQ_NEG:
            for (i = 0; i < iOutputBlockSize; i++)
            {
                (*pvecOutputData)[i] =
                    HilbertFilt(vecf_YR[unsigned(i)], vecf_YL[unsigned(i)]);
            }
            break;

        case CS_IQ_POS_ZERO:
            for (i = 0; i < iOutputBlockSize; i++)
            {
                /* Shift signal to vitual intermediate frequency before applying
                   the Hilbert filtering */
                _COMPLEX cCurSig = _COMPLEX(vecf_YL[unsigned(i)], vecf_YR[unsigned(i)]);

                cCurSig *= cCurExp;

                /* Rotate exp-pointer on step further by complex multiplication
                   with precalculated rotation vector cExpStep */
                cCurExp *= cExpStep;

                (*pvecOutputData)[i] =
                    HilbertFilt(cCurSig.real(), cCurSig.imag());
            }
            break;

        case CS_IQ_NEG_ZERO:
            for (i = 0; i < iOutputBlockSize; i++)
            {
                /* Shift signal to vitual intermediate frequency before applying
                   the Hilbert filtering */
                _COMPLEX cCurSig = _COMPLEX(vecf_YR[unsigned(i)], vecf_YL[unsigned(i)]);

                cCurSig *= cCurExp;

                /* Rotate exp-pointer on step further by complex multiplication
                   with precalculated rotation vector cExpStep */
                cCurExp *= cExpStep;

                (*pvecOutputData)[i] =
                    HilbertFilt(cCurSig.real(), cCurSig.imag());
            }
            break;

        case CS_IQ_POS_SPLIT:
            for (i = 0; i < iOutputBlockSize; i += 4)
            {
                (*pvecOutputData)[i + 0] =  vecf_YL[i + 0];
                (*pvecOutputData)[i + 1] = -vecf_YR[i + 1];
                (*pvecOutputData)[i + 2] = -vecf_YL[i + 2];
                (*pvecOutputData)[i + 3] =  vecf_YR[i + 3];
            }
            break;

        case CS_IQ_NEG_SPLIT:
            for (i = 0; i < iOutputBlockSize; i += 4)
            {
                (*pvecOutputData)[i + 0] =  vecf_YR[i + 0];
                (*pvecOutputData)[i + 1] = -vecf_YL[i + 1];
                (*pvecOutputData)[i + 2] = -vecf_YR[i + 2];
                (*pvecOutputData)[i + 3] =  vecf_YL[i + 3];
            }
            break;
        }
    }

    /* Upscale ratio equal to one */
    else {
        /* Write data to output buffer. Do not set the switch command inside
           the for-loop for efficiency reasons */
        switch (eInChanSelection)
        {
        case CS_LEFT_CHAN:
            for (i = 0; i < iOutputBlockSize; i++)
                (*pvecOutputData)[i] = sample2real(vecsSoundBuffer[2 * i]);
            break;

        case CS_RIGHT_CHAN:
            for (i = 0; i < iOutputBlockSize; i++)
                (*pvecOutputData)[i] = sample2real(vecsSoundBuffer[2 * i + 1]);
            break;

        case CS_MIX_CHAN:
            for (i = 0; i < iOutputBlockSize; i++)
            {
                /* Mix left and right channel together */
                const _REAL rLeftChan = sample2real(vecsSoundBuffer[2 * i]);
                const _REAL rRightChan = sample2real(vecsSoundBuffer[2 * i + 1]);
                (*pvecOutputData)[i] = (rLeftChan + rRightChan) / 2;
            }
            break;

        case CS_SUB_CHAN:
            for (i = 0; i < iOutputBlockSize; i++)
            {
                /* Subtract right channel from left */
                const _REAL rLeftChan = sample2real(vecsSoundBuffer[2 * i]);
                const _REAL rRightChan = sample2real(vecsSoundBuffer[2 * i + 1]);
                (*pvecOutputData)[i] = (rLeftChan - rRightChan) / 2;
            }
            break;

        /* I / Q input */
        case CS_IQ_POS:
            for (i = 0; i < iOutputBlockSize; i++)
            {
                (*pvecOutputData)[i] =
                    HilbertFilt(sample2real(vecsSoundBuffer[2 * i]),
                                sample2real(vecsSoundBuffer[2 * i + 1]));
            }
            break;

        case CS_IQ_NEG:
            for (i = 0; i < iOutputBlockSize; i++)
            {
                (*pvecOutputData)[i] =
                    HilbertFilt(sample2real(vecsSoundBuffer[2 * i + 1]),
                                sample2real(vecsSoundBuffer[2 * i]));
            }
            break;

        case CS_IQ_POS_ZERO:
            for (i = 0; i < iOutputBlockSize; i++)
            {
                /* Shift signal to vitual intermediate frequency before applying
                   the Hilbert filtering */
                _COMPLEX cCurSig = _COMPLEX(sample2real(vecsSoundBuffer[2 * i]),
                                            sample2real(vecsSoundBuffer[2 * i + 1]));

                cCurSig *= cCurExp;

                /* Rotate exp-pointer on step further by complex multiplication
                   with precalculated rotation vector cExpStep */
                cCurExp *= cExpStep;

                (*pvecOutputData)[i] =
                    HilbertFilt(cCurSig.real(), cCurSig.imag());
            }
            break;

        case CS_IQ_NEG_ZERO:
            for (i = 0; i < iOutputBlockSize; i++)
            {
                /* Shift signal to vitual intermediate frequency before applying
                   the Hilbert filtering */
                _COMPLEX cCurSig = _COMPLEX(sample2real(vecsSoundBuffer[2 * i + 1]),
                                            sample2real(vecsSoundBuffer[2 * i]));

                cCurSig *= cCurExp;

                /* Rotate exp-pointer on step further by complex multiplication
                   with precalculated rotation vector cExpStep */
                cCurExp *= cExpStep;

                (*pvecOutputData)[i] =
                    HilbertFilt(cCurSig.real(), cCurSig.imag());
            }
            break;

        case CS_IQ_POS_SPLIT: /* Require twice the bandwidth */
            for (i = 0; i < iOutputBlockSize; i++)
            {
                iPhase = (iPhase + 1) & 3;
                _REAL rValue = vecsSoundBuffer[2 * i]     * /*COS*/SineTable[iPhase + 1] -
                               vecsSoundBuffer[2 * i + 1] * /*SIN*/SineTable[iPhase];
                (*pvecOutputData)[i] = rValue;
            }
            break;

        case CS_IQ_NEG_SPLIT: /* Require twice the bandwidth */
            for (i = 0; i < iOutputBlockSize; i++)
            {
                iPhase = (iPhase + 1) & 3;
                _REAL rValue = vecsSoundBuffer[2 * i + 1] * /*COS*/SineTable[iPhase + 1] -
                               vecsSoundBuffer[2 * i]     * /*SIN*/SineTable[iPhase];
                (*pvecOutputData)[i] = rValue;
            }
            break;
        }
    }

    /* Flip spectrum if necessary ------------------------------------------- */
    if (bFippedSpectrum)
    {
        /* Since iOutputBlockSize is always even we can do some opt. here */
        for (i = 0; i < iOutputBlockSize; i+=2)
        {
            /* We flip the spectrum by using the mirror spectrum at the negative
               frequencies. If we shift by half of the sample frequency, we can
               do the shift without the need of a Hilbert transformation */
            (*pvecOutputData)[i] = -(*pvecOutputData)[i];
        }
    }

    /* Copy data in buffer for spectrum calculation */
    mutexInpData.Lock();
    vecrInpData.AddEnd((*pvecOutputData), iOutputBlockSize);
    mutexInpData.Unlock();

    /* Update level meter */
    SignalLevelMeter.Update((*pvecOutputData));
    Parameters.Lock();
    Parameters.SetIFSignalLevel(SignalLevelMeter.Level());
    Parameters.Unlock();

}

void CReceiveData::InitInternal(CParameter& Parameters)
{
    /* Init sound interface. Set it to one symbol. The sound card interface
    	   has to taken care about the buffering data of a whole MSC block.
    	   Use stereo input (* 2) */

    if (pSound == nullptr)
        return;

    Parameters.Lock();
    /* We define iOutputBlockSize as half the iSymbolBlockSize because
       if a positive frequency offset is present in drm signal,
       after some time a buffer overflow occur in the output buffer of
       InputResample.ProcessData() */
    /* Define output block-size */
    iOutputBlockSize = Parameters.CellMappingTable.iSymbolBlockSize / 2;
    iMaxOutputBlockSize = iOutputBlockSize * 2;
    /* Get signal sample rate */
    iSampleRate = Parameters.GetSigSampleRate();
    iUpscaleRatio = Parameters.GetSigUpscaleRatio();
    Parameters.Unlock();

    const int iOutputBlockAlignment = iOutputBlockSize & 3;
    if (iOutputBlockAlignment) {
        fprintf(stderr, "CReceiveData::InitInternal(): iOutputBlockAlignment = %i\n", iOutputBlockAlignment);
    }

    try {
        //printf("CReceiveData::InitInternal iSampleRate=%d iUpscaleRatio=%d iOutputBlockSize=%d\n", iSampleRate, iUpscaleRatio, iOutputBlockSize);
        const bool bChanged = (pSound == nullptr)? true : pSound->Init(iSampleRate / iUpscaleRatio, iOutputBlockSize * 2 / iUpscaleRatio, true);

        /* Clear input data buffer on change samplerate change */
        if (bChanged)
            ClearInputData();

        /* Init 2X upscaler if enabled */
        if (iUpscaleRatio > 1)
        {
            const int taps = (NUM_TAPS_UPSAMPLE_FILT + 3) & ~3;
            vecf_B.resize(taps, 0.0f);
            for (unsigned i = 0; i < NUM_TAPS_UPSAMPLE_FILT; i++)
                vecf_B[i] = float(dUpsampleFilt[i] * iUpscaleRatio);
            if (bChanged)
            {
                vecf_ZL.resize(0);
                vecf_ZR.resize(0);
            }
            vecf_ZL.resize(unsigned(iOutputBlockSize + taps) / 2, 0.0f);
            vecf_ZR.resize(unsigned(iOutputBlockSize + taps) / 2, 0.0f);
            vecf_YL.resize(unsigned(iOutputBlockSize));
            vecf_YR.resize(unsigned(iOutputBlockSize));
        }
        else
        {
            vecf_B.resize(0);
            vecf_YL.resize(0);
            vecf_YR.resize(0);
            vecf_ZL.resize(0);
            vecf_ZR.resize(0);
        }

        /* Init buffer size for taking stereo input */
        vecsSoundBuffer.Init(iOutputBlockSize * 2 / iUpscaleRatio);

        /* Init signal meter */
        SignalLevelMeter.Init(0);

        /* Inits for I / Q input, only if it is not already
           to keep the history intact */
        if (vecrReHist.Size() != NUM_TAPS_IQ_INPUT_FILT || bChanged)
        {
            vecrReHist.Init(NUM_TAPS_IQ_INPUT_FILT, 0.0);
            vecrImHist.Init(NUM_TAPS_IQ_INPUT_FILT, 0.0);
        }

        /* Start with phase null (can be arbitrarily chosen) */
        cCurExp = 1.0;

        /* Set rotation vector to mix signal from zero frequency to virtual
           intermediate frequency */
        const _REAL rNormCurFreqOffsetIQ = 2.0 * crPi * _REAL(VIRTUAL_INTERMED_FREQ / iSampleRate);

        cExpStep = _COMPLEX(cos(rNormCurFreqOffsetIQ), sin(rNormCurFreqOffsetIQ));


        /* OPH: init free-running symbol counter */
        iFreeSymbolCounter = 0;

    }
    catch (CGenErr GenErr)
    {
        pSound = nullptr;
    }
    catch (string strError)
    {
        pSound = nullptr;
    }
}

_REAL CReceiveData::HilbertFilt(const _REAL rRe, const _REAL rIm)
{
    /*
    	Hilbert filter for I / Q input data. This code is based on code written
    	by Cesco (HB9TLK)
    */

    /* Move old data */
    for (int i = 0; i < NUM_TAPS_IQ_INPUT_FILT - 1; i++)
    {
        vecrReHist[i] = vecrReHist[i + 1];
        vecrImHist[i] = vecrImHist[i + 1];
    }

    vecrReHist[NUM_TAPS_IQ_INPUT_FILT - 1] = rRe;
    vecrImHist[NUM_TAPS_IQ_INPUT_FILT - 1] = rIm;

    /* Filter */
    _REAL rSum = 0.0;
    for (unsigned i = 1; i < NUM_TAPS_IQ_INPUT_FILT; i += 2)
        rSum += _REAL(fHilFiltIQ[i]) * vecrImHist[int(i)];

    return (rSum + vecrReHist[IQ_INP_HIL_FILT_DELAY]) / 2;
}

void CReceiveData::InterpFIR_2X(const int channels, _SAMPLE* X, vector<float>& Z, vector<float>& Y, vector<float>& B)
{
    /*
        2X interpolating filter. When combined with CS_IQ_POS_SPLIT or CS_IQ_NEG_SPLIT
        input data mode, convert I/Q input to full bandwidth, code by David Flamand
    */
    int i, j;
    const int B_len = int(B.size());
    const int Z_len = int(Z.size());
    const int Y_len = int(Y.size());
    const int Y_len_2 = Y_len / 2;
    float *B_beg_ptr = &B[0];
    float *Z_beg_ptr = &Z[0];
    float *Y_ptr = &Y[0];
    float *B_end_ptr, *B_ptr, *Z_ptr;
    float y0, y1, y2, y3;

    /* Check for size and alignment requirement */
    if ((B_len & 3) || (Z_len != (B_len/2 + Y_len_2)) || (Y_len & 1))
        return;

    /* Copy the old history at the end */
    for (i = B_len/2-1; i >= 0; i--)
        Z_beg_ptr[Y_len_2 + i] = Z_beg_ptr[i];

    /* Copy the new sample at the beginning of the history */
    for (i = 0, j = 0; i < Y_len_2; i++, j+=channels)
        Z_beg_ptr[Y_len_2 - i - 1] = X[j];

    /* The actual lowpass filtering using FIR */
    for (i = Y_len_2-1; i >= 0; i--)
    {
        B_end_ptr  = B_beg_ptr + B_len;
        B_ptr      = B_beg_ptr;
        Z_ptr      = Z_beg_ptr + i;
        y0 = y1 = y2 = y3 = 0.0f;
        while (B_ptr != B_end_ptr)
        {
            y0 = y0 + B_ptr[0] * Z_ptr[0];
            y1 = y1 + B_ptr[1] * Z_ptr[0];
            y2 = y2 + B_ptr[2] * Z_ptr[1];
            y3 = y3 + B_ptr[3] * Z_ptr[1];
            B_ptr += 4;
            Z_ptr += 2;
        }
        *Y_ptr++ = y0 + y2;
        *Y_ptr++ = y1 + y3;
    }
}

CReceiveData::~CReceiveData()
{
}

/*
    Convert Real to I/Q frequency when bInvert is false
    Convert I/Q to Real frequency when bInvert is true
*/
_REAL CReceiveData::ConvertFrequency(_REAL rFrequency, bool bInvert) const
{
    const int iInvert = bInvert ? -1 : 1;

    if (eInChanSelection == CS_IQ_POS_SPLIT ||
            eInChanSelection == CS_IQ_NEG_SPLIT)
        rFrequency -= iSampleRate / 4 * iInvert;
    else if (eInChanSelection == CS_IQ_POS_ZERO ||
             eInChanSelection == CS_IQ_NEG_ZERO)
        rFrequency -= VIRTUAL_INTERMED_FREQ * iInvert;

    return rFrequency;
}

void CReceiveData::GetInputSpec(CVector<_REAL>& vecrData,
                                CVector<_REAL>& vecrScale)
{
    int i;

    /* Length of spectrum vector including Nyquist frequency */
    const int iLenSpecWithNyFreq = NUM_SMPLS_4_INPUT_SPECTRUM / 2 + 1;

    /* Init input and output vectors */
    vecrData.Init(iLenSpecWithNyFreq, 0.0);
    vecrScale.Init(iLenSpecWithNyFreq, 0.0);

    /* Init the constants for scale and normalization */
    const bool bNegativeFreq =
        eInChanSelection == CS_IQ_POS_SPLIT ||
        eInChanSelection == CS_IQ_NEG_SPLIT;

    const bool bOffsetFreq =
        eInChanSelection == CS_IQ_POS_ZERO ||
        eInChanSelection == CS_IQ_NEG_ZERO;

    const int iOffsetScale =
        bNegativeFreq ? iLenSpecWithNyFreq / 2 :
        (bOffsetFreq ? iLenSpecWithNyFreq * VIRTUAL_INTERMED_FREQ
         / (iSampleRate / 2) : 0);

    const _REAL rFactorScale = _REAL(iSampleRate / iLenSpecWithNyFreq / 2000);

    /* The calibration factor was determined experimentaly,
       give 0 dB for a full scale sine wave input (0 dBFS) */
    const _REAL rDataCalibrationFactor = 18.49;

    const _REAL rNormData = rDataCalibrationFactor /
                            (_REAL(_MAXSHORT * _MAXSHORT) *
                             NUM_SMPLS_4_INPUT_SPECTRUM *
                             NUM_SMPLS_4_INPUT_SPECTRUM);

    /* Copy data from shift register in Matlib vector */
    CRealVector vecrFFTInput(NUM_SMPLS_4_INPUT_SPECTRUM);
    mutexInpData.Lock();
    for (i = 0; i < NUM_SMPLS_4_INPUT_SPECTRUM; i++)
        vecrFFTInput[i] = vecrInpData[i];
    mutexInpData.Unlock();

    /* Get squared magnitude of spectrum */
    CRealVector vecrSqMagSpect(iLenSpecWithNyFreq);
    CFftPlans FftPlans;
    vecrSqMagSpect =
        SqMag(rfft(vecrFFTInput * Hann(NUM_SMPLS_4_INPUT_SPECTRUM), FftPlans));

    /* Log power spectrum data */
    for (i = 0; i < iLenSpecWithNyFreq; i++)
    {
        const _REAL rNormSqMag = vecrSqMagSpect[i] * rNormData;

        if (rNormSqMag > 0)
            vecrData[i] = 10.0 * log10(rNormSqMag);
        else
            vecrData[i] = RET_VAL_LOG_0;

        vecrScale[i] = _REAL(i - iOffsetScale) * rFactorScale;
    }

}

void CReceiveData::GetInputPSD(CVector<_REAL>& vecrData,
                               CVector<_REAL>& vecrScale,
                               const int iLenPSDAvEachBlock,
                               const int iNumAvBlocksPSD,
                               const int iPSDOverlap)
{
    CalculatePSD(vecrData, vecrScale, iLenPSDAvEachBlock,iNumAvBlocksPSD,iPSDOverlap);
}

void CReceiveData::CalculatePSD(CVector<_REAL>& vecrData,
                                CVector<_REAL>& vecrScale,
                                const int iLenPSDAvEachBlock,
                                const int iNumAvBlocksPSD,
                                const int iPSDOverlap)
{
    /* Length of spectrum vector including Nyquist frequency */
    const int iLenSpecWithNyFreq = iLenPSDAvEachBlock / 2 + 1;

    /* Init input and output vectors */
    vecrData.Init(iLenSpecWithNyFreq, 0.0);
    vecrScale.Init(iLenSpecWithNyFreq, 0.0);

    /* Init the constants for scale and normalization */
    const bool bNegativeFreq =
        eInChanSelection == CS_IQ_POS_SPLIT ||
        eInChanSelection == CS_IQ_NEG_SPLIT;

    const bool bOffsetFreq =
        eInChanSelection == CS_IQ_POS_ZERO ||
        eInChanSelection == CS_IQ_NEG_ZERO;

    const int iOffsetScale =
        bNegativeFreq ? iLenSpecWithNyFreq / 2 :
        (bOffsetFreq ? iLenSpecWithNyFreq * VIRTUAL_INTERMED_FREQ
         / (iSampleRate / 2) : 0);

    const _REAL rFactorScale = _REAL(iSampleRate / iLenSpecWithNyFreq / 2000);

    const _REAL rNormData = _REAL( _MAXSHORT * _MAXSHORT *
                            iLenPSDAvEachBlock * iLenPSDAvEachBlock *
                            iNumAvBlocksPSD * _REAL(PSD_WINDOW_GAIN));

    /* Init intermediate vectors */
    CRealVector vecrAvSqMagSpect(iLenSpecWithNyFreq, 0.0);
    CRealVector vecrFFTInput(iLenPSDAvEachBlock);

    /* Init Hamming window */
    CRealVector vecrHammWin(Hamming(iLenPSDAvEachBlock));

    /* Calculate FFT of each small block and average results (estimation
       of PSD of input signal) */
    CFftPlans FftPlans;
    int i;
    mutexInpData.Lock();
    for (i = 0; i < iNumAvBlocksPSD; i++)
    {
        /* Copy data from shift register in Matlib vector */
        for (int j = 0; j < iLenPSDAvEachBlock; j++)
            vecrFFTInput[j] = vecrInpData[j + i * (iLenPSDAvEachBlock - iPSDOverlap)];

        /* Apply Hamming window */
        vecrFFTInput *= vecrHammWin;

        /* Calculate squared magnitude of spectrum and average results */
        vecrAvSqMagSpect += SqMag(rfft(vecrFFTInput, FftPlans));
    }
    mutexInpData.Unlock();

    /* Log power spectrum data */
    for (i = 0; i <iLenSpecWithNyFreq; i++)
    {
        const _REAL rNormSqMag = vecrAvSqMagSpect[i] / rNormData;

        if (rNormSqMag > 0)
            vecrData[i] = 10.0 * log10(rNormSqMag);
        else
            vecrData[i] = RET_VAL_LOG_0;

        vecrScale[i] = _REAL(i - iOffsetScale) * rFactorScale;
    }
}

/* Calculate PSD and put it into the CParameter class.
 * The data will be used by the rsi output.
 * This function is called in a context where the Parameters structure is Locked.
 */
void CReceiveData::PutPSD(CParameter &Parameters)
{
    int i, j;

    CVector<_REAL>		vecrData;
    CVector<_REAL>		vecrScale;

    CalculatePSD(vecrData, vecrScale, LEN_PSD_AV_EACH_BLOCK_RSI, NUM_AV_BLOCKS_PSD_RSI, PSD_OVERLAP_RSI);

    /* Data required for rpsd tag */
    /* extract the values from -8kHz to +8kHz/18kHz relative to 12kHz, i.e. 4kHz to 20kHz */
    /*const int startBin = 4000.0 * LEN_PSD_AV_EACH_BLOCK_RSI /iSampleRate;
    const int endBin = 20000.0 * LEN_PSD_AV_EACH_BLOCK_RSI /iSampleRate;*/
    /* The above calculation doesn't round in the way FhG expect. Probably better to specify directly */

    /* For 20k mode, we need -8/+18, which is more than the Nyquist rate of 24kHz. */
    /* Assume nominal freq = 7kHz (i.e. 2k to 22k) and pad with zeroes (roughly 1kHz each side) */

    int iStartBin = 22;
    int iEndBin = 106;
    int iVecSize = iEndBin - iStartBin + 1; //85

    //_REAL rIFCentreFrequency = Parameters.FrontEndParameters.rIFCentreFreq;

    ESpecOcc eSpecOcc = Parameters.GetSpectrumOccup();
    if (eSpecOcc == SO_4 || eSpecOcc == SO_5)
    {
        iStartBin = 0;
        iEndBin = 127;
        iVecSize = 139;
    }
    /* Line up the the middle of the vector with the quarter-Nyquist bin of FFT */
    int iStartIndex = iStartBin - (LEN_PSD_AV_EACH_BLOCK_RSI/4) + (iVecSize-1)/2;

    /* Fill with zeros to start with */
    Parameters.vecrPSD.Init(iVecSize, 0.0);

    for (i=iStartIndex, j=iStartBin; j<=iEndBin; i++,j++)
        Parameters.vecrPSD[i] = vecrData[j];

    CalculateSigStrengthCorrection(Parameters, vecrData);

    CalculatePSDInterferenceTag(Parameters, vecrData);

}

/*
 * This function is called in a context where the Parameters structure is Locked.
 */
void CReceiveData::CalculateSigStrengthCorrection(CParameter &Parameters, CVector<_REAL> &vecrPSD)
{

    _REAL rCorrection = _REAL(0.0);

    /* Calculate signal power in measurement bandwidth */

    _REAL rFreqKmin, rFreqKmax;

    _REAL rIFCentreFrequency = Parameters.FrontEndParameters.rIFCentreFreq;

    if (Parameters.GetAcquiState() == AS_WITH_SIGNAL &&
            Parameters.FrontEndParameters.bAutoMeasurementBandwidth)
    {
        // Receiver is locked, so measure in the current DRM signal bandwidth Kmin to Kmax
        _REAL rDCFrequency = Parameters.GetDCFrequency();
        rFreqKmin = rDCFrequency + _REAL(Parameters.CellMappingTable.iCarrierKmin)/Parameters.CellMappingTable.iFFTSizeN * iSampleRate;
        rFreqKmax = rDCFrequency + _REAL(Parameters.CellMappingTable.iCarrierKmax)/Parameters.CellMappingTable.iFFTSizeN * iSampleRate;
    }
    else
    {
        // Receiver unlocked, or measurement is requested in fixed bandwidth
        _REAL rMeasBandwidth = Parameters.FrontEndParameters.rDefaultMeasurementBandwidth;
        rFreqKmin = rIFCentreFrequency - rMeasBandwidth/_REAL(2.0);
        rFreqKmax = rIFCentreFrequency + rMeasBandwidth/_REAL(2.0);
    }

    _REAL rSigPower = CalcTotalPower(vecrPSD, FreqToBin(rFreqKmin), FreqToBin(rFreqKmax));

    if (Parameters.FrontEndParameters.eSMeterCorrectionType == CFrontEndParameters::S_METER_CORRECTION_TYPE_AGC_ONLY)
    {
        /* Write it to the receiver params to help with calculating the signal strength */
        rCorrection += _REAL(10.0) * log10(rSigPower);
    }
    else if (Parameters.FrontEndParameters.eSMeterCorrectionType == CFrontEndParameters::S_METER_CORRECTION_TYPE_AGC_RSSI)
    {
        _REAL rSMeterBandwidth = Parameters.FrontEndParameters.rSMeterBandwidth;

        _REAL rFreqSMeterMin = _REAL(rIFCentreFrequency - rSMeterBandwidth / _REAL(2.0));
        _REAL rFreqSMeterMax = _REAL(rIFCentreFrequency + rSMeterBandwidth / _REAL(2.0));

        _REAL rPowerInSMeterBW = CalcTotalPower(vecrPSD, FreqToBin(rFreqSMeterMin), FreqToBin(rFreqSMeterMax));

        /* Write it to the receiver params to help with calculating the signal strength */

        rCorrection += _REAL(10.0) * log10(rSigPower/rPowerInSMeterBW);
    }

    /* Add on the calibration factor for the current mode */
    if (Parameters.GetReceiverMode() == RM_DRM)
        rCorrection += Parameters.FrontEndParameters.rCalFactorDRM;
    else if (Parameters.GetReceiverMode() == RM_AM)
        rCorrection += Parameters.FrontEndParameters.rCalFactorAM;

    Parameters.rSigStrengthCorrection = rCorrection;

    return;
}

/*
 * This function is called in a context where the Parameters structure is Locked.
 */
void CReceiveData::CalculatePSDInterferenceTag(CParameter &Parameters, CVector<_REAL> &vecrPSD)
{

    /* Interference tag (rnip) */

    // Calculate search range: defined as +/-5.1kHz except if locked and in 20k
    _REAL rIFCentreFrequency = Parameters.FrontEndParameters.rIFCentreFreq;

    _REAL rFreqSearchMin = rIFCentreFrequency - _REAL(RNIP_SEARCH_RANGE_NARROW);
    _REAL rFreqSearchMax = rIFCentreFrequency + _REAL(RNIP_SEARCH_RANGE_NARROW);

    ESpecOcc eSpecOcc = Parameters.GetSpectrumOccup();

    if (Parameters.GetAcquiState() == AS_WITH_SIGNAL &&
            (eSpecOcc == SO_4 || eSpecOcc == SO_5) )
    {
        rFreqSearchMax = rIFCentreFrequency + _REAL(RNIP_SEARCH_RANGE_WIDE);
    }
    int iSearchStartBin = FreqToBin(rFreqSearchMin);
    int iSearchEndBin = FreqToBin(rFreqSearchMax);

    if (iSearchStartBin < 0) iSearchStartBin = 0;
    if (iSearchEndBin > LEN_PSD_AV_EACH_BLOCK_RSI/2)
        iSearchEndBin = LEN_PSD_AV_EACH_BLOCK_RSI/2;

    _REAL rMaxPSD = _REAL(-1000.0);
    int iMaxPSDBin = 0;

    for (int i=iSearchStartBin; i<=iSearchEndBin; i++)
    {
        _REAL rPSD = _REAL(2.0) * pow(_REAL(10.0), vecrPSD[i]/_REAL(10.0));
        if (rPSD > rMaxPSD)
        {
            rMaxPSD = rPSD;
            iMaxPSDBin = i;
        }
    }

    // For total signal power, exclude the biggest one and e.g. 2 either side
    int iExcludeStartBin = iMaxPSDBin - RNIP_EXCLUDE_BINS;
    int iExcludeEndBin = iMaxPSDBin + RNIP_EXCLUDE_BINS;

    // Calculate power. TotalPower() function will deal with start>end correctly
    _REAL rSigPowerExcludingInterferer = CalcTotalPower(vecrPSD, iSearchStartBin, iExcludeStartBin-1) +
                                         CalcTotalPower(vecrPSD, iExcludeEndBin+1, iSearchEndBin);

    /* interferer level wrt signal power */
    Parameters.rMaxPSDwrtSig = _REAL(10.0) * log10(rMaxPSD / rSigPowerExcludingInterferer);

    /* interferer frequency */
    Parameters.rMaxPSDFreq = _REAL(iMaxPSDBin) * _REAL(iSampleRate) / _REAL(LEN_PSD_AV_EACH_BLOCK_RSI) - rIFCentreFrequency;

}


int CReceiveData::FreqToBin(_REAL rFreq)
{
    return int(rFreq/iSampleRate * LEN_PSD_AV_EACH_BLOCK_RSI);
}

_REAL CReceiveData::CalcTotalPower(CVector<_REAL> &vecrData, int iStartBin, int iEndBin)
{
    if (iStartBin < 0) iStartBin = 0;
    if (iEndBin > LEN_PSD_AV_EACH_BLOCK_RSI/2)
        iEndBin = LEN_PSD_AV_EACH_BLOCK_RSI/2;

    _REAL rSigPower = _REAL(0.0);
    for (int i=iStartBin; i<=iEndBin; i++)
    {
        _REAL rPSD = pow(_REAL(10.0), vecrData[i]/_REAL(10.0));
        // The factor of 2 below is needed because half of the power is in the negative frequencies
        rSigPower += rPSD * _REAL(2.0);
    }

    return rSigPower;
}
