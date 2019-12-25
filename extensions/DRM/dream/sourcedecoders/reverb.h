#ifndef REVERB_H
#define REVERB_H

#include "../Parameter.h"
#include "../util/Vector.h"
#include "caudioreverb.h"

class Reverb
{
public:
    Reverb();
    void Init(int outputSampleRate, bool bUse);
    ETypeRxStatus apply(bool bCurBlockOK, bool bCurBlockFaulty, CVector<_REAL> CurLeft, CVector<_REAL> CurRight);
private:
    bool bAudioWasOK, bUseReverbEffect;
    CVector<_REAL> OldLeft;
    CVector<_REAL> OldRight;
    CAudioReverb AudioRev;
};

#endif // REVERB_H
