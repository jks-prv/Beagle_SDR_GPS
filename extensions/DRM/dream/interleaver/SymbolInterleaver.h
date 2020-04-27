/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2001
 *
 * Author(s):
 *	Volker Fischer
 *
 * Description:
 *	See SymbolInterleaver.cpp
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

#if !defined(CONVINTERLEAVER_H__3B0BA660_CA63_4344_BB2B_23E7A0D31912__INCLUDED_)
#define CONVINTERLEAVER_H__3B0BA660_CA63_4344_BB2B_23E7A0D31912__INCLUDED_

#include "../GlobalDefinitions.h"
#include "../Parameter.h"
#include "../util/Modul.h"
#include "BlockInterleaver.h"


/* Definitions ****************************************************************/
#define D_LENGTH_SHORT_INTERL		1
#define D_LENGTH_LONG_INTERL		5

/* This constant is defined in DRM-standard for MSC Cell Interleaving */
#define SYMB_INTERL_CONST_T_0		5


/* Classes ********************************************************************/
class CSymbInterleaver : public CTransmitterModul<_COMPLEX, _COMPLEX>, 
						 public CBlockInterleaver
{
public:
	CSymbInterleaver() {}
	virtual ~CSymbInterleaver() {}

protected:
	int					iN_MUX;
	CMatrix<_COMPLEX>	matcInterlMemory;
	CVector<int>		veciCurIndex;
	CVector<int>		veciIntTable;
	int					iD;

	virtual void InitInternal(CParameter& TransmParam);
	virtual void ProcessDataInternal(CParameter& TransmParam);
};

class CSymbDeinterleaver : public CReceiverModul<CEquSig, CEquSig>, 
						   public CBlockInterleaver
{
public:
	CSymbDeinterleaver() {}
	virtual ~CSymbDeinterleaver() {}

protected:
	int					iN_MUX;
	CMatrix<CEquSig>	matcDeinterlMemory;
	CVector<int>		veciCurIndex;
	CVector<int>		veciIntTable;
	int					iD;
	int					iInitCnt;

	virtual void InitInternal(CParameter& Parameters);
	virtual void ProcessDataInternal(CParameter& Parameters);
};


#endif // !defined(CONVINTERLEAVER_H__3B0BA660_CA63_4344_BB2B_23E7A0D31912__INCLUDED_)
