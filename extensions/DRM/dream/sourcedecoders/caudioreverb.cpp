/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2004
 *
 * Author(s):
 *	Volker Fischer
 *
 * Description:
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

#include "caudioreverb.h"
#include "../matlib/MatlibStdToolbox.h"

CAudioReverb::CAudioReverb()
{

}

/*
    The following code is based on "JCRev: John Chowning's reverberator class"
    by Perry R. Cook and Gary P. Scavone, 1995 - 2004
    which is in "The Synthesis ToolKit in C++ (STK)"
    http://ccrma.stanford.edu/software/stk

    Original description:
    This class is derived from the CLM JCRev function, which is based on the use
    of networks of simple allpass and comb delay filters. This class implements
    three series allpass units, followed by four parallel comb filters, and two
    decorrelation delay lines in parallel at the output.
*/
void CAudioReverb::Init(CReal rT60, int iSampleRate)
{
    /* Delay lengths for 44100 Hz sample rate */
    int lengths[9] = { 1777, 1847, 1993, 2137, 389, 127, 43, 211, 179 };
    const CReal scaler = CReal(iSampleRate) / 44100.0;

    int delay, i;
    if (scaler != 1.0)
    {
        for (i = 0; i < 9; i++)
        {
            delay = int(Floor(scaler * lengths[i]));

            if ((delay & 1) == 0)
                delay++;

            while (!isPrime(delay))
                delay += 2;

            lengths[i] = delay;
        }
    }

    for (i = 0; i < 3; i++)
        allpassDelays_[i].Init(lengths[i + 4]);

    for (i = 0; i < 4; i++)
        combDelays_[i].Init(lengths[i]);

    setT60(rT60, iSampleRate);
    allpassCoefficient_ = 0.7;
    Clear();
}

bool CAudioReverb::isPrime(const int number)
{
/*
    Returns true if argument value is prime. Taken from "class Effect" in
    "STK abstract effects parent class".
*/
    if (number == 2)
        return true;

    if (number & 1)
    {
        for (int i = 3; i < int(Sqrt(CReal(number))) + 1; i += 2)
        {
            if ((number % i) == 0)
                return false;
        }

        return true;			/* prime */
    }
    else
        return false;			/* even */
}

void
CAudioReverb::Clear()
{
    /* Reset and clear all internal state */
    allpassDelays_[0].Reset(0);
    allpassDelays_[1].Reset(0);
    allpassDelays_[2].Reset(0);
    combDelays_[0].Reset(0);
    combDelays_[1].Reset(0);
    combDelays_[2].Reset(0);
    combDelays_[3].Reset(0);
}

void
CAudioReverb::setT60(const CReal rT60, int iSampleRate)
{
    /* Set the reverberation T60 decay time */
    for (int i = 0; i < 4; i++)
    {
        combCoefficient_[i] = pow( 10.0, CReal(-3.0 * combDelays_[i].  Size() / (rT60 * iSampleRate)));
    }
}

CReal CAudioReverb::ProcessSample(const CReal rLInput, const CReal rRInput)
{
    /* Compute one output sample */
    CReal
        temp,
        temp0,
        temp1,
        temp2;

    /* Mix stereophonic input signals to mono signal (since the maximum value of
       the input signal is 0.5 * max due to the current implementation in
       AudioSourceDecoder.cpp, we cannot get an overrun) */
    const CReal
        input = rLInput + rRInput;

    temp = allpassDelays_[0].Get();
    temp0 = allpassCoefficient_ * temp;
    temp0 += input;
    allpassDelays_[0].Add(int(temp0));
    temp0 = -(allpassCoefficient_ * temp0) + temp;

    temp = allpassDelays_[1].Get();
    temp1 = allpassCoefficient_ * temp;
    temp1 += temp0;
    allpassDelays_[1].Add(int(temp1));
    temp1 = -(allpassCoefficient_ * temp1) + temp;

    temp = allpassDelays_[2].Get();
    temp2 = allpassCoefficient_ * temp;
    temp2 += temp1;
    allpassDelays_[2].Add(int(temp2));
    temp2 = -(allpassCoefficient_ * temp2) + temp;

    const CReal
        temp3 = temp2 + (combCoefficient_[0] * combDelays_[0].Get());
    const CReal
        temp4 = temp2 + (combCoefficient_[1] * combDelays_[1].Get());
    const CReal
        temp5 = temp2 + (combCoefficient_[2] * combDelays_[2].Get());
    const CReal
        temp6 = temp2 + (combCoefficient_[3] * combDelays_[3].Get());

    combDelays_[0].Add(int(temp3));
    combDelays_[1].Add(int(temp4));
    combDelays_[2].Add(int(temp5));
    combDelays_[3].Add(int(temp6));

    return temp3 + temp4 + temp5 + temp6;
}
