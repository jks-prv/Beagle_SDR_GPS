/******************************************************************************\
 *
 * Copyright (c) 2013
 *
 * Author(s):
 *  David Flamand
 *
 * Description:
 *  AAC codec class
 *
 ******************************************************************************
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
\******************************************************************************/

#ifdef HAVE_LIBFDK_AAC

// NB v1.470: Because of the C_LINKAGE() change

#include "DRM_main.h"
#include "printf.h"

#include "fdk_aac_codec.h"
#include <FDK-AAC/aacenc_lib.h>
#include <FDK-AAC/FDK_audio.h>
#include "SDC.h"
#include <cstring>

FdkAacCodec::FdkAacCodec() :
    hDecoder(nullptr), hEncoder(nullptr), bUsac(false), decode_buf()
{
}


FdkAacCodec::~FdkAacCodec()
{
	DecClose();
	EncClose();
}

static void aacinfo(LIB_INFO& inf) {
    LIB_INFO info[12];
    memset(info, 0, sizeof(info));
    aacDecoder_GetLibInfo(info);
    stringstream version;
    for(int i=0; info[i].module_id!=FDK_NONE && i<12; i++) {
        if(info[i].module_id == FDK_AACDEC) {
            inf = info[i];
        }
    }
}

string
FdkAacCodec::DecGetVersion()
{
    stringstream version;
    LIB_INFO info;
    aacinfo(info);
    version << info.title << " version " << info.versionStr << " " << info.build_date << endl;
    return version.str();
}

bool
FdkAacCodec::CanDecode(CAudioParam::EAudCod eAudioCoding)
{
    LIB_INFO linfo;
    aacinfo(linfo);

    if(eAudioCoding == CAudioParam::AC_AAC) {
        if((linfo.flags & CAPF_AAC_DRM_BSFORMAT) == 0) {
            //cerr << "FDK aac but codec has no drm" << endl;
            return false;
        }
        if((linfo.flags & CAPF_SBR_DRM_BS) ==0 ) {
            //cerr << "FDK aac but codec has no sbr" << endl;
            return false;
        }
        if((linfo.flags & CAPF_SBR_PS_DRM) == 0) {
            //cerr << "FDK aac but codec has no parametric stereo" << endl;
            return false;
        }
        return true;
    }

#ifdef HAVE_USAC
    if (eAudioCoding == CAudioParam::AC_xHE_AAC) {
        if ((linfo.flags & CAPF_AAC_USAC) != 0) {
            //printf("FDK CAPF_AAC_USAC YES\n");
            return true;
        }
    }
#endif

    //printf("FDK can decode NO\n");
    return false;
}

static void logAOT(const CStreamInfo& info) {
    switch (info.aot) {
    case AUDIO_OBJECT_TYPE::AOT_DRM_AAC:
        cerr << " AAC";
        break;
    case AUDIO_OBJECT_TYPE::AOT_DRM_SBR:
        cerr << " AAC+SBR";
        break;
    case AUDIO_OBJECT_TYPE::AOT_DRM_MPEG_PS:
        cerr << " AAC+SBR+PS";
        break;
    case AUDIO_OBJECT_TYPE::AOT_DRM_SURROUND:
        cerr << " AAC+Surround";
        break;
#ifdef HAVE_USAC
    case AUDIO_OBJECT_TYPE::AOT_USAC:
    case AUDIO_OBJECT_TYPE::AOT_DRM_USAC:
        cerr << " xHE-AAC";
        break;
#endif
    default:
        cerr << "unknown object type";
    }
    if(info.extAot == AUDIO_OBJECT_TYPE::AOT_SBR) {
        cerr << "+SBR";
    }
}
    /*
     * AC_ER_VCB11 1 means use  virtual codebooks
     * AC_ER_RVLC  1 means use huffman codeword reordering
     * AC_ER_HCR   1 means use virtual codebooks
     * AC_SCALABLE AAC Scalable
     * AC_ELD AAC-ELD
     * AC_LD AAC-LD
     * AC_ER ER syntax
     * AC_BSAC BSAC
     * AC_USAC USAC
     * AC_RSV603DA RSVD60 3D audio
     * AC_HDAAC HD-AAC
     * AC_RSVD50 Rsvd50
     * AC_SBR_PRESENT SBR present flag (from ASC)
     * AC_SBRCRC  SBR CRC present flag. Only relevant for AAC-ELD for now.
     * AC_PS_PRESENT PS present flag (from ASC or implicit)
     * AC_MPS_PRESENT   MPS present flag (from ASC or implicit)
     * AC_DRM DRM bit stream syntax
     * AC_INDEP Independency flag
     * AC_MPEGD_RES MPEG-D residual individual channel data.
     * AC_SAOC_PRESENT SAOC Present Flag
     * AC_DAB DAB bit stream syntax
     * AC_ELD_DOWNSCALE ELD Downscaled playout
     * AC_LD_MPS Low Delay MPS.
     * AC_DRC_PRESENT  Dynamic Range Control (DRC) data found.
     * AC_USAC_SCFGI3  USAC flag: If stereoConfigIndex is 3 the flag is set.
     */

