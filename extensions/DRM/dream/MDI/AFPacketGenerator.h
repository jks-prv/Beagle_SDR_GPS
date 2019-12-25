/******************************************************************************\
 * British Broadcasting Corporation
 * Copyright (c) 2007
 *
 * Author(s):
 *	Volker Fischer, Oliver Haffenden
 *
 * Description:
 *	see AFPacketGenerator.cpp
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
#ifndef AF_PACKET_GENERATOR_H_INCLUDED
#define AF_PACKET_GENERATOR_H_INCLUDED

#include "../GlobalDefinitions.h"
#include "../Parameter.h"
#include "../util/Vector.h"
#include "../util/Buffer.h"
#include "../util/CRC.h"

class CTagPacketGenerator;

class CAFPacketGenerator
{
public:
	CAFPacketGenerator() : iSeqNumber(0) {}

	CVector<_BYTE> GenAFPacket(const bool bUseAFCRC, CTagPacketGenerator& TagPacketGenerator);

private:
	CVector<_BYTE> PackBytes(CVector<_BINARY> &vecbiPacket);
	int							iSeqNumber;
};

#endif
