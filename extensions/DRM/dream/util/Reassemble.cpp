/******************************************************************************\
 * BBC World Service
 * Copyright (c) 2006
 *
 * Author(s):
 *	Julian Cable
 *
 * Description:
 *	General Purpose Packet ReassemblerNs for data packet mode, MOT and PFT
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

#include "Reassemble.h"
#include <iostream>
using namespace std;

CReassemblerN::CReassemblerN (const CReassemblerN & r):iLastSegmentNum (r.iLastSegmentNum),
		iLastSegmentSize (r.iLastSegmentSize),
		iSegmentSize (r.iSegmentSize), Tracker (r.Tracker), bReady (r.bReady)
{
	vecData.resize (r.vecData.size ());
	vecData = r.vecData;
	vecLastSegment.resize (r.vecLastSegment.size ());
	vecLastSegment = r.vecLastSegment;
}

CReassemblerN & CReassemblerN::operator= (const CReassemblerN & r)
{
	iLastSegmentNum = r.iLastSegmentNum;
	iLastSegmentSize = r.iLastSegmentSize;
	iSegmentSize = r.iSegmentSize;
	Tracker = r.Tracker;
	vecData.resize (r.vecData.size ());
	vecData = r.vecData;
	vecLastSegment.resize (r.vecLastSegment.size ());
	vecLastSegment = r.vecLastSegment;
	bReady = r.bReady;

	return *this;
}

void
CReassemblerN::AddSegment (vector<_BYTE> &vecDataIn, int iSegNum, bool bLast)
{
	if (bLast)
	{
		if (iLastSegmentNum == -1)
		{
			iLastSegmentNum = iSegNum;
            iLastSegmentSize = int(vecDataIn.size());
			/* three cases:
			   1: single segment - easy! (actually degenerate with case 3)
			   2: multi-segment and the last segment came first.
			   3: normal - some segment, not the last, came first, 
			   we know the segment size
			 */
			if (iSegNum == 0)
			{					/* case 1 */
				iSegmentSize = vecDataIn.size();
                copyin(vecDataIn, 0);
			}
			else if (iSegmentSize == 0)
			{					/* case 2 */
                cachelast(vecDataIn, vecDataIn.size());
			}
			else
			{					/* case 3 */
                copyin(vecDataIn, size_t(iSegNum));
			}
		}						/* otherwise do nothing as we already have the last segment */
	}
	else
	{
		iSegmentSize = vecDataIn.size();
		if (Tracker.HaveSegment (iSegNum) == false)
		{
            copyin(vecDataIn, size_t(iSegNum));
		}
	}
	Tracker.AddSegment (iSegNum);	/* tracking the last segment makes the Ready work! */

	if ((iLastSegmentSize != -1)	/* we have the last segment */
		&& (bReady == false)	/* we haven't already completed reassembly */
		&& Tracker.Ready ()		/* there are no gaps */
		)
	{
		if (vecLastSegment.size () > 0)
		{
			/* we have everything, but the last segment came first */
            copylast();
		}
		bReady = true;
	}
}

void
CReassemblerN::copyin(vector < _BYTE > &vecDataIn, size_t iSegNum)
{
    size_t offset = iSegNum * iSegmentSize;
    size_t iNewSize = offset + vecDataIn.size();
    if (vecData.size () < iNewSize)
		vecData.resize (iNewSize);
    for (size_t i = 0; i < vecDataIn.size(); i++)
		vecData[offset + i] = vecDataIn[i];
}

void
CReassemblerN::cachelast(vector < _BYTE > &vecDataIn, size_t iSegSize)
{
    vecLastSegment.resize (iSegSize);
    for (size_t i = 0; i < iSegSize; i++)
		vecLastSegment[i] = vecDataIn[i];
}

void
CReassemblerN::copylast()
{
    size_t offset = size_t(iLastSegmentNum) * iSegmentSize;
    vecData.resize (vecData.size()+vecLastSegment.size ());
    for (size_t i = 0; i < size_t (vecLastSegment.size ()); i++)
		vecData[offset + i] = vecLastSegment[i];
    vecLastSegment.resize (0);
}

