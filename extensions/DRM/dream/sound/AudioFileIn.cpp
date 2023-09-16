/******************************************************************************\
 * British Broadcasting Corporation
 * Copyright (c) 2007, 2012, 2013
 *
 * Author(s):
 *	Julian Cable, David Flamand
 *
 * Decription:
 *  Read a file at the correct rate
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

// Copyright (c) 2017-2023 John Seamons, ZL4VO/KF6VO

#include "DRM_main.h"
#include "AudioFileIn.h"

#include <iostream>
#ifdef HAVE_LIBSNDFILE
# include <sndfile.h>
#endif
#include <cstdlib>
#include <cstring>
#include <string.h>

using namespace std;

CAudioFileIn::CAudioFileIn(): CSoundInInterface(), eFmt(fmt_other),
    pFileReceiver(nullptr), iSampleRate(0), iRequestedSampleRate(0), iBufferSize(0),
    iFileSampleRate(0), iFileChannels(0), pacer(nullptr),
    ResampleObjL(nullptr), ResampleObjR(nullptr), buffer(nullptr)
{
}

CAudioFileIn::~CAudioFileIn()
{
    Close();
}

void
CAudioFileIn::SetFileName(const string& strFileName, FileTyper::type type)
{
    drm_t *drm = &DRM_SHMEM->drm[(int) FROM_VOID_PARAM(TaskGetUserParam())];

    strInFileName = strFileName;
    string ext = "x";
    eFmt = fmt_other;
    size_t p = strInFileName.rfind('.');
    if (p != string::npos) {
        ext = strInFileName.substr(p+1);
        if (ext == "txt") eFmt = fmt_txt; else
        if (ext == "TXT") eFmt = fmt_txt; else
        if (ext.substr(0,2) == "iq") eFmt = fmt_raw_stereo; else
        if (ext.substr(0,2) == "IQ") eFmt = fmt_raw_stereo; else
        if (ext.substr(0,2) == "if") eFmt = fmt_raw_stereo; else
        if (ext.substr(0,2) == "IF") eFmt = fmt_raw_stereo; else
        if (ext.substr(0,2) == "be") eFmt = fmt_raw_stereo; else
        if (ext.substr(0,3) == "dat") eFmt = fmt_direct; else
        if (ext == "pcm") eFmt = fmt_raw_mono; else
        if (ext == "PCM") eFmt = fmt_raw_mono;
    }

    switch (eFmt)
    {
    case fmt_raw_stereo:
        iFileChannels = 2;
        if (ext != "x" && (ext.length() == 4 || ext.length() == 5)) /* e.g.: iq48, IF192 */
            iFileSampleRate = 1000 * atoi(ext.substr(2).c_str());
        else
            iFileSampleRate = DEFAULT_SOUNDCRD_SAMPLE_RATE;
        break;
    case fmt_direct:
        iFileChannels = 2;
        iFileSampleRate = snd_rate;
        assert(drm->init);
        drm->iq_rd_pos = N_DPBUF-1;
        drm->remainingIQ = drm->iq_bpos = 0;
        break;
    default:
        iFileChannels = 1;
        if (ext != "x" && (ext.length() == 5 || ext.length() == 6)) /* e.g.: TXT48, pcm192 */
            iFileSampleRate = 1000 * atoi(ext.substr(3).c_str());
        else
            iFileSampleRate = DEFAULT_SOUNDCRD_SAMPLE_RATE;
        break;
    }

#ifdef HAVE_LIBSNDFILE
    SF_INFO sfinfo;
    memset(&sfinfo, 0, sizeof(SF_INFO));
    switch (eFmt)
    {
    case fmt_txt:
        pFileReceiver = fopen(strInFileName.c_str(), "r");
        break;
    case fmt_raw_mono:
    case fmt_raw_stereo:
        sfinfo.samplerate = iFileSampleRate;
        sfinfo.channels = iFileChannels;
        sfinfo.format = SF_FORMAT_RAW|SF_FORMAT_PCM_16;
        sfinfo.format |= (ext != "x" && ext.substr(0,2) == "be")? SF_ENDIAN_BIG : SF_ENDIAN_LITTLE;
        pFileReceiver = (FILE*)sf_open(strInFileName.c_str(), SFM_READ, &sfinfo);
        if (pFileReceiver == nullptr)
            throw CGenErr(string("")+sf_strerror(0)+" raised on "+strInFileName);
        break;
    case fmt_other:
        pFileReceiver = (FILE*)sf_open(strInFileName.c_str(), SFM_READ, &sfinfo);
        if (pFileReceiver != nullptr)
        {
            iFileChannels = sfinfo.channels;
            iFileSampleRate = sfinfo.samplerate;
		} else {
            string errs = string("")+sf_strerror(0)+" for "+strInFileName;
            throw CGenErr(errs);
		}
        break;
    case fmt_direct:
        pFileReceiver = nullptr;
        break;
    default:
        pFileReceiver = nullptr;
        break;
    }
