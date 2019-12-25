/******************************************************************************\
 * British Broadcasting Corporation
 * Copyright (c) 2006
 *
 * Author(s):
 *	Julian Cable
 *
 * Description:
 *	see Pft.cpp
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

#ifndef PFT_H_INCLUDED
#define PFT_H_INCLUDED

#include "../util/Reassemble.h"
#include <map>

class CPft
{
public:
	CPft(int isrc=-1, int idst=-1);

	bool DecodePFTPacket(const std::vector<_BYTE>& vecIn, std::vector<_BYTE>& vecOut);
	static void MakePFTPackets(const std::vector < _BYTE > &vecbydata,
					 std::vector < std::vector < _BYTE > >&packets, 
					uint16_t sequence_counter, size_t fragment_size);

protected:

	bool DecodeSimplePFTPacket(const std::vector<_BYTE>& vecIn, std::vector<_BYTE>& vecOut);
	bool DecodePFTPacketWithFEC(const std::vector<_BYTE>& vecIn, std::vector<_BYTE>& vecOut);

	int iSource, iDest;
    std::map<int,CReassemblerN> mapFragments;
	int iHeaderLen;
	int iPseq;
	int iFindex;
	int iFcount;
	int iFEC;
	int iAddr;
	int iPlen;
};

#endif
