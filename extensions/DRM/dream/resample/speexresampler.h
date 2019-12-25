#ifndef SPEEXRESAMPLER_H
#define SPEEXRESAMPLER_H

#include <speex/speex_resampler.h>
#include "../util/Vector.h"

class SpeexResampler
{
public:
    SpeexResampler();
    virtual ~SpeexResampler();
    void Init(int iNewInputBlockSize, _REAL rNewRatio);
    void Init(int iNewOutputBlockSize, int iInputSamplerate, int iOutputSamplerate);
    void Resample(CVector<_REAL>& rInput, CVector<_REAL>& rOutput);
    void Reset();
private:
    SpeexResamplerState*	resampler;
    std::vector<float>			vecfInput;
    std::vector<float>			vecfOutput;
    size_t					iInputBuffered;

    size_t GetFreeInputSize() const;
    void Free();
};

#endif // SPEEXRESAMPLER_H
