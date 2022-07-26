/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2001
 *
 * Author(s):
 *	Volker Fischer, Alexander Kurpiers
 *
 * Description:
 *	DRM channel simulation
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

#include "ChannelSimulation.h"


/* Implementation *************************************************************/
void CDRMChannel::ProcessDataInternal(CParameter&)
{
    int			i, j;
    _COMPLEX	cCurTapSamp;

    /* Save old values from the end of the vector */
    for (i = 0; i < iMaxDelay; i++)
        veccHistory[i] = veccHistory[i + iInputBlockSize];

    /* Write new symbol in memory */
    for (i = iMaxDelay; i < iLenHist; i++)
        veccHistory[i] = (*pvecInputData)[i - iMaxDelay];

    /* Delay signal using history buffer, add tap gain (fading), multiply with
       exp-function (optimized implementation, see below) to implement
       doppler shift */
    /* Direct path */
    for (i = 0; i < iInputBlockSize; i++)
    {
        cCurTapSamp = tap[0].Update() * cCurExp[0];

        veccOutput[i] =	veccHistory[i + iMaxDelay /* - 0 */] * cCurTapSamp;

        /* Rotate exp-pointer one step further by complex multiplication with
           precalculated rotation vector cExpStep. This saves us from
           calling sin() and cos() functions all the time (iterative
           calculation of these functions) */
        cCurExp[0] *= cExpStep[0];

        /* Store the tap gain */
        (*pvecOutputData)[i].veccTap[0] = cCurTapSamp;
    }

    /* Echos */
    for (j = 1; j < iNumTaps; j++)
    {
        for (i = 0; i < iInputBlockSize; i++)
        {
            cCurTapSamp = tap[j].Update() * cCurExp[j];

            veccOutput[i] +=
                veccHistory[i + iMaxDelay - tap[j].GetDelay()] * cCurTapSamp;

            /* See above */
            cCurExp[j] *= cExpStep[j];

            /* Store the tap gain */
            (*pvecOutputData)[i].veccTap[j] = cCurTapSamp;
        }
    }

    /* Get real output vector and correct global gain */
    for (i = 0; i < iInputBlockSize; i++)
        (*pvecOutputData)[i].tOut = veccOutput[i].real() * rGainCorr;

    /* Additional white Gaussian noise (AWGN) */
    for (i = 0; i < iInputBlockSize; i++)
        (*pvecOutputData)[i].tOut += randn() * rNoisepwrFactor;


    /* Reference signals for channel estimation evaluation ------------------ */
    /* Input reference signal. "* 2" due to the real-valued signal */
    for (i = 0; i < iInputBlockSize; i++)
        (*pvecOutputData)[i].tIn = (*pvecInputData)[i].real() * 2;

    /* Channel reference signal (without additional noise) */
    for (i = 0; i < iInputBlockSize; i++)
        (*pvecOutputData)[i].tRef = veccOutput[i].real() * rGainCorr;
}