#else
    if (eFmt == fmt_txt)
        pFileReceiver = fopen(strInFileName.c_str(), "r");
    else
    if (eFmt != fmt_direct)
        pFileReceiver = fopen(strInFileName.c_str(), "rb");
#endif

    /* Check for errors */
#ifdef HAVE_LIBSNDFILE
    if (pFileReceiver != nullptr && type != FileTyper::pipe)
    {
        sf_count_t count;
        
        switch (eFmt)
        {
        case fmt_txt:
            // TODO
            break;
        case fmt_raw_mono:
        case fmt_raw_stereo:
        case fmt_other:
            count = sf_seek((SNDFILE*)pFileReceiver, 1, SEEK_SET);
            /* Must contain at least one sample, mono or stereo */
            if (sf_error((SNDFILE*)pFileReceiver) || count != 1 || iFileChannels < 1 || iFileChannels > 2)
            {
                sf_close((SNDFILE*)pFileReceiver);
                pFileReceiver = nullptr;
            }
            else
            {
                /* Reset position */
                sf_seek((SNDFILE*)pFileReceiver, 0, SEEK_SET);
            }
            break;
        case fmt_direct:
            break;
        }
    }
#else
    #error TODO
#endif

// The error is reported when reading (red light in system eval on interface IO led)
//    if (pFileReceiver == nullptr)
//        throw CGenErr("The file " + strInFileName + " could not be openned");

    iRequestedSampleRate = iFileSampleRate;
    if      (iRequestedSampleRate <= 24000) iRequestedSampleRate = 24000;
    else if (iRequestedSampleRate <= 48000) iRequestedSampleRate = 48000;
    else if (iRequestedSampleRate <= 96000) iRequestedSampleRate = 96000;
    else                                    iRequestedSampleRate = 192000;
    //printf("CAudioFileIn::SetFileName exit iFileSampleRate=%d iRequestedSampleRate=%d\n", iFileSampleRate, iRequestedSampleRate);
}

