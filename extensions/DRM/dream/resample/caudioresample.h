#ifndef CAUDIORESAMPLE_H
#define CAUDIORESAMPLE_H

#include "../util/Vector.h"

class CAudioResample
{
public:
    CAudioResample();
    virtual ~CAudioResample();

    void Init(int iNewInputBlockSize, _REAL rNewRatio);
    void Init(int iNewOutputBlockSize, int iInputSamplerate, int iOutputSamplerate);
    void Resample(CVector<_REAL>& rInput, CVector<_REAL>& rOutput);
    int GetFreeInputSize() const;
    int GetMaxInputSize() const;
    void Reset();

protected:
    _REAL					rRatio;
    int						iInputBlockSize;
    int						iOutputBlockSize;
    CShiftRegister<_REAL>	vecrIntBuff;
    int						iHistorySize;
};

#endif // CAUDIORESAMPLE_H
