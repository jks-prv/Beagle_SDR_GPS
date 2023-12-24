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

// Copyright (c) 2017 John Seamons, ZL4VO/KF6VO

#pragma once

typedef enum { WAIT_UNTIL_NO_USERS, FORCE_CHECK, FORCE_BUILD } update_check_e;

enum { RESTART_INSTALL_UPDATES = 0, RESTART_DELAY_UPDATES = 1 };

// "struct conn_st" because of forward reference from inclusion by conn.h
struct conn_st;
void check_for_update(update_check_e type, struct conn_st *conn);
void schedule_update(int min);