bool
CAudioFileIn::Init(int iNewSampleRate, int iNewBufferSize, bool bNewBlocking)
{
    drm_t *drm = &DRM_SHMEM->drm[(int) FROM_VOID_PARAM(TaskGetUserParam())];

	//qDebug("CAudioFileIn::Init() iNewSampleRate=%i iNewBufferSize=%i bNewBlocking=%i", iNewSampleRate, iNewBufferSize, bNewBlocking);
    //printf("CAudioFileIn::Init() %s iNewSampleRate=%d iNewBufferSize=%d bNewBlocking=%d pFileReceiver=%p\n", strInFileName.c_str(), iNewSampleRate, iNewBufferSize, bNewBlocking, pFileReceiver);

    if (pacer)
    {
        delete pacer;
        pacer = nullptr;
    }
    if (bNewBlocking)
    {
        double interval = double(iNewBufferSize/2) / double(iNewSampleRate);
        uint64_t interval_ns = uint64_t(1e9*interval);
        //if (drm->pace)
        //    interval_ns -= interval_ns / drm->pace;
        pacer = new CPacer(interval_ns);
        //printf("CAudioFileIn::Init iNewBufferSize=%d iNewSampleRate=%d iFileSampleRate=%d interval=%f\n", iNewBufferSize, iNewSampleRate, iFileSampleRate, interval);
    }

    if (pFileReceiver == nullptr && eFmt != fmt_direct)
        return true;

    bool bChanged = false;

	if (iSampleRate != iNewSampleRate)
    {
        //printf("CAudioFileIn::Init SR-CHANGE iSampleRate=%d <= iNewSampleRate=%d SETTING bChanged\n", iSampleRate, iNewSampleRate);
        iSampleRate = iNewSampleRate;
        bChanged = true;
    }

    if (iBufferSize != iNewBufferSize || bChanged)
    {
        //printf("CAudioFileIn::Init BUF-CHANGE iBufferSize=%d <= iNewBufferSize=%d bChanged=%d\n", iBufferSize, iNewBufferSize, bChanged);
        iBufferSize = iNewBufferSize;
        if (buffer)
            delete[] buffer;
        
        /* Create a resampler object if the file's sample rate isn't supported */
        if (iNewSampleRate != iFileSampleRate)
        {
            iOutBlockSize = iNewBufferSize / 2; /* Mono */
            //printf("CAudioFileIn::Init RESAMPLER REQUIRED iOutBlockSize=%d iFileSampleRate=%d iNewSampleRate=%d\n", iOutBlockSize, iFileSampleRate, iNewSampleRate);
            
            #ifdef USE_SRC
                if (ResampleObjL == nullptr)
                    ResampleObjL = new CSRCResample();
                ResampleObjL->Init(iOutBlockSize, iFileSampleRate, iNewSampleRate, iFileChannels);
                interleaved = 2;
            #else
                if (ResampleObjL == nullptr)
                    ResampleObjL = new CAudioResample();
                ResampleObjL->Init(iOutBlockSize, iFileSampleRate, iNewSampleRate);
                if (iFileChannels == 2)
                {
                    if (ResampleObjR == nullptr)
                        ResampleObjR = new CAudioResample();
                    ResampleObjR->Init(iOutBlockSize, iFileSampleRate, iNewSampleRate);
                }
                interleaved = 1;
            #endif
            
            const int iMaxInputSize = ResampleObjL->GetMaxInputSize();
            vecTempResBufIn.Init(iMaxInputSize * interleaved, (_REAL) 0.0);
            vecTempResBufOut.Init(iOutBlockSize * interleaved, (_REAL) 0.0);
            buffer = new short[iMaxInputSize * 2];

            if (bChanged)
            {
                if (ResampleObjL != nullptr)
                    ResampleObjL->Reset();
                if (ResampleObjR != nullptr)
                    ResampleObjR->Reset();
            }
        }
        else
        {
            buffer = new short[iNewBufferSize * 2];
        }
    }

    return bChanged;
}

