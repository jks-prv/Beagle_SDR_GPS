/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2001
 *
 * Author(s):
 *	Volker Fischer
 *
 * Description:
 *	See DataDecoder.cpp
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

#ifndef __DATA_ENCODER_H
#define __DATA_ENCODER_H

#include "../GlobalDefinitions.h"
#include "../Parameter.h"
#include "../util/Modul.h"
#include "../util/CRC.h"
#include "../util/Vector.h"
#include "DABMOT.h"
#include "MOTSlideShow.h"

class CDataEncoder
{
  public:
    CDataEncoder ()
    {
    }
    virtual ~ CDataEncoder ()
    {
    }

    int Init (CParameter & Param);
    void GeneratePacket (CVector < _BINARY > &vecbiPacket);

    CMOTSlideShowEncoder *GetSliShowEnc ()
    {
	return &MOTSlideShowEncoder;
    }

  protected:
    CMOTSlideShowEncoder MOTSlideShowEncoder;
    CVector < _BINARY > vecbiCurDataUnit;

    int iPacketLen;
    int iTotalPacketSize;
    int iCurDataPointer;
    int iPacketID;
    int iContinInd;
};

#endif
