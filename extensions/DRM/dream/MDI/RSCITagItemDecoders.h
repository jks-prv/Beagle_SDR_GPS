/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2004
 *
 * Author(s):
 *	Volker Fischer, Oliver Haffenden
 *
 * Description:
 *	see RSCITagItemDecoders.cpp
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

#ifndef RSCI_TAG_ITEM_DECODERS_H_INCLUDED
#define RSCI_TAG_ITEM_DECODERS_H_INCLUDED

#include "TagItemDecoder.h"

class CDRMReceiver;
class CRSISubscriber;

class CTagItemDecoderRdbv : public CTagItemDecoderRSI
{
public:
	CTagItemDecoderRdbv(CParameter* pP) : CTagItemDecoderRSI(pP, "rdbv") {}
	virtual void DecodeTag(CVector<_BINARY>& vecbiTag, const int iLenDataBits);
};

class CTagItemDecoderRsta : public CTagItemDecoderRSI
{
public:
	CTagItemDecoderRsta(CParameter* pP) : CTagItemDecoderRSI(pP, "rsta") {}
	virtual void DecodeTag(CVector<_BINARY>& vecbiTag, const int iLenDataBits);
};

class CTagItemDecoderRwmf : public CTagItemDecoderRSI
{
public:
	CTagItemDecoderRwmf(CParameter* pP) : CTagItemDecoderRSI(pP, "rwmf") {}
	virtual void DecodeTag(CVector<_BINARY>& vecbiTag, const int iLenDataBits);
};

class CTagItemDecoderRwmm : public CTagItemDecoderRSI
{
public:
	CTagItemDecoderRwmm(CParameter* pP) : CTagItemDecoderRSI(pP, "rwmm") {}
	virtual void DecodeTag(CVector<_BINARY>& vecbiTag, const int iLenDataBits);
};

class CTagItemDecoderRmer : public CTagItemDecoderRSI
{
public:
	CTagItemDecoderRmer(CParameter* pP) : CTagItemDecoderRSI(pP, "rmer") {}
	virtual void DecodeTag(CVector<_BINARY>& vecbiTag, const int iLenDataBits);
};

class CTagItemDecoderRdel : public CTagItemDecoderRSI
{
public:
	CTagItemDecoderRdel(CParameter* pP) : CTagItemDecoderRSI(pP, "rdel") {}
	virtual void DecodeTag(CVector<_BINARY>& vecbiTag, const int iLenDataBits);
};

class CTagItemDecoderRdop : public CTagItemDecoderRSI
{
public:
	CTagItemDecoderRdop(CParameter* pP) : CTagItemDecoderRSI(pP, "rdop") {}
	virtual void DecodeTag(CVector<_BINARY>& vecbiTag, const int iLenDataBits);
};

class CTagItemDecoderRpsd : public CTagItemDecoderRSI
{
public:
	CTagItemDecoderRpsd(CParameter* pP) : CTagItemDecoderRSI(pP, "rpsd") {}
	virtual void DecodeTag(CVector<_BINARY>& vecbiTag, const int iLenDataBits);
};

class CTagItemDecoderRpir : public CTagItemDecoderRSI
{
public:
	CTagItemDecoderRpir(CParameter* pP) : CTagItemDecoderRSI(pP, "rpir") {}
	virtual void DecodeTag(CVector<_BINARY>& vecbiTag, const int iLenDataBits);
};

class CTagItemDecoderRgps : public CTagItemDecoderRSI
{
public:
	CTagItemDecoderRgps(CParameter* pP) : CTagItemDecoderRSI(pP, "rgps") {}
	virtual void DecodeTag(CVector<_BINARY>& vecbiTag, const int iLenDataBits);
};

// RSCI control

class CTagItemDecoderRCI : public CTagItemDecoder
{
public:
	CTagItemDecoderRCI(const std::string& s) : pDRMReceiver(nullptr),tag(s) {}
	void SetReceiver(CDRMReceiver *pReceiver) {pDRMReceiver = pReceiver;}
	virtual std::string GetTagName() { return tag; }
protected:
	CDRMReceiver *pDRMReceiver;
	std::string tag;
};

class CTagItemDecoderCact : public CTagItemDecoderRCI
{
public:
	CTagItemDecoderCact() : CTagItemDecoderRCI("cact") {}
	virtual void DecodeTag(CVector<_BINARY>& vecbiTag, const int iLenDataBits);
};

class CTagItemDecoderCfre : public CTagItemDecoderRCI
{
public:
	CTagItemDecoderCfre() : CTagItemDecoderRCI("cfre") {}
	virtual void DecodeTag(CVector<_BINARY>& vecbiTag, const int iLenDataBits);
};

class CTagItemDecoderCdmo : public CTagItemDecoderRCI
{
public:
	CTagItemDecoderCdmo() : CTagItemDecoderRCI("cdmo") {}
	virtual void DecodeTag(CVector<_BINARY>& vecbiTag, const int iLenDataBits);
};

class CTagItemDecoderCrec : public CTagItemDecoderRCI
{
public:
	CTagItemDecoderCrec() : CTagItemDecoderRCI("crec") {}
	virtual void DecodeTag(CVector<_BINARY>& vecbiTag, const int iLenDataBits);
};

class CTagItemDecoderCpro : public CTagItemDecoderRCI
{
public:
	CTagItemDecoderCpro() : CTagItemDecoderRCI("crec"), pRSISubscriber(nullptr) {}
	virtual void DecodeTag(CVector<_BINARY>& vecbiTag, const int iLenDataBits);
	void SetSubscriber(CRSISubscriber *pSubscriber) {pRSISubscriber = pSubscriber;}
private:
	CRSISubscriber* pRSISubscriber;
};

#endif
