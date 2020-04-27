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
 *  This module defines both the CTagPacketGenerator base class and a CTagPacketGeneratorWithProfiles derived class.
 *
 *  CTagPacketGenerator does the job of putting together all the tag items, delegating the 
 *  actual generation to each of a collection of CTagItemGenerators.
 *  This collection should be filled up by the client code first calling Reset() and then 
 *  repeatedly calling AddTagItem() with a generator for each tag item.
 *  
 *  CTagPacketGeneratorWithProfiles is a derived class that takes care of protocols like RSCI/MDI
 *  in which a number of protocols are defined. The client should call SetProfile() before adding the
 *  tag items. Use AddTagItemIfInProfile() instead of AddTagItem().
 *  (Maybe this could just be an overridden version of AddTagItem()? )
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



#include "TagPacketGenerator.h"
#include <iostream>

CTagPacketGenerator::CTagPacketGenerator() : vecTagItemGenerators(0) 
{
}

void CTagPacketGenerator::PutTagPacketData(CVector<_BINARY> &vecbiDestination)
{
	for (size_t i=0; i<vecTagItemGenerators.size(); i++)
	{
		vecTagItemGenerators[i]->PutTagItemData(vecbiDestination);
	}
}

int CTagPacketGenerator::GetTagPacketLength()
{
	int iPayloadLenBytes = 0;
		
	for (size_t i=0; i<vecTagItemGenerators.size(); i++)
	{
		int n = vecTagItemGenerators[i]->GetTotalLength() / SIZEOF__BYTE;
		iPayloadLenBytes += n;
	}
	return iPayloadLenBytes;
}


void CTagPacketGenerator::AddTagItem(CTagItemGenerator *pGenerator)
{
	vecTagItemGenerators.push_back(pGenerator);
}


CTagPacketGeneratorWithProfiles::CTagPacketGeneratorWithProfiles(char cProf) : cProfile(cProf)
{
}

void CTagPacketGeneratorWithProfiles::SetProfile(char cProf)
{
	cProfile = cProf;
}


void CTagPacketGeneratorWithProfiles::PutTagPacketData(CVector<_BINARY> &vecbiDestination)
{
	for (size_t i=0; i<vecTagItemGenerators.size(); i++)
	{
		if (vecTagItemGenerators[i]->IsInProfile(cProfile))
			vecTagItemGenerators[i]->PutTagItemData(vecbiDestination);
	}
}

int CTagPacketGeneratorWithProfiles::GetTagPacketLength()
{
	int iPayloadLenBytes = 0;
		
	for (size_t i=0; i<vecTagItemGenerators.size(); i++)
	{
		if (vecTagItemGenerators[i]->IsInProfile(cProfile))
		{
			int n = vecTagItemGenerators[i]->GetTotalLength() / SIZEOF__BYTE;
			iPayloadLenBytes += n;
		}
	}
	return iPayloadLenBytes;
}
