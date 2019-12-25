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
 *  This module derives a class from the CTagPacketDecoder base class, specialised for the
 *  TAG items defined in the control part of RSCI.
 *  The TAG item decoders need to be given a pointer to the CDRMReceiver object so they
 *  can send commands (e.g. change frequency) to it.
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



#include "TagPacketDecoderRSCIControl.h"

// constructor: adds all of the decoders in the vocabulary to the list
CTagPacketDecoderRSCIControl::CTagPacketDecoderRSCIControl(void) 
: pDRMReceiver(nullptr)
, TagItemDecoderCact()
, TagItemDecoderCfre()
, TagItemDecoderCdmo()
, TagItemDecoderCrec()
, TagItemDecoderCpro()
{
	// Add each tag item decoder to the vocabulary
	AddTagItemDecoder(&TagItemDecoderCact);
	AddTagItemDecoder(&TagItemDecoderCfre);
	AddTagItemDecoder(&TagItemDecoderCdmo);
	AddTagItemDecoder(&TagItemDecoderCrec);
	AddTagItemDecoder(&TagItemDecoderCpro);
}


void CTagPacketDecoderRSCIControl::SetReceiver(CDRMReceiver *pReceiver)
{
	pDRMReceiver = pReceiver;
	TagItemDecoderCact.SetReceiver(pReceiver);
	TagItemDecoderCfre.SetReceiver(pReceiver);
	TagItemDecoderCdmo.SetReceiver(pReceiver);
	TagItemDecoderCrec.SetReceiver(pReceiver);
}

void CTagPacketDecoderRSCIControl::SetSubscriber(CRSISubscriber *pSubscriber)
{
	TagItemDecoderCpro.SetSubscriber(pSubscriber);
}
