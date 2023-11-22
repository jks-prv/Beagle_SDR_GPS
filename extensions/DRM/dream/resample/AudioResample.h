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

// NB: CAudioResample is also used by the HFDL and ALE_2G extensions

#ifndef CAUDIORESAMPLE_H
#define CAUDIORESAMPLE_H

#include "../util/Vector.h"

class CAudioResample
{
public:
    CAudioResample();
    virtual ~CAudioResample();

    _REAL					rRatio;
    int						iInputBlockSize;
    int						iOutputBlockSize;

    void Init(int iNewInputBlockSize, _REAL rNewRatio);
    void Init(int iNewOutputBlockSize, int iInputSamplerate, int iOutputSamplerate);
    void Resample(CVector<_REAL>& rInput, CVector<_REAL>& rOutput);
    int GetFreeInputSize() const;
    int GetMaxInputSize() const;
    void Reset();

protected:
    CShiftRegister<_REAL>	vecrIntBuff;
    int						iHistorySize;
};

#endif // CAUDIORESAMPLE_H
