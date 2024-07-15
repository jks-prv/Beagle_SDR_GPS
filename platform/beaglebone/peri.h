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

// Copyright (c) 2015-2022 John Seamons, ZL4VO/KF6VO

#pragma once

#if defined(CPU_AM3359) || defined(CPU_AM5729)
 #include "sitara.h"
#endif

#ifdef CPU_TDA4VM
 #include "jacinto.h"
#endif

extern volatile u4_t *spi_m, *gpio_m[];

extern gpio_t GPIO_NONE;
extern gpio_t FPGA_INIT, FPGA_PGM;
extern gpio_t SPIn_SCLK, SPIn_MISO, SPIn_MOSI, SPIn_CS0, SPIn_CS1;
extern gpio_t CMD_READY, SND_INTR;
extern gpio_t P911, P913, P915, P926;
extern gpio_t P811, P812, P813, P814, P815, P816, P817, P818, P819, P826;
extern gpio_t BOOT_BTN;

#define devio_check(gpio, dir, pmux_val1, pmux_val2) \
	_devio_check(#gpio, gpio, dir, pmux_val1, pmux_val2);
void _devio_check(const char *name, gpio_t gpio, gpio_dir_e dir, u4_t pmux_val1, u4_t pmux_val2);

#define gpio_setup(gpio, dir, initial, pmux_val1, pmux_val2) \
    _gpio_setup(#gpio, gpio, dir, initial, pmux_val1, pmux_val2);
void _gpio_setup(const char *name, gpio_t gpio, gpio_dir_e dir, u4_t initial, u4_t pmux_val1, u4_t pmux_val2);

void peri_init();
void gpio_test(int gpio_test);
void peri_free();
