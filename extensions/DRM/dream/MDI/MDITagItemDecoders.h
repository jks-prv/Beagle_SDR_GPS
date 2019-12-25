/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2004
 *
 * Author(s):
 *	Volker Fischer, Oliver Haffenden
 *
 * Description:
 *	see MDITagItemDecoders.cpp
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

#ifndef MDI_TAG_ITEM_DECODERS_H_INCLUDED
#define MDI_TAG_ITEM_DECODERS_H_INCLUDED

#include "../GlobalDefinitions.h"
#include "../Parameter.h"
#include "TagItemDecoder.h"

class CTagItemDecoderProTy : public CTagItemDecoder
{
public:
	class CDCPProtocol
	{
     public:
      std::string protocol;
      int major;
      int minor;
	};
	CTagItemDecoderProTy(){}
	virtual std::string GetTagName(void);
	virtual void DecodeTag(CVector<_BINARY>& vecbiTag, const int iLenDataBits);
	std::vector<CDCPProtocol> protocols;
};

class CTagItemDecoderLoFrCnt : public CTagItemDecoder   
{
public:
	CTagItemDecoderLoFrCnt(){}
	virtual std::string GetTagName(void);
	virtual void DecodeTag(CVector<_BINARY>& vecbiTag, const int iLenDataBits);
	uint32_t dlfc;
};

class CTagItemDecoderFAC : public CTagItemDecoder       
{
public:
	CTagItemDecoderFAC(){}
	virtual std::string GetTagName(void);
	virtual void DecodeTag(CVector<_BINARY>& vecbiTag, const int iLenDataBits);
	CVector<_BINARY> vecbidata;
};

class CTagItemDecoderSDC : public CTagItemDecoder       
{
public:
	CTagItemDecoderSDC(){}
	virtual std::string GetTagName(void);
	virtual void DecodeTag(CVector<_BINARY>& vecbiTag, const int iLenDataBits);
	CVector<_BINARY> vecbidata;
};

class CTagItemDecoderRobMod : public CTagItemDecoder    
{
public:
	CTagItemDecoderRobMod(){}
	virtual std::string GetTagName(void);
	virtual void DecodeTag(CVector<_BINARY>& vecbiTag, const int iLenDataBits);
	CVector<_BINARY> vecbidata;
	ERobMode	eRobMode;
};

class CTagItemDecoderStr : public CTagItemDecoder       
{
public:
	CTagItemDecoderStr() : vecbidata(),iStreamNumber(0) {}
	virtual void DecodeTag(CVector<_BINARY>& vecbiTag, const int iLenDataBits);
	virtual std::string GetTagName(void);
	CVector<_BINARY> vecbidata;
	int iStreamNumber;
};

class CTagItemDecoderSDCChanInf : public CTagItemDecoderRSI
{
public:
    CTagItemDecoderSDCChanInf(CParameter* pP) : CTagItemDecoderRSI(pP, "sdci") {}
	virtual std::string GetTagName(void);
	virtual void DecodeTag(CVector<_BINARY>& vecbiTag, const int iLenDataBits);
	CVector<_BINARY> vecbidata;
};

class CTagItemDecoderInfo : public CTagItemDecoder      
{
public:
	CTagItemDecoderInfo(){}
	virtual std::string GetTagName(void);
	virtual void DecodeTag(CVector<_BINARY>& vecbiTag, const int iLenDataBits);
	std::string strInfo;
};

class CTagItemDecoderRxDemodMode : public CTagItemDecoder
{
public:
		CTagItemDecoderRxDemodMode() : eMode(RM_DRM){}
		virtual std::string GetTagName(void);
		virtual void DecodeTag(CVector<_BINARY>& vecbiTag, const int iLenDataBits);
		ERecMode eMode;
};

class CTagItemDecoderAMAudio : public CTagItemDecoder
{
public:
		CTagItemDecoderAMAudio() : vecbidata() {}
		virtual void DecodeTag(CVector<_BINARY>& vecbiTag, const int iLenDataBits);
		virtual std::string GetTagName(void);
		CVector<_BINARY> vecbidata;
		CAudioParam AudioParams;
};

#endif
