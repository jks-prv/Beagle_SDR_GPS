/******************************************************************************\
 *
 * Copyright (c) 2013
 *
 * Author(s):
 *  David Flamand
 *
 * Description:
 *  Null codec class, this codec is used when the requested codec is not
 *  available.
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

#include "null_codec.h"

NullCodec::NullCodec()
{
}

NullCodec::~NullCodec()
{
}

/* Decoder */

string NullCodec::DecGetVersion()
{
	return string();
}

bool NullCodec::CanDecode(CAudioParam::EAudCod eAudioCoding)
{
	(void)eAudioCoding;
	return false;
}

bool NullCodec::DecOpen(const CAudioParam& AudioParam, int& iAudioSampleRate)
{
	int iSampleRate = 24000;
	switch (AudioParam.eAudioSamplRate)
	{
    case CAudioParam::AS_9_6KHZ:
        iSampleRate = 9600;
		break;
	case CAudioParam::AS_12KHZ:
		iSampleRate = 12000;
		break;
	case CAudioParam::AS_16KHZ:
		iSampleRate = 16000;
		break;
    case CAudioParam::AS_19_2KHZ:
        iSampleRate = 19200;
        break;
    case CAudioParam::AS_24KHZ:
		iSampleRate = 24000;
		break;
    case CAudioParam::AS_32KHZ:
        iSampleRate = 32000;
        break;
    case CAudioParam::AS_38_4KHZ:
        iSampleRate = 38400;
        break;
    case CAudioParam::AS_48KHZ:
		iSampleRate = 48000;
		break;
	}
    iAudioSampleRate = iSampleRate;
	return false;
}

CAudioCodec::EDecError NullCodec::Decode(const vector<uint8_t>& audio_frame, uint8_t aac_crc_bits, CVector<_REAL>& left, CVector<_REAL>& right)
{
    (void) audio_frame;
    (void) aac_crc_bits;
    (void) left;
    (void) right;
    return DECODER_ERROR_UNKNOWN;
}

void NullCodec::DecClose()
{
}

void NullCodec::DecUpdate(CAudioParam& AudioParam)
{
	(void)AudioParam;
}

/* Encoder */

string NullCodec::EncGetVersion()
{
	return string();
}

bool NullCodec::CanEncode(CAudioParam::EAudCod eAudioCoding)
{
	(void)eAudioCoding;
	return false;
}

bool NullCodec::EncOpen(const CAudioParam&, unsigned long& lNumSampEncIn, unsigned long& lMaxBytesEncOut)
{
    lNumSampEncIn = 1;
    lMaxBytesEncOut = 1;
	return false;
}

int NullCodec::Encode(CVector<_SAMPLE>& vecsEncInData, unsigned long lNumSampEncIn, CVector<uint8_t>& vecsEncOutData, unsigned long lMaxBytesEncOut)
{
	(void)vecsEncInData;
	(void)lNumSampEncIn;
	(void)vecsEncOutData;
	(void)lMaxBytesEncOut;
	return 0;
}

void NullCodec::EncClose()
{
}

void NullCodec::EncSetBitrate(int iBitRate)
{
	(void)iBitRate;
}

void NullCodec::EncUpdate(CAudioParam& AudioParam)
{
	(void)AudioParam;
}
