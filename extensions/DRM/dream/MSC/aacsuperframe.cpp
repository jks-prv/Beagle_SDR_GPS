#include "aacsuperframe.h"

AACSuperFrame::AACSuperFrame():AudioSuperFrame (),
    lengthPartA(0),lengthPartB(0),superFrameDurationMilliseconds(0),headerBytes(0),aacCRC()
{
}

void AACSuperFrame::init(const CAudioParam &audioParam, ERobMode eRobMode, unsigned int lengthPartA, unsigned int lengthPartB)
{
    size_t numFrames = 0;
    switch(eRobMode) {
    case ERobMode::RM_ROBUSTNESS_MODE_A:
    case ERobMode::RM_ROBUSTNESS_MODE_B:
    case ERobMode::RM_ROBUSTNESS_MODE_C:
    case ERobMode::RM_ROBUSTNESS_MODE_D:
        switch(audioParam.eAudioSamplRate) {
        case CAudioParam::AS_12KHZ:
            numFrames = 5;
            break;
        case CAudioParam::AS_24KHZ:
            numFrames = 10;
            break;
        default:
            break;
        }
        superFrameDurationMilliseconds = 400;
        break;
    case ERobMode::RM_ROBUSTNESS_MODE_E:
        switch(audioParam.eAudioSamplRate) {
        case CAudioParam::AS_24KHZ:
            numFrames = 5;
            break;
        case CAudioParam::AS_48KHZ:
            numFrames = 10;
            break;
        default:
            break;
        }
        superFrameDurationMilliseconds = 200;
        break;
    case ERobMode::RM_NO_MODE_DETECTED:
        break;
    }
    if (numFrames != 0) {
        audioFrame.resize(numFrames);   // KiwiSDR: asan says leak?
        aacCRC.resize(numFrames);       // KiwiSDR: asan says leak?
    }
    this->lengthPartA = lengthPartA;
    this->lengthPartB = lengthPartB;
}

bool AACSuperFrame::header(CVectorEx<_BINARY>& header)
{
    bool ok = true;
    unsigned numBorders = audioFrame.size()-1;
    unsigned headerBits = 12*numBorders;
    if (numBorders == 9) {
        headerBits += 4;
    }
    headerBytes = headerBits / 8;
    unsigned crcBytes = audioFrame.size();
    size_t audioPayloadLength = lengthPartA + lengthPartB - headerBytes - crcBytes; // Table 11 Note 1
    size_t previous_border = 0;
    size_t sumOfFrameLengths = 0;
    for (size_t n = 0; n < numBorders; n++) {
        unsigned frameBorder = header.Separate(12);
        if(frameBorder<previous_border) frameBorder += 4096; // Table 11 Note 2
        size_t frameLength = frameBorder-previous_border; // frame border in bytes
        sumOfFrameLengths += frameLength;
        if(sumOfFrameLengths<audioPayloadLength) {
            audioFrame[n].resize(frameLength);
            previous_border = frameBorder;
        }
        else {
            ok = false;
        }
    }
    if (numBorders == 9) {
        // reserved
        header.Separate(4);
    }
    size_t frameLength = audioPayloadLength - previous_border;
    if(ok) {
        sumOfFrameLengths += frameLength;
    }
    if(sumOfFrameLengths == audioPayloadLength) {
        audioFrame[numBorders].resize(frameLength);
        return true;
    }
    return false;
}

bool AACSuperFrame::parse(CVectorEx<_BINARY>& asf)
{
    unsigned numFrames = audioFrame.size();
    bool ok = header(asf);
    if(!ok) {
        return false;
    }
    // higher protected part
    unsigned higherProtectedBytes = 0;
    if(lengthPartA>0) {
        higherProtectedBytes =  (lengthPartA-headerBytes-aacCRC.size())/numFrames;
    }
    for(size_t f=0; f<numFrames; f++) {
        for (size_t b = 0; b < higherProtectedBytes; b++) {
            audioFrame[f][b] = asf.Separate(8);
        }
        aacCRC[f] = asf.Separate(8);
    }
    // lower protected part
    for(size_t f=0; f<numFrames; f++) {
        unsigned lowerProtectedBytes = audioFrame[f].size() - higherProtectedBytes;
        //cerr << "frame " << f << " of " << numFrames << " size " << audioFrame[f].size() << " lower " << lowerProtectedBytes << endl;
        for (size_t b = 0; b < lowerProtectedBytes; b++) {
            audioFrame[f][higherProtectedBytes+b] = asf.Separate(8);
        }
    }
    return true; // TODO return false if errors
}
