/******************************************************************************\
 * British Broadcasting Corporation
 * Copyright (c) 2006
 *
 * Author(s):
 *	Julian Cable
 *
 * Description:
 *	ETSI DAB/DRM Electronic Programme Guide utilities
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

#include "epgutil.h"
#include <iomanip>
#include <iostream>
#include <sstream>
#ifdef _WIN32
#include <io.h>
#include <direct.h>
#else
#include <sys/stat.h>
#endif
using namespace std;

void
mkdirs (const string & path)
{
    /* forward slashes only, since this is a MOT ContentName */
    size_t n = path.find_last_of ('/');
    if (n == string::npos)
	return;
    string left, sep = "";
    for (size_t p = 0; p < n;)
      {
	  size_t q = path.find ('/', p);
	  if (q == string::npos)
	      return;
	  string dir = path.substr (p, q - p);
	  left += sep + dir;
#ifdef _WIN32
	  sep = "\\";
	  _mkdir (left.c_str ());
#else
	  sep = "/";
	  mkdir (left.c_str (), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
#endif
	  p = q + 1;
      }
}

// this is the old dream one
string
epgFilename (const CDateAndTime & date, uint32_t sid, int type, bool advanced)
{
    string name;
    ostringstream s (name);
    s << setfill ('0') << setw (4) << date.year;
    s << setw (2) << int (date.month) << setw(2) << int (date.day);
    s << hex << setw (4) << ((unsigned long) sid);
    switch (type)
      {
      case 0:
	  s << 'S';
	  break;
      case 1:
	  s << 'P';
	  break;
      case 2:
	  s << 'G';
	  break;
      }
    if (advanced)
	s << ".EHA";
    else
	s << ".EHB";
    return s.str ();
}

string
epgFilename_etsi (const CDateAndTime & date, uint32_t sid, int type, bool advanced)
{
    (void)type;
    string name;
    ostringstream s (name);
    s << "w" << setfill ('0') << setw (4) << date.year;
    s << setw (2) << int (date.month) << setw(2) << int (date.day);
    s << "d" << hex << setw (4) << ((unsigned long) sid);
    if (advanced)
	s << ".EHA";
    else
	s << ".EHB";
    return s.str ();
}

string
epgFilename_dab (const CDateAndTime & date, uint32_t sid, int type, bool advanced)
{
    (void)type;
    string name;
    ostringstream s (name);
    s << "w" << setfill ('0') << setw (4) << date.year;
    s << setw (2) << int (date.month) << setw(2) << int (date.day);
    s << "d" << hex << setw (6) << ((unsigned long) sid);
    if (advanced)
	s << ".EHA";
    else
	s << ".EHB";
    return s.str ();
}
