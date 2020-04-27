#include "cspectrumresample.h"

CSpectrumResample::CSpectrumResample() : iOutputBlockSize(0)
{

}

CSpectrumResample::~CSpectrumResample()
{

}


void CSpectrumResample::Resample(CVector<_REAL>* prInput, CVector<_REAL>** pprOutput,
    int iNewOutputBlockSize, bool bResample)
{
    if (!bResample)
        iNewOutputBlockSize = 0;

    if (iNewOutputBlockSize != iOutputBlockSize)
    {
        iOutputBlockSize = iNewOutputBlockSize;

        /* Allocate memory for internal buffer */
        vecrIntBuff.Init(iNewOutputBlockSize);
    }

    int iInputBlockSize = prInput->Size();
    _REAL rRatio = _REAL(iInputBlockSize) / _REAL(iOutputBlockSize);

    if (!bResample || rRatio <= (_REAL) 1.0)
    {
        /* If ratio is 1 or less, no resampling is needed */
        *pprOutput = prInput;
    }
    else
    {
        int j, i;
        CVector<_REAL>* prOutput;
        prOutput = &vecrIntBuff;
        _REAL rBorder = rRatio;
        _REAL rMax = -1.0e10;
        _REAL rValue;

        /* Main loop */
        for (j = 0, i = 0; j < iInputBlockSize && i < iOutputBlockSize; j++)
        {
            rValue = (*prInput)[j];
            /* We only take the maximum value within the interval,
               because what is important it's the signal
               and not the lack of signal */
            if (rValue > rMax) rMax = rValue;
            if (j > (int)floor(rBorder))
            {
                (*prOutput)[i++] = rMax - 6.0;
                rMax = -1.0e10;
                rBorder = rRatio * i;
            }
        }
        if (i < iOutputBlockSize)
            (*prOutput)[i] = rMax - 6.0;
        *pprOutput = prOutput;
    }
}
