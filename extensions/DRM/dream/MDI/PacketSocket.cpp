/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright(c) 2004
 *
 * Author(s):
 *	Volker Fischer, Julian Cable, Oliver Haffenden
 *
 * Description:
 *
 * This is an implementation of the CPacketSocket interface that wraps up a socket.
 *
 *
 ******************************************************************************
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or(at your option) any later
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

/*
--rsiout 127.0.0.1:60023 --rciin 60021 --rsioutprofile D

if I run it with the command:
--rsiout 127.0.0.1:60023 --rciin 60021 --rsiout 127.0.0.1:60022 --rciin 60021 --rsioutprofile D
*/

#include "PacketSocket.h"
#include <iostream>
#include <sstream>
#include <cerrno>
#include <cctype>
#include <cstring>
#include <stdlib.h> /* for atol() */

#ifdef _WIN32
/* Always include winsock2.h before windows.h */
# include <Ws2tcpip.h>
# include <windows.h>
inline int inet_aton(const char*s, void * a) {
    ((in_addr*)a)->s_addr = inet_addr(s);
    return 1;
}
# define inet_pton(a, b, c) inet_aton(b, c)
# define inet_ntop(a, b, c, d) inet_ntoa(*(in_addr*)b)
#else
# include <arpa/inet.h>
# include <sys/types.h>
# include <sys/socket.h>
# include <unistd.h>
# include <fcntl.h>
# define SOCKET_ERROR				(-1)
# define INVALID_SOCKET				(-1)
#endif

using namespace std;

CPacketSocketNative::CPacketSocketNative():
        pPacketSink(nullptr), HostAddrOut(),
        writeBuf(),udp(true),
        s(INVALID_SOCKET), origin(""), dest("")
{
	memset(&sourceAddr, 0, sizeof(sourceAddr));
	memset(&destAddr, 0, sizeof(destAddr));
	memset(&HostAddrOut, 0, sizeof(HostAddrOut));
}

CPacketSocketNative::~CPacketSocketNative()
{
}

// Set the sink which will receive the packets
void
CPacketSocketNative::SetPacketSink(CPacketSink * pSink)
{
    pPacketSink = pSink;
}

// Stop sending packets to the sink
void
CPacketSocketNative::ResetPacketSink()
{
    pPacketSink = nullptr;
}

// Send packet to the socket
void
CPacketSocketNative::SendPacket(const vector < _BYTE > &vecbydata, uint32_t, uint16_t)
{
    if (s == INVALID_SOCKET)
        return;
    if (udp)
    {
        string ss;
        GetDestination(ss);
        cerr << "send packet " << ss << endl;
        int n = sendto(s, (char*)&vecbydata[0], vecbydata.size(), 0, (sockaddr*)&HostAddrOut, sizeof(HostAddrOut));
		if(n==SOCKET_ERROR) {
#ifdef _WIN32
			int err = GetLastError();
			cerr << "socket send failed " << err << endl;
#endif
		}
    }
    else
        (void)send(s, (char*)&vecbydata[0], vecbydata.size(), 0);
}

vector<string>
CPacketSocketNative::parseDest(const string& input)
{
    istringstream iss(input);
    string item;
    vector<string> v;
    for (string item; getline(iss, item, ':'); )
    {
        v.push_back(item);
    }
    return v;
}

