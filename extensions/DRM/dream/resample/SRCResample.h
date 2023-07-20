#ifndef CSRCRESAMPLE_H
#define CSRCRESAMPLE_H

#include "../util/Vector.h"
#include "samplerate.h"

class CSRCResample
{
public:
    CSRCResample();
    virtual ~CSRCResample();
    
    SRC_STATE   *srcState;
    SRC_DATA    srcData;

    double      rRatio;
    int         iInputBlockSize;
    int         iOutputBlockSize;

    void Init(int iNewOutputBlockSize, int iInputSamplerate, int iOutputSamplerate, int n_chan);
    void Resample(CVector<_REAL>& rInput, CVector<_REAL>& rOutput);
    int GetFreeInputSize() const;
    int GetMaxInputSize() const;
    void Reset();

protected:
    CShiftRegister<_REAL>	vecrIntBuff;
    int						iHistorySize;
};

#endif // CSRCRESAMPLE_H
