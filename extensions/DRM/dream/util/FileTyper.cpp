/******************************************************************************\
 * Copyright (c) 2013
 *
 * Author(s):
 *	Julian Cable
 *
 * Description:
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

#include "FileTyper.h"

#include "sys/stat.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifdef HAVE_LIBSNDFILE
# include <sndfile.h>
#endif

#ifdef HAVE_LIBPCAP
# include <cstdio>
# include <pcap.h>
#endif

using namespace std;

FileTyper::FileTyper()
{
}

FileTyper::type FileTyper::resolve(const string& str)
{
    string s;
    s = str;
    string ext;
    size_t p = str.rfind('.');
    if (p != string::npos)
        ext = s.substr(p+1);
    if (ext.substr(0,3) == "dat") {
        return pcm;
    }

    // allow parameters at end of string starting with a #
    size_t n = str.find_last_of('#');
    if(n==string::npos)
        s = str;
    else
        s = str.substr(0, n);

    struct stat st;
    int rv = stat(s.c_str(), &st);
    if (rv == 0 && st.st_mode & S_IFIFO) {
        return pipe;
    }

#ifdef HAVE_LIBSNDFILE
    SF_INFO info;
    info.format=0;
    SNDFILE* fd = sf_open(s.c_str(), SFM_READ, &info);
    if(fd)
    {
        sf_close(fd);
        return pcm;
    }
#endif
#ifdef HAVE_LIBPCAP
    char errbuf[PCAP_ERRBUF_SIZE];
    pcap_t* pF = pcap_open_offline(s.c_str(), errbuf);
    if(pF)
    {
        pcap_close(pF);
        return pcap;
    }
#endif
    FILE* f = fopen(s.c_str(), "rb");
    if (f)
    {
        char c[5];
        size_t r = fread(&c, 1, 4, f); (void)r;
        fclose(f);
        //printf("FileTyper NOT SND %s %4s %u %u %u %u\n", s.c_str(), c,
        //    (unsigned char)c[0], (unsigned char)c[1], (unsigned char)c[2], (unsigned char)c[3]);
        c[4]='\0';
        if(strcmp(c, "fio_")==0) return file_framing;
        if(strcmp(c, "RIFF")==0) return pcm;
        c[2]='\0';
        if(strcmp(c, "AF")==0) return raw_af;
        if(strcmp(c, "PF")==0) return raw_pft;
    }
    
    return unrecognised;
}
