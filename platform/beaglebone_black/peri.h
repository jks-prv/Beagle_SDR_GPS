//////////////////////////////////////////////////////////////////////////
// Homemade GPS Receiver
// Copyright (C) 2013 Andrew Holme
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// http://www.holmea.demon.co.uk/GPS/Main.htm
//////////////////////////////////////////////////////////////////////////

#ifndef _PERI_H_
#define _PERI_H_

#include "sitara.h"

extern volatile u4_t *spi, *_gpio[];

extern gpio_t GPIO_NONE;
extern gpio_t FPGA_INIT, FPGA_PGM;
extern gpio_t SPI0_SCLK, SPI0_MISO, SPI0_MOSI, SPI0_CS0, SPI0_CS1;
extern gpio_t GPIO0_15;

#define devio_setup(gpio, dir, pmux_val) \
	_devio_setup(#gpio, gpio, dir, pmux_val);
void _devio_setup(const char *name, gpio_t gpio, gpio_dir_e dir, u4_t pmux_val);

#define gpio_setup(gpio, dir, initial, pmux_val1, pmux_val2) \
	_gpio_setup(#gpio, gpio, dir, initial, pmux_val1, pmux_val2);
void _gpio_setup(const char *name, gpio_t gpio, gpio_dir_e dir, u4_t initial, u4_t pmux_val1, u4_t pmux_val2);

void peri_init();
void peri_free();

enum next_serno_e { SERNO_READ, SERNO_WRITE, SERNO_ALLOC };

int eeprom_next_serno(next_serno_e type, int set_serno);
int eeprom_check();
void eeprom_write();

#endif
