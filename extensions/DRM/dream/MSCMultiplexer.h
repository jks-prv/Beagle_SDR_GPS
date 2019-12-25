/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2001
 *
 * Author(s):
 *	Volker Fischer
 *
 * Description:
 *	See MSCMultiplexer.cpp
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

#if !defined(MSCMUX_H__3B0BA660_JLBVEOUB239485BB2B_23E7A0D31912__INCLUDED_)
#define MSCMUX_H__3B0BA660_JLBVEOUB239485BB2B_23E7A0D31912__INCLUDED_

#include "GlobalDefinitions.h"
#include "Parameter.h"
#include "util/Buffer.h"
#include "util/Modul.h"
#include "util/Vector.h"

/* Classes ********************************************************************/
class CMSCDemultiplexer : public CReceiverModul<_BINARY, _BINARY>
{
public:
    CMSCDemultiplexer() {}
    virtual ~CMSCDemultiplexer() {}

protected:
    struct SStreamPos
    {
        int	iOffsetLow;
        int	iOffsetHigh;
        int	iLenLow;
        int	iLenHigh;
    };

    SStreamPos			StreamPos[4];

    SStreamPos GetStreamPos(CParameter& Param, const int iStreamID);
    void ExtractData(CVectorEx<_BINARY>& vecIn, CVectorEx<_BINARY>& vecOut,
                     SStreamPos& StrPos);

    virtual void InitInternal(CParameter& Parameters);
    virtual void ProcessDataInternal(CParameter& Parameters);
};


#endif // !defined(MSCMUX_H__3B0BA660_JLBVEOUB239485BB2B_23E7A0D31912__INCLUDED_)
