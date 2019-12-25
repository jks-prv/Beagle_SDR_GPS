/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2001
 *
 * Author(s):
 *	Volker Fischer, Julian Cable
 *
 * Description:
 *	Data module (using multimedia information carried in DRM stream)
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

#include "DataEncoder.h"

/* Implementation *************************************************************/

void
CDataEncoder::GeneratePacket(CVector < _BINARY > &vecbiPacket)
{
	int i;
	bool bLastFlag;

	/* Init size for whole packet, not only body */
	vecbiPacket.Init(iTotalPacketSize);
	vecbiPacket.ResetBitAccess();

	/* Calculate remaining data size to be transmitted */
	const int iRemainSize = vecbiCurDataUnit.Size() - iCurDataPointer;

	/* Header --------------------------------------------------------------- */
	/* First flag */
	if (iCurDataPointer == 0)
		vecbiPacket.Enqueue((uint32_t) 1, 1);
	else
		vecbiPacket.Enqueue((uint32_t) 0, 1);

	/* Last flag */
	if (iRemainSize > iPacketLen)
	{
		vecbiPacket.Enqueue((uint32_t) 0, 1);
		bLastFlag = false;
	}
	else
	{
		vecbiPacket.Enqueue((uint32_t) 1, 1);
		bLastFlag = true;
	}

	/* Packet Id */
	vecbiPacket.Enqueue((uint32_t) iPacketID, 2);

	/* Padded packet indicator (PPI) */
	if (iRemainSize < iPacketLen)
		vecbiPacket.Enqueue((uint32_t) 1, 1);
	else
		vecbiPacket.Enqueue((uint32_t) 0, 1);

	/* Continuity index (CI) */
	vecbiPacket.Enqueue((uint32_t) iContinInd, 3);

	/* Increment index modulo 8 (1 << 3) */
	iContinInd++;
	if (iContinInd == 8)
		iContinInd = 0;

	/* Body ----------------------------------------------------------------- */
	if (iRemainSize >= iPacketLen)
	{
		if (iRemainSize == iPacketLen)
		{
			/* Last packet */
			for (i = 0; i < iPacketLen; i++)
				vecbiPacket.Enqueue(vecbiCurDataUnit.Separate(1), 1);
		}
		else
		{
			for (i = 0; i < iPacketLen; i++)
			{
				vecbiPacket.Enqueue(vecbiCurDataUnit.Separate(1), 1);
				iCurDataPointer++;
			}
		}
	}
	else
	{
		/* Padded packet. If the PPI is 1 then the first byte shall indicate
		   the number of useful bytes that follow, and the data field is
		   completed with padding bytes of value 0x00 */
		vecbiPacket.Enqueue((uint32_t) (iRemainSize / SIZEOF__BYTE),
							SIZEOF__BYTE);

		/* Data */
		for (i = 0; i < iRemainSize; i++)
			vecbiPacket.Enqueue(vecbiCurDataUnit.Separate(1), 1);

		/* Padding */
		for (i = 0; i < iPacketLen - iRemainSize; i++)
			vecbiPacket.Enqueue(vecbiCurDataUnit.Separate(1), 1);
	}

	/* If this was the last packet, get data for next data unit */
	if (bLastFlag)
	{
		/* Generate new data unit */
		MOTSlideShowEncoder.GetDataUnit(vecbiCurDataUnit);
		vecbiCurDataUnit.ResetBitAccess();

		/* Reset data pointer and continuity index */
		iCurDataPointer = 0;
	}

	/* CRC ------------------------------------------------------------------ */
	CCRC CRCObject;

	/* Reset bit access */
	vecbiPacket.ResetBitAccess();

	/* Calculate the CRC and put it at the end of the segment */
	CRCObject.Reset(16);

	/* "byLengthBody" was defined in the header */
	for (i = 0; i < (iTotalPacketSize / SIZEOF__BYTE - 2); i++)
		CRCObject.AddByte(_BYTE(vecbiPacket.Separate(SIZEOF__BYTE)));

	/* Now, pointer in "enqueue"-function is back at the same place, add CRC */
	vecbiPacket.Enqueue(CRCObject.GetCRC(), 16);
}

int
CDataEncoder::Init(CParameter & Param)
{
	/* Init packet length and total packet size (the total packet length is
	   three bytes longer as it includes the header and CRC fields) */

// TODO we only use always the first service right now
	const int iCurSelDataServ = 0;

// CAudioSourceEncoderImplementation::InitInternalTx already have the lock
//	Param.Lock();

	iPacketLen =
		Param.Service[iCurSelDataServ].DataParam.iPacketLen * SIZEOF__BYTE;
	iTotalPacketSize = iPacketLen + 24 /* CRC + header = 24 bits */ ;

	iPacketID = Param.Service[iCurSelDataServ].DataParam.iPacketID;

// CAudioSourceEncoderImplementation::InitInternalTx already have the lock
//	Param.Unlock();

	/* Init DAB MOT encoder object */
	MOTSlideShowEncoder.Init();

	/* Generate first data unit */
	MOTSlideShowEncoder.GetDataUnit(vecbiCurDataUnit);
	vecbiCurDataUnit.ResetBitAccess();

	/* Reset pointer to current position in data unit and continuity index */
	iCurDataPointer = 0;
	iContinInd = 0;

	/* Return total packet size */
	return iTotalPacketSize;
}
