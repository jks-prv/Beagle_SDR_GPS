/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2001
 *
 * Author(s):
 *	Volker Fischer
 *
 * Description:
 *	Channel estimation in time direction, base class
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

#include "ChanEstTime.h"


/* Implementation *************************************************************/
void CPilotModiClass::InitRot(CParameter& Parameter) 
{
	/* Inits for timing correction. We need FFT size and start carrier */
	/* Pre calculate the argument of the exp function */
	rArgMult = (_REAL) -2.0 * crPi / Parameter.CellMappingTable.iFFTSizeN;
	
	/* Index of minimum useful carrier */
	iKminAbs = Parameter.CellMappingTable.iShiftedKmin;
}

_COMPLEX CPilotModiClass::Rotate(const _COMPLEX cI, const int iCN, 
								 const int iTiDi) const
{
	/* If "iTiDi" equals "0", rArg is also "0", we need no cos or sin
	   function */
	if (iTiDi != 0)
	{
		/* First calculate the argument */
		const _REAL rArg = rArgMult * iTiDi * (iKminAbs + iCN);

		/* * exp(2 * pi * TimeDiff / norm) */
		return _COMPLEX(cos(rArg), sin(rArg)) * cI;
	}
	else
		return cI;
}
