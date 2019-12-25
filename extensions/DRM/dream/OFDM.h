/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2001
 *
 * Author(s):
 *	Volker Fischer
 *
 * Description:
 *	See OFDM.cpp
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

#if !defined(OFDM_H__3B0BA660_CA63_4344_BB2B_23E7A0D31912__INCLUDED_)
#define OFDM_H__3B0BA660_CA63_4344_BB2B_23E7A0D31912__INCLUDED_

#include "Parameter.h"
#include "util/Modul.h"
#include "matlib/MatlibSigProToolbox.h"


/* Definitions ****************************************************************/
/* Time constant for IIR averaging of PSD estimation */
#define TICONST_PSD_EST_OFDM			((CReal) 1.0) /* sec */


/* Classes ********************************************************************/
class COFDMModulation : public CTransmitterModul<_COMPLEX, _COMPLEX>
{
public:
    COFDMModulation() : rDefCarOffset((_REAL) VIRTUAL_INTERMED_FREQ) {}
    virtual ~COFDMModulation() {}

    void SetCarOffset(const _REAL rNewCarOffset)
    {
        rDefCarOffset = rNewCarOffset;
    }

protected:
    CFftPlans				FftPlan;

    CComplexVector			veccFFTInput;
    CComplexVector			veccFFTOutput;

    int						iShiftedKmin;
    int						iEndIndex;
    int						iDFTSize;
    int						iGuardSize;

    _COMPLEX				cCurExp;
    _COMPLEX				cExpStep;
    _REAL					rDefCarOffset;

    virtual void InitInternal(CParameter& TransmParam);
    virtual void ProcessDataInternal(CParameter& TransmParam);
};

class COFDMDemodulation : public CReceiverModul<_COMPLEX, _COMPLEX>
{
public:
    /* bUseRecFilter shall be set the false as default since in case of
       BERIDEAL simulations, no filter shall be applied */
    COFDMDemodulation() : iLenPowSpec(0) {}
    virtual ~COFDMDemodulation() {}

    void GetPowDenSpec(CVector<_REAL>& vecrData, CVector<_REAL>& vecrScale);

protected:
    CFftPlans				FftPlan;
    CComplexVector			veccFFTInput;
    CComplexVector			veccFFTOutput;

    CVector<_REAL>			vecrPowSpec;
    int						iSampleRate;
    int						iLenPowSpec;

    int						iShiftedKmin;
    int						iShiftedKmax;
    int						iDFTSize;
    int						iGuardSize;
    int						iNumCarrier;

    CReal					rLamPSD;


    virtual void InitInternal(CParameter& Parameters);
    virtual void ProcessDataInternal(CParameter& Parameters);
};

class COFDMDemodSimulation :
            public CSimulationModul<CChanSimDataMod, _COMPLEX, CChanSimDataDemod>
{
public:
    COFDMDemodSimulation() {}
    virtual ~COFDMDemodSimulation() {}

protected:
    CFftPlans				FftPlan;
    CComplexVector			veccFFTInput;
    CComplexVector			veccFFTOutput;

    int						iStartPointGuardRemov;

    int						iShiftedKmin;
    int						iShiftedKmax;
    int						iDFTSize;
    int						iGuardSize;
    int						iNumCarrier;
    int						iSymbolBlockSize;
    int						iSymbolCounterTiSy;
    int						iNumSymPerFrame;

    int						iNumTapsChan;

    virtual void InitInternal(CParameter& Parameters);
    virtual void ProcessDataInternal(CParameter& Parameters);
};


#endif // !defined(OFDM_H__3B0BA660_CA63_4344_BB2B_23E7A0D31912__INCLUDED_)
