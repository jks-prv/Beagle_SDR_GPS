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

// Copyright (c) 2022 John Seamons, ZL4VO/KF6VO

#pragma once

typedef enum { NAT_NO_DELETE, NAT_DELETE } nat_delete_e;
void UPnP_port(nat_delete_e nat_delete);

typedef enum { WAKEUP_REG, WAKEUP_REG_STATUS } wakeup_reg_e;
bool wakeup_reg_kiwisdr_com(wakeup_reg_e wakeup_reg);

#define FILE_DOWNLOAD_RELOAD        0
#define FILE_DOWNLOAD_DIFF_RESTART  1

void my_kiwi_register(bool reg, int root_pwd_unset = 0, int debian_pwd_default = 0);
void file_GET(void *param);
