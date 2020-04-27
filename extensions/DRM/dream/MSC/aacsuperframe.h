#ifndef AACSUPERFRAME_H
#define AACSUPERFRAME_H

#include "audiosuperframe.h"

class AACSuperFrame: public AudioSuperFrame
{
public:
    AACSuperFrame();
    void init(const CAudioParam& audioParam,  ERobMode eRobMode, unsigned lengthPartA, unsigned lengthPartB);
    virtual bool parse(CVectorEx<_BINARY>& asf);
    virtual unsigned getNumFrames() { return unsigned(audioFrame.size()); }
    virtual unsigned getSuperFrameDurationMilliseconds() { return superFrameDurationMilliseconds; }
    virtual void getFrame(std::vector<uint8_t>& frame, uint8_t& crc, unsigned i) { frame = audioFrame[i]; crc = aacCRC[i]; }
private:
    unsigned lengthPartA, lengthPartB, superFrameDurationMilliseconds;
    unsigned headerBytes;
    std::vector<uint8_t> aacCRC;
    bool header(CVectorEx<_BINARY>&);
    void dump();
    size_t lastFrameLength;
};

#endif // AACSUPERFRAME_H
