/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2001
 *
 * Author(s):
 *	Volker Fischer
 *
 * Description:
 *	See Data.cpp
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

#if !defined(IDEALCHANEST_H__3B0BA660_CA3452341912__INCLUDED_)
#define IDEALCHANEST_H__3B0BA660_CA3452341912__INCLUDED_

#include "../Parameter.h"
#include "../util/Modul.h"
#include "../matlib/Matlib.h"
#include "../util/Vector.h"
#include "ChanEstTime.h" /* Because of "CPilotModiClass" */
#include "../interleaver/SymbolInterleaver.h" /* Because of "D_LENGTH_LONG_INTERL" */


/* Classes ********************************************************************/
class CIdealChanEst :
	public CSimulationModul<CEquSig, CEquSig, CChanSimDataDemod>, 
	public CPilotModiClass
{
public:
	CIdealChanEst() {}
	virtual ~CIdealChanEst() {}

	void GetResults(CVector<_REAL>& vecrResults);

protected:
	int	iNumCarrier;
	int iNumSymPerFrame;
	int iStartDCCar;
	int iNumDCCarriers;
	int iChanEstDelay;
	int iDFTSize;

	CVector<_COMPLEX>	veccEstChan;
	CVector<_COMPLEX>	veccRefChan;
	CVector<_REAL>		vecrMSEAverage;
	long int			lAvCnt;
	int					iStartCnt;

	virtual void InitInternal(CParameter& Parameters);
	virtual void ProcessDataInternal(CParameter& Parameters);
};


#endif // !defined(IDEALCHANEST_H__3B0BA660_CA3452341912__INCLUDED_)
