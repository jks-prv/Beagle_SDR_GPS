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

// Copyright (c) 2021 John Seamons, ZL4VO/KF6VO

#pragma once

#include "types.h"
#include "kiwi.h"
#include "printf.h"
#include "misc.h"

//#define CRYPT_PW

extern char *current_authkey;

char *kiwi_authkey();
bool admin_pwd_unsafe();

bool kiwi_crypt_file_read(const char *fn, int *seq, char **salt, char **hash);
char *kiwi_crypt_generate(const char *key, int seq);
bool kiwi_crypt_validate(const char *key, char *salt, char *hash);