void CDRMChannel::InitInternal(CParameter& Parameters)
{
    Parameters.Lock();
    iSampleRate = Parameters.GetSigSampleRate();
    /* Set channel parameter according to selected channel number (table B.1) */
    switch (Parameters.iDRMChannelNum)
    {
    case 1:
        /* AWGN */
        iNumTaps = 1;

        tap[0].Init(
		/* Sample Rate */	iSampleRate,
		/* Delay: */	(_REAL) 0.0,
                /* Gain: */		(_REAL) 1.0,
                /* Fshift: */	(_REAL) 0.0,
                /* Fd: */		(_REAL) 0.0);
        break;

    case 2:
        /* Rice with delay */
        iNumTaps = 2;

        tap[0].Init(
		/* Sample Rate */	iSampleRate,
/* Delay: */	(_REAL) 0.0,
                                 /* Gain: */		(_REAL) 1.0,
                                 /* Fshift: */	(_REAL) 0.0,
                                 /* Fd: */		(_REAL) 0.0);

        tap[1].Init(
		/* Sample Rate */	iSampleRate,
	/* Delay: */	(_REAL) 1.0,
                                 /* Gain: */		(_REAL) 0.5,
                                 /* Fshift: */	(_REAL) 0.0,
                                 /* Fd: */		(_REAL) 0.1);
        break;

    case 3:
        /* US Consortium */
        iNumTaps = 4;

        tap[0].Init(
		/* Sample Rate */	iSampleRate,
/* Delay: */	(_REAL) 0.0,
                                 /* Gain: */		(_REAL) 1.0,
                                 /* Fshift: */	(_REAL) 0.1,
                                 /* Fd: */		(_REAL) 0.1);

        tap[1].Init(
		/* Sample Rate */	iSampleRate,
/* Delay: */	(_REAL) 0.7,
                                 /* Gain: */		(_REAL) 0.7,
                                 /* Fshift: */	(_REAL) 0.2,
                                 /* Fd: */		(_REAL) 0.5);

        tap[2].Init(
		/* Sample Rate */	iSampleRate,
/* Delay: */	(_REAL) 1.5,
                                 /* Gain: */		(_REAL) 0.5,
                                 /* Fshift: */	(_REAL) 0.5,
                                 /* Fd: */		(_REAL) 1.0);

        tap[3].Init(
		/* Sample Rate */	iSampleRate,
/* Delay: */	(_REAL) 2.2,
                                 /* Gain: */		(_REAL) 0.25,
                                 /* Fshift: */	(_REAL) 1.0,
                                 /* Fd: */		(_REAL) 2.0);
        break;

    case 4:
        /* CCIR Poor */
        iNumTaps = 2;

        tap[0].Init(
		/* Sample Rate */	iSampleRate,
	/* Delay: */	(_REAL) 0.0,
                                 /* Gain: */		(_REAL) 1.0,
                                 /* Fshift: */	(_REAL) 0.0,
                                 /* Fd: */		(_REAL) 1.0);

        tap[1].Init(
		/* Sample Rate */	iSampleRate,
	/* Delay: */	(_REAL) 2.0,
                                 /* Gain: */		(_REAL) 1.0,
                                 /* Fshift: */	(_REAL) 0.0,
                                 /* Fd: */		(_REAL) 1.0);
        break;

    case 5:
        /* Channel no 5 */
        iNumTaps = 2;

        tap[0].Init(
		/* Sample Rate */	iSampleRate,
	/* Delay: */	(_REAL) 0.0,
                                 /* Gain: */		(_REAL) 1.0,
                                 /* Fshift: */	(_REAL) 0.0,
                                 /* Fd: */		(_REAL) 2.0);

        tap[1].Init(
		/* Sample Rate */	iSampleRate,
	/* Delay: */	(_REAL) 4.0,
                                 /* Gain: */		(_REAL) 1.0,
                                 /* Fshift: */	(_REAL) 0.0,
                                 /* Fd: */		(_REAL) 2.0);
        break;

    case 6:
        /* Channel #6 */
        iNumTaps = 4;

        tap[0].Init(
		/* Sample Rate */	iSampleRate,
	/* Delay: */	(_REAL) 0.0,
                                 /* Gain: */		(_REAL) 0.5,
                                 /* Fshift: */	(_REAL) 0.0,
                                 /* Fd: */		(_REAL) 0.1);

        tap[1].Init(
		/* Sample Rate */	iSampleRate,
	/* Delay: */	(_REAL) 2.0,
                                 /* Gain: */		(_REAL) 1.0,
                                 /* Fshift: */	(_REAL) 1.2,
                                 /* Fd: */		(_REAL) 2.4);

        tap[2].Init(
		/* Sample Rate */	iSampleRate,
	/* Delay: */	(_REAL) 4.0,
                                 /* Gain: */		(_REAL) 0.25,
                                 /* Fshift: */	(_REAL) 2.4,
                                 /* Fd: */		(_REAL) 4.8);

        tap[3].Init(
		/* Sample Rate */	iSampleRate,
	/* Delay: */	(_REAL) 6.0,
                                 /* Gain: */		(_REAL) 0.0625,
                                 /* Fshift: */	(_REAL) 3.6,
                                 /* Fd: */		(_REAL) 7.2);
        break;


        /* My own test channels, NOT DEFINED IN THE DRM STANDARD! --------------- */
    case 7:
        /* Channel without fading and doppler shift */
        iNumTaps = 4;

        tap[0].Init(
		/* Sample Rate */	iSampleRate,
	/* Delay: */	(_REAL) 0.0,
                                 /* Gain: */		(_REAL) 1.0,
                                 /* Fshift: */	(_REAL) 0.0,
                                 /* Fd: */		(_REAL) 0.0);

        tap[1].Init(
		/* Sample Rate */	iSampleRate,
	/* Delay: */	(_REAL) 0.7,
                                 /* Gain: */		(_REAL) 0.7,
                                 /* Fshift: */	(_REAL) 0.0,
                                 /* Fd: */		(_REAL) 0.0);

        tap[2].Init(
		/* Sample Rate */	iSampleRate,
	/* Delay: */	(_REAL) 1.5,
                                 /* Gain: */		(_REAL) 0.5,
                                 /* Fshift: */	(_REAL) 0.0,
                                 /* Fd: */		(_REAL) 0.0);

        tap[3].Init(
		/* Sample Rate */	iSampleRate,
	/* Delay: */	(_REAL) 2.2,
                                 /* Gain: */		(_REAL) 0.25,
                                 /* Fshift: */	(_REAL) 0.0,
                                 /* Fd: */		(_REAL) 0.0);
        break;

    case 8:
        /* Only one fading path */
        iNumTaps = 1;

        tap[0].Init(
		/* Sample Rate */	iSampleRate,
	/* Delay: */	(_REAL) 0.0,
                                 /* Gain: */		(_REAL) 1.0,
                                 /* Fshift: */	(_REAL) 0.0,
                                 /* Fd: */		(_REAL) Parameters.iSpecChDoppler);
        break;

    case 9:
        /* Two moderate fading taps close to each other with equal gain */
        iNumTaps = 2;

        tap[0].Init(
		/* Sample Rate */	iSampleRate,
	/* Delay: */	(_REAL) 0.0,
                                 /* Gain: */		(_REAL) 1.0,
                                 /* Fshift: */	(_REAL) 0.0,
                                 /* Fd: */		(_REAL) 1.0);

        tap[1].Init(
		/* Sample Rate */	iSampleRate,
	/* Delay: */	(_REAL) 0.7,
                                 /* Gain: */		(_REAL) 1.0,
                                 /* Fshift: */	(_REAL) 0.0,
                                 /* Fd: */		(_REAL) 1.0);
        break;

    case 10:
        /* Frequency offset */
        iNumTaps = 1;

        tap[0].Init(
		/* Sample Rate */	iSampleRate,
	/* Delay: */	(_REAL) 0.0,
                                 /* Gain: */		(_REAL) 1.0,
                                 /* Fshift: */	(_REAL) Parameters.iSpecChDoppler,
                                 /* Fd: */		(_REAL) 0.0);
        break;

    case 11:
        /* Same as channel 5 but with variable Doppler */
        iNumTaps = 2;

        tap[0].Init(
		/* Sample Rate */	iSampleRate,
	/* Delay: */	(_REAL) 0.0,
                                 /* Gain: */		(_REAL) 1.0,
                                 /* Fshift: */	(_REAL) 0.0,
                                 /* Fd: */		(_REAL) Parameters.iSpecChDoppler);

        tap[1].Init(
		/* Sample Rate */	iSampleRate,
	/* Delay: */	(_REAL) 4.0,
                                 /* Gain: */		(_REAL) 1.0,
                                 /* Fshift: */	(_REAL) 0.0,
                                 /* Fd: */		(_REAL) Parameters.iSpecChDoppler);
        break;

    case 12:
        /* Same as 8 but we need to redefined because of OFDM.cpp module and
           MMSE estimation */
        iNumTaps = 1;

        tap[0].Init(
		/* Sample Rate */	iSampleRate,
	/* Delay: */	(_REAL) 0.0,
                                 /* Gain: */		(_REAL) 1.0,
                                 /* Fshift: */	(_REAL) 0.0,
                                 /* Fd: */		(_REAL) Parameters.iSpecChDoppler);
        break;
    }


    /* Init exponent steps (for doppler shift) and gain correction ---------- */
    rGainCorr = (_REAL) 0.0;
    for (int i = 0; i < iNumTaps; i++)
    {
        /* Exponent function for shifting (doppler shift) */
        cCurExp[i] = (_REAL) 1.0;

        cExpStep[i] =
            _COMPLEX(cos(tap[i].GetFShift()), sin(tap[i].GetFShift()));

        /* Gain correction denominator */
        rGainCorr += tap[i].GetGain() * tap[i].GetGain();

        /* Get path delays for global struct */
        Parameters.iPathDelay[i] = tap[i].GetDelay();
    }

    /* Final gain correction value. "* 2" due to the real-valued signals */
    rGainCorr = (_REAL) 1.0 / sqrt(rGainCorr) * 2;

    /* Set number of taps and gain correction in global struct */
    Parameters.iNumTaps = iNumTaps;
    Parameters.rGainCorr = rGainCorr / 2;


    /* Memory allocation ---------------------------------------------------- */
    /* Maximum delay */
    iMaxDelay = tap[iNumTaps - 1].GetDelay();

    /* Allocate memory for history, init vector with zeros. This history is used
       for generating path delays */
    iLenHist = Parameters.CellMappingTable.iSymbolBlockSize + iMaxDelay;
    veccHistory.Init(iLenHist, _COMPLEX((_REAL) 0.0, (_REAL) 0.0));

    /* Allocate memory for temporary output vector for complex values */
    veccOutput.Init(Parameters.CellMappingTable.iSymbolBlockSize);


    /* Calculate noise power factors for a given SNR ------------------------ */
    /* Spectrum width (N / T_u) */
    const _REAL rSpecOcc = (_REAL) Parameters.CellMappingTable.iNumCarrier /
                           Parameters.CellMappingTable.iFFTSizeN * iSampleRate;

    /* Bandwidth correction factor for noise (f_s / (2 * B))*/
    const _REAL rBWFactor = (_REAL) iSampleRate / 2 / rSpecOcc;

    /* Calculation of the gain factor for noise generator */
    rNoisepwrFactor =
        sqrt(pow((_REAL) 10.0, -Parameters.GetSystemSNRdB() / 10) *
             Parameters.CellMappingTable.rAvPowPerSymbol * 2 * rBWFactor);


    /* Set seed of random noise generator */
    srand((unsigned) time(nullptr));

    /* Define block-sizes for input and output */
    iInputBlockSize = Parameters.CellMappingTable.iSymbolBlockSize;
    iOutputBlockSize = Parameters.CellMappingTable.iSymbolBlockSize;
    Parameters.Unlock();
}

