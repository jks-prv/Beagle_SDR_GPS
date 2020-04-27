/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2004
 *
 * Author(s):
 *	Volker Fischer, Oliver Haffenden
 *
 * Description:
 *	Implementation of CPacketSocket interface that wraps up a Socket. See PacketSocket.cpp.
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

#ifndef PACKET_SOCKET_H_INCLUDED
#define PACKET_SOCKET_H_INCLUDED

#ifdef _WIN32
# include <winsock2.h>
#else
  typedef int SOCKET;
# include <netinet/in.h>
#endif


/* Maximum number of bytes received from the network interface. Maximum data
   rate of DRM is approx. 80 kbps. One MDI packet must be sent each DRM frame
   which is every 400 ms -> 0.4 * 80000 / 8 = 4000 bytes. Allocating more than
   double of this size should be ok for all possible cases */
#define MAX_SIZE_BYTES_NETW_BUF		10000

#include "PacketInOut.h"

class CPacketSocketNative :
	public CPacketSocket
{
public:
	CPacketSocketNative();
	virtual ~CPacketSocketNative();
	// Set the sink which will receive the packets
	virtual void SetPacketSink(CPacketSink *pSink);
	// Stop sending packets to the sink
	virtual void ResetPacketSink(void);

	// Send packet to the socket
	void SendPacket(const std::vector<_BYTE>& vecbydata, uint32_t addr=0, uint16_t port=0);

	virtual bool SetDestination(const std::string& str);
	virtual bool SetOrigin(const std::string& str);

	virtual bool GetDestination(std::string& str);
	virtual bool GetOrigin(std::string& str);

	void poll();

private:
	void pollStream();
	void pollDatagram();

    std::vector<std::string> parseDest(const std::string & strNewAddr);
	CPacketSink *pPacketSink;

	sockaddr_in sourceAddr, destAddr;
	sockaddr_in HostAddrOut;
	std::vector<_BYTE>	writeBuf;
	bool udp;
	SOCKET s;
	std::string origin, dest;
};
#endif
