/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2001
 *
 * Author(s):
 *	Volker Fischer
 *
 * Description:
 *	Audio source encoder/decoder
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

#include <DRM_main.h>
#include "AudioSourceDecoder.h"
#include "printf.h"
#include "timer.h"

#include <iostream>
#include <sstream>
#include <string>
#include <algorithm>
#include <cmath>
#ifdef _WIN32
# include <direct.h>
#else
# include <sys/stat.h>
# include <sys/types.h>
#endif
#include <unistd.h>

#include "../MSC/aacsuperframe.h"
#include "../MSC/xheaacsuperframe.h"

/* Implementation *************************************************************/

CAudioSourceDecoder::CAudioSourceDecoder()
    :	bWriteToFile(false), TextMessage(false),
      bUseReverbEffect(true), codec(nullptr)
{
    /* Initialize Audio Codec List */
    CAudioCodec::InitCodecList();

    /* Needed by fdrmdialog.cpp to report missing codec */
    bCanDecodeAAC  = CAudioCodec::GetDecoder(CAudioParam::AC_AAC,  true) != nullptr;
    bCanDecodexHE_AAC = CAudioCodec::GetDecoder(CAudioParam::AC_xHE_AAC, true) != nullptr;
    bCanDecodeOPUS = CAudioCodec::GetDecoder(CAudioParam::AC_OPUS, true) != nullptr;
}

CAudioSourceDecoder::~CAudioSourceDecoder()
{
    /* Unreference Audio Codec List */
    CAudioCodec::UnrefCodecList();
}



