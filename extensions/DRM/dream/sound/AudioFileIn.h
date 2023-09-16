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

#ifndef _AUDIOFILEIN
#define _AUDIOFILEIN

#include "soundinterface.h"
#include "../util/Pacer.h"
#include "../util/FileTyper.h"
#include "../resample/AudioResample.h"

//#define USE_SRC
#ifdef USE_SRC
    #include "../resample/SRCResample.h"
#endif

/* Classes ********************************************************************/
class CAudioFileIn : public CSoundInInterface
{
public:
    CAudioFileIn();
    virtual ~CAudioFileIn();

    virtual void		Enumerate(std::vector<std::string>&, std::vector<std::string>&) {}
    virtual void		SetDev(std::string sNewDevice) {sCurrentDevice = sNewDevice;}
    virtual std::string		GetDev() {return sCurrentDevice;}
    virtual void		SetFileName(const std::string& strFileName, FileTyper::type type);
    virtual int			GetSampleRate() {return iRequestedSampleRate;}

    virtual bool	Init(int iNewSampleRate, int iNewBufferSize, bool bNewBlocking);
    virtual bool 	Read(CVector<short>& psData);
    virtual void 		Close();
	virtual std::string GetVersion() { return "Dream Audio File Reader"; }

protected:
    std::string         strInFileName;
    int                 interleaved;
    CVector<_REAL>		vecTempResBufIn;
    CVector<_REAL>		vecTempResBufOut;
    enum { fmt_txt, fmt_raw_mono, fmt_raw_stereo, fmt_other, fmt_direct } eFmt;
    FILE*				pFileReceiver;
    int					iSampleRate;
    int					iRequestedSampleRate;
    int					iBufferSize;
    int					iFileSampleRate;
    int					iFileChannels;
    CPacer*				pacer;
#ifdef USE_SRC
    CSRCResample*		ResampleObjL;
    CSRCResample*		ResampleObjR;
#else
    CAudioResample*		ResampleObjL;
    CAudioResample*		ResampleObjR;
#endif
    short*				buffer;
    int					iOutBlockSize;
    std::string         sCurrentDevice;
};

#endif