static void logFlags(const CStreamInfo& info) {

    if((info.flags & AC_USAC) == AC_USAC) {
        cerr << "+USAC";
    }
    if((info.flags & AC_SBR_PRESENT) == AC_SBR_PRESENT) {
        cerr << "+SBR";
    }
    if((info.flags & AC_SBRCRC) == AC_SBRCRC) {
        cerr << "+SBR-CRC";
    }
    if((info.flags & AC_PS_PRESENT) == AC_PS_PRESENT) {
        cerr << "+PS";
    }
    if((info.flags & AC_MPS_PRESENT) == AC_MPS_PRESENT) {
        cerr << "+MPS";
    }
    if((info.flags & AC_INDEP) == AC_INDEP) {
        cerr << " (independent)";
    }
}

#if 1
static void logNumbers(const CStreamInfo& info) {
    cerr << " channels_coded=" << info.aacNumChannels
         << " coded_sample_rate=" << info.aacSampleRate
         << " channels=" << info.numChannels
         << " channel_config=" << info.channelConfig
         << " sample_rate=" << info.sampleRate
         << " extended_sample_rate=" << info.extSamplingRate
         << " samples_per_frame=" << info.aacSamplesPerFrame
         << " decoded_audio_frame_size=" << info.frameSize
         << " flags=" << hex << info.flags << dec;
    cerr << " channel_0_type=" << int(info.pChannelType[0]) << " index=" << int(info.pChannelIndices[0]);
    if(info.numChannels==2)
        cerr << " channel_1_type=" << int(info.pChannelType[1]) << " index=" << int(info.pChannelIndices[1]);
}
#endif

/*      FROM AACCodec:
    In case of SBR, AAC sample rate is half the total sample rate. Length of output is doubled if SBR is used
    if (AudioParam.eSBRFlag == CAudioParam::SB_USED)
    {
        iAudioSampleRate = iAACSampleRate * 2;
    }
    else
    {
        iAudioSampleRate = iAACSampleRate;
    }
*/

