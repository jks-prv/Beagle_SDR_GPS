/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2001
 *
 * Author(s):
 *	Volker Fischer
 *
 * Description:
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

#if !defined(ENERGYDISPERSAL_H__3B0BA660_CA63_4344_2B_23453E7A0D31912__INCLUDED_)
#define ENERGYDISPERSAL_H__3B0BA660_CA63_4344_2B_23453E7A0D31912__INCLUDED_

#include "../GlobalDefinitions.h"
#include "../util/Vector.h"


/* Classes ********************************************************************/
class CEngergyDispersal
{
public:
    CEngergyDispersal() {}
    virtual ~CEngergyDispersal() {}

    void ProcessData(CVector<_BINARY>* pbiData);
    void Init(int iNewNumInBits, int iNewLengthVSPP);

protected:
    int			iNumInBits;
    int			iStartIndVSPP;
    int			iEndIndVSPP;
    uint32_t	iShiftRegisterSPP;
    uint32_t	iShiftRegisterVSPP;
};


#endif // !defined(ENERGYDISPERSAL_H__3B0BA660_CA63_4344_2B_23453E7A0D31912__INCLUDED_)