bool
CAudioFileIn::Read(CVector<short>& psData)
{
    drm_t *drm = &DRM_SHMEM->drm[(int) FROM_VOID_PARAM(TaskGetUserParam())];

    //real_printf("CAudioFileIn::Read pacer=%d pFileReceiver=%p too_small=%d direct=%d\n", pacer, pFileReceiver, (psData.Size() < iBufferSize), (eFmt == fmt_direct));
    if (pacer)
        pacer->wait();

    if (pFileReceiver == nullptr && eFmt != fmt_direct) {
        return true;
    }

    if(psData.Size() < iBufferSize) {
        return true;
    }

    const int iFrames = ResampleObjL ? ResampleObjL->GetFreeInputSize() : iBufferSize/2;
    //real_printf("iFrames=%d ResampleObjL=%p GetFreeInputSize=%d iBufferSize=%d\n", iFrames, ResampleObjL, ResampleObjL ? ResampleObjL->GetFreeInputSize() : 1138, iBufferSize/2);
    int i;

#if 0
    if (eFmt == fmt_txt)
    {
        for (i = 0; i < iFrames; i++)
        {
            float tIn;
            if (fscanf(pFileReceiver, "%e\n", &tIn) == EOF)
            {
                /* If end-of-file is reached, stop simulation */
                return false;
            }
            psData[2*i] = (short)tIn;
            psData[2*i+1] = (short)tIn;
        }
        return false;
    }
#endif
    
    bool bError = false;
    int iRemainingFrame = iFrames;
    int iReadFrame = 0;

    if (eFmt == fmt_direct) {
        static u4_t last_time;
        assert(drm->init);
        iq_buf_t *iq = &RX_SHMEM->iq_buf[drm->rx_chan];
        if (last_time == 0) last_time = timer_sec();

        //real_printf("n%d "); fflush(stdout);
        while (iRemainingFrame > 0) {
            int i;
            TYPECPX *iqp;
            s4_t s4;
            
            #if 0
                //jks-direct
                size_t frames = iRemainingFrame;
                u2_t t;
                for (size_t i=0; i < frames; i++) {
                    if (drm->s2p >= drm->s2p_end)
                        drm->s2p = drm->s2p_start;
                    t = (u2_t) *drm->s2p;
                    drm->s2p++;
                    buffer[i*2] = (s2_t) FLIP16(t);
                    t = (u2_t) *drm->s2p;
                    drm->s2p++;
                    buffer[i*2+1] = (s2_t) FLIP16(t);
                    drm->i_samples++;
                    if (drm->i_samples >= 12000) {
                        //real_printf("."); fflush(stdout);
                        drm->i_samples = 0;
                    }
                }
                iRemainingFrame -= frames;
                iReadFrame += frames;
            #else
                if (drm->remainingIQ > 0) {
                    //real_printf("%d|%d|%d ", iRemainingFrame, drm->remainingIQ, drm->iq_rd_pos); fflush(stdout);
                    iqp = &iq->iq_samples[drm->iq_rd_pos][drm->iq_bpos];
                    for (; drm->remainingIQ > 0 && iRemainingFrame > 0; ) {
                        s4 = (s4_t) iqp->re;
                        //if (s4 > 32767 || s4 < -32768) printf("re range %d\n", s4);
                        buffer[iReadFrame*2] = (s2_t) s4;
                        s4 = (s4_t) iqp->im;
                        //if (s4 > 32767 || s4 < -32768) printf("im range %d\n", s4);
                        buffer[iReadFrame*2+1] = (s2_t) s4;
                        //if ((drm->i_samples & 0x3ff) == 0) real_printf("%d,%.0f ", iqp->re, buffer[iReadFrame*2]);
                        iqp++;
                        drm->iq_bpos++;
                        drm->remainingIQ--;
                        iReadFrame++;
                        iRemainingFrame--;
                        drm->i_samples++;
                        drm->i_tsamples++;
                        #if 0
                        if (drm->i_samples >= 12000) {
                            //real_printf("."); fflush(stdout);
                            drm->i_samples = 0;
                        }
                        #endif
                    }
                    
                    #if 0
                        u4_t now = timer_sec();
                        if (drm->i_epoch == 0) {
                            drm->i_epoch = timer_ms();
                            real_printf("i-reset "); fflush(stdout);
                        }
                        u4_t msec = timer_ms() - drm->i_epoch;
                        float avg = (float) drm->i_tsamples / msec * 1000.0;
                        if (timer_sec() != last_time) {
                            real_printf("i-%d,%.1f(%d,%d) ", drm->i_samples, avg, drm->no_input, drm->sent_silence); fflush(stdout);
                            drm->i_samples = 0;
                            last_time = now;
                        }
                    #endif
                } else {
                
                    // NB v1.470: Because of the C_LINKAGE(void *_TaskSleep(...)) change
                    // we need to touch this file so that ../build/obj_keep/AudioFileIn.o
                    // gets rebuilt and doesn't end up with a link time error.

                    //real_printf("[wait%d] ", iq->iq_wr_pos); fflush(stdout);
                    drm->iq_rd_pos = (drm->iq_rd_pos+1) & (N_DPBUF-1);
                    int trig = 0;
                    while (drm->iq_rd_pos == iq->iq_wr_pos) {
                        if (trig == 0) drm->no_input++;
                        trig = 1;
                        if (drm->debug & 1) {
                            NextTask("drm Y");
                        } else
                        if (drm->debug & 2) {
                            TaskSleepReasonUsec("drm YLP", 1);
                        } else
                        if (drm->debug & 4) {
                            TaskSleepReasonUsec("drm YLP", 100);
                        } else {
                            DRM_YIELD_LOWER_PRIO();     // TaskSleepReasonUsec("drm YLP", 1000)
                        }
                    }
                    //real_printf("[r%dw%d] ", drm->iq_rd_pos, iq->iq_wr_pos); fflush(stdout);
                    drm->remainingIQ = FASTFIR_OUTBUF_SIZE;
                    drm->iq_bpos = 0;
                }
            #endif
        }
        bError = false;
    } else {

#ifdef HAVE_LIBSNDFILE
    while (iRemainingFrame > 0)
    {
        if (pFileReceiver == nullptr) // file was closed in a different thread. TODO make this not possible
            return true;
        sf_count_t c = sf_readf_short((SNDFILE*)pFileReceiver, &buffer[iReadFrame * iFileChannels], iRemainingFrame);
	    if (c != sf_count_t(iRemainingFrame))
	    {
            /* rewind */
            if (sf_error((SNDFILE*)pFileReceiver) || sf_seek((SNDFILE*)pFileReceiver, 0, SEEK_SET) == -1)
            {
                memset(&buffer[iReadFrame * iFileChannels], 0, iRemainingFrame * iFileChannels);
                bError = true;
                break;
            }
	    }
        iRemainingFrame -= c;
        iReadFrame += c;
    }
    assert(drm->init);
    #if 0
    drm->i_samples += iFrames;
    if (drm->i_samples >= 12000) {
        real_printf("f"); fflush(stdout);
        drm->i_samples = 0;
    }
    #endif
#else
    #error TODO
    while (iRemainingFrame > 0)
    {
        if (pFileReceiver == nullptr) // file was closed in a different thread. TODO make this not possible
            return true;
        size_t c = fread(&buffer[iReadFrame * iFileChannels], sizeof(short), size_t(iRemainingFrame), pFileReceiver);
        if (c != size_t(iRemainingFrame))
        {
            /* rewind */
            if (ferror(pFileReceiver) || fseek(pFileReceiver, 0, SEEK_SET) == -1)
            {
                memset(&buffer[iReadFrame * iFileChannels], 0, iRemainingFrame * iFileChannels);
                bError = true;
                break;
            }
        }
        iRemainingFrame -= c;
        iReadFrame += c;
    }
#endif

    }

    if (ResampleObjL)
    {   /* Resampling is needed */
    
        if (iFileChannels == 2)
        {   /* Stereo */

            #ifdef USE_SRC
                #define _2CH 2
                for (i = 0; i < iFrames * _2CH; i++) {
                    vecTempResBufIn[i] = buffer[i];
                }
                ResampleObjL->Resample(vecTempResBufIn, vecTempResBufOut);
                for (i = 0; i < iOutBlockSize * _2CH; i++) {
                    psData[i] = Real2Sample(vecTempResBufOut[i]);
                }
            #else
                /* Left channel */
                for (i = 0; i < iFrames; i++) {
                    short b = buffer[2*i];
                    vecTempResBufIn[i] = b;
                }

                ResampleObjL->Resample(vecTempResBufIn, vecTempResBufOut);
                for (i = 0; i < iOutBlockSize; i++)
                    psData[i*2] = Real2Sample(vecTempResBufOut[i]);

                /* Right channel */
                for (i = 0; i < iFrames; i++) {
                    short b = buffer[2*i+1];
                    vecTempResBufIn[i] = b;
                }
                ResampleObjR->Resample(vecTempResBufIn, vecTempResBufOut);
                for (i = 0; i < iOutBlockSize; i++) {
                    psData[i*2+1] = Real2Sample(vecTempResBufOut[i]);
                }
            #endif
        }
        else
        {   /* Mono */
            for (i = 0; i < iFrames; i++)
                vecTempResBufIn[i] = buffer[i];
            ResampleObjL->Resample(vecTempResBufIn, vecTempResBufOut);
            for (i = 0; i < iOutBlockSize; i++) {
                psData[i*2] = psData[i*2+1] = Real2Sample(vecTempResBufOut[i]);
            }
        }
    }
    else
    {   /* Resampling is not needed, only copy data */
        if (iFileChannels == 2)
        {   /* Stereo */
            for (i = 0; i < iFrames; i++)
            {
                psData[2*i] = buffer[2*i];
                psData[2*i+1] = buffer[2*i+1];
            }
        }
        else
        {   /* Mono */
            for (i = 0; i < iFrames; i++)
                psData[2*i] = psData[2*i+1] = buffer[i];
        }
    }

    drm_next_task("afi");
    return bError;
}

void
CAudioFileIn::Close()
{
    /* Close file (if opened) */
    if (pFileReceiver != nullptr)
    {
#ifdef HAVE_LIBSNDFILE
        if (eFmt == fmt_txt)
            fclose(pFileReceiver);
        else
        if (eFmt != fmt_direct)
            sf_close((SNDFILE*)pFileReceiver);
#else
        if (eFmt != fmt_direct)
            fclose(pFileReceiver);
#endif
        pFileReceiver = nullptr;
    }

	if (buffer != nullptr)
		delete[] buffer;
    buffer = nullptr;

    if (pacer != nullptr)
        delete pacer;
    pacer = nullptr;

    if (ResampleObjL != nullptr)
        delete ResampleObjL;
    ResampleObjL = nullptr;

    if (ResampleObjR != nullptr)
        delete ResampleObjR;
    ResampleObjR = nullptr;

    vecTempResBufIn.Init(0, (_REAL) 0.0);
    vecTempResBufOut.Init(0, (_REAL) 0.0);

    iSampleRate = 0;
    iBufferSize = 0;
}
