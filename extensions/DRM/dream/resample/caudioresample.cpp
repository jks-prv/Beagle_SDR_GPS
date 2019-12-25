#include "caudioresample.h"
#include "ResampleFilter.h"

CAudioResample::CAudioResample()
{

}

CAudioResample::~CAudioResample()
{

}

void CAudioResample::Resample(CVector<_REAL>& rInput, CVector<_REAL>& rOutput)
{
    int j;

    if (rRatio == (_REAL) 1.0)
    {
        /* If ratio is 1, no resampling is needed, just copy vector */
        for (j = 0; j < iOutputBlockSize; j++)
            rOutput[j] = rInput[j];
    }
    else
    {
        /* Move old data from the end to the history part of the buffer and
           add new data (shift register) */
        vecrIntBuff.AddEnd(rInput, iInputBlockSize);

        /* Main loop */
        for (j = 0; j < iOutputBlockSize; j++)
        {
            /* Phase for the linear interpolation-taps */
            const int ip =
                (int) (j * INTERP_DECIM_I_D / rRatio) % INTERP_DECIM_I_D;

            /* Sample position in input vector */
            const int in = (int) (j / rRatio) + RES_FILT_NUM_TAPS_PER_PHASE;

            /* Convolution */
            _REAL ry = (_REAL) 0.0;
            for (int i = 0; i < RES_FILT_NUM_TAPS_PER_PHASE; i++)
                ry += fResTaps1To1[ip][i] * vecrIntBuff[in - i];

            rOutput[j] = ry;
        }
    }
}
void CAudioResample::Init(int iNewInputBlockSize, _REAL rNewRatio)
{
    rRatio = rNewRatio;
    iInputBlockSize = iNewInputBlockSize;
    iOutputBlockSize = (int) (iInputBlockSize * rRatio);
    //printf("CAudioResample::Init-1 rRatio=%f iInputBlockSize=%d iOutputBlockSize=%d\n", rRatio, iInputBlockSize, iOutputBlockSize);
    Reset();
}
void CAudioResample::Init(const int iNewOutputBlockSize, const int iInputSamplerate, const int iOutputSamplerate)
{
    rRatio = _REAL(iOutputSamplerate) / iInputSamplerate;
    iInputBlockSize = (int) (iNewOutputBlockSize / rRatio);
    iOutputBlockSize = iNewOutputBlockSize;
    //printf("CAudioResample::Init-2 rRatio=%f iInputBlockSize=%d iOutputBlockSize=%d\n", rRatio, iInputBlockSize, iOutputBlockSize);
    Reset();
}

int CAudioResample::GetMaxInputSize() const
{
    return iInputBlockSize;
}
int CAudioResample::GetFreeInputSize() const
{
    return iInputBlockSize;
}
void CAudioResample::Reset()
{
    /* Allocate memory for internal buffer, clear sample history */
    vecrIntBuff.Init(iInputBlockSize + RES_FILT_NUM_TAPS_PER_PHASE,
        (_REAL) 0.0);
}
