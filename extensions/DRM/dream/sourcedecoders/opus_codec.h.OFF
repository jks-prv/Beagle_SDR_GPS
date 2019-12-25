/******************************************************************************\
 *
 * Copyright (c) 2012-2013
 *
 * Author(s):
 *  David Flamand
 *
 * Description:
 *  See opus_codec.cpp
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

#ifndef _OPUS_CODEC_H_
#define _OPUS_CODEC_H_

#include "AudioCodec.h"
#include "../GlobalDefinitions.h"
#include "../Parameter.h"
#include "../util/CRC.h"
#ifdef USE_OPUS_LIBRARY
# include <opus/opus.h>
#else
# include "opus_dll.h"
#endif

#define OPUS_DESCRIPTION "Opus Interactive Audio Codec"
#define OPUS_WEBSITE_LINK "http://www.opus-codec.org"
#define OPUS_PCM_FRAME_SIZE 960  // (in samples) = 48000Hz * 0.020ms
#define OPUS_MAX_PCM_FRAME 3840  // (in samples) = 48000Hz * 0.060ms
#define OPUS_MAX_DATA_FRAME 1276 // (in bytes)   = CELT CBR BITRATE 510400

/*************
 *  generic
 */

bool opus_init(
	void
	);

const char *opusGetVersion(
	void
	);

void opusSetupParam(
	CAudioParam &AudioParam,
	int toc
	);

/*************
 *  encoder
 */

typedef struct _opus_encoder {
	OpusEncoder *oe;
	int samplerate;
	int channels;
	int bitrate;
	int samples_per_channel;
	int frames_per_packet;
	int bytes_per_frame;
	CCRC *CRCObject;
} opus_encoder;

opus_encoder *opusEncOpen(
	unsigned long sampleRate,
	unsigned int numChannels,
	unsigned int bytes_per_frame,
	unsigned long *inputSamples,
	unsigned long *maxOutputBytes
	);

int opusEncEncode(opus_encoder *enc,
	int32_t *inputBuffer,
	unsigned int samplesInput,
	unsigned char *outputBuffer,
	unsigned int bufferSize
	);

int opusEncClose(
	opus_encoder *enc
	);

void opusEncSetParam(opus_encoder *enc,
	CAudioParam& AudioParam
	);

/*************
 *  decoder
 */

typedef struct _opus_decoder {
	OpusDecoder *od;
	int samplerate;
	int channels;
	int changed;
	int last_good_toc;
	CCRC *CRCObject;
	opus_int16 out_pcm[OPUS_MAX_PCM_FRAME * 2]; // 2 = stereo
} opus_decoder;

opus_decoder *opusDecOpen(
	void
	);

void opusDecClose(
	opus_decoder *dec
	);

int opusDecInit(
	opus_decoder *dec,
	unsigned long samplerate,
	unsigned char channels
	);

void *opusDecDecode(
	opus_decoder *dec,
	unsigned char *error,
	unsigned char *channels,
	unsigned char *buffer,
	unsigned long buffer_size
	);

/*************
 *  class
 */

class OpusCodec : public CAudioCodec
{
public:
	OpusCodec();
	virtual ~OpusCodec();
	/* Decoder */
	virtual std::string DecGetVersion();
	virtual bool CanDecode(CAudioParam::EAudCod eAudioCoding);
    virtual bool DecOpen(const CAudioParam& AudioParam, int& iAudioSampleRate);
    virtual EDecError Decode(const std::vector<uint8_t>& audio_frame, uint8_t aac_crc_bits, CVector<_REAL>& left,  CVector<_REAL>& right);
    virtual void DecClose();
	virtual void DecUpdate(CAudioParam& AudioParam);
    virtual void Init(const CAudioParam& AudioParam, int iInputBlockSize);
    /* Encoder */
	virtual std::string EncGetVersion();
	virtual bool CanEncode(CAudioParam::EAudCod eAudioCoding);
    virtual bool EncOpen(const CAudioParam& AudioParam, unsigned long& lNumSampEncIn, unsigned long& lMaxBytesEncOut);
    virtual int Encode(CVector<_SAMPLE>& vecsEncInData, unsigned long lNumSampEncIn, CVector<uint8_t>& vecsEncOutData, unsigned long lMaxBytesEncOut);
    virtual void EncClose();
	virtual void EncSetBitrate(int iBitRate);
	virtual void EncUpdate(CAudioParam& AudioParam);
    virtual std::string fileName(const CParameter&) const { return "";}
protected:
	opus_decoder *hOpusDecoder;
	opus_encoder *hOpusEncoder;
};

#endif // _OPUS_CODEC_H_
