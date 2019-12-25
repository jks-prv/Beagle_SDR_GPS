/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2004
 *
 * Author(s):
 *	Volker Fischer, Oliver Haffenden
 *
 * Description:
 *	see TagPacketDecoder.cpp
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

#ifndef TAG_PACKET_DECODER_H_INCLUDED
#define TAG_PACKET_DECODER_H_INCLUDED

#include "../GlobalDefinitions.h"
#include "MDIDefinitions.h"
#include "../util/Vector.h"
#include "PacketInOut.h"

class CTagItemDecoder;

class CTagPacketDecoder
{
public:
	CTagPacketDecoder();
	enum Error { E_OK, E_CRC, E_VERSION, E_LENGTH, E_SYNC, E_PROTO };
	// This should be in its own class
	virtual Error DecodeAFPacket(CVectorEx<_BINARY>& vecbiAFPkt);

	// Decode all the tags in the tag packet. To do things before or after the decoding,
	// override this and call the base class function to do the decoding
	virtual void DecodeTagPackets(CVectorEx<_BINARY>& vecbiPkt, int iPayloadLen);
	virtual ~CTagPacketDecoder() {}

protected:
	// Go through all the tag item decoders to find one that matches the current tag name.
	int DecodeTag(CVector<_BINARY>& vecbiTag);

	void AddTagItemDecoder(CTagItemDecoder *pTagItemDecoder);

	void InitTagItemDecoders(void);
private:
	CVector<CTagItemDecoder *> vecpTagItemDecoders;
	uint16_t iSeqNumber;
};

#endif
