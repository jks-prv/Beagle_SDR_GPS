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

#ifndef _EPGUTIL_H
#define _EPGUTIL_H

#include "DABMOT.h"

void mkdirs (const std::string & path);

std::string epgFilename (const CDateAndTime & date,
		    uint32_t sid, int type, bool advanced);
std::string epgFilename_etsi (const CDateAndTime & date,
		    uint32_t sid, int type, bool advanced);
std::string epgFilename_dab (const CDateAndTime & date,
		    uint32_t sid, int type, bool advanced);

std::string epgFilename2 (const CDateAndTime & date,
		    uint32_t sid, int type, bool advanced);

#endif
