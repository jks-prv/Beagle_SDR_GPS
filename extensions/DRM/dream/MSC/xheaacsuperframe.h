#ifndef XHEAACSUPERFRAME_H
#define XHEAACSUPERFRAME_H

#include "audiosuperframe.h"
#include "frameborderdescription.h"
#include <deque>

class XHEAACSuperFrame: public AudioSuperFrame
{
public:
    XHEAACSuperFrame();
    void init(const CAudioParam& audioParam, unsigned frameSize);
    virtual bool parse(CVectorEx<_BINARY>& asf);
    virtual unsigned getNumFrames() { return borders.size(); }
    virtual unsigned getSuperFrameDurationMilliseconds();
    virtual void getFrame(std::vector<uint8_t>& frame, uint8_t& crc, unsigned i);
private:
    unsigned numChannels;
    unsigned superFrameSize;
    deque<uint8_t> payload;
    std::vector<unsigned> frameSize;
    std::vector<unsigned> borders;
};

#endif // XHEAACSUPERFRAME_H