void CTapgain::Init(int iNewSampleRate, _REAL rNewDelay, _REAL rNewGain, _REAL rNewFshift,
                    _REAL rNewFd)
{
    _REAL	s;
    int		k;

    /* Set internal parameters (convert units and normalize values) */

    samplerate = iNewSampleRate;
    delay = DelMs2Sam(rNewDelay);
    gain = rNewGain;
    fshift = NormShift(rNewFshift);
    fd = rNewFd;

    s = (_REAL) 0.5 * fd / samplerate;

    /* If tap is not fading, return function */
    if (s == (_REAL) 0.0)
        return;

    if (s > 0.03)
    {
        interpol = 0;
        polyinterpol = 1;
        phase = -1;
    }
    else
    {
        if (s > 0.017)
        {
            interpol = 0;
            polyinterpol = 2;
            phase = 0;
        }
        else
        {
            if (s > 0.0084)
            {
                interpol = 0;
                polyinterpol = 4;
                phase = 0;
            }
            else
            {
                if (s > 0.0042)
                {
                    interpol = 0;
                    polyinterpol = 8;
                    phase = 0;
                }
                else
                {
                    interpol = (int) (0.0042 / s + 1);
                    polyinterpol = 8;
                    s = s * interpol;
                    phase = 0;
                }
            }
        }
    }

    gausstp(taps, s, polyinterpol);

    /* Initialize FIR buffer */
    for (k = 0; k < FIRLENGTH; k++)
    {
        fir_buff[k][0] = randn();
        fir_buff[k][1] = randn();
    }

    if (interpol)
    {
        /* Compute nextI and nextQ */
        nextI = (_REAL) 0.0;
        nextQ = (_REAL) 0.0;

        /* FIR filter */
        for (k = 0; k < FIRLENGTH; k++)
        {
            nextI += fir_buff[k][0] * taps[FIRLENGTH - k - 1];
            nextQ += fir_buff[k][1] * taps[FIRLENGTH - k - 1];
        }
    }

    over_cnt = 0;
    fir_index = 0;
}