void
CAudioSourceDecoder::ProcessDataInternal(CParameter & Parameters)
{
    bool bCurBlockOK;
    bool bGoodValues = false;


    Parameters.Lock();
    Parameters.vecbiAudioFrameStatus.Init(0);
    Parameters.vecbiAudioFrameStatus.ResetBitAccess();
    Parameters.Unlock();

    /* Check if something went wrong in the initialization routine */
    if (DoNotProcessData)
    {
        return;
    }

    //cerr << "got one logical frame of length " << pvecInputData->Size() << " bits" << endl;

    /* Text Message ********************************************************** */
    if (bTextMessageUsed)
    {
        /* Decode last four bytes of input block for text message */
        for (int i = 0; i < SIZEOF__BYTE * NUM_BYTES_TEXT_MESS_IN_AUD_STR; i++)
            vecbiTextMessBuf[i] = (*pvecInputData)[iInputBlockSize - SIZEOF__BYTE * NUM_BYTES_TEXT_MESS_IN_AUD_STR + i];

        TextMessage.Decode(vecbiTextMessBuf);
    }

    /* Audio data header parsing ********************************************* */
    /* Check if audio shall not be decoded */
    if (DoNotProcessAudDecoder)
    {
        return;
    }

    /* Reset bit extraction access */
    (*pvecInputData).ResetBitAccess();

    try {
        bGoodValues = pAudioSuperFrame->parse(*pvecInputData);
    } catch(std::exception& e) {
        printf("DRM SuperFrame parse EXCEPTION %s\n", e.what());
        bGoodValues = false;
    }

    /* Audio decoding ******************************************************** */
    /* Init output block size to zero, this variable is also used for
       determining the position for writing the output vector */
    iOutputBlockSize = 0;
    int iResOutBlockSize = 0;
    //cerr << "audio superframe with " << pAudioSuperFrame->getNumFrames() << " frames" << endl;
    //real_printf("%s ", inputSampleRate? "OK":"ZERO!"); fflush(stdout);
    //real_printf(">%d ", inputSampleRate); fflush(stdout);

    int num_frames = pAudioSuperFrame->getNumFrames();
    //u4_t start = timer_us();
    for (size_t j = 0; j < num_frames; j++)
    {
        bool bCodecUpdated = false;
        bool bCurBlockFaulty = false; // just for Opus or any other codec with FEC

        drm_next_task("superFrame");
        
        if (bGoodValues)
        {
            CAudioCodec::EDecError eDecError;
            vector<uint8_t> audio_frame;
            uint8_t aac_crc_bits;
            pAudioSuperFrame->getFrame(audio_frame, aac_crc_bits, j);

            if (inputSampleRate != outputSampleRate) {
                eDecError = codec->Decode(audio_frame, aac_crc_bits, vecTempResBufInLeft, vecTempResBufInRight);

                if (eDecError == CAudioCodec::DECODER_ERROR_OK) {
                    /* Resample data */
                    iResOutBlockSize = outputSampleRate * vecTempResBufInLeft.Size() / inputSampleRate;
                    //printf("$ ASF %d/%d OK inputSampleRate=%d \n", j, num_frames, inputSampleRate);

                    // KiwiSDR: comment "NOOP for AAC" not true for us since our outputSampleRate = 12k, not usual soundcard 48k!
                    if (iResOutBlockSize != vecTempResBufOutCurLeft.Size()) { // NOOP for AAC, needed for xHE-AAC
                        vecTempResBufOutCurLeft.Init(iResOutBlockSize, 0.0);
                        vecTempResBufOutCurRight.Init(iResOutBlockSize, 0.0);
                        _REAL rRatio = _REAL(outputSampleRate) / _REAL(inputSampleRate);
                        ResampleObjL.Init(vecTempResBufInLeft.Size(), rRatio);
                        ResampleObjR.Init(vecTempResBufInLeft.Size(), rRatio);
                        init_LPF = true;
                    }
                    
                    drm_t *drm = &DRM_SHMEM->drm[(int) FROM_VOID_PARAM(TaskGetUserParam())];
                    if (init_LPF || use_LPF != drm->use_LPF) {
                        bool _20k = (outputSampleRate > 12000);
                        int attn = (drm->dbgUs && drm->p_i[0])? drm->p_i[0] : 20;
                        int hbw  = (drm->dbgUs && drm->p_i[1])? drm->p_i[1] : (_20k?  8000 : 5000);
                        int stop = (drm->dbgUs && drm->p_i[2])? drm->p_i[2] : (_20k? 10125 : 6000);

                        // ntaps, scale, stopAttn, Fpass, Fstop, Fsr, dump
                        lpfL.InitLPFilter(0, 1, attn, hbw, stop, inputSampleRate);
                        lpfR.InitLPFilter(0, 1, attn, hbw, stop, inputSampleRate);
                        use_LPF = drm->use_LPF;
                        do_LPF = (drm->use_LPF && attn > 1);
                        if (do_LPF)
                            alt_printf("DRM LPF #frames=%d size=%d sr=%d|%d %d|%d|%d #taps=%d\n",
                                num_frames, vecTempResBufInLeft.Size(), inputSampleRate, outputSampleRate,
                                attn, hbw, stop, lpfL.m_NumTaps);
                        else
                            alt_printf("DRM LPF OFF\n");
                        init_LPF = false;
                    }

                    if (do_LPF) {
                        lpfL.ProcessFilter(vecTempResBufInLeft.Size(), &vecTempResBufInLeft[0], &vecTempResBufInLeft[0]);
                        lpfR.ProcessFilter(vecTempResBufInRight.Size(), &vecTempResBufInRight[0], &vecTempResBufInRight[0]);
                    }
                    ResampleObjL.Resample(vecTempResBufInLeft, vecTempResBufOutCurLeft);
                    ResampleObjR.Resample(vecTempResBufInRight, vecTempResBufOutCurRight);
                } else {
                    //printf("$ ASF %d/%d ERROR inputSampleRate=%d \n", j, num_frames, inputSampleRate);
                }
            }
            else {
                eDecError = codec->Decode(audio_frame, aac_crc_bits, vecTempResBufOutCurLeft, vecTempResBufOutCurRight);
                //printf("$ ASF %d/%d no resample\n", j, num_frames);
            }

            iResOutBlockSize = vecTempResBufOutCurLeft.Size();

            bCurBlockOK = (eDecError == CAudioCodec::DECODER_ERROR_OK);

            /* Call decoder update */
            if (!bCodecUpdated)
            {
                bCodecUpdated = true;
                Parameters.Lock();
                int iCurSelAudioServ = Parameters.GetCurSelAudioService();
                codec->DecUpdate(Parameters.Service[iCurSelAudioServ].AudioParam);
                Parameters.Unlock();
            }
            /* OPH: add frame status to vector for RSCI */
            Parameters.Lock();
            Parameters.vecbiAudioFrameStatus.Add(eDecError == CAudioCodec::DECODER_ERROR_OK ? 0 : 1);
            Parameters.Unlock();
        }
        else
        {
            /* DRM super-frame header was wrong, set flag to "bad block" */
            bCurBlockOK = false;
            /* OPH: update audio status vector for RSCI */
            Parameters.Lock();
            Parameters.vecbiAudioFrameStatus.Add(1);
            Parameters.Unlock();
            //printf("$ ASF %d/%d BAD BLOCK\n", j, num_frames);
        }

        // This code is independent of particular audio source type and should work with all codecs

        /* Postprocessing of audio blocks, status informations -------------- */
        ETypeRxStatus status = reverb.apply(bCurBlockOK, bCurBlockFaulty, vecTempResBufOutCurLeft, vecTempResBufOutCurRight);

        if (bCurBlockOK && !bCurBlockFaulty)
        {
            /* Increment correctly decoded audio blocks counter */
            iNumCorDecAudio++;
        } else {
            //printf("#### bCurBlockOK=%d bCurBlockFaulty=%d\n", bCurBlockOK, bCurBlockFaulty);
        }

        #if 0
        {
            double l = 0.0, r = 0.0;
            for(int i=0; i<vecTempResBufOutCurLeft.Size(); i++) {
                l += vecTempResBufOutCurLeft[i];
                r += vecTempResBufOutCurRight[i];
            }
            cerr << "energy after resampling and reverb left " << (l/vecTempResBufOutCurLeft.Size()) << " right " << (l/vecTempResBufOutCurRight.Size()) << endl;
        }
        #endif

        Parameters.Lock();
        Parameters.ReceiveStatus.SLAudio.SetStatus(status);
        Parameters.ReceiveStatus.LLAudio.SetStatus(status);
        Parameters.AudioComponentStatus[Parameters.GetCurSelAudioService()].SetStatus(status);
        Parameters.Unlock();

        /* Conversion from _REAL to _SAMPLE with special function */
        for (int i = 0; i < iResOutBlockSize; i++)
        {
            #if 0
                if (i > vecTempResBufOutCurLeft.Size()) {
                    real_printf("i=%d vecTempResBufOutCurLeft.Size=%d\n",
                        i, vecTempResBufOutCurLeft.Size());
                }
                if (iOutputBlockSize + i * 2 + 1 > (*pvecOutputData).Size()) {
                    real_printf("i=%d iResOutBlockSize=%d iOutputBlockSize=%d pvecOutputData.Size=%d\n",
                        i, iResOutBlockSize, iOutputBlockSize, (*pvecOutputData).Size());
                }
            #endif
            (*pvecOutputData)[iOutputBlockSize + i * 2] = Real2Sample(vecTempResBufOutCurLeft[i]);	/* Left channel */
            (*pvecOutputData)[iOutputBlockSize + i * 2 + 1] = Real2Sample(vecTempResBufOutCurRight[i]);	/* Right channel */
        }

        /* Add new block to output block size ("* 2" for stereo output block) */
        iOutputBlockSize += iResOutBlockSize * 2;

        if(iOutputBlockSize==0) {
            //cerr << "iOutputBlockSize is zero" << endl;
        }
        else {
            #if 0
            double d=0.0;
            for (int i = 0; i < iOutputBlockSize; i++)
            {
                double n = (*pvecOutputData)[i];
                d += n*n;
            }
            cerr << "energy after converting " << iOutputBlockSize << " samples back to int " << sqrt(d/iOutputBlockSize) << endl;
            #endif
        }

    }
    //real_printf("%.1f ", (timer_us() - start)/1e3); fflush(stdout);
}

