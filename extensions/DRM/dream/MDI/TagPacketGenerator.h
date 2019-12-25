/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2004
 *
 * Author(s):
 *	Volker Fischer, Oliver Haffenden
 *
 * Description:
 *	see TagPacketGenerator.cpp
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

#ifndef TAG_PACKET_GENERATOR_H_INCLUDED
#define TAG_PACKET_GENERATOR_H_INCLUDED

#include "../GlobalDefinitions.h"
#include "MDIDefinitions.h"
#include "MDITagItems.h"
#include "MDITagItems.h"
#include "../Parameter.h"
#include "../util/Vector.h"
#include "../util/Buffer.h"
#include "../util/CRC.h"


class CTagPacketGenerator
{
public:
	CTagPacketGenerator(); 
	virtual ~CTagPacketGenerator(){}
	void Reset() {vecTagItemGenerators.clear();}
	void AddTagItem(CTagItemGenerator *pGenerator);
	virtual void PutTagPacketData(CVector<_BINARY> &vecbiDestination); // Call this to write the tag packet (i.e. all the tag items) to the std::vector
	virtual int GetTagPacketLength();
	virtual void SetProfile(const char /*cProfile*/) {}
protected:
	std::vector<CTagItemGenerator *> vecTagItemGenerators;
};


class CTagPacketGeneratorWithProfiles : public CTagPacketGenerator
{
public:
	CTagPacketGeneratorWithProfiles(const char cProfile = '\0');
	virtual ~CTagPacketGeneratorWithProfiles(){}
	/* The following functions are overridden to check the profile for each tag item */
	virtual void PutTagPacketData(CVector<_BINARY> &vecbiDestination); // Call this to write the tag packet (i.e. all the tag items) to the std::vector
	virtual int GetTagPacketLength(void);
	virtual void SetProfile(const char cProfile);
private:
	char cProfile;
};

#endif
