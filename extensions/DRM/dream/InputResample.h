/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2001
 *
 * Author(s):
 *	Volker Fischer
 *
 * Description:
 *	See InputResample.cpp
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

#if !defined(INPUTRESAMPLE_H__3B0BA660_CA63_4344_BB2B_2OJVBEWJBWV_INCLUDED_)
#define INPUTRESAMPLE_H__3B0BA660_CA63_4344_BB2B_2OJVBEWJBWV_INCLUDED_

#include "Parameter.h"
#include "util/Modul.h"
#include "resample/Resample.h"


/* Definitions ****************************************************************/
#define MAX_RESAMPLE_OFFSET			200 /* Hz */


/* Classes ********************************************************************/
class CInputResample : public CReceiverModul<_REAL, _REAL>
{
public:
    CInputResample() : bSyncInput(false) {}
    virtual ~CInputResample() {}

    /* To set the module up for synchronized DRM input data stream */
    void SetSyncInput(bool bNewS) {
        bSyncInput = bNewS;
    }

protected:
    CResample	ResampleObj;
    bool	bSyncInput;

    virtual void InitInternal(CParameter& Parameters);
    virtual void ProcessDataInternal(CParameter& Parameters);
};


#endif // !defined(INPUTRESAMPLE_H__3B0BA660_CA63_4344_BB2B_2OJVBEWJBWV_INCLUDED_)
