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

#ifndef FILETYPER_H
#define FILETYPER_H

#include <string>

class FileTyper
{
public:
    enum type { unrecognised, pcap, file_framing, raw_af, raw_pft, pcm, pipe };
    FileTyper();
    static type resolve(const std::string& s);
    static bool is_rxstat(type t) { return (t != pcm) && (t != unrecognised); }
};

#endif // FILETYPER_H
