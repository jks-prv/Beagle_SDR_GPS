/******************************************************************************\
 * British Broadcasting Corporation
 * Copyright (c) 2007
 *
 * Author(s):
 *	Julian Cable, Oliver Haffenden
 *
 * Description:
 *
 * see PacketSourceFile.h
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

#include "PacketSourceFile.h"
#include <iostream>
#include <cstdlib>
#include <cerrno>

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

#include "../util/Pacer.h"

#ifdef HAVE_LIBPCAP
# include <pcap.h>
#endif

using namespace std;

const size_t iMaxPacketSize = 4096;
const size_t iAFHeaderLen = 10;
const size_t iAFCRCLen = 2;

CPacketSourceFile::CPacketSourceFile():pPacketSink(nullptr),
    last_packet_time(0),pacer(nullptr),
    pF(nullptr), wanted_dest_port(-1), eFileType(pcap)
{
    pacer = new CPacer(400000000ULL);
}

void CPacketSourceFile::poll()
{
    pacer->wait();
    vector<_BYTE> vecbydata (iMaxPacketSize);
    int interval;
    if(pF)
    {
        vecbydata.resize(0); // in case we don't find anything
        switch(eFileType)
        {
        case pcap:
            readPcap(vecbydata, interval);
            break;
        case ff:
            readFF(vecbydata, interval);
            break;
        case af:
            readRawAF(vecbydata, interval);
            break;
        case pf:
            readRawPFT(vecbydata, interval);
            break;
        }

        /* Decode the incoming packet */
        if (pPacketSink != nullptr)
            pPacketSink->SendPacket(vecbydata);
    }
}

bool
CPacketSourceFile::SetOrigin(const string& origin)
{
    string str = origin;
    size_t p = str.find_last_of('#');
    if(p!=string::npos)
    {
        wanted_dest_port = atoi(str.substr(p+1).c_str());
        str = str.substr(0, p);
    }
    if(str.rfind(".pcap") == str.length()-5)
    {
#ifdef HAVE_LIBPCAP
        char errbuf[PCAP_ERRBUF_SIZE];
        pF = pcap_open_offline(str.c_str(), errbuf);
#endif
        eFileType = pcap;
    }
    else
    {
        pF = fopen(str.c_str(), "rb");
        if ( pF != nullptr)
        {
            char c;
            size_t n = fread(&c, sizeof(c), 1, (FILE *) pF);
            (void)n;
            fseek ( (FILE *) pF , 0 , SEEK_SET );
            if(c=='A') eFileType = af;
            if(c=='P') eFileType = pf;
            if(c=='f') eFileType = ff;
        }
    }
    return pF != nullptr;
}

CPacketSourceFile::~CPacketSourceFile()
{
    if(eFileType != pcap && pF)
        fclose((FILE*)pF);
    else if(pF)
    {
#ifdef HAVE_LIBPCAP
        pcap_close((pcap_t*)pF);
#endif
    }
    pF = 0;
}

// Set the sink which will receive the packets
void
CPacketSourceFile::SetPacketSink(CPacketSink * pSink)
{
    pPacketSink = pSink;
}

// Stop sending packets to the sink
void
CPacketSourceFile::ResetPacketSink()
{
    pPacketSink = nullptr;
}

void
CPacketSourceFile::readTagPacketHeader(string& tag, uint32_t& len)
{
    uint32_t bytes;
    size_t n;

    tag = "";
    for(int i=0; i<4; i++)
    {
        char c;
        n = fread(&c, sizeof(char), 1, (FILE *) pF);
        tag += c;
    }

    n = fread(&bytes, sizeof(bytes), 1, (FILE *) pF);
    len = ntohl(bytes)/8;

    (void)n;
}

void
CPacketSourceFile::readFF(vector<_BYTE>& vecbydata, int& interval)
{

    string tag;
    uint32_t fflen,len;
    readTagPacketHeader(tag, fflen);

    interval = 400;

    if(tag != "fio_")
    {
        fclose((FILE *) pF);
        pF = 0;
        return;
    }

    long remaining = fflen; // use a signed number here to help loop exit
    while(remaining>0)
    {
        readTagPacketHeader(tag, len);
        remaining -= 8; // 4 bytes tag, 4 bytes length;
        remaining -= len;

        if(tag=="time")
        {
            if(len != 8)
            {
                cout << "weird length in FF " << int(len) << " expected 8" << endl;
                fclose((FILE *) pF);
                pF = 0;
                return;
            }
            // read the time tag packet payload
            uint32_t s,ns;
            size_t n = fread(&s, sizeof(s), 1, (FILE *) pF);
            n = fread(&ns, sizeof(ns), 1, (FILE *) pF);
            (void)n;
            //TODO update last packet and interval times
            readTagPacketHeader(tag, len);
            remaining -= 8; // 4 bytes tag, 4 bytes length;
            remaining -= len;
        }

        if(tag=="afpf")
        {
            uint32_t l = readRawAF(vecbydata, interval);
            if(l!=len) {
                cout << "CPacketSourceFile::readFF why not" << endl;
            }
        }
        else
        {
            cout << "unkown tag packet in FF " << tag << endl;
            fseek((FILE*)pF, len, SEEK_CUR);
        }
    }
}

