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

#include "AudioResample.h"
#include "ResampleFilter.h"
#include "printf.h"

CAudioResample::CAudioResample()
{

}

CAudioResample::~CAudioResample()
{

}

void CAudioResample::Resample(CVector<_REAL>& rInput, CVector<_REAL>& rOutput)
{
    int j;

    if (rRatio == (_REAL) 1.0)
    {
        /* If ratio is 1, no resampling is needed, just copy vector */
        for (j = 0; j < iOutputBlockSize; j++)
            rOutput[j] = rInput[j];
    }
    else
    {
        /* Move old data from the end to the history part of the buffer and
           add new data (shift register) */
        vecrIntBuff.AddEnd(rInput, iInputBlockSize);

        /* Main loop */
        for (j = 0; j < iOutputBlockSize; j++)
        {
            /* Phase for the linear interpolation-taps */
            const int ip =
                (int) (j * INTERP_DECIM_I_D / rRatio) % INTERP_DECIM_I_D;

            /* Sample position in input vector */
            const int in = (int) (j / rRatio) + RES_FILT_NUM_TAPS_PER_PHASE;

            /* Convolution */
            _REAL ry = (_REAL) 0.0;
            for (int i = 0; i < RES_FILT_NUM_TAPS_PER_PHASE; i++)
                ry += fResTaps1To1[ip][i] * vecrIntBuff[in - i];

            rOutput[j] = ry;
        }
    }
}

void CAudioResample::Init(int iNewInputBlockSize, _REAL rNewRatio)
{
    rRatio = rNewRatio;
    iInputBlockSize = iNewInputBlockSize;
    iOutputBlockSize = (int) (iInputBlockSize * rRatio);
    //printf("CAudioResample::Init rRatio=%f iInputBlockSize=%d iOutputBlockSize=%d\n", rRatio, iInputBlockSize, iOutputBlockSize);
    Reset();
}

void CAudioResample::Init(const int iNewOutputBlockSize, const int iInputSamplerate, const int iOutputSamplerate)
{
    double _rRatio = (double) iOutputSamplerate / iInputSamplerate;
    double ibs_r = (double) iNewOutputBlockSize / _rRatio;
    iInputBlockSize = (int) round(ibs_r);
    iOutputBlockSize = iNewOutputBlockSize;
    
    // Note that iNewOutputBlockSize depends on the signal MSC: MSC:16 => 320, MSC:64 => 200
    // In Kiwi 20.25 kHz mode the ibs_r value is fractional when trying to decode a signal with MSC:64
    // i.e. ibs_r = 200 / (24000/20250) = 168.75
    // We round this to 169, but the sample rate must be adjusted to account for the ratio 200/169 = 1.183432
    // This is the same as if we had an effective srate of 20280 such that 200 / (24000/20280) = 169
    // DRM is able to cope with this sampling rate error (20280 - 20250 = 30 Hz)
    // In all other cases there is no sampling rate error (12k mode, 20.25k mode MSC:16)
    
    rRatio = (_REAL) iOutputBlockSize / iInputBlockSize;
    _REAL effective_srate = (_REAL) iOutputSamplerate / rRatio;
    printf("CAudioResample::Init effective_srate=%.0f rRatio=%f iInputBlockSize=%d|%f iOutputBlockSize=%d\n",
        effective_srate, rRatio, iInputBlockSize, ibs_r, iOutputBlockSize);
    Reset();
}

int CAudioResample::GetMaxInputSize() const
{
    return iInputBlockSize;
}

int CAudioResample::GetFreeInputSize() const
{
    return iInputBlockSize;
}

void CAudioResample::Reset()
{
    /* Allocate memory for internal buffer, clear sample history */
    vecrIntBuff.Init(iInputBlockSize + RES_FILT_NUM_TAPS_PER_PHASE,
        (_REAL) 0.0);
}
