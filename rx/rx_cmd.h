/*
--------------------------------------------------------------------------------
This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public
License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.
This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.
You should have received a copy of the GNU Library General Public
License along with this library; if not, write to the
Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
Boston, MA  02110-1301, USA.
--------------------------------------------------------------------------------
*/

// Copyright (c) 2020 John Seamons, ZL/KF6VO

#pragma once

enum rx_common_cmd_key_e {
    CMD_KEEPALIVE=1, CMD_OPTIONS, CMD_AUTH, CMD_SAVE_CFG, CMD_SAVE_ADM, CMD_DX_UPD, CMD_DX_FILTER, CMD_MARKER,
    CMD_GET_DX_JSON, CMD_GET_CONFIG, CMD_STATS_UPD, CMD_GET_USERS, CMD_IDENT_USER, CMD_NEED_STATUS, CMD_GEO_LOC,
    CMD_GEO_JSON, CMD_BROWSER, CMD_WF_COMP, CMD_INACTIVITY_ACK, CMD_PREF_EXPORT, CMD_PREF_IMPORT,
    CMD_OVERRIDE, CMD_NOCACHE, CMD_CTRACE, CMD_DEBUG_VAL, CMD_DEBUG_MSG, CMD_IS_ADMIN,
    CMD_GET_AUTHKEY, CMD_CLK_ADJ, CMD_SERVER_DE_CLIENT, CMD_X_DEBUG
};
