#include "aacsuperframe.h"
#include "printf.h"

AACSuperFrame::AACSuperFrame():AudioSuperFrame (),
    lengthPartA(0), lengthPartB(0), superFrameDurationMilliseconds(0), headerBytes(0), aacCRC()
{
}

void AACSuperFrame::init(const CAudioParam &audioParam, ERobMode eRobMode, unsigned int lengthPartA, unsigned int lengthPartB)
{
    printf("DRM AAC init\n");
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
        audioFrame.resize(numFrames);
        aacCRC.resize(numFrames);
    }
    this->lengthPartA = lengthPartA;
    this->lengthPartB = lengthPartB;
}

void AACSuperFrame::dump()
{
    unsigned numFrames = audioFrame.size();
    unsigned numBorders = numFrames-1;
    unsigned headerBits = 12*numBorders;
    if (numBorders == 9) {
        headerBits += 4;
    }
    headerBytes = headerBits / 8;
    unsigned crcBytes = numFrames;
    unsigned audioPayloadLength = lengthPartA + lengthPartB - headerBytes - crcBytes; // Table 11 Note 1
    unsigned frameLength, sumOfFrameLengths = 0;

    printf("DRM AAC numFrames=%d apl=%d=%d+%d-%d-%d audioFrame: ",
        numFrames, audioPayloadLength, lengthPartA, lengthPartB, headerBytes, crcBytes);

    for (int f=0; f < numFrames; f++) {
        frameLength = (f < numBorders)? (unsigned long) audioFrame[f].size() : lastFrameLength;
        printf("%d:%d ", f, frameLength);
        sumOfFrameLengths += frameLength;
    }
    
    printf(" sum=%d(%d)\n", sumOfFrameLengths, audioPayloadLength);
}

bool AACSuperFrame::header(CVectorEx<_BINARY>& header)
{
    bool ok = true;
    unsigned numFrames = audioFrame.size();
    unsigned numBorders = numFrames-1;
    unsigned headerBits = 12*numBorders;
    if (numBorders == 9) {
        headerBits += 4;
    }
    headerBytes = headerBits / 8;
    unsigned crcBytes = numFrames;
    size_t audioPayloadLength = lengthPartA + lengthPartB - headerBytes - crcBytes; // Table 11 Note 1
    size_t previous_border = 0;
    size_t sumOfFrameLengths = 0;

    try {

        for (int n = 0; n < numBorders; n++) {
            unsigned frameBorder = header.Separate(12);
            if (frameBorder < previous_border) frameBorder += 4096; // Table 11 Note 2
            size_t frameLength = frameBorder - previous_border; // frame border in bytes
            sumOfFrameLengths += frameLength;
            //printf("%d:%d:%zu:%zu%s ", n, frameBorder, frameLength, sumOfFrameLengths, (ok && sumOfFrameLengths >= audioPayloadLength)? "#":"");
    
            if (sumOfFrameLengths < audioPayloadLength) {
                audioFrame[n].resize(frameLength);
                previous_border = frameBorder;
            } else {
                ok = false;
            }
        }
        if (numBorders == 9)
            header.Separate(4);     // reserved

        lastFrameLength = audioPayloadLength - previous_border;
        if (ok) sumOfFrameLengths += lastFrameLength;

        //printf("%d:na:%zu:%zu [sum=%zu ==? apl=%zu] ", numBorders, lastFrameLength, sumOfFrameLengths, sumOfFrameLengths, audioPayloadLength);
        if (sumOfFrameLengths == audioPayloadLength) {
            audioFrame[numBorders].resize(lastFrameLength);
            //printf("OK\n");
            //printf("AAC header %zu OK\n", sumOfFrameLengths);
            return true;
        }
    } catch(std::exception& e) {
        printf("DRM AAC header EXCEPTION %s\n", e.what());
        dump();
        return false;
    }

    //printf("DRM AAC header ERROR\n");
    //dump();
    return false;
}

bool AACSuperFrame::parse(CVectorEx<_BINARY>& asf)
{
    unsigned numFrames = audioFrame.size();
    if (numFrames == 0 || !header(asf)) return false;

    // higher protected part
    unsigned higherProtectedBytes = 0;
    if (lengthPartA > 0) {
        higherProtectedBytes = (lengthPartA - headerBytes - aacCRC.size()) / numFrames;
    }

    unsigned lowerProtectedBytes;
    size_t f, b;
    const char *which;

    try {

        protectedPart = HPP;
        for (f=0; f < numFrames; f++) {
            for (b = 0; b < higherProtectedBytes; b++) {
                audioFrame[f].at(b) = asf.Separate(8);
            }
            aacCRC[f] = asf.Separate(8);    // NB: for EEP all CRC is located together after the header (higherProtectedBytes = 0)
        }

        // lower protected part
        protectedPart = LPP;
        for (f=0; f < numFrames; f++) {
            lowerProtectedBytes = audioFrame[f].size() - higherProtectedBytes;
            //cerr << "frame " << f << " of " << numFrames << " size " << audioFrame[f].size() << " lower " << lowerProtectedBytes << endl;
            for (b = 0; b < lowerProtectedBytes; b++) {
                audioFrame[f].at(higherProtectedBytes+b) = asf.Separate(8);
            }
        }

    } catch(std::exception& e) {
        printf("DRM AAC parse EXCEPTION %s %s\n", (protectedPart == HPP)? "HPP" : "LPP", e.what());
        printf("DRM AAC f=%zu b=%zu asf=%lu higherProtectedBytes=%d lowerProtectedBytes=%d\n",
            f, b, (unsigned long) asf.size(), higherProtectedBytes, lowerProtectedBytes);
        dump();
        return false;
    }
    
    return true;
}
