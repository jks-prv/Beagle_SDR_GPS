/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2001
 *
 * Author(s):
 *	Volker Fischer
 *
 * Description:
 *	See FAC.cpp
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

#if !defined(FAC_H__3B0BA660_CA63VEUASDVN89LKVNE877A0D31912__INCLUDED_)
#define FAC_H__3B0BA660_CA63VEUASDVN89LKVNE877A0D31912__INCLUDED_

#include "../GlobalDefinitions.h"
#include "../tables/TableFAC.h"
#include "../Parameter.h"
#include "../util/CRC.h"
#include "../util/Vector.h"


/* Classes ********************************************************************/
class CFACTransmit
{
public:
    CFACTransmit():FACRepetitionCounter(0) {}
    virtual ~CFACTransmit() {}

    /* "pbiFACData" contains 72 bits */
    void FACParam(CVector<_BINARY>* pbiFACData, CParameter& Parameter);
    void Init(CParameter& Parameter);

protected:
    CCRC CRCObject;
    std::vector<int>	FACRepetition; /* See 6.3.6 */
    size_t		FACNumRep;
    size_t		FACRepetitionCounter;
};

class CFACReceive
{
public:
    CFACReceive() {}
    virtual ~CFACReceive() {}

    /* "pbiFACData" contains 72 bits */
    bool FACParam(CVector<_BINARY>* pbiFACData, CParameter& Parameter);

protected:
    CCRC CRCObject;
};


#endif // !defined(FAC_H__3B0BA660_CA63VEUASDVN89LKVNE877A0D31912__INCLUDED_)
