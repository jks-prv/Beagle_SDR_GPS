/******************************************************************************\
 * British Broadcasting Corporation
 * Copyright (c) 2007
 *
 * Author(s):
 *	Volker Fischer, Andrew Murphy, Andrea Russo, Oliver Haffenden
 *
 * Description:
 *	See PlotManager.cpp
 *
 *
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

#if !defined(PLOT_MANAGER_H_INCLUDED)
#define PLOT_MANAGER_H_INCLUDED

#include "GlobalDefinitions.h"
#include "Parameter.h"

/* Definitions ****************************************************************/

/* Length of the history for synchronization parameters (used for the plot) */
#define LEN_HIST_PLOT_SYNC_PARMS		2250



class CPlotManager
{
public:

    CPlotManager();

    void SetReceiver(CDRMReceiver *pRx) {
        pReceiver = pRx;
    }

    void Init();

    void SetCurrentCDAud(int iN) {
        iCurrentCDAud = iN;
    }

    void UpdateParamHistories(ERecState eReceiverState);

    void UpdateParamHistoriesRSIIn();

    void GetTransferFunction(CVector<_REAL>& vecrData,
                             CVector<_REAL>& vecrGrpDly,	CVector<_REAL>& vecrScale);

    void GetAvPoDeSp(CVector<_REAL>& vecrData, CVector<_REAL>& vecrScale,
                     _REAL& rLowerBound, _REAL& rHigherBound,
                     _REAL& rStartGuard, _REAL& rEndGuard, _REAL& rPDSBegin,
                     _REAL& rPDSEnd);

    void GetSNRProfile(CVector<_REAL>& vecrData, CVector<_REAL>& vecrScale);

    void GetInputPSD(CVector<_REAL>& vecrData, CVector<_REAL>& vecrScale);

    /* Interfaces to internal parameters/std::vectors used for the plot */
    void GetFreqSamOffsHist(CVector<_REAL>& vecrFreqOffs,
                            CVector<_REAL>& vecrSamOffs, CVector<_REAL>& vecrScale,
                            _REAL& rFreqAquVal);

    void GetDopplerDelHist(CVector<_REAL>& vecrLenIR,
                           CVector<_REAL>& vecrDoppler, CVector<_REAL>& vecrScale);

    void GetSNRHist(CVector<_REAL>& vecrSNR, CVector<_REAL>& vecrCDAud,
                    CVector<_REAL>& vecrScale);




private:
    CDRMReceiver			*pReceiver;
    /* Storing parameters for plot */
    CShiftRegister<_REAL>	vecrFreqSyncValHist;
    CShiftRegister<_REAL>	vecrSamOffsValHist;
    CShiftRegister<_REAL>	vecrLenIRHist;
    CShiftRegister<_REAL>	vecrDopplerHist;
    CShiftRegister<_REAL>	vecrSNRHist;
    CShiftRegister<int>		veciCDAudHist;
    int						iSymbolCount;
    _REAL					rSumDopplerHist;
    _REAL					rSumSNRHist;
    int						iCurrentCDAud;
    CMutex					MutexHist;

};

#endif