_COMPLEX CTapgain::Update()
{
    int			k;
    _COMPLEX	out;

    /* If tap is not fading, just return gain */
    if (fd == (_REAL) 0.0)
        return gain;

    /* Over_cnt is always zero if no interpolation is used */
    if (!over_cnt)
    {
        lastI = nextI;
        lastQ = nextQ;

        /* Get new noise sample */
        if ((phase == -1) || (phase == 0))
        {
            fir_buff[fir_index][0] = randn();
            fir_buff[fir_index][1] = randn();

            fir_index = (fir_index - 1 + FIRLENGTH) % FIRLENGTH;
        }

        /* Compute new filter output */
        nextI = (_REAL) 0.0;
        nextQ = (_REAL) 0.0;

        if (phase == -1)
        {
            /* FIR */
            for (k = 0; k < FIRLENGTH; k++)
            {
                nextI +=
                    fir_buff[(k + fir_index) % FIRLENGTH][0] *
                    taps[FIRLENGTH - k - 1];
                nextQ +=
                    fir_buff[(k + fir_index) % FIRLENGTH][1] *
                    taps[FIRLENGTH - k - 1];
            }
        }
        else
        {
            /* Polyphase FIR with interpolation */
            for (k = 0; k < FIRLENGTH; k++)
            {
                nextI +=
                    fir_buff[(k + fir_index) % FIRLENGTH][0] *
                    taps[polyinterpol * (FIRLENGTH - k) - phase - 1];
                nextQ +=
                    fir_buff[(k + fir_index) % FIRLENGTH][1] *
                    taps[polyinterpol * (FIRLENGTH - k) - phase - 1];
            }

            phase = (phase + 1) % polyinterpol;
        }
    }

    if (interpol)
    {
        /* Linear interpolation */
        out  = _COMPLEX((nextI - lastI) * (_REAL) over_cnt /
                        interpol + lastI,
                        (nextQ - lastQ) * (_REAL) over_cnt /
                        interpol + lastQ);

        if (++over_cnt == interpol)
            over_cnt = 0;
    }
    else
        out = _COMPLEX(nextI, nextQ);

    /* Weight with gain */
    const _REAL ccGainCorr = sqrt((_REAL) 2.0);
    return out * gain / ccGainCorr;
}

