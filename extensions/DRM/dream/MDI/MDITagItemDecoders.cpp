/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2004
 *
 * Author(s):
 *	Volker Fischer, Julian Cable, Oliver Haffenden
 *
 * Description:
  *	Implements Digital Radio Mondiale (DRM) Multiplex Distribution Interface
 *	(MDI), Receiver Status and Control Interface (RSCI)
 *  and Distribution and Communications Protocol (DCP) as described in
 *	ETSI TS 102 820,  ETSI TS 102 349 and ETSI TS 102 821 respectively.
 *
 *  This module derives, from the CTagItemDecoder base class, tag item decoders specialised to decode each of the tag
 *  items defined in MDI.
 *  They generally write the decoded data into the CMDIPacket object which they hold a
 *  pointer to.
 *
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


#include "MDITagItemDecoders.h"
#include "../Parameter.h"
#include <iostream>

string CTagItemDecoderProTy::GetTagName(void) {return "*ptr";}
void CTagItemDecoderProTy::DecodeTag(CVector<_BINARY>& vecbiTag, const int iLen)
{
/*
	Changes to the protocol which will allow existing decoders to still function
	will be represented by an increment of the minor version number only. Any
	new features added by the change will obviously not need to be supported by
	older modulators. Existing TAG Items will not be altered except for the
	definition of bits previously declared Rfu. New TAG Items may be added.

	Changes to the protocol which will render previous implementations unable to
	correctly process the new format will be represented by an increment of the
	major version number. Older implementations should not attempt to decode
	such MDI packets. Changes may include modification to or removal of existing
	TAG Item definitions.
*/

	/* Protocol type and revision (*ptr) always 8 bytes long */
	if (iLen != 64)
		return; // TODO: error handling!!!!!!!!!!!!!!!!!!!!!!

	CDCPProtocol p;
	/* Decode protocol type (32 bits = 4 bytes) */
	p.protocol = "";
	for (int i = 0; i < 4 /* bytes */; i++)
		p.protocol += (_BYTE) vecbiTag.Separate(SIZEOF__BYTE);


	/* Get major and minor revision of protocol */
	p.major = (int) vecbiTag.Separate(16);
	p.minor = (int) vecbiTag.Separate(16);

	protocols.push_back(p);

	SetReady(true);
}

string CTagItemDecoderLoFrCnt::GetTagName(void) {return "dlfc";}
void CTagItemDecoderLoFrCnt::DecodeTag(CVector<_BINARY>& vecbiTag, const int iLen)
{
	/* DRM logical frame count (dlfc) always 4 bytes long */
	if (iLen != 32)
		return; // TODO: error handling!!!!!!!!!!!!!!!!!!!!!!

	dlfc = (uint32_t) vecbiTag.Separate(32);

	SetReady(true);
}



string CTagItemDecoderFAC::GetTagName(void) {return "fac_";}
void CTagItemDecoderFAC::DecodeTag(CVector<_BINARY>& vecbiTag, const int iLen)
{
	/* Fast access channel (fac_) always 9 bytes long */
	if (iLen != NUM_FAC_BITS_PER_BLOCK)
		return; // TODO: error handling!!!!!!!!!!!!!!!!!!!!!!

	/* Copy incoming FAC data */
	vecbidata.Init(NUM_FAC_BITS_PER_BLOCK);
	vecbidata.ResetBitAccess();

	for (int i = 0; i < NUM_FAC_BITS_PER_BLOCK / SIZEOF__BYTE; i++)
	{
		vecbidata.
			Enqueue(vecbiTag.Separate(SIZEOF__BYTE), SIZEOF__BYTE);
	}

	SetReady(true);
}


string CTagItemDecoderSDC::GetTagName(void) {return "sdc_";}
void CTagItemDecoderSDC::DecodeTag(CVector<_BINARY>& vecbiTag, const int iLen)
{
	/* Check that this is not a dummy packet with zero length */
	if (iLen == 0)
		return; // TODO: error handling!!!!!!!!!!!!!!!!!!!!!!

	/* Rfu */
	vecbiTag.Separate(4);

	/* Copy incoming SDC data */
	const int iSDCDataSize = iLen - 4;

	vecbidata.Init(iSDCDataSize);
	vecbidata.ResetBitAccess();

	/* We have to copy bits instead of bytes since the length of SDC data is
	   usually not a multiple of 8 */
	for (int i = 0; i < iSDCDataSize; i++)
		vecbidata.Enqueue(vecbiTag.Separate(1), 1);

	SetReady(true);
}



