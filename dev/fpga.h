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

// Copyright (c) 2023 John Seamons, ZL4VO/KF6VO

#pragma once

#include "types.h"

u2_t ctrl_get();
void ctrl_clr_set(u2_t clr, u2_t set);
void ctrl_positive_pulse(u2_t bits);
void ctrl_set_ser_dev(u2_t ser_dev);
void ctrl_clr_ser_dev();

typedef union {
    u2_t word;
    struct {
        u2_t fpga_id:4, stat_user:4, fpga_ver:4, fw_id:3, ovfl:1;
    };
} stat_reg_t;
stat_reg_t stat_get();

u64_t fpga_dna();
u2_t getmem(u2_t addr);
void setmem(u2_t addr, u2_t data);
void printmem(const char *str, u2_t addr);