void CTapgain::gausstp(_REAL taps[], _REAL& s, int& over) const
{
    /* Calculate impulse response of FIR filter to implement
    the Watterson modell (Gaussian PSD) */

    /* "2 * s" is the doppler spread */
    for (int n = 0; n < (FIRLENGTH * over); n++)
    {
        taps[n] = sqrt(sqrt(crPi * (_REAL) 8.0) * s * over) *
                  exp(-fsqr((_REAL) 2.0 * crPi * s * (n - FIRLENGTH * over / 2)));
    }
}

int CTapgain::DelMs2Sam(const _REAL rDelay) const
{
    /* Delay in samples. The channel taps are shifted to the taps that are
       possible for the given sample rate -> approximation! */
    return (int) (rDelay /* ms */ * samplerate / 1000);
}

_REAL CTapgain::NormShift(const _REAL rShift) const
{
    /* Normalize doppler shift */
    return (_REAL) 2.0 * crPi / samplerate * rShift;
}

_REAL CChannelSim::randn() const
{
    const int iNoRand = 10;
    const _REAL rFactor = (_REAL) ((double) sqrt((_REAL) 12.0 / iNoRand) / RAND_MAX);
    const int iRandMaxHalf = RAND_MAX / 2;

    /* Add some constant distributed random processes to get Gaussian
       distribution */
    _REAL rNoise = 0;
    for (int i = 0; i < iNoRand; i++)
        rNoise += rand() - iRandMaxHalf;

    /* Apply amplification factor */
    return rNoise * rFactor;
}