string CTagItemDecoderRobMod::GetTagName(void) {return "robm";}
void CTagItemDecoderRobMod::DecodeTag(CVector<_BINARY>& vecbiTag, const int iLen)
{
	/* Robustness mode (robm) always one byte long */
	if (iLen != 8)
		return; // TODO: error handling!!!!!!!!!!!!!!!!!!!!!!

	switch (vecbiTag.Separate(8))
	{
	case 0:
		/* Robustness mode A */
		eRobMode = RM_ROBUSTNESS_MODE_A;
		break;

	case 1:
		/* Robustness mode B */
		eRobMode = RM_ROBUSTNESS_MODE_B;
		break;

	case 2:
		/* Robustness mode C */
		eRobMode = RM_ROBUSTNESS_MODE_C;
		break;

	case 3:
		/* Robustness mode D */
		eRobMode = RM_ROBUSTNESS_MODE_D;
		break;
	}

	SetReady(true);
}


string CTagItemDecoderStr::GetTagName(void)
{
	switch (iStreamNumber)
	{
	case 0: return "str0";
	case 1: return "str1";
	case 2: return "str2";
	case 3: return "str3";
	default: return "str?";
	}
	SetReady(true);
}

void CTagItemDecoderStr::DecodeTag(CVector<_BINARY>& vecbiTag, const int iLen)
{
	/* Copy stream data */
	vecbidata.Init(iLen);
	vecbidata.ResetBitAccess();

	for (int i = 0; i < iLen / SIZEOF__BYTE; i++)
	{
		vecbidata.
			Enqueue(vecbiTag.Separate(SIZEOF__BYTE), SIZEOF__BYTE);
	}
	SetReady(true);
}



string CTagItemDecoderSDCChanInf::GetTagName(void) {return "sdci";}

void CTagItemDecoderSDCChanInf::DecodeTag(CVector<_BINARY>& vecbiTag, const int iLen)
{
	if (iLen == 0)
		return;

	/* Get the number of streams */
	const int iNumStreams = (iLen - 8) / 3 / SIZEOF__BYTE;

	/* Get protection levels */
	/* Rfu */
	vecbiTag.Separate(4);

	/* Protection level for part A */ // TODO
    CMSCProtLev MSCProtLev;
    MSCProtLev.iPartA = vecbiTag.Separate(2);

	/* Protection level for part B */ // TODO
    MSCProtLev.iPartB = vecbiTag.Separate(2);

	/* Get stream parameters */

	/* Determine if hierarchical modulation is used */ // TODO
	bool bHierarchical = false;

    // don't store these if sdci tag packet signals zero length - dr111 bug
    // rely on the sdc_ tag packet to do something sensible.
    pParameter->Lock();
	for (int i = 0; i < iNumStreams; i++)
	{
		/* In case of hirachical modulation stream 0 describes the protection
		   level and length of hierarchical data */
		if ((i == 0) && (bHierarchical))
		{
			/* Protection level for hierarchical */ // TODO
			vecbiTag.Separate(2);

			/* rfu */
			vecbiTag.Separate(10);

            /* Data length for hierarchical */
            int iLenPartB = vecbiTag.Separate(12);

            /* Set new parameters in global struct. Length of part A is zero
               with hierarchical modulation */
            if(iLenPartB>0)
                pParameter->SetStreamLen(i, 0, iLenPartB);
        }
		else
		{
            /* Data length for part A */
            int iLenPartA = vecbiTag.Separate(12);

            /* Data length for part B */
            int iLenPartB = vecbiTag.Separate(12);

            if(iLenPartA>0 || iLenPartB>0)
                pParameter->SetStreamLen(i, iLenPartA, iLenPartB);
        }
	}
    pParameter->Unlock();
	SetReady(true);
}

string CTagItemDecoderRxDemodMode::GetTagName(void) {return "rdmo";}

