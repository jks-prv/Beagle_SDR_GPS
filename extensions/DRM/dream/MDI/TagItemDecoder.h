/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2004
 *
 * Author(s):
 *	Volker Fischer, Oliver Haffenden
 *
 * Description:
 *	Pure abstract base class for a tag item decoder. The client (CTagPacketDecoder) should read the name and length,
 *  and check the name against the name returned by GetTagName(). If it matches, the rest of the
 *  tag body should be passed to DecodeTag().
 *  Specialised derived classes should be constructed with any pointers they need to act upon the
 *  decoded information. This might be the CTagPacketDecoder subclass that owns them, or some other
 *  object that will receive the information.
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

#ifndef TAG_ITEM_DECODER_H_INCLUDED
#define TAG_ITEM_DECODER_H_INCLUDED

#include "../GlobalDefinitions.h"
#include "../Parameter.h"
#include "MDIDefinitions.h"
#include "../util/Vector.h"


class CTagItemDecoder
{
public:
	// Decode the tag item body. The name and length should already have been consumed by the
	// caller (CTagPacketDecoder).
	virtual void DecodeTag(CVector<_BINARY>& vecbiTag, const int iLenDataBits) = 0;

	// This function must return the name of the tag item that this decoder decodes.
	virtual std::string GetTagName(void) = 0;
	virtual ~CTagItemDecoder() {}

	CTagItemDecoder() : bIsReady(false) {};
	// initialise any internal state variables. TODO: Make this pure to force implementer to think?
	virtual void Init(void) {bIsReady = false;}

	virtual bool IsReady(void) {return bIsReady;}

protected:
	void SetReady(bool bReady) {bIsReady = bReady;}

private:
	bool bIsReady;

};

// RSCI Status
class CTagItemDecoderRSI : public CTagItemDecoder
{
public:
    CTagItemDecoderRSI(CParameter* pP, const std::string& s) : pParameter(pP), tag(s) {}
    void SetParameterPtr(CParameter *pP) {pParameter = pP;}
    virtual std::string GetTagName() { return tag; }
protected:

    _REAL decodeDb(CVector<_BINARY>& vecbiTag);
    CParameter *pParameter;
    std::string tag;
};

#endif
