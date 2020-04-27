#ifndef AUDIOPARAM_H
#define AUDIOPARAM_H

#include "../util/Vector.h"

class CAudioParam
{
public:

    /* AC: Audio Coding */
    enum EAudCod { AC_AAC=0, AC_OPUS=1, AC_RESERVED=2, AC_xHE_AAC=3, AC_NONE=4 };

    /* SB: SBR */
    enum ESBRFlag { SBR_NOT_USED=0, SBR_USED=1 };

    /* AM: Audio Mode */
    enum EAudioMode { AM_MONO=0, AM_P_STEREO=1, AM_STEREO=2, AM_RESERVED=3 };

    /* AS: Audio Sampling rate - complex map so don't try to assign by casting */
    enum EAudSamRat { AS_9_6KHZ, AS_12KHZ, AS_16KHZ, AS_19_2KHZ, AS_24KHZ, AS_32KHZ, AS_38_4KHZ, AS_48KHZ };

    /* MPEG Surround */
    enum ESurround { MS_NONE=0, MS_5_1=2, MS_7_1=3, MS_OTHER=7, MS_RESERVED_1=1, MS_RESERVED_4=4, MS_RESERVED_5=5, MS_RESERVED_6=6, };

    /* OB: Opus Audio Bandwidth, coded in audio data stream */
    enum EOPUSBandwidth { OB_NB, OB_MB, OB_WB, OB_SWB, OB_FB };

    /* OS: Opus Audio Sub Codec, coded in audio data stream */
    enum EOPUSSubCod { OS_SILK, OS_HYBRID, OS_CELT };

    /* OC: Opus Audio Channels, coded in audio data stream */
    enum EOPUSChan { OC_MONO, OC_STEREO };

    /* OG: Opus encoder signal type, for encoder only */
    enum EOPUSSignal { OG_VOICE, OG_MUSIC };

    /* OA: Opus encoder intended application, for encoder only */
    enum EOPUSApplication { OA_VOIP, OA_AUDIO };

    CAudioParam(): strTextMessage(), iStreamID(STREAM_ID_NOT_USED),
            eAudioCoding(AC_NONE), eSBRFlag(SBR_NOT_USED), eAudioSamplRate(AS_24KHZ),
            bTextflag(false), bEnhanceFlag(false), eSurround(MS_NONE), eAudioMode(AM_MONO),
            xHE_AAC_config(),
            eOPUSBandwidth(OB_FB), eOPUSSubCod(OS_SILK), eOPUSChan(OC_STEREO),
            eOPUSSignal(OG_MUSIC), eOPUSApplication(OA_AUDIO),
            bOPUSForwardErrorCorrection(false), bOPUSRequestReset(false),
            bParamChanged(false)
    {
    }

    virtual ~CAudioParam();

    CAudioParam(const CAudioParam& ap):
            strTextMessage(ap.strTextMessage),
            iStreamID(ap.iStreamID),
            eAudioCoding(ap.eAudioCoding),
            eSBRFlag(ap.eSBRFlag),
            eAudioSamplRate(ap.eAudioSamplRate),
            bTextflag(ap.bTextflag),
            bEnhanceFlag(ap.bEnhanceFlag),
            eSurround(ap.eSurround),
            eAudioMode(ap.eAudioMode),
            xHE_AAC_config(ap.xHE_AAC_config),
            eOPUSBandwidth(ap.eOPUSBandwidth),
            eOPUSSubCod(ap.eOPUSSubCod),
            eOPUSChan(ap.eOPUSChan),
            eOPUSSignal(ap.eOPUSSignal),
            eOPUSApplication(ap.eOPUSApplication),
            bOPUSForwardErrorCorrection(ap.bOPUSForwardErrorCorrection),
            bOPUSRequestReset(ap.bOPUSRequestReset),
            bParamChanged(ap.bParamChanged)
    {
    }
    
    CAudioParam& operator=(const CAudioParam& ap)
    {
        strTextMessage = ap.strTextMessage;
        iStreamID = ap.iStreamID;
        eAudioCoding = ap.eAudioCoding;
        eSBRFlag = ap.eSBRFlag;
        eAudioSamplRate = ap.eAudioSamplRate;
        bTextflag =	ap.bTextflag;
        bEnhanceFlag = ap.bEnhanceFlag;
        eSurround = ap.eSurround;
        eAudioMode = ap.eAudioMode;
        xHE_AAC_config = ap.xHE_AAC_config;
        eOPUSBandwidth = ap.eOPUSBandwidth;
        eOPUSSubCod = ap.eOPUSSubCod;
        eOPUSChan = ap.eOPUSChan;
        eOPUSSignal = ap.eOPUSSignal;
        eOPUSApplication = ap.eOPUSApplication;
        bOPUSForwardErrorCorrection = ap.bOPUSForwardErrorCorrection;
        bOPUSRequestReset = ap.bOPUSRequestReset;
        bParamChanged = ap.bParamChanged;
        return *this;
    }

    bool setFromType9Bits(CVector<_BINARY>& biData, unsigned numBytes);
    void setFromType9Bytes(const std::vector<uint8_t>& data);
    std::vector<uint8_t> getType9Bytes() const;
    void EnqueueType9(CVector<_BINARY>& biData) const;

    /* Text-message */
    std::string strTextMessage;	/* Max length is (8 * 16 Bytes) */

    int iStreamID;			/* Stream Id of the stream which carries the audio service */

    EAudCod eAudioCoding;	/* This field indicated the source coding system */
    ESBRFlag eSBRFlag;		/* SBR flag */
    EAudSamRat eAudioSamplRate;	/* Audio sampling rate */
    bool bTextflag;		/* Indicates whether a text message is present or not */
    bool bEnhanceFlag;	/* Enhancement flag */
    ESurround eSurround;    /* MPEG Surround */

    /* For AAC: Mono, P-Stereo, Stereo --------------------------------- */
    EAudioMode eAudioMode;	/* Audio mode */

    /* for xHE-AAC ------------------------------------------------------ */
    std::vector<_BYTE> xHE_AAC_config;

    /* For OPUS --------------------------------------------------------- */
    EOPUSBandwidth eOPUSBandwidth; /* Audio bandwidth */
    EOPUSSubCod eOPUSSubCod; /* Audio sub codec */
    EOPUSChan eOPUSChan;	/* Audio channels */
    EOPUSSignal eOPUSSignal; /* Encoder signal type */
    EOPUSApplication eOPUSApplication; /* Encoder intended application */
    bool bOPUSForwardErrorCorrection; /* Encoder Forward Error Correction enabled */
    bool bOPUSRequestReset; /* Request encoder reset */

    /* CAudioParam has changed */
    bool bParamChanged;

    /* This function is needed for detection changes in the class */
    bool operator!=(const CAudioParam AudioParam)
    {
        if (iStreamID != AudioParam.iStreamID)
            return true;
        if (eAudioCoding != AudioParam.eAudioCoding)
            return true;
        if (eSBRFlag != AudioParam.eSBRFlag)
            return true;
        if (eAudioSamplRate != AudioParam.eAudioSamplRate)
            return true;
        if (bTextflag != AudioParam.bTextflag)
            return true;
        if (bEnhanceFlag != AudioParam.bEnhanceFlag)
            return true;
        if (eAudioMode != AudioParam.eAudioMode)
            return true;
        if(xHE_AAC_config != AudioParam.xHE_AAC_config)
            return true;
        return false;
    }

    static const char* AudioCodingCStr(CAudioParam::EAudCod eAudioCoding);
};

#endif // AUDIOPARAM_H
