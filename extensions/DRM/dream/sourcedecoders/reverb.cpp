#include "reverb.h"

Reverb::Reverb()
{

}

void Reverb::Init(int outputSampleRate, bool bUse)
{
    //printf("Reverb::Init outputSampleRate=%d\n", outputSampleRate);
    /* Clear reverberation object */
    AudioRev.Init(1.0 /* seconds delay */, outputSampleRate);
    AudioRev.Clear();
    bUseReverbEffect = bUse;
    OldLeft.Init(0);
    OldRight.Init(0);
}

ETypeRxStatus Reverb::apply(bool bCurBlockOK, bool bCurBlockFaulty, CVector<_REAL> CurLeft, CVector<_REAL> CurRight)
{
    int iResOutBlockSize = CurLeft.Size();
    if(OldLeft.Size()==0) OldLeft.Init(iResOutBlockSize, 0.0);
    if(OldRight.Size()==0) OldRight.Init(iResOutBlockSize, 0.0);

    bool okToReverb = bUseReverbEffect;
    if(OldLeft.Size()!=iResOutBlockSize) okToReverb = false;
    if(OldRight.Size()!=iResOutBlockSize) okToReverb = false;

    vector<_REAL> tempLeft, tempRight;
    tempLeft.resize(unsigned(iResOutBlockSize));
    tempRight.resize(unsigned(iResOutBlockSize));
    for (int i = 0; i < iResOutBlockSize; i++)
    {
        tempLeft[unsigned(i)] = CurLeft[i];
        tempRight[unsigned(i)] = CurRight[i];
    }

    ETypeRxStatus status = DATA_ERROR;
    if (bCurBlockOK == false)
    {
        if (bAudioWasOK)
        {
            /* Post message to show that CRC was wrong (yellow light) */
            status = DATA_ERROR;

            /* Fade-out old block to avoid "clicks" in audio. We use linear
               fading which gives a log-fading impression */
            for (int i = 0; i < iResOutBlockSize; i++)
            {
                /* Linear attenuation with time of OLD buffer */
                const _REAL rAtt = 1.0 - _REAL(i / iResOutBlockSize);

                OldLeft[i] *= rAtt;
                OldRight[i] *= rAtt;

                if (okToReverb)
                {
//printf("$1 "); fflush(stdout);
                    /* Fade in input signal for reverberation to avoid clicks */
                    const _REAL rAttRev = _REAL( i / iResOutBlockSize);

                    /* Cross-fade reverberation effect */
                    const _REAL rRevSam = (1.0 - rAtt) * AudioRev.ProcessSample(OldLeft[i] * rAttRev, OldRight[i] * rAttRev);

                    /* Mono reverbration signal */
                    OldLeft[i] += rRevSam;
                    OldRight[i] += rRevSam;
                }
            }

            /* Set flag to show that audio block was bad */
            bAudioWasOK = false;
        }
        else
        {
            status = CRC_ERROR;

            if (okToReverb)
            {
//printf("$2 "); fflush(stdout);
                /* Add Reverberation effect */
                for (int i = 0; i < iResOutBlockSize; i++)
                {
                    /* Mono reverberation signal */
                    OldLeft[i] = OldRight[i] = AudioRev.ProcessSample(0, 0);
                }
            }
        }

        /* Write zeros in current output buffer */
        for (int i = 0; i < iResOutBlockSize; i++)
        {
            tempLeft[unsigned(i)] = 0.0;
            tempRight[unsigned(i)] = 0.0;
        }
    }
    else
    {
        /* Increment correctly decoded audio blocks counter */
        if (bCurBlockFaulty) {
            status = DATA_ERROR;
        }
        else {
            status = RX_OK;
        }

        if (bAudioWasOK == false)
        {
            if (okToReverb)
            {
//printf("$3 "); fflush(stdout);
                /* Add "last" reverbration only to old block */
                for (int i = 0; i < iResOutBlockSize; i++)
                {
                    /* Mono reverberation signal */
                    OldLeft[i] = OldRight[i] = AudioRev.ProcessSample(OldLeft[i], OldRight[i]);
                }
            }

            /* Fade-in new block to avoid "clicks" in audio. We use linear
               fading which gives a log-fading impression */
            for (int i = 0; i < iResOutBlockSize; i++)
            {
                /* Linear attenuation with time */
                const _REAL rAtt = _REAL( i / iResOutBlockSize);

                tempLeft[unsigned(i)] *= rAtt;
                tempRight[unsigned(i)] *= rAtt;

                if (okToReverb)
                {
//printf("$4 "); fflush(stdout);
                    /* Cross-fade reverberation effect */
                    const _REAL rRevSam = (1.0 - rAtt) * AudioRev.ProcessSample(0, 0);

                    /* Mono reverberation signal */
                    tempLeft[unsigned(i)] += rRevSam;
                    tempRight[unsigned(i)] += rRevSam;
                }
            }

            /* Reset flag */
            bAudioWasOK = true;
        }
    }

    /* Store reverberated block into output */
    for (int i = 0; i < iResOutBlockSize; i++)
    {
        CurLeft[i] = OldLeft[i];
        CurRight[i] = OldRight[i];
    }

    /* Store current audio block for next time */
    OldLeft.Init(iResOutBlockSize);
    OldRight.Init(iResOutBlockSize);
    for (int i = 0; i < iResOutBlockSize; i++)
    {
        OldLeft[i] = tempLeft[i];
        OldRight[i] = tempRight[i];
    }

    return status;
}
