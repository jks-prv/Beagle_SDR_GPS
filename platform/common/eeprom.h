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

#include "peri.h"

struct eeprom_t {
	#define	EE_HEADER	0xAA5533EE
	u4_t header;                // 0x00
	
	#define	EE_FMT_REV	"A1"
	char fmt_rev[2];            // 0x04
	
	char board_name[32];        // 0x06
	char version[4];            // 0x26
	char mfg[16];               // 0x2a
	char part_no[16];           // 0x3a
	char week[2];               // 0x4a
	char year[2];               // 0x4c
	char assembly[4];           // 0x4e
	char serial_no[4];          // 0x52
	
	u2_t n_pins;                // 0x56
	u2_t io_pins[EE_NPINS];     // 0x58
	
	#define	EE_MA_3V3	250
	#define	EE_MA_5INT	0
	#define	EE_MA_5EXT	0
	#define	EE_MA_DC	1500
	u2_t mA_3v3;                // 0xec
	u2_t mA_5int;               // 0xee
	u2_t mA_5ext;               // 0xf0
	u2_t mA_DC;                 // 0xf2
	
	u1_t free[1];               // 0xf4+
} __attribute__((packed));

extern eeprom_t eeprom;

//#define TEST_FLAG_EEPROM
#ifdef TEST_FLAG_EEPROM
    void eeprom_test();
#else
    #define eeprom_test()
#endif

typedef enum { SERNO_READ, SERNO_WRITE, SERNO_ALLOC } next_serno_e;

int eeprom_next_serno(next_serno_e type, int set_serno);
int eeprom_check();
void eeprom_write(next_serno_e type, int serno);
void eeprom_update();
