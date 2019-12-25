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

#if !defined(CELLMAPPINGTABLE_H__3B0BA660_CA63_4347A0D31912__INCLUDED_)
#define CELLMAPPINGTABLE_H__3B0BA660_CA63_4347A0D31912__INCLUDED_

#include "../GlobalDefinitions.h"
#include "../tables/TableCarMap.h"
#include "../tables/TableFAC.h"
#include "../util/Vector.h"


/* Definitions ****************************************************************/
/* Power definitions for pilots, boosted pilots and data cells (average) */
#define AV_DATA_CELLS_POWER		((_REAL) 1.0)
#define AV_PILOT_POWER			((_REAL) 2.0)
#define AV_BOOSTED_PIL_POWER	((_REAL) 4.0)


/* We define a bit for each flag to allow multiple assignments */
#define	CM_DC					1	/* Bit 0 */ // CM: Carrier Mapping
#define	CM_MSC					2	/* Bit 1 */
#define	CM_SDC					4	/* Bit 2 */
#define	CM_FAC					8	/* Bit 3 */
#define	CM_TI_PI				16	/* Bit 4 */
#define	CM_FRE_PI				32	/* Bit 5 */
#define	CM_SCAT_PI				64	/* Bit 6 */
#define	CM_BOOSTED_PI			128	/* Bit 7 */

/* Definitions for checking the cells */
#define _IsDC(a)				((a) & CM_DC)

#define _IsMSC(a)				((a) & CM_MSC)
#define _IsSDC(a)				((a) & CM_SDC)
#define _IsFAC(a)				((a) & CM_FAC)

#define _IsData(a)				(((a) & CM_MSC) || ((a) & CM_SDC) || ((a) & CM_FAC))


#define _IsTiPil(a)				((a) & CM_TI_PI)
#define _IsFreqPil(a)			((a) & CM_FRE_PI)
#define _IsScatPil(a)			((a) & CM_SCAT_PI)

#define _IsPilot(a)				(((a) & CM_TI_PI) || ((a) & CM_FRE_PI) || ((a) & CM_SCAT_PI))
#define _IsBoosPil(a)			((a) & CM_BOOSTED_PI)


/* Classes ********************************************************************/
class CCellMappingTable
{
public:
	CCellMappingTable() : iNumSymbolsPerSuperframe(0) {}
	virtual ~CCellMappingTable() {}

	void MakeTable(ERobMode eNewRobustnessMode, ESpecOcc eNewSpectOccup, int iSampleRate);

	struct CRatio {int iEnum; int iDenom;};

	/* Mapping table and pilot cell matrix */
	CMatrix<int>		matiMapTab; 
	CMatrix<_COMPLEX>	matcPilotCells;

	int					iNumSymbolsPerSuperframe;
	int					iNumSymPerFrame; /* Number of symbols per frame */
	int					iNumCarrier;
	int					iScatPilTimeInt; /* Time interpolation */
	int					iScatPilFreqInt; /* Frequency interpolation */

	int					iMaxNumMSCSym; /* Max number of MSC cells in a symbol */

	/* Number of MSC cells in a symbol */
	CVector<int>		veciNumMSCSym; 

	/* Number of FAC cells in a symbol */
	CVector<int>		veciNumFACSym; 

	/* Number of SDC cells in a symbol */
	CVector<int>		veciNumSDCSym;

	int					iFFTSizeN; /* FFT size of the OFDM modulation */
	int					iCarrierKmin; /* Carrier index of carrier with lowest frequency */
	int					iCarrierKmax; /* Carrier index of carrier with highest frequency */
	int					iIndexDCFreq; /* Index of DC carrier */
	int					iShiftedKmin; /* Shifted carrier min ("sound card pass-band") */
	int					iShiftedKmax; /* Shifted carrier max ("sound card pass-band") */
	CRatio				RatioTgTu; /* Ratio between guard-interval and useful part */
	int					iGuardSize; /* Length of guard-interval measured in "time-bins" */
	int					iSymbolBlockSize; /* Useful part plus guard-interval in "time-bins" */
	int					iNumIntpFreqPil; /* Number of time-interploated frequency pilots */

	int					iNumUsefMSCCellsPerFrame; /* Number of MSC cells per multiplex frame N_{MUX} */
	int					iNumSDCCellsPerSFrame; /* Number of SDC cells per super-frame */

	/* Needed for SNR estimation and simulation */
	_REAL				rAvPowPerSymbol; /* Total average power per symbol */
	_REAL				rAvScatPilPow; /* Average power of scattered pilots per cell */

protected:
	/* Internal parameters for MakeTable function --------------------------- */
	struct CScatPilots
	{	
		CScatPilots(): piConst(nullptr), iColSizeWZ(0), piW(nullptr), piZ(nullptr),
		iQ(0),piGainTable(nullptr) {}

		/* For the pase */
		const int*  piConst;
		int			iColSizeWZ;
		const int*	piW;
		const int*	piZ;
		int			iQ;

		/* For the gain */
		const int*	piGainTable;
	};


private:
	_COMPLEX	Polar2Cart(const _REAL rAbsolute, const int iPhase) const;
	int			mod(const int ix, const int iy) const;
};


#endif // !defined(CELLMAPPINGTABLE_H__3B0BA660_CA63_4347A0D31912__INCLUDED_)
