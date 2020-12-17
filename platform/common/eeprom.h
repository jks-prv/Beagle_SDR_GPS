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

// Copyright (c) 2016 John Seamons, ZL/KF6VO

#pragma once

typedef enum { SERNO_READ, SERNO_WRITE, SERNO_ALLOC } next_serno_e;

//#define TEST_FLAG_EEPROM
#ifdef TEST_FLAG_EEPROM
    void eeprom_test();
#else
    #define eeprom_test()
#endif

int eeprom_next_serno(next_serno_e type, int set_serno);
int eeprom_check();
void eeprom_write(next_serno_e type, int serno);
void eeprom_update();
