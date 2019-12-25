/******************************************************************************\
 * British Broadcasting Corporation
 * Copyright (c) 2007
 *
 * Author(s):
 *	Oliver Haffenden, Julian Cable
 *
 * Description:
 *
 * See PacketSinkFile.h
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

#include "PacketSinkFile.h"

/* include this here mostly for htonl */
#ifdef _WIN32
  /* Always include winsock2.h before windows.h */
    /* winsock2.h is already included into libpcap */
# include <winsock2.h>
# include <ws2tcpip.h>
# include <windows.h>
#else
# include <netinet/in.h>
# include <arpa/inet.h>
/* Some defines needed for compatibility when using Linux, Darwin, ... */
typedef int SOCKET;
# define SOCKET_ERROR				(-1)
# define INVALID_SOCKET				(-1)
#endif

#ifdef HAVE_LIBPCAP
# include <pcap.h>
#endif

using namespace std;

CPacketSinkFile::CPacketSinkFile()
: pFile(0), bIsRecording(0), bChangeReceived(false)
{
}

bool CPacketSinkFile::SetDestination(const string& strFName)
{
	strFileName = strFName;
	bChangeReceived = true;
	return true;
}

void CPacketSinkFile::StartRecording()
{
	bIsRecording = true;
	bChangeReceived = true;
}

void CPacketSinkFile::StopRecording()
{
	bIsRecording = false;
	bChangeReceived = true;
}

void CPacketSinkFile::SendPacket(const vector<_BYTE>& vecbydata, uint32_t, uint16_t)
{
	if (bChangeReceived) // something has changed, so close the file if it's open
	{
        if (pFile)
        {
			close();
		}
        pFile = 0;
		bChangeReceived = false;
	}

    if (!bIsRecording) // not recording
    {
            if (pFile != 0) // close file if one is open
            {
                    close();
                    pFile = 0;
            }
            return;
    }

    if (!pFile) // either wasn't open, or we just closed it
    {
            open();
            if (!pFile)
          {
                    // Failed to open
                    bIsRecording = false;
                    return;
            }
    }

	write(vecbydata);
}

CPacketSinkRawFile::CPacketSinkRawFile():CPacketSinkFile() {}

CPacketSinkRawFile::~CPacketSinkRawFile()
{
	if(pFile)
		fclose(pFile);
}

void
CPacketSinkRawFile::open()
{
	pFile = fopen(strFileName.c_str(), "wb");
}

void
CPacketSinkRawFile::close()
{
	fclose(pFile);
}

void
CPacketSinkRawFile::write(const vector<_BYTE>& vecbydata)
{
	fwrite(&vecbydata[0],1,vecbydata.size(),pFile);
}

/* File: ETSI TS 102 821 V0.0.2f (2003-10)

PFT Fragments or AF Packets may be stored in a file for offline distribution,
  archiving or any other purpose.  A standard mapping has been defined using
   the Hierarchical TAG Item option available,  however other mappings may be
    (or this one extended) in the future.  The top level TAG Item has the TAG
    Name fio_ and is used to encapsulate a TAG packet,  one part of which is
    the AF Packet or PFT Fragment in an afpf TAG Item.  Additional TAG Items
    have been defined to monitor the reception or control the replay of the packets.

B.3.1	File IO (fio_)
The fio_ TAG Item is the highest layer TAG Item in the file TAG Item hierarchy.

Tag name: fio_
Tag length: 8*n bits
Tag Value: This TAG Item acts as a container for an afpf TAG Item.  A time TAG Item
may optionally be present.

B.3.1.1	AF Packet / PFT Fragment (afpf)
The afpf TAG Item contains an entire AF Packet or PFT Fragment as the TAG Value.

Figure 16: AF Packet or PFT Fragment

B.3.1.2	Timestamp (time)
The time TAG Item may occur in the payload of any fio_ TAG Item.
It may record the time of reception of the payload,  or it may indicate
the intended time of replay.  The time value given in the TI_SEC and TI_NSEC
 fields may be relative to the start of the file,  or any other reference desired.

Figure 17: File timestamp
TI_SEC:  the number of whole SI seconds,  in the range 0-2^32-1)
TI_NSEC:  the number of whole SI nanoseconds,  in the range 0-999 999 999.
  Values outside of this range are not defined
*/

CPacketSinkFileFraming::CPacketSinkFileFraming():CPacketSinkFile() {}

CPacketSinkFileFraming::~CPacketSinkFileFraming()
{
	if(pFile)
		fclose(pFile);
}

void
CPacketSinkFileFraming::open()
{
	pFile = fopen(strFileName.c_str(), "wb");
}

void
CPacketSinkFileFraming::close()
{
	fclose(pFile);
}

void
CPacketSinkFileFraming::write(const vector<_BYTE>& vecbydata)
{
	uint32_t p, n;
	n = 4+4+vecbydata.size(); // afpf
	// Tag Item fio_
	fwrite("fio_",4,1,pFile);
	p = htonl(8*n);
    fwrite(&p,4,1,pFile);
	// nested tag packets
	// Tag Item afpf
	fwrite("afpf",4,1,pFile);
	n = vecbydata.size();
	p = htonl(8*n);
    fwrite(&p,4,1,pFile);
	fwrite(&vecbydata[0], 1, n, pFile);
}

CPacketSinkPcapFile::CPacketSinkPcapFile():CPacketSinkFile() {}

CPacketSinkPcapFile::~CPacketSinkPcapFile()
{
#ifdef HAVE_LIBPCAP
	if(pFile)
    	pcap_dump_close((pcap_dumper_t *)pFile);
#endif
}

void
CPacketSinkPcapFile::open()
{
#ifdef HAVE_LIBPCAP
    pcap_t *p = pcap_open_dead(DLT_RAW, 65536);
	pFile = (FILE*)pcap_dump_open(p, strFileName.c_str());
#endif
}

void
CPacketSinkPcapFile::close()
{
#ifdef HAVE_LIBPCAP
    pcap_dump_close((pcap_dumper_t *)pFile);
#endif
}

void
CPacketSinkPcapFile::write(const vector<_BYTE>& vecbydata)
{
#ifdef HAVE_LIBPCAP
	vector<_BYTE> out;
	size_t u = vecbydata.size()+8;
	size_t c = u+20;
    // Ip header - fields in network byte order
	out.push_back(0x45);
	out.push_back(0x00);
    out.push_back(c >> 8);
    out.push_back(c & 0xff);
    out.push_back(0x08);
    out.push_back(0xd8);
    out.push_back(0x40);
    out.push_back(0x00);
    out.push_back(0x7f);
    out.push_back(0x11); // udp
	out.insert(out.end(), 10, 0);
    // udp header - fields in network byte order
    out.push_back(0);
    out.push_back(0);
    out.push_back(4);
    out.push_back(0);
    out.push_back(u >> 8);
    out.push_back(u & 0xff);
    out.push_back(0); out.push_back(0); // TODO UDP Header CRC
	out.insert(out.end(), vecbydata.begin(), vecbydata.end());
    pcap_pkthdr hdr;
	time_t t;
	time(&t);
    hdr.ts.tv_sec = t;
    hdr.ts.tv_usec = 0; /* TODO more precise timestamps */
	hdr.caplen = c;
	hdr.len = c;
    pcap_dump((u_char*)pFile, &hdr, (u_char*)&out[0]);
    pcap_dump_flush((pcap_dumper_t *)pFile);
#else
	(void)vecbydata;
#endif
}

