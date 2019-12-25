/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2001
 *
 * Author(s):
 *	Volker Fischer
 *
 * Description:
 *	Base class of all channel estimation modules in time diretion
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

#if !defined(CHANESTTIME_H__3B0BA6346234660_CA63_34523457A0D31912__INCLUDED_)
#define CHANESTTIME_H__3B0BA6346234660_CA63_34523457A0D31912__INCLUDED_

#include "../GlobalDefinitions.h"
#include "../Parameter.h"
#include "../util/Vector.h"
#include "../matlib/Matlib.h"


/* Classes ********************************************************************/
/* Pilot modification class (e.g., rotation of complex std::vector) */
class CPilotModiClass
{
public:
	CPilotModiClass() {}
	virtual ~CPilotModiClass() {}

	void InitRot(CParameter& Parameter);

//protected:
	_COMPLEX Rotate(const _COMPLEX cI, const int iCN, const int iTiDi) const;

private:
	int		iKminAbs;
	_REAL	rArgMult;
};


/* Base class for channel estimation in time direction. Defines the prototypes
   which are common for derived classes */
class CChanEstTime : public CPilotModiClass
{
public:
	CChanEstTime() {}
	virtual ~CChanEstTime() {}

	virtual _REAL Estimate(CVectorEx<_COMPLEX>* pvecInputData, 
						  CComplexVector& veccOutputData, 
						  CVector<int>& veciMapTab, 
						  CVector<_COMPLEX>& veccPilotCells, _REAL rSNR) = 0;

	virtual int Init(CParameter& Parameter) = 0;
};


#endif // !defined(CHANESTTIME_H__3B0BA6346234660_CA63_34523457A0D31912__INCLUDED_)
