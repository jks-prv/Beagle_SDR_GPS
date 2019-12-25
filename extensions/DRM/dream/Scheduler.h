/******************************************************************************\
 * British Broadcasting Corporation
 * Copyright (c) 2012
 *
 * Author(s):
 *      Julian Cable
 *
 * Description:
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

#ifndef __SCHEDULER_H
#define __SCHEDULER_H

#include <string>
#include <map>
#include <queue>
#include "util/Settings.h"

using namespace std;

class CScheduler
{
public:
	struct SEvent { time_t time; int frequency; };
	CScheduler(bool test=false):schedule(),events(),iniFile(),testMode(test){}
	bool LoadSchedule(const std::string& filename);
	bool empty() const;
	SEvent front(); // get next event 
	SEvent pop(); // remove first event from queue
private:
	map<time_t,int> schedule; // map seconds from start of day to schedule event, frequency or -1 for off
	queue<SEvent> events;
	CIniFile iniFile;
	bool testMode;
	void fill();
	void before();
	int parse(std::string);
	std::string format(time_t);
	std::string format(const struct tm&);
};

#if 0
int dprintf(const char *format, ...);
#endif

#endif
