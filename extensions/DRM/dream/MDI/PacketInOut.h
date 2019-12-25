/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2004
 *
 * Author(s):
 *	Volker Fischer, Oliver Haffenden
 *
 * Description:
 *	This defines some pure abstract base classes. A CPacketSink is anything that can receive a
 *  packet, while a CPacketSource is anything that can produce packets.
 *  It's intended to be used for DCP packets although it could be used for IPIO as well.
 *
 *  A CPacketSocket is both a source and a sink, and is used to wrap up a concrete socket
 *  implementation such as QSocket or CSocket.
 *
 *  The intention is that this could be expanded to support RS232 and file as sources and sinks
 *  of DCP packets. The File version should support the FF (file framing) layer of DCP,
 *  by defining a new CTagPacketDecoder subclass and CTagItemDecoder subclasses.
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

#ifndef PACKET_IN_OUT_H_INCLUDED
#define PACKET_IN_OUT_H_INCLUDED

#include "../GlobalDefinitions.h"
#include "../util/Vector.h"


// Pure abstract class (interface) representing a sink of packets (e.g. DCP, IPIO)
class CPacketSink
{
public:
	virtual ~CPacketSink() {}
	virtual void SendPacket(const std::vector<_BYTE>& vecbydata, uint32_t addr=0, uint16_t port=0) = 0;
	virtual bool SetDestination(const std::string& str) = 0;
	virtual bool GetDestination(std::string& str) = 0;
};


// Pure abstract class (interface) representing a source of packets (e.g. DCP, IPIO)

class CPacketSource
{
public:
	virtual ~CPacketSource() {}
	// Set the sink which will receive the packets
	virtual void SetPacketSink(CPacketSink *pSink) = 0;
	// Stop sending packets to the sink
	virtual void ResetPacketSink() = 0;
	virtual bool SetOrigin(const std::string& str) = 0;
	virtual bool GetOrigin(std::string& str) = 0;
	virtual void poll() = 0;
};


// Pure abstract class representing a socket (which can send and receive)
// used to wrap up QT or Windows socket
// Multiple inheritance is ok because both parents are pure abstract (interfaces). Discuss.
class CPacketSocket : public CPacketSink, public CPacketSource
{
public:

};


#endif