bool
CPacketSocketNative::SetDestination(const string & strNewAddr)
{
	dest = strNewAddr;
    /* syntax
       1:  <port>                send to port on localhost
       2:  <ip>:<port>           send to port on host or port on m/c group
       3:  <ip>:<ip>:<port>      send to port on m/c group via interface
       prefix with "t" for tcp
     */
    int ttl = 127;
    bool bAddressOK = true;
    in_addr AddrInterface;
    AddrInterface.s_addr = htonl(INADDR_ANY);
    vector<string> parts = parseDest(strNewAddr);
	HostAddrOut.sin_family = AF_INET; 
    if (tolower(parts[0][0])=='t')
    {
        udp = false;
        parts[0] = parts[0].substr(1);
    }
    switch (parts.size())
    {
    case 1: // Just a port - send to ourselves
        bAddressOK = inet_pton(AF_INET, "127.0.0.1", &HostAddrOut.sin_addr.s_addr);
        HostAddrOut.sin_port = ntohs(atol(parts[0].c_str()));
        break;
    case 2: // host and port, unicast
        bAddressOK = inet_pton(AF_INET, parts[0].c_str(), &HostAddrOut.sin_addr.s_addr);
        HostAddrOut.sin_port = ntohs(atol(parts[1].c_str()));
        {
            string s;
            GetDestination(s);
            cerr << "host and port, unicast " << s << endl;
        }
        break;
    case 3: // interface, host and port, usually multicast udp
        inet_pton(AF_INET, parts[0].c_str(), &AddrInterface.s_addr);
        bAddressOK = inet_pton(AF_INET, parts[1].c_str(), &HostAddrOut.sin_addr.s_addr);
        HostAddrOut.sin_port = ntohs(atol(parts[2].c_str()));
        break;
    default:
        bAddressOK = false;
    }
    if (udp)
    {
        if (s == INVALID_SOCKET)
            s = socket(AF_INET, SOCK_DGRAM, 0);

        if (setsockopt(s, IPPROTO_IP, IP_TTL, (char*)&ttl, sizeof(ttl))==SOCKET_ERROR)
            bAddressOK = false;
        if (AddrInterface.s_addr != htonl(INADDR_ANY))
        {
            if (setsockopt(s, IPPROTO_IP, IP_MULTICAST_IF,
                           (char *) &AddrInterface, sizeof(AddrInterface)) == SOCKET_ERROR)
            {
                bAddressOK = false;
            }
        }
    }
    else
    {
        if (s == INVALID_SOCKET)
            s = socket(AF_INET, SOCK_STREAM, 0);
        int n = connect(s, (sockaddr*)&HostAddrOut, sizeof(HostAddrOut));
        bAddressOK = n==0;
    }
    return bAddressOK;
}

bool
CPacketSocketNative::GetDestination(string & str)
{
    stringstream s;
    char buf[32];
    s << inet_ntop(AF_INET, &HostAddrOut.sin_addr.s_addr, buf, sizeof(buf)) << ":" << ntohs(HostAddrOut.sin_port);
    str = s.str();
    (void)buf; // MSVC2008 warning C4101: 'buf' : unreferenced local variable
    return true;
}

bool
CPacketSocketNative::GetOrigin(string& str)
{
	if(sourceAddr.sin_family == 0)
		return false;
    stringstream s;
    char buf[32];
	(void)buf;
    s << inet_ntop(AF_INET, &sourceAddr.sin_addr.s_addr, buf, sizeof(buf)) << ":" << ntohs(sourceAddr.sin_port);
    str = s.str();
    return true;
}

bool
CPacketSocketNative::SetOrigin(const string & strNewAddr)
{
	origin = strNewAddr;
    /* syntax (unwanted fields can be empty, e.g. <source ip>::<group ip>:<port>
       1:  <port>
       2:  <group ip>:<port>
       3:  <interface ip>:<group ip>:<port>
       4:  <source ip>:<interface ip>:<group ip>:<port>
       5: - for TCP - no need to separately set origin
     */
    if (strNewAddr == "-")
    {
        udp = false;
        if (s == INVALID_SOCKET)
            s = socket(AF_INET, SOCK_STREAM, 0);
        return true;
    }

    if (s == INVALID_SOCKET)
    {
        s = socket(AF_INET, SOCK_DGRAM, 0);
    }

    int port=-1;
    in_addr gp, ifc;
    vector<string> parts = parseDest(strNewAddr);
	sourceAddr.sin_family = AF_INET;
	bool ok=true;
    int p=-1,o=-1,g=-1,i=-1;
    switch (parts.size())
    {
    case 1:
        p=0;
        break;
    case 2:
        p=1;
        g=0;
        break;
    case 3:
        p=2;
        i=0;
        g=1;
        break;
    case 4:
        p=3;
        o=0;
        i=1;
        g=2;
        break;
    default:
        ok = false;
    }

    if (p>=0 && parts[p].length() > 0)
        port = atol(parts[p].c_str());

    if (o>=0 && parts[o].length() > 0)
    {
        inet_pton(AF_INET, parts[o].c_str(), &sourceAddr.sin_addr);
    }
    else
    {
        sourceAddr.sin_addr.s_addr=INADDR_ANY;
    }

    if (i>=0 && parts[i].length() > 0)
        inet_pton(AF_INET, parts[i].c_str(), &ifc.s_addr);
    else
        ifc.s_addr=INADDR_ANY;

    if (g>=0 && parts[g].length() > 0)
    {
        inet_pton(AF_INET, parts[g].c_str(), &gp.s_addr);

        /* Multicast ? */
        uint32_t mc = htonl(0xe0000000);
        if ((gp.s_addr & mc) == mc)	/* multicast! */
        {
            int optval = 1;
            setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char*)&optval, sizeof optval);
            sockaddr_in sa;
            sa.sin_family = AF_INET;
            sa.sin_addr.s_addr = gp.s_addr;
            sa.sin_port = htons(port);
            ::bind(s, (sockaddr*)&sa, sizeof(sa));
            if (ok == false)
            {
                throw CGenErr("Can't bind to port to receive packets");
            }
            struct ip_mreq mreq;
            mreq.imr_multiaddr.s_addr = gp.s_addr;
            mreq.imr_interface.s_addr = ifc.s_addr;
            int n = setsockopt(s, IPPROTO_IP, IP_ADD_MEMBERSHIP,(char *) &mreq, sizeof(mreq));
            if (n == SOCKET_ERROR)
                ok = false;
            if (!ok)
            {
                cerr << "Can't join multicast group" << endl;
            }
        }
        else /* one address specified, but not multicast - listen on a specific interface */
        {
            cerr << "listen on interface " << endl;
            sockaddr_in sa;
            sa.sin_family = AF_INET;
            sa.sin_addr.s_addr = gp.s_addr;
            sa.sin_port = htons(port);
            ::bind(s, (sockaddr*)&sa, sizeof(sa));
        }
    }
    else
    {
        /* bind to a port on any interface. */
        sourceAddr.sin_family = AF_INET;
        sourceAddr.sin_addr.s_addr = htonl(INADDR_ANY);
        sourceAddr.sin_port = htons(port);
        int r = ::bind(s, (sockaddr*)&sourceAddr, sizeof(sourceAddr));
        if (r < 0)
        {
            perror("bind() failed");
        }
    }
