/******************************************************************************\
 *
 * Copyright (c) 2013
 *
 * Author(s):
 *  David Flamand
 *
 * Description:
 *  See null_codec.cpp
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

#ifndef _NULL_CODEC_H_
#define _NULL_CODEC_H_

#include "AudioCodec.h"

class NullCodec : public CAudioCodec
{
public:
	NullCodec();
	virtual ~NullCodec();
	/* Decoder */
	virtual std::string DecGetVersion();
	virtual bool CanDecode(CAudioParam::EAudCod eAudioCoding);
    virtual bool DecOpen(const CAudioParam& AudioParam, int& iAudioSampleRate);
    virtual EDecError Decode(const std::vector<uint8_t>& audio_frame, uint8_t aac_crc_bits, CVector<_REAL>& left,  CVector<_REAL>& right);
    virtual void DecClose();
	virtual void DecUpdate(CAudioParam& AudioParam);
    /* Encoder */
	virtual std::string EncGetVersion();
	virtual bool CanEncode(CAudioParam::EAudCod eAudioCoding);
    virtual bool EncOpen(const CAudioParam& AudioParam, unsigned long& lNumSampEncIn, unsigned long& lMaxBytesEncOut);
    virtual int Encode(CVector<_SAMPLE>& vecsEncInData, unsigned long lNumSampEncIn, CVector<uint8_t>& vecsEncOutData, unsigned long lMaxBytesEncOut);
	virtual void EncClose();
	virtual void EncSetBitrate(int iBitRate);
	virtual void EncUpdate(CAudioParam& AudioParam);
    virtual std::string fileName(const CParameter&) const { return "";}

};

#endif // _NULL_CODEC_H_
