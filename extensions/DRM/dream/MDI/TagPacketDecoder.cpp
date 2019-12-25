/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2004
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
 *  This module defines the base class CTagPacketDecoder which can be used to
 *  derive a decoder specialised to a particular TAG layer application.
 *  The class deals with AF packet decoding, and TAG packet decoding.
 *
 *  The TAG packet decoding works by keeping a collection (vector of pointers) of TAG item decoders. This collection
 *  should be populated by the constructor of the derived class. This class loops through each TAG item,
 *  checking the TAG name against all the decoders' TAG names. If it matches, it calls the TAG item decoder
 *  itself to do the actual decoding.
 *
 *  TODO: It would be better to separate the AF decoding into its own class.
 *  PFT also needs to be added (again, in its own class).
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

#include "TagPacketDecoder.h"
#include "TagItemDecoder.h"
#include "../util/CRC.h"
#include <iostream>

CTagPacketDecoder::CTagPacketDecoder() : vecpTagItemDecoders(0),iSeqNumber(0xffff)
{
}

// This should be in its own class
CTagPacketDecoder::Error
CTagPacketDecoder::DecodeAFPacket(CVectorEx<_BINARY>& vecbiAFPkt)
{

	int i;

	/* CRC check ------------------------------------------------------------ */
	CCRC CRCObject;
	// FIXME: is this length always the correct length? In the actual packet
	// there is also a value for the length included!!!???!???
	int iLenAFPkt = vecbiAFPkt.Size();

	/* We do the CRC check at the beginning no matter if it is used or not
	   since we have to reset bit access for that */
	/* Reset bit extraction access */
	vecbiAFPkt.ResetBitAccess();

	/* Check the CRC of this packet */
	CRCObject.Reset(16);

	/* "- 2": 16 bits for CRC at the end */
	for (i = 0; i < iLenAFPkt / SIZEOF__BYTE - 2; i++)
	{
		CRCObject.AddByte((_BYTE) vecbiAFPkt.Separate(SIZEOF__BYTE));
	}

	int iCRC = vecbiAFPkt.Separate(16);

	const bool bCRCOk = CRCObject.CheckCRC(iCRC);
    (void)bCRCOk;

	/* Actual packet decoding ----------------------------------------------- */
	vecbiAFPkt.ResetBitAccess();

	/* SYNC: two-byte ASCII representation of "AF" (2 bytes) */
	string strSyncASCII = "";
	for (i = 0; i < 2; i++)
	{
		strSyncASCII += (_BYTE) vecbiAFPkt.Separate(SIZEOF__BYTE);
	}
	/* Check if string is correct */
	if (strSyncASCII.compare("AF") != 0)
	{
		cerr << "not an AF packet" << endl;
		return E_SYNC;
	}

	/* LEN: length of the payload, in bytes (4 bytes long -> 32 bits) */
	const int iPayLLen = (int) vecbiAFPkt.Separate(32);

	/* SEQ: sequence number. Each AF Packet shall increment the sequence number
	   by one for each packet sent, regardless of content. There shall be no
	   requirement that the first packet received shall have a specific value.
	   The counter shall wrap from FFFF_[16] to 0000_[16], thus the value shall
	   count, FFFE_[16], FFFF_[16], 0000_[16], 0001_[16], etc.
	   (2 bytes long -> 16 bits) */

	const int iCurSeqNum = (int) vecbiAFPkt.Separate(16);
	(void)iCurSeqNum;
	iSeqNumber++;
	if(iSeqNumber!=iCurSeqNum)
	{
		if(iSeqNumber!=0xffff)
			cerr << "AF SEQ: expected " << iSeqNumber << " got " << iCurSeqNum << endl;
		iSeqNumber=iCurSeqNum;
	}

	/* AR: AF protocol Revision -
	   a field combining the CF, MAJ and MIN fields */
	/* CF: CRC Flag, 0 if the CRC field is not used (CRC value shall be
	   0000_[16]) or 1 if the CRC field contains a valid CRC (1 bit long) */
	if (vecbiAFPkt.Separate(1)==1)
	{
		/* Use CRC check which was already done */
//		if (!bCRCOk)
//			return E_CRC;
	}

	/* MAJ: major revision of the AF protocol in use (3 bits long) */
	const int iMajRevAFProt = (int) vecbiAFPkt.Separate(3);
	(void)iMajRevAFProt;

	/* MIN: minor revision of the AF protocol in use (4 bits long) */
	const int iMinRevAFProt = (int) vecbiAFPkt.Separate(4);
	(void)iMinRevAFProt;

// TODO: check if protocol versions match our version!!!!!!!!!!!!!!!!!!!!!!!!!!!!!


	/* Protocol Type (PT): single byte encoding the protocol of the data carried
	   in the payload. For TAG Packets, the value shall be the ASCII
	   representation of "T" */
	if ((_BYTE) vecbiAFPkt.Separate(SIZEOF__BYTE) != 'T')
	{
		return E_PROTO;
	}


	/* Payload -------------------------------------------------------------- */
	DecodeTagPackets(vecbiAFPkt, iPayLLen);

	return E_OK;
}

	// Decode all the tags in the tag packet. To do things before or after the decoding,
	// override this and call the base class function to do the decoding
void CTagPacketDecoder::DecodeTagPackets(CVectorEx<_BINARY>& vecbiPkt, const int iPayloadLen)
{
	/* Decode all tags */
	int iCurConsBytes = 0;

	/* Each tag must have at least a header with 8 bytes -> "- 8" */
	while (iCurConsBytes < iPayloadLen - 8)
		iCurConsBytes += DecodeTag(vecbiPkt);
}

	// Go through all the tag item decoders to find one that matches the current tag name.
int CTagPacketDecoder::DecodeTag(CVector<_BINARY>& vecbiTag)
{
	int i;

	/* Decode tag name (always four bytes long) */
	string strTagName = "";
	for (i = 0; i < 4; i++)
		strTagName += (_BYTE) vecbiTag.Separate(SIZEOF__BYTE);
	/* Get tag data length (4 bytes = 32 bits) */
	const int iLenDataBits = vecbiTag.Separate(32);
	/* Check the tag name against each tag decoder in the vector of tag decoders */
	bool bTagWasDec = false;
	for (i=0; i<vecpTagItemDecoders.Size(); i++)
	{
		if (strTagName.compare(vecpTagItemDecoders[i]->GetTagName()) == 0) // it's this tag
		{
			vecpTagItemDecoders[i]->DecodeTag(vecbiTag, iLenDataBits);
			bTagWasDec = true;
		}
	}

	/* Take care of tags which are not supported */
	if (bTagWasDec == false)
	{
		/* Take bits from tag vector */
		for (i = 0; i < iLenDataBits; i++)
			vecbiTag.Separate(1);
	}

	/* Return number of consumed bytes. This number is the actual body plus two
	   times for bytes for the header = 8 bytes */
	return iLenDataBits / SIZEOF__BYTE + 8;

}


void CTagPacketDecoder::AddTagItemDecoder(CTagItemDecoder *pTagItemDecoder)
{
	vecpTagItemDecoders.Add(pTagItemDecoder);
}

void CTagPacketDecoder::InitTagItemDecoders(void)
{
	for (int i=0; i<vecpTagItemDecoders.Size(); i++)
	{
		vecpTagItemDecoders[i]->Init();
	}

}