bool
FdkAacCodec::DecOpen(const CAudioParam& AudioParam, int& iAudioSampleRate)
{
    unsigned int type9Size;
    UCHAR *t9;
    hDecoder = aacDecoder_Open (TRANSPORT_TYPE::TT_DRM, 3);

    // provide a default value for iAudioSampleRate in case we can't do better. TODO xHEAAC
    int iDefaultSampleRate;
    switch (AudioParam.eAudioSamplRate)
    {
    case CAudioParam::AS_12KHZ:
        iDefaultSampleRate = 12000;
        break;

    case CAudioParam::AS_24KHZ:
        iDefaultSampleRate = 24000;
        break;
    default:
        iDefaultSampleRate = 12000;
        break;
    }
    if (AudioParam.eSBRFlag == CAudioParam::SBR_USED)
    {
        iDefaultSampleRate = iDefaultSampleRate * 2;
    }

    if(hDecoder == nullptr) {
        iAudioSampleRate = iDefaultSampleRate;
        return false;
    }

    vector<uint8_t> type9 = AudioParam.getType9Bytes();
    type9Size = type9.size();
    t9 = &type9[0];

    // KiwiSDR NB: *this* is where CAudioSourceDecoder::inputSampleRate is set
    // as used in CAudioSourceDecoder::InitInternal()
    // Not obvious, but it is passed by pointer.
    
    //cerr << "type9 " << hex; for(size_t i=0; i<type9Size; i++) cerr << int(type9[i]) << " "; cerr << dec << endl;
    AAC_DECODER_ERROR err = aacDecoder_ConfigRaw(hDecoder, &t9, &type9Size);

    if (err == AAC_DEC_OK) {
        CStreamInfo *pinfo = aacDecoder_GetStreamInfo(hDecoder);
        if (pinfo==nullptr) {
            cerr << "FDK DecOpen No stream info" << endl;
            iAudioSampleRate = iDefaultSampleRate;
            return true;// TODO
        }
        cerr << "FDK DecOpen";
        logAOT(*pinfo);
        logFlags(*pinfo);
        logNumbers(*pinfo);
        cerr << endl;
        iAudioSampleRate = pinfo->extSamplingRate;

    #ifdef KIWISDR
        // KiwiSDR: fix to detect mono non-SBR, e.g. All India Radio, 7550 kHz
        if (iAudioSampleRate == 0) iAudioSampleRate = pinfo->aacSampleRate;
        if (iAudioSampleRate == 0) {
    #else
        if (pinfo->extSamplingRate == 0) {
    #endif
            cerr << "FDK DecOpen codec returned sample rate = 0?" << endl;
            iAudioSampleRate = iDefaultSampleRate; // get from AudioParam if codec couldn't get it
        }

        if(pinfo->aot == AUDIO_OBJECT_TYPE::AOT_USAC) bUsac = true;
#ifdef HAVE_USAC
        else if(pinfo->aot == AUDIO_OBJECT_TYPE::AOT_DRM_USAC) bUsac = true;
#endif
        else bUsac = false;

        return true;
    }

    drm_t *drm = &DRM_SHMEM->drm[(int) FROM_VOID_PARAM(TaskGetUserParam())];
    drm->i_epoch = drm->i_samples = drm->i_tsamples = 0;

    iAudioSampleRate = iDefaultSampleRate;  // get from AudioParam if codec couldn't get it
    return true; // TODO
 }

CAudioCodec::EDecError FdkAacCodec::Decode(const vector<uint8_t>& audio_frame, uint8_t aac_crc_bits,
    CVector<_REAL>& left, CVector<_REAL>& right)
{
    drm_t *drm = &DRM_SHMEM->drm[(int) FROM_VOID_PARAM(TaskGetUserParam())];

    writeFile(audio_frame);
    vector<uint8_t> data;
    uint8_t* pData;
    UINT bufferSize;

    if (bUsac) {
        pData = const_cast<uint8_t*>(&audio_frame[0]);
        bufferSize = audio_frame.size();
    }
    else {
        data.resize(audio_frame.size()+1);
        data[0] = aac_crc_bits;

        for (size_t i = 0; i < audio_frame.size(); i++)
            data[i + 1] = audio_frame[i];

        pData = &data[0];
        bufferSize = data.size();
    }
    UINT bytesValid = bufferSize;

    drm_next_task("FDK fill enter");
    AAC_DECODER_ERROR err = aacDecoder_Fill(hDecoder, &pData, &bufferSize, &bytesValid);
    drm_next_task("FDK fill exit");
    if (err != AAC_DEC_OK) {
        cerr << "FDK fill failed " << int(err) << endl;
        return CAudioCodec::DECODER_ERROR_UNKNOWN;
    }

    CStreamInfo *pinfo = aacDecoder_GetStreamInfo(hDecoder);
    if (pinfo == nullptr) {
        cerr << "FDK No stream info" << endl;
        //return nullptr; this breaks everything!
    }

    #if 0
        uint16_t now = timer_sec()/8;
        if (now != last_time) {
            last_time = now;
            cerr << "Decode";
            logAOT(*pinfo);
            logFlags(*pinfo);
            logNumbers(*pinfo);
            cerr << endl;
        }
    #endif

    if (pinfo->aacNumChannels == 0) {
        cerr << "FDK zero output coded channels: err=" << err << endl;
        //printf("err ZOCC ");
        //return CAudioCodec::DECODER_ERROR_UNKNOWN;
    }
    else {
        //cerr << pinfo->aacNumChannels << " aac channels " << endl;
    }

    if (err != AAC_DEC_OK) {
        cerr << "FDK Fill failed: " << err << endl;
        //printf("err FILL ");
        return CAudioCodec::DECODER_ERROR_UNKNOWN;
    }
    //cerr << "aac decode after fill bufferSize " << bufferSize << ", bytesValid " << bytesValid << endl;
    if (bytesValid != 0) {
        cerr << "FDK Unable to feed all " << bufferSize << " input bytes, bytes left " << bytesValid << endl;
        //printf("err FEED ");
        return CAudioCodec::DECODER_ERROR_UNKNOWN;
    }

    if (pinfo->numChannels == 0) {
        cerr << "FDK zero output channels: err=" << err << endl;
        //printf("err ZOC ");
        //return CAudioCodec::DECODER_ERROR_UNKNOWN;
    }

    size_t output_size = pinfo->frameSize * pinfo->numChannels;
    if (sizeof (decode_buf) < sizeof(int16_t)*output_size) {
        cerr << "FDK can't fit output into decoder buffer" << endl;
        //printf("err FIT ");
        return CAudioCodec::DECODER_ERROR_UNKNOWN;
    }

    memset(decode_buf, 0, sizeof(int16_t)*output_size);
    drm_next_task("FDK decode enter");
    err = aacDecoder_DecodeFrame(hDecoder, decode_buf, output_size, 0);
    drm_next_task("FDK decode exit");

    if (err == AAC_DEC_OK) {
        //double d = 0.0;
        //for(size_t i=0; i<output_size; i++) d += double(decode_buf[i]);
        //cerr << "energy in good frame " << (d/output_size) << endl;
        ;
    }
    else if (err == AAC_DEC_PARSE_ERROR) {
        //cerr << "FDK Error parsing bitstream." << endl;
        ext_send_msg(drm->rx_chan, false, "EXT annotate=3");
        //printf("err PARSE ");
        return CAudioCodec::DECODER_ERROR_UNKNOWN;
    }
    else if (err == AAC_DEC_OUTPUT_BUFFER_TOO_SMALL) {
        cerr << "FDK The provided output buffer is too small." << endl;
        //printf("err BUFSMALL ");
    }
    else if (err == AAC_DEC_OUT_OF_MEMORY) {
        cerr << "FDK Heap returned NULL pointer. Output buffer is invalid." << endl;
        //printf("err NOMEM ");
    }
    else if (err == AAC_DEC_CRC_ERROR) {
        //cerr << "FDK CRC error." << endl;     // happens often with marginal signals
        ext_send_msg(drm->rx_chan, false, "EXT annotate=1");
        //printf("err CRC ");
        return CAudioCodec::DECODER_ERROR_UNKNOWN;
    }
    else if (err == AAC_DEC_UNKNOWN) {
        cerr << "FDK Error condition is of unknown reason, or from a another module. Output buffer is invalid." << endl;
    }
    else {
        cerr << "FDK Other error " << hex << int(err) << dec << endl;
    }
    
    if (err != AAC_DEC_OK) {
        ext_send_msg(drm->rx_chan, false, "EXT annotate=2");
        //printf("err 0x%x ", err);
        return CAudioCodec::DECODER_ERROR_UNKNOWN;
    }

    left.Init(pinfo->frameSize, 0.0);
    right.Init(pinfo->frameSize, 0.0);

    if (pinfo->numChannels == 1)
    {
        /* Mono */
        //cerr << "mono " << pinfo->frameSize << endl;
        //_REAL d = 0.0;
        //for(size_t i=0; i<output_size; i++) d += _REAL(decode_buf[i]);
        //cerr << "energy in good frame " << (d/output_size) << endl;
        //printf("m%d_%.1f ", pinfo->frameSize, d/output_size); fflush(stdout);
        for(int i = 0; i<pinfo->frameSize; i++) {
            left[int(i)] = _REAL(decode_buf[i]) / 2.0;
            right[int(i)] = _REAL(decode_buf[i]) / 2.0;
        }
    }
    else
    {
        /* Stereo docs claim non-interleaved but we are getting interleaved! */
        //cerr << "stereo " << pinfo->frameSize << endl;
        //printf("s%d ", pinfo->frameSize); fflush(stdout);
        for(int i = 0; i<pinfo->frameSize; i++) {
            left[int(i)] = _REAL(decode_buf[2*i]);
            right[int(i)] = _REAL(decode_buf[2*i+1]);
        }
    }
    return CAudioCodec::DECODER_ERROR_OK;
}

void
FdkAacCodec::DecClose()
{
    closeFile();
    if (hDecoder != nullptr)
	{
        aacDecoder_Close(hDecoder);
        hDecoder = nullptr;
	}
}

void
FdkAacCodec::DecUpdate(CAudioParam&)
{
}


string
FdkAacCodec::EncGetVersion()
{
    /*
    LIB_INFO info;
    aacEncGetLibInfo(&info);
    stringstream version;
    version << int((info.version >> 24) & 0xff)
            << '.'
            << int((info.version >> 16) & 0xff)
            << '.'
            << int((info.version >> 8) & 0xff);
    return version.str();
    */
    return "1.0";
}

bool
FdkAacCodec::CanEncode(CAudioParam::EAudCod eAudioCoding)
{
    if(eAudioCoding != CAudioParam::AC_AAC)
        return false;
    if(hEncoder == nullptr) {
        AACENC_ERROR r = aacEncOpen(&hEncoder, 0x01|0x02|0x04|0x08, 0); // allocate all modules except the metadata module, let library allocate channels in case we get to use MPS
        if(r!=AACENC_OK) {
            hEncoder = nullptr;
            return false;
        }
        UINT aot = aacEncoder_GetParam(hEncoder, AACENC_AOT);
        if(aot==AOT_DRM_AAC)
                return true;
        if(aot==AOT_DRM_SBR)
                return true;
        if(aot==AOT_DRM_MPEG_PS)
                return true;
        //if(aot==AOT_DRM_SURROUND)
        //        return true;
        r = aacEncoder_SetParam(hEncoder, AACENC_AOT, AOT_DRM_SBR);
        aacEncoder_SetParam(hEncoder, AACENC_AOT, aot);
        aacEncClose(&hEncoder);
        hEncoder = nullptr;
        return r==AACENC_OK;
    }
    else {
        UINT aot = aacEncoder_GetParam(hEncoder, AACENC_AOT);
        if(aot==AOT_DRM_AAC)
                return true;
        if(aot==AOT_DRM_SBR)
                return true;
        if(aot==AOT_DRM_MPEG_PS)
                return true;
        //if(aot==AOT_DRM_SURROUND)
        //        return true;
    }
    return false;
}

bool
FdkAacCodec::EncOpen(const CAudioParam& AudioParam, unsigned long& lNumSampEncIn, unsigned long& lMaxBytesEncOut)
{
    AACENC_ERROR r = aacEncOpen(&hEncoder, 0x01|0x02|0x04|0x08, 0); // allocate all modules except the metadata module, let library allocate channels in case we get to use MPS
    if(r!=AACENC_OK) {
        cerr << "error opening encoder " << r << endl;
        return false;
    }
    /*
     *  AOT_DRM_AAC      = 143,  Virtual AOT for DRM (ER-AAC-SCAL without SBR)
     *  AOT_DRM_SBR      = 144,  Virtual AOT for DRM (ER-AAC-SCAL with SBR)
     *  AOT_DRM_MPEG_PS  = 145,  Virtual AOT for DRM (ER-AAC-SCAL with SBR and MPEG-PS)
     *  AOT_DRM_SURROUND = 146,  Virtual AOT for DRM Surround (ER-AAC-SCAL (+SBR) +MPS)
     *  AOT_DRM_USAC     = 147   Virtual AOT for DRM with USAC
     * NB decoder uses only AOT_DRM_AAC and puts SBR, PS in sub fields - what should we do with the encoder?
     */
    switch (AudioParam.eAudioMode) {
    case CAudioParam::AM_RESERVED:break;
    case CAudioParam::AM_MONO:
        if(AudioParam.eSBRFlag) {
            r = aacEncoder_SetParam(hEncoder, AACENC_AOT, AOT_DRM_SBR);
        }
        else {
            r = aacEncoder_SetParam(hEncoder, AACENC_AOT, AOT_DRM_AAC); // OpenDigitalRadio version doesn't claim to support AOT_DRM_AAC
        }
        break;
    case CAudioParam::AM_P_STEREO:
        r = aacEncoder_SetParam(hEncoder, AACENC_AOT, AOT_DRM_MPEG_PS);
        break;
    case CAudioParam::AM_STEREO:
        r = aacEncoder_SetParam(hEncoder, AACENC_AOT, AOT_DRM_SBR);
        break;
    //case CAudioParam::AM_SURROUND:
        //r = aacEncoder_SetParam(hEncoder, AACENC_AOT, AOT_DRM_SURROUND);
    }
    if(r!=AACENC_OK) {
        if(r==AACENC_INVALID_CONFIG) {
            cerr << "invalid config setting DRM mode" << endl;
        }
        else {
            cerr << "error setting DRM mode" << hex << r << dec << endl;
        }
        return false;
    }
    switch (AudioParam.eAudioSamplRate) {
    case CAudioParam::AS_12KHZ:
        r = aacEncoder_SetParam(hEncoder, AACENC_SAMPLERATE, 12000);
        break;
    case CAudioParam::AS_24KHZ:
        r = aacEncoder_SetParam(hEncoder, AACENC_SAMPLERATE, 24000);
        break;
    case CAudioParam::AS_16KHZ:
    case CAudioParam::AS_32KHZ:
    case CAudioParam::AS_9_6KHZ:
    case CAudioParam::AS_19_2KHZ:
    case CAudioParam::AS_38_4KHZ:
    case CAudioParam::AS_48KHZ:
        ;
    }
    if(r!=AACENC_OK) {
        cerr << "error setting sample rate " << hex << r << dec << endl;
        return false;
    }
    switch (AudioParam.eAudioMode) {
    case CAudioParam::AM_MONO:
        r = aacEncoder_SetParam(hEncoder, AACENC_CHANNELMODE, MODE_1);
        break;
    case CAudioParam::AM_P_STEREO:
        r = aacEncoder_SetParam(hEncoder, AACENC_CHANNELMODE, MODE_2);
        break;
    case CAudioParam::AM_STEREO:
        r = aacEncoder_SetParam(hEncoder, AACENC_CHANNELMODE, MODE_2);
       break;
    case CAudioParam::AM_RESERVED:;
    //case CAudioParam::AM_SURROUND:
        //r = aacEncoder_SetParam(hEncoder, AACENC_CHANNELMODE, MODE_6_1); // TODO provide more options ES 201 980 6.4.3.10
    }
    if(r!=AACENC_OK) {
        cerr << "error setting channel mode " << hex << r << dec << endl;
        return false;
    }
    r = aacEncoder_SetParam(hEncoder, AACENC_TRANSMUX, TT_DRM); // OpenDigitalRadio doesn't have TT_DRM in it
    if(r!=AACENC_OK) {
        cerr << "error setting transport type " << hex << r << dec << endl;
        return false;
    }
    r = aacEncoder_SetParam(hEncoder, AACENC_BITRATE,  1000*960*8/400);
    if(r!=AACENC_OK) {
        cerr << "error setting bitrate " << hex << r << dec << endl;
        return false;
    }
    r = aacEncoder_SetParam(hEncoder, AACENC_GRANULE_LENGTH, 960); // might let us set frameLength
    if(r!=AACENC_OK) {
        cerr << "error setting frame length " << hex << r << dec << endl;
        return false;
    }
    lNumSampEncIn=960;
    lMaxBytesEncOut=769; // TODO
    r = aacEncEncode(hEncoder, nullptr, nullptr, nullptr, nullptr);
    if(r!=AACENC_OK) {
        cerr << "error initialising encoder " << hex << r << dec << endl;
        return false;
    }
    AACENC_InfoStruct info;
    r = aacEncInfo(hEncoder, &info);
    if(r!=AACENC_OK) {
        cerr << "error getting encoder info " << hex << r << dec << endl;
        return false;
    }
    cerr << "maxOutBufBytes " << info.maxOutBufBytes << endl;
    cerr << "maxAncBytes " << info.maxAncBytes << endl;
    cerr << "inputChannels " << info.inputChannels << endl;
    cerr << "frameLength " << info.frameLength << endl;
    cerr << "maxAncBytes " << info.maxAncBytes << endl;
    return true;
}

int
FdkAacCodec::Encode(CVector<_SAMPLE>&, unsigned long, CVector<uint8_t>&, unsigned long)
{
    int bytesEncoded = 0;
    if (hEncoder != nullptr) {
    }
    return bytesEncoded;
}

void
FdkAacCodec::EncClose()
{
    if (hEncoder != nullptr)
	{
        aacEncClose(&hEncoder);
        hEncoder = nullptr;
	}
}

void
FdkAacCodec::EncSetBitrate(int iBitRate)
{
    if (hEncoder != nullptr)
	{
        aacEncoder_SetParam(hEncoder, AACENC_BITRATE, iBitRate);
    }
}

void
FdkAacCodec::EncUpdate(CAudioParam&)
{
}

string
FdkAacCodec::fileName(const CParameter& Parameters) const
{
    // Store AAC-data in file
    stringstream ss;
    ss << "aac_";

//    Parameters.Lock(); // TODO CAudioSourceDecoder::InitInternal() already have the lock
    if (Parameters.
            Service[Parameters.GetCurSelAudioService()].AudioParam.
            eAudioSamplRate == CAudioParam::AS_12KHZ)
    {
        ss << "12kHz_";
    }
    else
        ss << "24kHz_";

    switch (Parameters.
            Service[Parameters.GetCurSelAudioService()].
            AudioParam.eAudioMode)
    {
    case CAudioParam::AM_MONO:
        ss << "mono";
        break;

    case CAudioParam::AM_P_STEREO:
        ss << "pstereo";
        break;

    case CAudioParam::AM_STEREO:
        ss << "stereo";
        break;
    case CAudioParam::AM_RESERVED:;
    }

    if (Parameters.
            Service[Parameters.GetCurSelAudioService()].AudioParam.
            eSBRFlag == CAudioParam::SBR_USED)
    {
        ss << "_sbr";
    }
    ss << ".dat";

    return ss.str();
}

#endif
