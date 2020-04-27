/******************************************************************************\
 * British Broadcasting Corporation
 * Copyright (c) 2007
 *
 * Author(s):
 *	Julian Cable
 *
 * Description:
 *	dummy sound classes
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

#ifndef SOUNDNULL_H
#define SOUNDNULL_H

#include "soundinterface.h"

/* Classes ********************************************************************/
class CSoundInNull : public CSoundInInterface
{
public:
    CSoundInNull() {}
    virtual ~CSoundInNull();
    virtual bool	Init(int, int, bool) {
        return true;
    }
    virtual bool	Read(CVector<short>&) {
        return false;
    }
    virtual void		Enumerate(std::vector<std::string>&choices, std::vector<std::string>&) {
        choices.push_back("(File or Network)");
    }
    virtual std::string		GetDev() {
        return sDev;
    }
    virtual void		SetDev(std::string sNewDev) {
        sDev = sNewDev;
    }
    virtual void		Close() {}
	virtual std::string		GetVersion() { return "no audio interface"; }
private:
    std::string sDev;
};

class CSoundOutNull : public CSoundOutInterface
{
public:
    CSoundOutNull() {}
    virtual ~CSoundOutNull();
    virtual bool	Init(int, int, bool) {
        return true;
    }
    virtual bool	Write(CVector<short>&) {
        return false;
    }

    virtual void		Enumerate(std::vector<std::string>& choices, std::vector<std::string>&){
        choices.push_back("(None)");
    }

    virtual std::string		GetDev() {
        return sDev;
    }
    virtual void		SetDev(std::string sNewDev) {
        sDev = sNewDev;
    }
    virtual void		Close() {}
	virtual std::string		GetVersion() { return "no audio interface"; }
private:
    std::string sDev;
};

#endif
