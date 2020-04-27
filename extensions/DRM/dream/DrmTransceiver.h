/******************************************************************************\
 *
 * Copyright (c) 2012
 *
 * Author(s):
 *	David Flamand
 *
 * Description:
 *	DRM receiver-transmitter base class
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

#ifndef DRM_TRANSCEIVER_H
#define DRM_TRANSCEIVER_H

#include "sound/soundinterface.h"
#include <vector>

class CSettings;
class CParameter;
enum ERunState { STOPPED, RUNNING, STOP_REQUESTED, RESTART };

class CDRMTransceiver
{
public:
    CDRMTransceiver() {}
    virtual ~CDRMTransceiver();

    virtual void LoadSettings() = 0;
    virtual void SaveSettings() = 0;
    virtual void SetInputDevice(std::string) = 0;
    virtual void SetOutputDevice(std::string) = 0;
    virtual void EnumerateInputs(std::vector<std::string>& names, std::vector<std::string>& descriptions)=0;
    virtual void EnumerateOutputs(std::vector<std::string>& names, std::vector<std::string>& descriptions)=0;
    virtual CSettings*				GetSettings() = 0;
    virtual void					SetSettings(CSettings* pNewSettings) = 0;
    virtual CParameter*				GetParameters() = 0;
    virtual bool				IsReceiver() const = 0;
    virtual bool				IsTransmitter() const = 0;

};

#endif
