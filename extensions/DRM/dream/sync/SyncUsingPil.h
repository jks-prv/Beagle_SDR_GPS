/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2001
 *
 * Author(s):
 *	Volker Fischer
 *
 * Description:
 *	See SyncUsingPil.cpp
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

#if !defined(SYNCUSINGPIL_H__3B0BA660_CA63_434OBUVEE7A0D31912__INCLUDED_)
#define SYNCUSINGPIL_H__3B0BA660_CA63_434OBUVEE7A0D31912__INCLUDED_

#include "../Parameter.h"
#include "../util/Modul.h"
#include "../util/Vector.h"
#include "../chanest/ChanEstTime.h"


/* Definitions ****************************************************************/
/* Time constant for IIR averaging of frequency offset estimation */
#define TICONST_FREQ_OFF_EST			((CReal) 1.0) /* sec */

#ifdef USE_SAMOFFS_TRACK_FRE_PIL
/* Time constant for IIR averaging of sample rate offset estimation */
# define TICONST_SAMRATE_OFF_EST		((CReal) 20.0) /* sec */
# define CONTR_SAMP_OFF_INTEGRATION		((_REAL) 3.0)
#endif


/* Classes ********************************************************************/
class CSyncUsingPil : public CReceiverModul<_COMPLEX, _COMPLEX>, 
					  public CPilotModiClass
{
public:
	CSyncUsingPil() :
		iPosFreqPil(NUM_FREQ_PILOTS),
		cOldFreqPil(NUM_FREQ_PILOTS),
		iSymbCntFraSy(0),
		iNumSymPerFrame(0),
		bSyncInput(false), bAquisition(false),
		bTrackPil(false)
#ifdef USE_SAMOFFS_TRACK_FRE_PIL
		, cFreqPilotPhDiff(NUM_FREQ_PILOTS)
#endif
	  {}
	virtual ~CSyncUsingPil() {}

	/* To set the module up for synchronized DRM input data stream */
	void SetSyncInput(bool bNewS) {bSyncInput = bNewS;}

	void StartAcquisition();
	void StopAcquisition() {bAquisition = false;}

	void StartTrackPil() {bTrackPil = true;}
	void StopTrackPil() {bTrackPil = false;}

protected:
	class CPilotCorr
	{
	public:
		int			iIdx1, iIdx2;
		CComplex	cPil1, cPil2;
	};

	/* Variables for frequency pilot estimation */
	CVector<int>			iPosFreqPil;
	CVector<CComplex>		cOldFreqPil;

	CReal					rNormConstFOE;
	CReal					rSampleFreqEst;

	int						iSymbCntFraSy;
	int						iInitCntFraSy;

	int						iNumSymPerFrame;
	int						iNumCarrier;

	bool				bBadFrameSync;
	bool				bInitFrameSync;
	bool				bFrameSyncWasOK;

	bool				bSyncInput;

	bool				bAquisition;
	bool				bTrackPil;

	int						iMiddleOfInterval;

	CReal					rLamFreqOff;
	CComplex				cFreqOffVec;


	/* Variables for frame synchronization */
	CShiftRegister<CReal>	vecrCorrHistory;

	/* DRM frame synchronization based on time pilots */
	CVector<CPilotCorr>		vecPilCorr;
	int						iNumPilPairs;
	CComplex				cR_HH;

	ERobMode				eCurRobMode;

	CReal					rAvFreqPilDistToDC;
	CReal					rPrevSamRateOffset;

#ifdef USE_SAMOFFS_TRACK_FRE_PIL
	CReal					rLamSamRaOff;
	CVector<CComplex>		cFreqPilotPhDiff;
#endif

	virtual void InitInternal(CParameter& Parameters);
	virtual void ProcessDataInternal(CParameter& Parameters);
};


#endif // !defined(SYNCUSINGPIL_H__3B0BA660_CA63_434OBUVEE7A0D31912__INCLUDED_)
