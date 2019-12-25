/******************************************************************************\
 * British Broadcasting Corporation
 * Copyright (c) 2007
 *
 * Author(s):
 *	Julian Cable
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

#ifndef _TABLE_STATIONS_H
#define _TABLE_STATIONS_H

#include <string>
#include <map>

class CStationData
{
public:
	CStationData();

	std::string eibi_language(const std::string& code);
	std::string itu_r_country(const std::string& code);
	std::string eibi_target(const std::string& code);
	std::string eibi_station(const std::string& country, const std::string& stn);

private:
	std::map<std::string,std::string> l;
	std::map<std::string,std::string> c;
	std::map<std::string,std::string> t;
	std::map<std::string, std::map<std::string,std::string> > s;
};

#endif
