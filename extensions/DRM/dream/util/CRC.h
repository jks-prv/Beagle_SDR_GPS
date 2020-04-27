/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2001
 *
 * Author(s):
 *	Volker Fischer
 *
 * Description:
 *	See CRC.cpp
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

#if !defined(CRC_H__3B0BA660_CA63_4VASDGLJNAJ2B_23E7A0D31912__INCLUDED_)
#define CRC_H__3B0BA660_CA63_4VASDGLJNAJ2B_23E7A0D31912__INCLUDED_

#include "../GlobalDefinitions.h"


/* Classes ********************************************************************/
class CCRC
{
public:
	CCRC();
	virtual ~CCRC() {}

	void Reset(const int iNewDegree);
	void AddByte(const _BYTE byNewInput);
	void AddBit(const _BINARY biNewInput);
	bool CheckCRC(const uint32_t iCRC);
	uint32_t GetCRC();

protected:
	int			iDegIndex;
	uint32_t	iBitOutPosMask;

	uint32_t	iPolynMask[16];
	uint32_t	iStateShiftReg;
};


#endif // !defined(CRC_H__3B0BA660_CA63_4VASDGLJNAJ2B_23E7A0D31912__INCLUDED_)