void
CAudioSourceDecoder::InitInternal(CParameter & Parameters)
{
    /* Close previous decoder instance if any */
    CloseDecoder();

    /*
        Since we use the exception mechanism in this init routine, the sequence of
        the individual initializations is very important!
        Requirement for text message is "stream is used" and "audio service".
        Requirement for AAC decoding are the requirements above plus "audio coding
        is AAC"
    */
    int iCurAudioStreamID;
    int iMaxLenResamplerOutput;
    int iCurSelServ;

    /* Init error flags and output block size parameter. The output block
       size is set in the processing routine. We must set it here in case
       of an error in the initialization, this part in the processing
       routine is not being called */
    DoNotProcessAudDecoder = false;
    DoNotProcessData = false;
    iOutputBlockSize = 0;

    /* Set audiodecoder to empty string - means "unknown" and "can't decode" to GUI */
    audiodecoder = "";

    try
    {
        Parameters.Lock();

        /* Init counter for correctly decoded audio blocks */
        iNumCorDecAudio = 0;

        /* Init "audio was ok" flag */
        bAudioWasOK = true;

        /* Get number of total input bits for this module */
        iInputBlockSize = Parameters.iNumAudioDecoderBits;

        /* Get current selected audio service */
        iCurSelServ = Parameters.GetCurSelAudioService();

        /* Get current selected audio param */
        CAudioParam& AudioParam(Parameters.Service[iCurSelServ].AudioParam);

        /* Get current audio coding */
        eAudioCoding = AudioParam.eAudioCoding;

        /* Current audio stream ID */
        iCurAudioStreamID = AudioParam.iStreamID;

        /* The requirement for this module is that the stream is used and the
           service is an audio service. Check it here */
        if ((Parameters.Service[iCurSelServ].eAudDataFlag != CService::SF_AUDIO) || (iCurAudioStreamID == STREAM_ID_NOT_USED))
        {
            throw CInitErr(ET_ALL);
        }

        int iTotalFrameSize = Parameters.Stream[iCurAudioStreamID].iLenPartA + Parameters.Stream[iCurAudioStreamID].iLenPartB;

        /* Init text message application ------------------------------------ */
        if (AudioParam.bTextflag)
        {
            bTextMessageUsed = true;

            /* Get a pointer to the string */
            TextMessage.Init(&AudioParam.strTextMessage);

            /* Init vector for text message bytes */
            vecbiTextMessBuf.Init(SIZEOF__BYTE * NUM_BYTES_TEXT_MESS_IN_AUD_STR);

            iTotalFrameSize -= NUM_BYTES_TEXT_MESS_IN_AUD_STR;
        }
        else {
            bTextMessageUsed = false;
        }
        
        if (eAudioCoding == CAudioParam::AC_xHE_AAC) {
            XHEAACSuperFrame* p = new XHEAACSuperFrame();
            // part B should be enough as xHE-AAC MUST be EEP but its easier to just add them
            p->init(AudioParam, iTotalFrameSize);
            pAudioSuperFrame = p;
        }
        else {
            AACSuperFrame *p = new AACSuperFrame();
            p->init(AudioParam,  Parameters.GetWaveMode(), Parameters.Stream[iCurAudioStreamID].iLenPartA, Parameters.Stream[iCurAudioStreamID].iLenPartB);
            pAudioSuperFrame = p;
        }
        
        /* Get decoder instance */
        codec = CAudioCodec::GetDecoder(eAudioCoding);

        if (codec->CanDecode(eAudioCoding))
            audiodecoder = codec->DecGetVersion();

        if (bWriteToFile)
        {
            codec->openFile(Parameters);
        }
        else {
            codec->closeFile();
        }

        codec->Init(AudioParam, iInputBlockSize);

        /* Init decoder */
        // KiwiSDR NB: inputSampleRate is set here because DecOpen() decl passes inputSampleRate by pointer
        codec->DecOpen(AudioParam, inputSampleRate);
        cerr << "DecOpen sample rate " << inputSampleRate << endl;

        int iLenDecOutPerChan = 0; // no need to use the one from the codec
        int numFrames = pAudioSuperFrame->getNumFrames();
        if (numFrames == 0) {
            // xHE-AAC - can't tell yet!
            //printf("$ xHE-AAC - can't tell yet! (\n");
        }
        else {
            int samplesPerChannelPerSuperFrame = pAudioSuperFrame->getSuperFrameDurationMilliseconds() * inputSampleRate / 1000;
            iLenDecOutPerChan = samplesPerChannelPerSuperFrame / numFrames;
            //printf("$ samplesPerChannelPerSuperFrame=%d numFrames=%d\n", samplesPerChannelPerSuperFrame, numFrames);
        }
        //printf("$ iLenDecOutPerChan=%d\n", iLenDecOutPerChan);

        /* set string for GUI */
        Parameters.audiodecoder = audiodecoder;

        /* Set number of Audio frames for log file */
        // TODO Parameters.iNumAudioFrames = iNumAudioFrames;

        outputSampleRate = Parameters.GetAudSampleRate();
        Parameters.Unlock();

        /* Since we do not do Mode E or correct for sample rate offsets here (yet), we do not
           have to consider larger buffers. An audio frame always corresponds to 400 ms */
        iMaxLenResamplerOutput = int(_REAL(outputSampleRate) * 0.4 /* 400ms */  * 2 /* for stereo */ );
        iMaxLenResamplerOutput *= 8;    // KiwiSDR: to prevent buffer overruns with xHE-AAC (as detected by clang asan)
        //printf("$ iMaxLenResamplerOutput=%d\n", iMaxLenResamplerOutput);

        if (inputSampleRate != outputSampleRate) {
            if (iLenDecOutPerChan == 0)
                printf("DRM iLenDecOutPerChan == 0: numFrames=%d iLenDecOutPerChan=%d inputSampleRate=%d outputSampleRate=%d\n",
                    numFrames, iLenDecOutPerChan, inputSampleRate, outputSampleRate);
            _REAL rRatio = _REAL(outputSampleRate) / _REAL(inputSampleRate);
            /* Init resample objects */
            ResampleObjL.Init(iLenDecOutPerChan, rRatio);
            ResampleObjR.Init(iLenDecOutPerChan, rRatio);
            init_LPF = true;
        }

        int iResOutBlockSize;
        if (inputSampleRate == 0) {
            iResOutBlockSize = iLenDecOutPerChan;
        } else {
            iResOutBlockSize = outputSampleRate * iLenDecOutPerChan / inputSampleRate;
        }
        //printf("$$$ numFrames=%d iLenDecOutPerChan=%d iResOutBlockSize=%d iMaxLenResamplerOutput=%d inputSampleRate=%d outputSampleRate=%d\n",
        //    numFrames, iLenDecOutPerChan, iResOutBlockSize, iMaxLenResamplerOutput, inputSampleRate, outputSampleRate);

        //cerr << "output block size per channel " << iResOutBlockSize << " = samples " << iLenDecOutPerChan << " * " << Parameters.GetAudSampleRate() << " / " << iAudioSampleRate << endl;

        /* Additional buffers needed for resampling since we need conversion
           between _REAL and _SAMPLE. We have to init the buffers with
           zeros since it can happen, that we have bad CRC right at the
           start of audio blocks */
        //printf("$ vecTempResBufInLeft.Init iLenDecOutPerChan=%d\n", iLenDecOutPerChan);
        vecTempResBufInLeft.Init(iLenDecOutPerChan, 0.0);
        vecTempResBufInRight.Init(iLenDecOutPerChan, 0.0);
        vecTempResBufOutCurLeft.Init(iResOutBlockSize, 0.0);
        vecTempResBufOutCurRight.Init(iResOutBlockSize, 0.0);

        reverb.Init(outputSampleRate, bUseReverbEffect);

        /* With this parameter we define the maximum length of the output
           buffer. The cyclic buffer is only needed if we do a sample rate
           correction due to a difference compared to the transmitter. But for
           now we do not correct and we could stay with a single buffer
           Maybe TODO: sample rate correction to avoid audio dropouts */
        iMaxOutputBlockSize = iMaxLenResamplerOutput;
    }

    catch(CInitErr CurErr)
    {
        Parameters.Unlock();

        switch (CurErr.eErrType)
        {
        case ET_ALL:
            /* An init error occurred, do not process data in this module */
            DoNotProcessData = true;
            break;

        case ET_AUDDECODER:
            /* Audio part should not be decoded, set flag */
            DoNotProcessAudDecoder = true;
            break;

        default:
            DoNotProcessData = true;
        }

        /* In all cases set output size to zero */
        iOutputBlockSize = 0;
    }
}

int
CAudioSourceDecoder::GetNumCorDecAudio()
{
    /* Return number of correctly decoded audio blocks. Reset counter
       afterwards */
    const int iRet = iNumCorDecAudio;

    iNumCorDecAudio = 0;

    return iRet;
}

void
CAudioSourceDecoder::CloseDecoder()
{
    if (codec != nullptr)
        codec->DecClose();
}

