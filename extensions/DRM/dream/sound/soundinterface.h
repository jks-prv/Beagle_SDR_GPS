/******************************************************************************\
 * British Broadcasting Corporation
 * Copyright (c) 2007
 *
 * Author(s):
 *	Julian Cable
 *
 * Decription:
 * sound interfaces
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

#ifndef SOUNDINTERFACE_H
#define SOUNDINTERFACE_H

#include "selectioninterface.h"
#include "../util/Vector.h"

class CSoundInInterface : public CSelectionInterface
{
public:
    virtual 		~CSoundInInterface()=0;

    /* sound card interface - used by ReadData */
    virtual bool Init(int iSampleRate, int iNewBufferSize, bool bNewBlocking)=0;
    virtual bool Read(CVector<short>& psData)=0;
    virtual void     Close()=0;
	virtual std::string	GetVersion() = 0;

};

class CSoundOutInterface : public CSelectionInterface
{
public:
    virtual 		~CSoundOutInterface()=0;

    /* sound card interface - used by WriteData */
    virtual bool Init(int iSampleRate, int iNewBufferSize, bool bNewBlocking)=0;
    virtual bool Write(CVector<short>& psData)=0;
    virtual void     Close()=0;
	virtual std::string	GetVersion() = 0;
};

#endif
