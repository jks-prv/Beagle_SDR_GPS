/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2001
 *
 * Author(s):
 *	Alexander Kurpiers, Volker Fischer
 *
 * Description:
 *	See ChannelSimulation.cpp
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

#if !defined(DRMCHANNEL_H__3B0BA660_CA63_4344_BB2B_2OJVBEWJBWV_INCLUDED_)
#define DRMCHANNEL_H__3B0BA660_CA63_4344_BB2B_2OJVBEWJBWV_INCLUDED_

#include "../Parameter.h"
#include "../util/Modul.h"
#include <time.h>


/* Definitions ****************************************************************/
#define FIRLENGTH					24

#define fsqr(a)						((a) * (a))


/* Classes ********************************************************************/
class CChannelSim
{
public:
    inline _REAL randn() const;
};

class CTapgain : public CChannelSim
{
public:
    void Init(int iNewSampleRate, _REAL rNewDelay, _REAL rNewGain, _REAL rNewFshift, _REAL rNewFd);
    _COMPLEX Update();
    _REAL GetGain() const {
        return gain;
    }
    int GetDelay() const {
        return delay;
    }
    _REAL GetFShift() const {
        return fshift;
    }


protected:
    _REAL		taps[FIRLENGTH * 8]; /* FIR filter coefficients */
    int			over_cnt;		/* Counter for oversampling */
    int			phase;			/* Phase for polyphase oversampling */
    int			interpol;		/* interpolation factor for linear interpolation*/
    int			polyinterpol;	/* interpolation factor for polyphase interpolation */
    int			fir_index;		/* index to FIR buffer */
    _REAL		fir_buff[FIRLENGTH][2]; /* FIR buffer */
    _REAL		lastI, lastQ;	/* last FIR output, needed for interpolation */
    _REAL		nextI, nextQ;	/* next FIR output */

    _REAL fd;					/* Fading rate */
    _REAL fshift;				/* Doppler shift */
    _REAL gain;					/* Tap-gain */
    int delay;					/* Path delay */
    int samplerate;				/* Sound Card Sample Rate */

    int		DelMs2Sam(const _REAL rDelay) const;
    _REAL	NormShift(const _REAL rShift) const;
    void	gausstp(_REAL taps[], _REAL& s, int& over) const;
};

class CDRMChannel :
            /* The third template argument "_COMPLEX" is not used since this module
               has only one input and one output buffer */
            public CSimulationModul<_COMPLEX, CChanSimDataMod, _COMPLEX>, CChannelSim
{
public:
    CDRMChannel() {}
    virtual ~CDRMChannel() {}

protected:
    CTapgain			tap[4];
    _COMPLEX			cCurExp[4];
    _COMPLEX			cExpStep[4];

    int					iSampleRate;
    int					iNumTaps;
    CVector<_COMPLEX>	veccHistory;
    int					iMaxDelay;
    int					iLenHist;
    CVector<_COMPLEX>	veccOutput;
    _REAL				rGainCorr;
    _REAL				rNoisepwrFactor;

    void InitTapgain(CTapgain& tapg);

    virtual void InitInternal(CParameter& Parameters);
    virtual void ProcessDataInternal(CParameter& Parameters);
};


#endif // !defined(DRMCHANNEL_H__3B0BA660_CA63_4344_BB2B_2OJVBEWJBWV_INCLUDED_)
