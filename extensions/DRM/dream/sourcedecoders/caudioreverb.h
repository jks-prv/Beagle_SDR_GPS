#ifndef CAUDIOREVERB_H
#define CAUDIOREVERB_H

#include "../matlib/Matlib.h"
#include "../util/Vector.h"

class CAudioReverb
{
public:
    CAudioReverb();
    void Init(CReal rT60, int iSampleRate);
    void Clear();
    CReal ProcessSample(const CReal rLInput, const CReal rRInput);

protected:
    void setT60(const CReal rT60, int iSampleRate);
    bool isPrime(const int number);

    CFIFO<int>	allpassDelays_[3];
    CFIFO<int>	combDelays_[4];
    CReal		allpassCoefficient_;
    CReal		combCoefficient_[4];
};

#endif // CAUDIOREVERB_H
