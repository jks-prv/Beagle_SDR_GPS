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

// Copyright (c) 2020-2023 John Seamons, ZL4VO/KF6VO

#pragma once

enum rx_common_cmd_key_e {
    CMD_KEEPALIVE=1, CMD_OPTIONS, CMD_AUTH, CMD_SAVE_CFG, CMD_SAVE_DXCFG, CMD_SAVE_ADM, CMD_DX_UPD, CMD_DX_FILTER, CMD_MARKER,
    CMD_GET_DX, CMD_DX_SET, CMD_GET_CONFIG, CMD_STATS_UPD, CMD_GET_USERS, CMD_REQUIRE_ID, CMD_IDENT_USER, CMD_NEED_STATUS,
    CMD_GEO_LOC, CMD_GEO_JSON, CMD_BROWSER, CMD_WF_COMP, CMD_INACTIVITY_ACK, CMD_NOTIFY_MSG, CMD_PREF_EXPORT, CMD_PREF_IMPORT,
    CMD_OVERRIDE, CMD_NOCACHE, CMD_CTRACE, CMD_DEBUG_VAL, CMD_MSG_LOG, CMD_DEBUG_MSG, CMD_DEVL, CMD_KICK_ADMINS,
    CMD_IS_ADMIN, CMD_FORCE_CLOSE_ADMIN, CMD_GET_AUTHKEY, CMD_CLK_ADJ, CMD_ANT_SWITCH, CMD_XFER_STATS,
    CMD_SERVER_DE_CLIENT
};

// NB: must match kiwi.js
#define BADP_OK                             0
#define BADP_TRY_AGAIN                      1
#define BADP_STILL_DETERMINING_LOCAL_IP     2
#define BADP_NOT_ALLOWED_FROM_IP            3
#define BADP_NO_ADMIN_PWD_SET               4
#define BADP_NO_MULTIPLE_CONNS              5
#define BADP_DATABASE_UPDATE_IN_PROGRESS    6
#define BADP_ADMIN_CONN_ALREADY_OPEN        7