#ifdef _WIN32
    u_long mode = 1;
    (void)ioctlsocket(s, FIONBIO, &mode);
#else
    fcntl(s, F_SETFL, O_NONBLOCK);  // set to non-blocking
#endif
    return true;
}

void
CPacketSocketNative::poll()
{
    if (udp)
        pollDatagram();
    else
        pollStream();
}

void
CPacketSocketNative::pollStream()
{
    vector < _BYTE > vecbydata(MAX_SIZE_BYTES_NETW_BUF);
    /* Read block from network interface */
    int iNumBytesRead = ::recv(s, (char *) &vecbydata[0], MAX_SIZE_BYTES_NETW_BUF, 0);
    if (iNumBytesRead > 0)
    {
        /* Decode the incoming packet */
        if (pPacketSink != nullptr)
        {
            vecbydata.resize(iNumBytesRead);
            // TODO - is there any reason or possibility to optionally filter on source address?
            pPacketSink->SendPacket(vecbydata, 0, 0);
        }
    }
}

void
CPacketSocketNative::pollDatagram()
{
    vector < _BYTE > vecbydata(MAX_SIZE_BYTES_NETW_BUF);
    int readBytes = 0;
	{
		stringstream s;
		char buf[32];
		(void)buf;
		s << "poll src: " << inet_ntop(AF_INET, &sourceAddr.sin_addr.s_addr, buf, sizeof(buf))
		  << ":" << ntohs(sourceAddr.sin_port)
		  << " dst: " << inet_ntop(AF_INET, &HostAddrOut.sin_addr.s_addr, buf, sizeof(buf)) 
		  << ":" << ntohs(HostAddrOut.sin_port);
		//qDebug(s.str().c_str());
	}

    do {
        sockaddr_in sender;
        socklen_t l = sizeof(sender);
        readBytes = ::recvfrom(s, (char*)&vecbydata[0], MAX_SIZE_BYTES_NETW_BUF, 0, (sockaddr*)&sender, &l);
        if (readBytes>0) {
			{
				static int n=0;
				stringstream s;
				char buf[32];
				(void)buf;
				s << (n++) << " got from: " << inet_ntop(AF_INET, &sender.sin_addr.s_addr, buf, sizeof(buf))
				  << ":" << ntohs(sender.sin_port);
				//qDebug(s.str().c_str());
			}
            vecbydata.resize(readBytes);
            if (sourceAddr.sin_addr.s_addr == htonl(INADDR_ANY))
            {
                pPacketSink->SendPacket(vecbydata, sender.sin_addr.s_addr, sender.sin_port);
            }
            else
            {
                if (sourceAddr.sin_addr.s_addr == sender.sin_addr.s_addr) // optionally filter on source address
                {
                    pPacketSink->SendPacket(vecbydata, sender.sin_addr.s_addr, sender.sin_port);
                }
            }
        }
    } while (readBytes>0);
}
