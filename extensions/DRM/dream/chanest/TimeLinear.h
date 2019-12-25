/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2001
 *
 * Author(s):
 *	Volker Fischer
 *
 * Description:
 *	See TimeLinear.cpp
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

#if !defined(TIMELINEAR_H__3B0BA6346234660_CA63_4344_BBE7A0D31912__INCLUDED_)
#define TIMELINEAR_H__3B0BA6346234660_CA63_4344_BBE7A0D31912__INCLUDED_

#include "../GlobalDefinitions.h"
#include "../Parameter.h"
#include "../util/Vector.h"
#include "../OFDMcellmapping/OFDMCellMapping.h"
#include "../matlib/Matlib.h"
#include "ChanEstTime.h"


/* Definitions ****************************************************************/
#define MAX_NO_SYMB_IN_HISTORY_LIN			(RMA_SCAT_PIL_TIME_INT + 1)		


/* Classes ********************************************************************/
class CTimeLinear : public CChanEstTime
{
public:
	CTimeLinear() {}
	virtual ~CTimeLinear() {}

	virtual int Init(CParameter& Parameter);
	virtual _REAL Estimate(CVectorEx<_COMPLEX>* pvecInputData, 
						   CComplexVector& veccOutputData, 
						   CVector<int>& veciMapTab, 
						   CVector<_COMPLEX>& veccPilotCells, _REAL rSNR);

protected:
	int					iNumCarrier;
	int					iNumIntpFreqPil;
	int					iScatPilFreqInt;
	CMatrix<_COMPLEX>	matcChanEstHist;

	int					iLenHistBuff;

	CShiftRegister<int>	vecTiCorrHist;
	int					iLenTiCorrHist;
};


#endif // !defined(TIMELINEAR_H__3B0BA6346234660_CA63_4344_BBE7A0D31912__INCLUDED_)
