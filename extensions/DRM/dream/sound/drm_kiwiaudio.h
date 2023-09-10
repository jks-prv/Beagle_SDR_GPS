/******************************************************************************\
 * British Broadcasting Corporation
 * Copyright (c) 2007
 *
 * Author(s):
 *	Julian Cable
 *
 * Decription:
 * PortAudio sound interface
 *
 ******************************************************************************
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

#ifndef _DRM_KIWIAUDIO_H
#define _DRM_KIWIAUDIO_H

#include "../sound/soundinterface.h"

using std::string;

class CKiwiCommon: public CSelectionInterface
{
public:
    CKiwiCommon(bool);
    virtual     ~CKiwiCommon();

    virtual void	Enumerate(std::vector<string>& names, std::vector<string>& descriptions);
    virtual void	SetDev(std::string sNewDevice);
    virtual std::string	GetDev();

    bool		Init(int iSampleRate, int iNewBufferSize, bool bNewBlocking);
    void        ReInit();
    bool		Write(CVector<short>& psData);
    void        Close();

    int xruns;

protected:
    //PaStream *stream;
    std::vector<string> names;
    //std::vector<PaDeviceIndex> devices;
    std::string dev;
    bool is_capture, blocking, device_changed, xrun;
    int framesPerBuffer;
    int iBufferSize;
    double samplerate;
};

class CSoundOutKiwi: public CSoundOutInterface
{
public:
    CSoundOutKiwi();
    virtual         ~CSoundOutKiwi();
    virtual void	Enumerate(std::vector<string>& names, std::vector<string>& descriptions)
    {
        hw.Enumerate(names, descriptions);
    }
    virtual void	SetDev(std::string sNewDevice)
    {
        hw.SetDev(sNewDevice);
    }
    virtual std::string	GetDev()
    {
        return hw.GetDev();
    }
    virtual std::string	GetVersion()
    {
        return "kiwiaudio out";
    }
    virtual bool	Init(int iSampleRate, int iNewBufferSize, bool bNewBlocking);
    virtual void    Close();
    virtual bool	Write(CVector<short>& psData);

protected:
    CKiwiCommon hw;
};

#endif
