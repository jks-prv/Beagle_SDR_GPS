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

#include "Scheduler.h"
#include <iostream>
#include <sstream>
#include <cstdlib>
#include <ctime>
#ifdef _WIN32
# include <windows.h>
#endif

#ifdef _WIN32
# include "windows/platform_util.h"
#endif
#ifdef __ANDROID_API__
# include "android/platform_util.h"
#endif

// get next event
CScheduler::SEvent CScheduler::front()
{
	if(events.empty())
	{
		fill();
	}
	if(testMode)
		cerr << format(events.front().time) << " " << events.front().frequency << endl;
	return events.front();
}

// remove first event from queue and return the front
CScheduler::SEvent CScheduler::pop()
{
	events.pop();
	return front();
}

bool CScheduler::empty() const
{
	return events.empty();
}

bool CScheduler::LoadSchedule(const string& filename)
{
	bool ok = iniFile.LoadIni(filename.c_str());
	if(!ok)
		return false;

	if(testMode)
		before();
	for(int i=1; i<99; i++)
	{
		ostringstream ss;
		ss << "Freq" << i;
		string f = iniFile.GetIniSetting("Settings", ss.str());
		ss.str("");
		ss << "StartTime" << i;
		string starttime = iniFile.GetIniSetting("Settings", ss.str());
		ss.str("");
		ss << "EndTime" << i;
		string endtime = iniFile.GetIniSetting("Settings", ss.str());
		if(starttime == endtime)
			break;
		time_t start = parse(starttime);
		time_t end = parse(endtime);
		schedule[start] = int(atol(f.c_str()));
		schedule[end] = -1;
	}
	fill();
	return true;
}

void CScheduler::fill()
{
	time_t dt = time(nullptr); // daytime
	struct tm dts;
	dts = *gmtime(&dt);
	dts.tm_hour = 0;
	dts.tm_min = 0;
	dts.tm_sec = 0;
	time_t sod = timegm(&dts); // start of daytime
	// resolve schedule to absolute time
	map<time_t,int> abs_sched;
	map<time_t,int>::const_iterator i;
	for(i = schedule.begin(); i != schedule.end(); i++)
	{
		time_t st = sod + i->first;
		if (st < dt)
			st += 24 * 60 * 60; // want tomorrow's.
		abs_sched[st] = i->second;
	}
	for (i = abs_sched.begin(); i != abs_sched.end(); i++)
	{
		SEvent e;
		e.time = i->first;
		e.frequency = i->second;
		events.push(e);
	}
}

int CScheduler::parse(string s)
{
	int hh,mm,ss;
	char c;
	istringstream iss(s);
	iss >> hh >> c >> mm >> c >> ss;
	return 60*(mm + 60*hh)+ss;
}

string CScheduler::format(time_t t)
{
	struct tm g = *gmtime(&t);
	return format(g);
}

string CScheduler::format(const struct tm& g)
{
	ostringstream ss;
	if(g.tm_hour < 10)
		ss << '0';
	ss << g.tm_hour << ":";
	if(g.tm_min < 10)
		ss << '0';
	ss << g.tm_min << ":" ;
	if(g.tm_sec < 10)
		ss << '0';
	ss << g.tm_sec;
	return ss.str();
}

void CScheduler::before()
{
	time_t t = time(nullptr);
	t += 10;
	iniFile.PutIniSetting("Settings", "StartTime1", format(t));
	t += 30;
	iniFile.PutIniSetting("Settings", "EndTime1", format(t));
	iniFile.PutIniSetting("Settings", "StartTime2", format(t));
	t += 30;
	iniFile.PutIniSetting("Settings", "EndTime2", format(t));
	cerr << "CScheduler::before()" << endl;
}
