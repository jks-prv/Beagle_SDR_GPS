/******************************************************************************\
 * British Broadcasting Corporation
 * Copyright (c) 2007
 *
 * Author(s):
 *	Julian Cable
 *
 * Decription:
 * KiwiAudio sound interface
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

// NB v1.470: Because of the C_LINKAGE() change

#include "drm_kiwiaudio.h"
#include "DRM_main.h"

#include <iostream>
#include <cstring>

using std::vector;
using std::cout;
using std::endl;
using std::cerr;

CKiwiCommon::CKiwiCommon(bool cap):xruns(0),
        names(),
        is_capture(cap), blocking(true), device_changed(true), xrun(false),
        framesPerBuffer(0)
{
    vector < string > choices;
    vector < string > descriptions;
    Enumerate(choices, descriptions);
}

CKiwiCommon::~CKiwiCommon()
{
    Close();
}

void
CKiwiCommon::Enumerate(vector < string > &choices, vector < string > &descriptions)
{
    vector < string > tmp;

    names.clear();
    descriptions.clear();
	names.push_back("kiwi"); /* default device */
	descriptions.push_back("kiwi");
    choices = names;
}

void
CKiwiCommon::SetDev(string sNewDevice)
{
    if (dev != sNewDevice)
    {
        dev = sNewDevice;
        device_changed = true;
    }
}

string
CKiwiCommon::GetDev()
{
    return dev;
}

/* buffer_size is in samples - frames would be better */
bool
CKiwiCommon::Init(int iSampleRate, int iNewBufferSize, bool bNewBlocking)
{
    if (device_changed == false && double(iSampleRate) == samplerate)
        return false;

    unsigned long channels=2;

    samplerate = ext_update_get_sample_rateHz(RX_CHAN_CUR);

    if (is_capture)
        framesPerBuffer = iNewBufferSize / channels;
    else
        framesPerBuffer = 256;

    blocking = bNewBlocking; /* TODO honour this */
    iBufferSize = iNewBufferSize;

    ReInit();

    return true;
}

void
CKiwiCommon::ReInit()
{
    Close();

    device_changed = false;
    xrun = false;
}

void
CKiwiCommon::Close()
{
    device_changed = true;
}

bool
CKiwiCommon::Write(CVector < short >&psData)
{
    if (device_changed) {
        ReInit();
    }

    return false;
}

CSoundOutKiwi::CSoundOutKiwi():hw(false)
{
}

CSoundOutKiwi::~CSoundOutKiwi()
{
    Close();
}

bool
CSoundOutKiwi::Init(int iSampleRate, int iNewBufferSize, bool bNewBlocking)
{
    drm_buf_t *drm_buf = &DRM_SHMEM->drm_buf[(int) FROM_VOID_PARAM(TaskGetUserParam())];
    //printf("$$$$ drm_kiwiaudio rx_chan=%d pid=%d\n", TaskGetUserParam(), getpid());
    drm_buf->out_rd_pos = drm_buf->out_wr_pos = drm_buf->out_pos = 0;
    drm_buf->out_samps = (snd_rate == SND_RATE_4CH)? 4800 : 8100;
    return hw.Init(iSampleRate, iNewBufferSize, bNewBlocking);
}

bool
CSoundOutKiwi::Write(CVector<short>& psData)
{
    drm_buf_t *drm_buf = &DRM_SHMEM->drm_buf[(int) FROM_VOID_PARAM(TaskGetUserParam())];

    size_t mono_samps = psData.Size();
    size_t stereo_samps = mono_samps/2;
    if (stereo_samps != drm_buf->out_samps) {
        printf("CSoundOutKiwi::Write mono=%d stereo=%d out_samps=%d N_DRM_OSAMPS=%d\n", mono_samps, stereo_samps, drm_buf->out_samps, N_DRM_OSAMPS);
        panic("drm_kiwiaudio");
    }
    TYPESTEREO16 *o_samps = drm_buf->out_samples[drm_buf->out_wr_pos];
    size_t bytes = mono_samps * sizeof(short);
    memcpy(o_samps, &psData[0], bytes);
    drm_buf->out_wr_pos = (drm_buf->out_wr_pos + 1) & (N_DRM_OBUF-1);

    #if 0
        u4_t d = pos_wrap_diff(drm_buf->out_wr_pos, drm_buf->out_rd_pos, N_DRM_OBUF);
        //if (d > 4)
        { real_printf("d%d ", d); fflush(stdout); }
    #endif
    
    #if 0
        static u4_t kstart, ksamps, kmeas, klimit;
        if (ksamps == 0) {
            kstart = timer_ms();
            kmeas = 3;
            klimit = kmeas * 1000;
        }
        ksamps += stereo_samps;
        u4_t diff = timer_ms() - kstart;
        if (diff > klimit) {
            real_printf("KSO %d sps, %d sec\n", ksamps/kmeas, kmeas); fflush(stdout);
            ksamps = 0;
            /*
            static int pdump;
            pdump++;
            if (pdump == 2)
                panic("pdump");
            */
        }
    #endif

    return hw.Write(psData);
}

void
CSoundOutKiwi::Close()
{
    hw.Close();
    cout << "play close" << endl;
}
