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

// Copyright (c) 2015 John Seamons, ZL4VO/KF6VO

#pragma once

#include "types.h"

// CAUTION: must match order in kiwi.js
// CAUTION: add new entries at the end

const char * const mode_lc[] = {
    "am", "amn", "usb", "lsb", "cw", "cwn", "nbfm", "iq", "drm",
    "usn", "lsn", "sam", "sau", "sal", "sas", "qam", "nnfm", "amw"
};

const char * const mode_uc[] = {
    "AM", "AMN", "USB", "LSB", "CW", "CWN", "NBFM", "IQ", "DRM",
    "USN", "LSN", "SAM", "SAU", "SAL", "SAS", "QAM", "NNFM", "AMW"
};

#define IS_AM       0x0001
#define IS_SSB      0x0002
#define IS_USB      0x0004
#define IS_LSB      0x0008
#define IS_CW       0x0010
#define IS_NBFM     0x0020
#define IS_IQ       0x0040
#define IS_DRM      0x0080
#define IS_SAM      0x0100
#define IS_STEREO   0x0200
#define IS_NARROW   0x0400
#define IS_WIDE     0x0800

const int mode_flags[] = {
    IS_AM, IS_AM|IS_NARROW,
    IS_SSB|IS_USB, IS_SSB|IS_LSB,
    IS_CW, IS_CW|IS_NARROW,
    IS_NBFM,
    IS_IQ|IS_STEREO, IS_DRM|IS_STEREO,
    IS_SSB|IS_USB|IS_NARROW, IS_SSB|IS_LSB|IS_NARROW,
    IS_SAM, IS_SAM, IS_SAM, IS_SAM|IS_STEREO, IS_SAM|IS_STEREO, IS_NBFM|IS_NARROW,
    IS_AM|IS_WIDE
};

const int mode_hbw[] = {
    9800/2, 5000/2, 2400/2, 2400/2, 400/2, 60/2, 12000/2, 10000/2, 10000/2,
    2100/2, 2100/2, 9800/2, 9800/2, 9800/2, 9800/2, 9800/2, 6000/2, 12000/2
};

const int mode_offset[] = {
    0, 0, 1500, -1500, 0, 0, 0, 0, 0,
    1350, -1350, 0, 0, 0, 0, 0, 0, 0
};

typedef enum {
    MODE_AM, MODE_AMN, MODE_USB, MODE_LSB, MODE_CW, MODE_CWN, MODE_NBFM, MODE_IQ, MODE_DRM,
    MODE_USN, MODE_LSN, MODE_SAM, MODE_SAU, MODE_SAL, MODE_SAS, MODE_QAM, MODE_NNFM, MODE_AMW
} mode_e;
