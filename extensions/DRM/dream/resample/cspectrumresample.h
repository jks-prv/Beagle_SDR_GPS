#ifndef CSPECTRUMRESAMPLE_H
#define CSPECTRUMRESAMPLE_H

#include "../util/Vector.h"

class CSpectrumResample
{
public:
    CSpectrumResample();
    virtual ~CSpectrumResample();

    void Resample(CVector<_REAL>* prInput, CVector<_REAL>** pprOutput, int iNewOutputBlockSize, bool bResample);

protected:
    CVector<_REAL>			vecrIntBuff;
    int						iOutputBlockSize;
};

#endif // CSPECTRUMRESAMPLE_H
