/******************************************************************************\
 * British Broadcasting Corporation
 * Copyright (c) 2007
 *
 * Author(s):
 *	Volker Fischer, Julian Cable, Oliver Haffenden
 *
 * Description:
 *	Implements Digital Radio Mondiale (DRM) Multiplex Distribution Interface
 *	(MDI), Receiver Status and Control Interface (RSCI)  
 *  and Distribution and Communications Protocol (DCP) as described in
 *	ETSI TS 102 820,  ETSI TS 102 349 and ETSI TS 102 821 respectively.
 *
 *  This module defines the CAFPacketGenerator class
 *
 *  This class generates the AF packet header and CRC, and takes as an argument a TAG packet
 *  generator which generates the tag packet, i.e. the payload.
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

#include "AFPacketGenerator.h"
#include "TagPacketGenerator.h"
#include <iostream>

#include "../util/LogPrint.h"
// CAFPacketGenerator

CVector<_BYTE> CAFPacketGenerator::GenAFPacket(const bool bUseAFCRC, CTagPacketGenerator& TagPacketGenerator)
{
/*
	The AF layer encapsulates a single TAG Packet. Mandatory TAG items:
	*ptr, dlfc, fac_, sdc_, sdci, robm, str0-3
*/
	CVector<_BINARY>	vecbiAFPkt;

	/* Payload length in bytes */
// TODO: check if padding bits are needed to get byte alignment!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	int iPayloadLenBytes = TagPacketGenerator.GetTagPacketLength();
		
	/* 10 bytes AF header, 2 bytes CRC, payload */
	const int iAFPktLenBits =
		iPayloadLenBytes * SIZEOF__BYTE + 12 * SIZEOF__BYTE;

	/* Init vector length */
	vecbiAFPkt.Init(iAFPktLenBits);
	vecbiAFPkt.ResetBitAccess();

	/* SYNC: two-byte ASCII representation of "AF" */
	vecbiAFPkt.Enqueue((uint32_t) 'A', SIZEOF__BYTE);
	vecbiAFPkt.Enqueue((uint32_t) 'F', SIZEOF__BYTE);

	/* LEN: length of the payload, in bytes (4 bytes long -> 32 bits) */
	vecbiAFPkt.Enqueue((uint32_t) iPayloadLenBytes, 32);

	/* SEQ: sequence number. Each AF Packet shall increment the sequence number
	   by one for each packet sent, regardless of content. There shall be no
	   requirement that the first packet received shall have a specific value.
	   The counter shall wrap from FFFF_[16] to 0000_[16], thus the value shall
	   count, FFFE_[16], FFFF_[16], 0000_[16], 0001_[16], etc.
	   (2 bytes long -> 16 bits) */
	vecbiAFPkt.Enqueue((uint32_t) iSeqNumber, 16);

	iSeqNumber++;
	if (iSeqNumber > 0xFFFF)
		iSeqNumber = 0;

	/* AR: AF protocol Revision -
	   a field combining the CF, MAJ and MIN fields */
	/* CF: CRC Flag, 0 if the CRC field is not used (CRC value shall be
	   0000_[16]) or 1 if the CRC field contains a valid CRC (1 bit long) */
	if (bUseAFCRC)
		vecbiAFPkt.Enqueue((uint32_t) 1, 1);
	else
		vecbiAFPkt.Enqueue((uint32_t) 0, 1);

	/* MAJ: major revision of the AF protocol in use (3 bits long) */
	vecbiAFPkt.Enqueue((uint32_t) AF_MAJOR_REVISION, 3);

	/* MIN: minor revision of the AF protocol in use (4 bits long) */
	vecbiAFPkt.Enqueue((uint32_t) AF_MINOR_REVISION, 4);

	/* Protocol Type (PT): single byte encoding the protocol of the data carried
	   in the payload. For TAG Packets, the value shall be the ASCII
	   representation of "T" */
	vecbiAFPkt.Enqueue((uint32_t) 'T', SIZEOF__BYTE);


	/* Payload -------------------------------------------------------------- */

// TODO: copy data byte wise -> check if possible to do that...

	TagPacketGenerator.PutTagPacketData(vecbiAFPkt);

	/* CRC: CRC calculated as described in annex A if the CF field is 1,
	   otherwise 0000_[16] */
	if (bUseAFCRC)
	{
		CCRC CRCObject;

		/* CRC -------------------------------------------------------------- */
		/* Calculate the CRC and put at the end of the stream */
		CRCObject.Reset(16);

		vecbiAFPkt.ResetBitAccess();

		/* 2 bytes CRC -> "- 2" */
		for (int i = 0; i < iAFPktLenBits / SIZEOF__BYTE - 2; i++)
			CRCObject.AddByte((_BYTE) vecbiAFPkt.Separate(SIZEOF__BYTE));

		/* Now, pointer in "enqueue"-function is back at the same place, 
		   add CRC */
		vecbiAFPkt.Enqueue(CRCObject.GetCRC(), 16);
	}
	else
		vecbiAFPkt.Enqueue((uint32_t) 0, 16);

	return PackBytes(vecbiAFPkt);
}

CVector<_BYTE> CAFPacketGenerator::PackBytes(CVector<_BINARY> &vecbiPacket)
{
	/* Convert to bytes and return */
	CVector<_BYTE> vecbyPacket;
	vecbiPacket.ResetBitAccess();
	size_t bits = vecbiPacket.Size();
	size_t bytes = bits / SIZEOF__BYTE;
	vecbyPacket.reserve(bytes);
	for(size_t i=0; i<bytes; i++)
	{
	 	_BYTE byte = (_BYTE)vecbiPacket.Separate(SIZEOF__BYTE);
		vecbyPacket.push_back(byte);
	}
	return vecbyPacket;
}
