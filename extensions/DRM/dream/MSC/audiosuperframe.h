#ifndef AUDIOSUPERFRAME_H
#define AUDIOSUPERFRAME_H

#include "../Parameter.h"

class AudioSuperFrame
{
public:
    AudioSuperFrame();
    virtual ~AudioSuperFrame();
    virtual bool parse(CVectorEx<_BINARY>& asf)=0;
    virtual unsigned getNumFrames()=0;
    virtual unsigned getSuperFrameDurationMilliseconds()=0;
    virtual void getFrame(std::vector<uint8_t>& , uint8_t& crc, unsigned i)=0;
protected:
    std::vector<std::vector <uint8_t> > audioFrame;
    enum EProtectedPart { HPP, LPP };
    EProtectedPart protectedPart;
};

#endif // AUDIOSUPERFRAME_H