void CTagItemDecoderRxDemodMode::DecodeTag(CVector<_BINARY>& vecbiTag, int iLen)
{
	string strMode = "";
	for (int i = 0; i < iLen / SIZEOF__BYTE; i++)
		strMode += (_BYTE) vecbiTag.Separate(SIZEOF__BYTE);
	if (strMode == "drm_")
		eMode = RM_DRM;
	else if (strMode == "am__")
		eMode = RM_AM;
	else if (strMode == "fm__")
		eMode = RM_FM;
	else
		eMode = RM_AM;

	SetReady(true);
}

string CTagItemDecoderAMAudio::GetTagName(void) { return "rama";}

void CTagItemDecoderAMAudio::DecodeTag(CVector<_BINARY>& vecbiTag, int iLen)
{

	/* Audio coding */
    AudioParams.eAudioCoding = CAudioParam::EAudCod(vecbiTag.Separate(2));

	/* SBR flag */
    uint32_t iVal = vecbiTag.Separate(1);
	AudioParams.eSBRFlag = (iVal == 1 ? CAudioParam::SBR_USED : CAudioParam::SBR_NOT_USED);
	/* Audio mode */
	iVal = vecbiTag.Separate(2);
	switch (iVal)
	{
		case 0: AudioParams.eAudioMode = CAudioParam::AM_MONO; break;
		case 1: AudioParams.eAudioMode = CAudioParam::AM_P_STEREO; break;
		case 2: AudioParams.eAudioMode = CAudioParam::AM_STEREO; break;
		default: AudioParams.eAudioMode = CAudioParam::AM_MONO;
	}
	/* Audio sampling rate */
	iVal = vecbiTag.Separate(3);
    if(AudioParams.eAudioCoding == CAudioParam::AC_xHE_AAC) {
        switch (iVal)
        {
            case 0: AudioParams.eAudioSamplRate = CAudioParam::AS_9_6KHZ; break;
            case 1: AudioParams.eAudioSamplRate = CAudioParam::AS_12KHZ; break;
            case 2: AudioParams.eAudioSamplRate = CAudioParam::AS_16KHZ; break;
            case 3: AudioParams.eAudioSamplRate = CAudioParam::AS_19_2KHZ; break;
            case 4: AudioParams.eAudioSamplRate = CAudioParam::AS_24KHZ; break;
            case 5: AudioParams.eAudioSamplRate = CAudioParam::AS_32KHZ; break;
            case 6: AudioParams.eAudioSamplRate = CAudioParam::AS_38_4KHZ; break;
            case 7: AudioParams.eAudioSamplRate = CAudioParam::AS_48KHZ; break;
        }
    }
    else {
        switch (iVal)
        {
            case 1: AudioParams.eAudioSamplRate = CAudioParam::AS_12KHZ; break;
            case 3: AudioParams.eAudioSamplRate = CAudioParam::AS_24KHZ; break;
            case 5: AudioParams.eAudioSamplRate = CAudioParam::AS_48KHZ; break;
            case 7: // Old OPUS EXPERIMENTAL
                AudioParams.eAudioCoding = CAudioParam::AC_OPUS;
                AudioParams.eAudioMode = CAudioParam::AM_STEREO;
                AudioParams.eAudioSamplRate = CAudioParam::AS_48KHZ;
                break;
        }
    }
	// coder field and some rfus (TODO: code the coder field correctly for all cases)
	vecbiTag.Separate(8);
	/* Copy stream data */
	vecbidata.Init(iLen-16);
	vecbidata.ResetBitAccess();
	for (int i = 0; i < (iLen-16) / SIZEOF__BYTE; i++)
		vecbidata.Enqueue(vecbiTag.Separate(SIZEOF__BYTE), SIZEOF__BYTE);

	SetReady(true);
}

string CTagItemDecoderInfo::GetTagName(void) {return "info";}

void CTagItemDecoderInfo::DecodeTag(CVector<_BINARY>& vecbiTag, const int iLen)
{
	/* Decode info string */
	strInfo = "";
	for (int i = 0; i < iLen / SIZEOF__BYTE; i++)
		strInfo += (_BYTE) vecbiTag.Separate(SIZEOF__BYTE);

	SetReady(true);
}