int
CPacketSourceFile::readRawAF(vector<_BYTE>& vecbydata, int& interval)
{
    char sync[2];
    uint32_t bytes;

    interval = 400;

    size_t n = fread(sync, 2, 1, (FILE *) pF);
    n = fread(&bytes, 4, 1, (FILE *) pF);
    fseek((FILE*)pF, -6, SEEK_CUR);

    if (sync[0] != 'A' || sync[1] != 'F')
    {
        // throw?
        fclose((FILE *) pF);
        pF = 0;
        return 0;
    }
    // get the length
    size_t iAFPacketLen = iAFHeaderLen + ntohl(bytes) + iAFCRCLen;

    if (iAFPacketLen > iMaxPacketSize)
    {
        // throw?
        fclose((FILE *) pF);
        pF = 0;
        return 0;
    }

    // initialise the output vector
    vecbydata.resize(iAFPacketLen);

    n = fread(&vecbydata[0], 1, iAFPacketLen, (FILE *)pF);

    last_packet_time += interval;

    return n;
}

// not robust against the sync characters appearing in the payload!!!!
void
CPacketSourceFile::readRawPFT(vector<_BYTE>& vecbydata, int& interval)
{
    char sync[2];

    interval = 400;

    size_t n = fread(sync, 2, 1, (FILE *) pF);
    if (sync[0] != 'P' || sync[1] != 'F')
    {
        // throw?
        fclose((FILE *) pF);
        pF = 0;
        return;
    }
    vecbydata.resize(0);
    vecbydata.push_back(sync[0]);
    vecbydata.push_back(sync[1]);
    char prev = '\0';;
    while(true)
    {
        char c;
        n = fread(&c, 1, 1, (FILE *) pF);
        if(prev == 'P' && c == 'F')
        {
            fseek((FILE*)pF, -2, SEEK_CUR);
            vecbydata.pop_back();
            break;
        }
        prev = c;
        vecbydata.push_back(c);
    }

    (void)n;
}

void
CPacketSourceFile::readPcap(vector<_BYTE>& vecbydata, int& interval)
{
    int link_len = 0;
    const _BYTE* pkt_data = nullptr;
    timeval packet_time = { 0, 0 };
    while(true)
    {
#ifdef HAVE_LIBPCAP
        struct pcap_pkthdr *header;
        int res;
        const u_char* data;
        /* Retrieve the packet from the file */
        if((res = pcap_next_ex( (pcap_t*)pF, &header, &data)) != 1)
        {
            pcap_close((pcap_t*)pF);
            pF = nullptr;
            return;
        }
        int lt = pcap_datalink((pcap_t*)pF);
        pkt_data = (_BYTE*)data;
        /* 14 bytes ethernet header */
        if(lt==DLT_EN10MB)
        {
            link_len=14;
        }
#ifdef DLT_LINUX_SLL
        /* linux cooked */
        if(lt==DLT_LINUX_SLL)
        {
            link_len=16;
        }
#endif
        /* raw IP header ? */
        if(lt==DLT_RAW)
        {
            link_len=0;
        }
        //packet_time = header->ts; try this for BSD
		packet_time.tv_sec = header->ts.tv_sec;
		packet_time.tv_usec = header->ts.tv_usec;
#endif
        if(pkt_data == nullptr)
            return;

        /* 4n bytes IP header, 8 bytes UDP header */
        uint8_t proto = pkt_data[link_len+9];
        if(proto == 0x11) // UDP
        {
            int udp_ip_hdr_len = 4*(pkt_data[link_len] & 0x0f) + 8;
            int ip_packet_len = ntohs(*(uint16_t*)&pkt_data[link_len+2]);
            int dest_port = ntohs(*(uint16_t*)&pkt_data[link_len+udp_ip_hdr_len-6]);
            if((wanted_dest_port==-1) || (dest_port == wanted_dest_port)) // wanted port
            {
                int data_len = ip_packet_len - udp_ip_hdr_len;
                vecbydata.resize (data_len);
                for(int i=0; i<data_len; i++)
                    vecbydata[i] = pkt_data[link_len+udp_ip_hdr_len+i];
                break;
            }
        }
    }
    if(last_packet_time == 0)
    {
        last_packet_time = 1000*uint64_t(packet_time.tv_sec);
        last_packet_time += uint64_t(packet_time.tv_usec)/1000;
    }
    uint64_t pt;
    pt = 1000*uint64_t(packet_time.tv_sec);
    pt += uint64_t(packet_time.tv_usec)/1000;
    interval = pt - last_packet_time;
    last_packet_time = pt;
}
