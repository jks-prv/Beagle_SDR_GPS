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
 *	This is a class derived from CTagPacketDecoder, specialised for the MDI application.
 *
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


#include "TagPacketDecoderMDI.h"

CTagPacketDecoderMDI::CTagPacketDecoderMDI()
:	TagItemDecoderProTy()
,	TagItemDecoderLoFrCnt()
,	TagItemDecoderFAC()
,	TagItemDecoderSDC()
,	TagItemDecoderRobMod()
,	TagItemDecoderStr()
,	TagItemDecoderSDCChanInf(0)
,	TagItemDecoderInfo()
,	TagItemDecoderRxDemodMode()
,	TagItemDecoderAMAudio()

,	TagItemDecoderRmer(0)
,	TagItemDecoderRwmf(0)
,	TagItemDecoderRwmm(0)
,	TagItemDecoderRdbv(0)
,	TagItemDecoderRpsd(0)
,	TagItemDecoderRpir(0)
,	TagItemDecoderRdop(0)
,	TagItemDecoderRdel(0)
,	TagItemDecoderRgps(0)

{

	// Add the tag item decoders to the base class list of decoders
	// This defines the vocabulary of this particular decoder
	AddTagItemDecoder(&TagItemDecoderProTy);
	AddTagItemDecoder(&TagItemDecoderLoFrCnt);
	AddTagItemDecoder(&TagItemDecoderFAC);
	AddTagItemDecoder(&TagItemDecoderSDC);
	AddTagItemDecoder(&TagItemDecoderRobMod);
	TagItemDecoderStr.resize(MAX_NUM_STREAMS);
	for(int i=0; i<MAX_NUM_STREAMS; i++)
	{
		TagItemDecoderStr[i].iStreamNumber = i;
        AddTagItemDecoder(&TagItemDecoderStr[i]);
	}
	AddTagItemDecoder(&TagItemDecoderSDCChanInf);
	AddTagItemDecoder(&TagItemDecoderInfo);
	AddTagItemDecoder(&TagItemDecoderRxDemodMode);
	AddTagItemDecoder(&TagItemDecoderAMAudio);

	// RSCI-specific
	AddTagItemDecoder(&TagItemDecoderRmer);
	AddTagItemDecoder(&TagItemDecoderRwmf);
	AddTagItemDecoder(&TagItemDecoderRwmm);
	AddTagItemDecoder(&TagItemDecoderRdbv);
	AddTagItemDecoder(&TagItemDecoderRpsd);
	AddTagItemDecoder(&TagItemDecoderRpir);
	AddTagItemDecoder(&TagItemDecoderRdop);
	AddTagItemDecoder(&TagItemDecoderRdel);
	AddTagItemDecoder(&TagItemDecoderRgps);

}

void CTagPacketDecoderMDI::SetParameterPtr(CParameter *pP)
{
	// Pass this pointer to all of the tag item decoders that need it, i.e. the RSCI-specific ones
    TagItemDecoderSDCChanInf.SetParameterPtr(pP);
    TagItemDecoderRmer.SetParameterPtr(pP);
	TagItemDecoderRwmf.SetParameterPtr(pP);
	TagItemDecoderRwmm.SetParameterPtr(pP);
	TagItemDecoderRdbv.SetParameterPtr(pP);
	TagItemDecoderRpsd.SetParameterPtr(pP);
	TagItemDecoderRpir.SetParameterPtr(pP);
	TagItemDecoderRdop.SetParameterPtr(pP);
	TagItemDecoderRdel.SetParameterPtr(pP);
	TagItemDecoderRgps.SetParameterPtr(pP);
}

void CTagPacketDecoderMDI::DecodeTagPackets(CVectorEx<_BINARY>& vecbiPkt, int iPayloadLen)
{
	// Initialise all the decoders: this will set them to "not ready"
	InitTagItemDecoders();
	// Set strx tags data to zero length in case they are not present
	for(int i=0; i<MAX_NUM_STREAMS; i++)
	{
		TagItemDecoderStr[i].vecbidata.Init(0);
	}
	TagItemDecoderAMAudio.vecbidata.Init(0);
	TagItemDecoderFAC.vecbidata.Init(0);
	TagItemDecoderSDC.vecbidata.Init(0);

	TagItemDecoderRobMod.Init();
	// Call base class function to do the actual decoding
	CTagPacketDecoder::DecodeTagPackets(vecbiPkt, iPayloadLen);
}
