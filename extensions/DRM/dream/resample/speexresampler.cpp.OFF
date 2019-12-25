#include "speexresampler.h"
#include <cstring>
#include <iostream>
using namespace std;

#define RESAMPLING_QUALITY 6 /* 0-10 : 0=fast/bad 10=slow/good */

SpeexResampler::SpeexResampler():resampler(nullptr), vecfInput(), vecfOutput(), iInputBuffered(0)
{
}

SpeexResampler::~SpeexResampler()
{
    Free();
}

void SpeexResampler::Free()
{
    if (resampler != nullptr)
    {
        speex_resampler_destroy(resampler);
        resampler = nullptr;
    }
    vecfInput.resize(0);
    vecfOutput.resize(0);
}

/* this function is only called when the input and output sample rates are different */
void SpeexResampler::Resample(CVector<_REAL>& rInput, CVector<_REAL>& rOutput)
{    
    size_t iOutputBlockSize = vecfOutput.size();
    if ((rOutput.Size() != int(iOutputBlockSize)) || (GetFreeInputSize()<rInput.size())) {
        cerr << "SpeexResampler::Resample(): initialisation needed" << endl;
        iOutputBlockSize = size_t(rOutput.Size());
        Init(rInput.size(), _REAL(rOutput.Size())/_REAL(rInput.size()));
    }

    for (size_t i = 0; i < GetFreeInputSize(); i++)
        vecfInput[i+iInputBuffered] = float(rInput[int(i)]);

    spx_uint32_t input_frames_used = spx_uint32_t(rInput.size());
    spx_uint32_t output_frames_gen = spx_uint32_t(rOutput.size());
    size_t input_frames = iInputBuffered + GetFreeInputSize();

    if (resampler != nullptr)
    {
        int err = speex_resampler_process_float(
            resampler,
            0,
            &vecfInput[0],
            &input_frames_used,
            &vecfOutput[0],
            &output_frames_gen);
        if (err != RESAMPLER_ERR_SUCCESS) {
            cerr << "SpeexResampler::Init(): libspeexdsp error: " << speex_resampler_strerror(err) << endl;
            input_frames_used = 0;
            output_frames_gen = 0;
        }
    }

    if (output_frames_gen != iOutputBlockSize)
        cerr << "SpeexResampler::Resample(): output_frames_gen(" << output_frames_gen << ") != iOutputBlockSize(" << iOutputBlockSize << ")" << endl;

    for (size_t i = 0; i < iOutputBlockSize; i++)
        rOutput[int(i)] = _REAL(vecfOutput[i]);

    iInputBuffered = input_frames - input_frames_used;
    if(iInputBuffered>0) {
        cerr << "resampling buffered " << iInputBuffered << " frames" << endl;
        if(vecfInput.size()<iInputBuffered) {
            vecfInput.resize(iInputBuffered);
        }
        for (size_t i = 0; i < iInputBuffered; i++)
            vecfInput[i] = vecfInput[i+input_frames_used];
    }
}

size_t SpeexResampler::GetFreeInputSize() const
{
    return vecfInput.size() - iInputBuffered;
}

void SpeexResampler::Reset()
{
    iInputBuffered = 0;
    if (resampler != nullptr)
    {
        int err = speex_resampler_reset_mem(resampler);
        if (err != RESAMPLER_ERR_SUCCESS)
            cerr << "SpeexResampler::Init(): libspeexdsp error: " << speex_resampler_strerror(err) << endl;
    }
}

void SpeexResampler::Init(const int iNewInputBlockSize, const _REAL rNewRatio)
{
    Free();
    if (iNewInputBlockSize==0)
        return;
    if(rNewRatio<0.1) {
        cerr << "resampler initialised with too great a compression ratio" << endl;
        return;
    }
    size_t iInputBlockSize = size_t(iNewInputBlockSize);
    size_t iOutputBlockSize = size_t(iNewInputBlockSize * rNewRatio);
    iInputBuffered = 0;
    int err;
    resampler = speex_resampler_init(1, spx_uint32_t(iInputBlockSize), spx_uint32_t(iOutputBlockSize), RESAMPLING_QUALITY, &err);
    if (resampler == nullptr)
        cerr << "SpeexResampler::Init(): libspeexdsp error: " << speex_resampler_strerror(err)<< endl;
    vecfInput.resize(size_t(iInputBlockSize));
    vecfOutput.resize(size_t(iOutputBlockSize));
}

void SpeexResampler::Init(int iNewOutputBlockSize, int iInputSamplerate, int iOutputSamplerate)
{
    size_t iInputBlockSize = size_t((iOutputSamplerate * iNewOutputBlockSize) / iInputSamplerate);
    size_t iOutputBlockSize = size_t(iNewOutputBlockSize);
    int err = RESAMPLER_ERR_SUCCESS;
    if (resampler == nullptr)
    {
        resampler = speex_resampler_init(1, spx_uint32_t(iInputSamplerate), spx_uint32_t(iOutputSamplerate), RESAMPLING_QUALITY, &err);
        iInputBuffered = 0;
    }
    else
    {
        err = speex_resampler_set_rate(resampler, spx_uint32_t(iInputSamplerate), spx_uint32_t(iOutputSamplerate));
    }
    if (err == RESAMPLER_ERR_SUCCESS) {
        vecfInput.resize(size_t(iInputBlockSize));
        vecfOutput.resize(size_t(iOutputBlockSize));
    }
    else {
        cerr << "SpeexResampler::Init(): libspeexdsp error: " << speex_resampler_strerror(err) << endl;
        Free();
    }
}
