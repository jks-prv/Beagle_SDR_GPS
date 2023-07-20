/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2001
 *
 * Author(s):
 *	Volker Fischer, Ollie Haffenden
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

#ifndef _AUDIOSOURCEENCODER_H_INCLUDED_
#define _AUDIOSOURCEENCODER_H_INCLUDED_

#include "../GlobalDefinitions.h"
#include "../Parameter.h"
#include "../util/Modul.h"
#include "../util/CRC.h"
#include "../TextMessage.h"
#ifdef HAVE_SPEEX
# include "../resample/speexresampler.h"
#else
# include "../resample/AudioResample.h"
#endif
#include "../datadecoding/DataEncoder.h"
#include "../util/Utilities.h"
#include "AudioCodec.h"

/* Definition */
#define MAX_ENCODED_CHANNELS 2


/* Classes ********************************************************************/
class CAudioSourceEncoderImplementation
{
public:
	CAudioSourceEncoderImplementation();
	virtual ~CAudioSourceEncoderImplementation();

	void SetTextMessage(const std::string& strText);
	void ClearTextMessage();

	void SetPicFileName(const std::string& strFileName, const std::string& strFormat)
		{DataEncoder.GetSliShowEnc()->AddFileName(strFileName, strFormat);}
	void ClearPicFileNames()
		{DataEncoder.GetSliShowEnc()->ClearAllFileNames();}
	void SetPathRemoval(bool bRemovePath)
		{DataEncoder.GetSliShowEnc()->SetPathRemoval(bRemovePath);}
	bool GetTransStat(std::string& strCPi, _REAL& rCPe)
		{return DataEncoder.GetSliShowEnc()->GetTransStat(strCPi, rCPe);}

    bool CanEncode(CAudioParam::EAudCod eAudCod) {
        switch (eAudCod)
        {
        case CAudioParam::AC_NONE: return true;
        case CAudioParam::AC_AAC:  return bCanEncodeAAC;
        case CAudioParam::AC_OPUS: return bCanEncodeOPUS;
        case CAudioParam::AC_xHE_AAC: return false;
        case CAudioParam::AC_RESERVED: return false;
        }
        return false;
    }

protected:
	void CloseEncoder();

	CTextMessageEncoder		TextMessage;
	bool				bUsingTextMessage;
	CDataEncoder			DataEncoder;
	int						iTotPacketSize;
	bool				bIsDataService;
	int						iTotNumBitsForUsage;

	CAudioCodec*			codec;

	unsigned long			lNumSampEncIn;
	unsigned long			lMaxBytesEncOut;
	unsigned long			lEncSamprate;
	CVector<_BYTE>			aac_crc_bits;
	CVector<_SAMPLE>		vecsEncInData;
	CMatrix<_BYTE>			audio_frame;
	CVector<int>			veciFrameLength;
	int						iNumChannels;
	int						iNumAudioFrames;
	int						iNumBorders;
	int						iAudioPayloadLen;
	int						iNumHigherProtectedBytes;

#ifdef HAVE_SPEEX
    SpeexResampler			ResampleObj[MAX_ENCODED_CHANNELS];
#else
    CAudioResample			ResampleObj[MAX_ENCODED_CHANNELS];
#endif
	CVector<_REAL>			vecTempResBufIn[MAX_ENCODED_CHANNELS];
	CVector<_REAL>			vecTempResBufOut[MAX_ENCODED_CHANNELS];

	bool				bCanEncodeAAC;
	bool				bCanEncodeOPUS;

public:
	virtual void InitInternalTx(CParameter& Parameters, int &iInputBlockSize, int &iOutputBlockSize);
	virtual void InitInternalRx(CParameter& Parameters, int &iInputBlockSize, int &iOutputBlockSize);
	virtual void ProcessDataInternal(CParameter& Parameters, CVectorEx<_SAMPLE>* pvecInputData,
					CVectorEx<_BINARY>* pvecOutputData, int &iInputBlockSize, int &iOutputBlockSize);
};

class CAudioSourceEncoderRx : public CReceiverModul<_SAMPLE, _BINARY>
{
public:
	CAudioSourceEncoderRx() {}
	virtual ~CAudioSourceEncoderRx() {}

protected:
	CAudioSourceEncoderImplementation AudioSourceEncoderImpl;

	virtual void InitInternal(CParameter& Parameters)
	{
		AudioSourceEncoderImpl.InitInternalRx(Parameters, iInputBlockSize, iOutputBlockSize);
	}

	virtual void ProcessDataInternal(CParameter& Parameters)
	{
		AudioSourceEncoderImpl.ProcessDataInternal(Parameters, pvecInputData, pvecOutputData, iInputBlockSize, iOutputBlockSize);
	}
};

class CAudioSourceEncoder : public CTransmitterModul<_SAMPLE, _BINARY>
{
public:
	CAudioSourceEncoder() {}
	virtual ~CAudioSourceEncoder() {}

	void SetTextMessage(const std::string& strText) {AudioSourceEncoderImpl.SetTextMessage(strText);}
	void ClearTextMessage() {AudioSourceEncoderImpl.ClearTextMessage();}

	void SetPicFileName(const std::string& strFileName, const std::string& strFormat)
			{AudioSourceEncoderImpl.SetPicFileName(strFileName, strFormat);}

	void ClearPicFileNames() {AudioSourceEncoderImpl.ClearPicFileNames();}

	void SetPathRemoval(bool bRemovePath)
			{AudioSourceEncoderImpl.SetPathRemoval(bRemovePath);}

	bool GetTransStat(std::string& strCPi, _REAL& rCPe)
			{return AudioSourceEncoderImpl.GetTransStat(strCPi, rCPe);}

	bool CanEncode(CAudioParam::EAudCod eAudCod)
			{return AudioSourceEncoderImpl.CanEncode(eAudCod);}

protected:
	CAudioSourceEncoderImplementation AudioSourceEncoderImpl;

	virtual void InitInternal(CParameter& Parameters)
	{
		AudioSourceEncoderImpl.InitInternalTx(Parameters, iInputBlockSize, iOutputBlockSize);
	}

	virtual void ProcessDataInternal(CParameter& Parameters)
	{
		AudioSourceEncoderImpl.ProcessDataInternal(Parameters, pvecInputData, pvecOutputData, iInputBlockSize, iOutputBlockSize);
	}

};

#endif // _AUDIOSOURCEENCODER_H_INCLUDED_
