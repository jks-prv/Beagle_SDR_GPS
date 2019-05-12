/*
 * slowrx - an SSTV decoder
 * * * * * * * * * * * * * *
 * 
 * Copyright (c) 2007-2013, Oona Räisänen (OH2EIQ [at] sral.fi)
 */

#include "sstv.h"

/*
 * Mode specifications
 *
 * Name          Long, human-readable name for the mode
 * ShortName     Abbreviation for the mode, used in filenames
 * SyncTime      Duration of synchronization pulse in seconds
 * PorchTime     Duration of sync porch pulse in seconds
 * SeptrTime     Duration of channel separator pulse in seconds
 * PixelTime     Duration of one pixel in seconds
 * LineTime      Time in seconds from the beginning of a sync pulse to the beginning of the next one
 * ImgWidth      Pixels per scanline
 * NumLines      Number of scanlines
 * LineHeight    Height of one scanline in pixels (1 or 2)
 * ColorEnc      Color format (GBR, RGB, YUV, BW)
 *
 *
 * Note that these timings do not fully describe the workings of the different modes.
 *
 * References: 
 *             
 *             JL Barber N7CXI (2000): "Proposal for SSTV Mode Specifications". Presented at the
 *             Dayton SSTV forum, 20 May 2000.
 *
 *             Dave Jones KB4YZ (1999): "SSTV modes - line timing".
 *             <http://www.tima.com/~djones/line.txt>
 */

ModeSpec_t ModeSpec[] = {
    {},

  {  // N7CXI, 2000
    (char *) "Martin M1",
    (char *) "M1",
    4.862e-3,
    0.572e-3,
    0.572e-3,
    0.4576e-3,
    446.446e-3,
    320,
    256,
    1,
    GBR },

  {  // N7CXI, 2000
    (char *) "Martin M2",
    (char *) "M2",
    4.862e-3,
    0.572e-3,
    0.572e-3,
    0.2288e-3,
    226.7986e-3,
    320,
    256,
    1,
    GBR },

  {   // KB4YZ, 1999
    (char *) "Martin M3",
    (char *) "M3",
    4.862e-3,
    0.572e-3,
    0.572e-3,
    0.2288e-3,
    446.446e-3,
    320,
    128,
    2,
    GBR },

  {   // KB4YZ, 1999
    (char *) "Martin M4",
    (char *) "M4",
    4.862e-3,
    0.572e-3,
    0.572e-3,
    0.2288e-3,
    226.7986e-3,
    320,
    128,
    2,
    GBR },

  {  // N7CXI, 2000
    (char *) "Scottie S1",
    (char *) "S1",
    9e-3,           // sync
    1.5e-3,         // porch
    1.5e-3,         // septr
    0.4320e-3,      // pixel
    428.38e-3,      // line
    320,
    256,
    1,
    GBR },

  {  // N7CXI, 2000
    (char *) "Scottie S2",
    (char *) "S2",
    9e-3,
    1.5e-3,
    1.5e-3,
    0.2752e-3,
    277.692e-3,
    320,
    256,
    1,
    GBR },

  {  // N7CXI, 2000
    (char *) "Scottie DX",
    (char *) "SDX",
    9e-3,
    1.5e-3,
    1.5e-3,
    1.08053e-3,
    1050.3e-3,
    320,
    256,
    1,
    GBR },


    // correct Robot color timings from: github.com/n5ac/mmsstv/blob/master/Main.cpp LineR[NN] routines
    
  {  // N7CXI, 2000
    (char *) "Robot 72",    // 4:2:2 format
    (char *) "R72",
    9e-3,
    3e-3,
    6e-3,
    0.215625e-3,            // 138:69:69 ms
    300e-3,                 // 200 LPM
    320,
    240,
    1,
    YUV },

  {  // N7CXI, 2000
    (char *) "Robot 36",    // 4:2:0 format
    (char *) "R36",
    9e-3,
    3e-3,
    6e-3,
    0.1375e-3,              // 88:44:0 ms
    150e-3,                 // 400 LPM
    320,
    240,
    1,
    YUV },

  {  // N7CXI, 2000
    (char *) "Robot 24",    // 4:2:2 format
    (char *) "R24",
    6e-3,
    2e-3,
    4e-3,
    0.14375e-3,             // 92:46:46 ms
    200e-3,                 // 300 LPM
    320,
    120,
    2,
    YUV },

  {  // N7CXI, 2000
    (char *) "Robot 24 B/W",
    (char *) "R24BW",
    7e-3,
    0e-3,
    0e-3,
    0.291e-3,
    100e-3,
    320,
    240,
    1,
    BW },

  {  // N7CXI, 2000
    (char *) "Robot 12 B/W",
    (char *) "R12BW",
    7e-3,
    0e-3,
    0e-3,
    0.291e-3,
    100e-3,
    320,
    120,
    2,
    BW },

  {  // N7CXI, 2000
    (char *) "Robot 8 B/W",
    (char *) "R8BW",
    7e-3,
    0e-3,
    0e-3,
    0.188e-3,
    67e-3,
    320,
    120,
    2,
    BW },

  { // KB4YZ, 1999
    (char *) "Wraase SC-2 120",     // 2:4:2 format
    (char *) "W2120",
    5e-3,
    1.0525e-3,
    0e-3,
    0.489039081e-3,                 // (LT-sync) / (320*3)
    475.530018e-3,                  // 126.175 LPM
    320,
    256,
    1,
    RGB },

  {  // N7CXI, 2000
    (char *) "Wraase SC-2 180",
    (char *) "W2180",
    5.5225e-3,
    0.5e-3,
    0e-3,
    0.734532e-3,
    711.0225e-3,
    320,
    256,
    1,
    RGB },

  {  // N7CXI, 2000
    (char *) "PD-50",
    (char *) "PD50",
    20e-3,
    2.08e-3,
    0e-3,
    0.286e-3,
    388.16e-3,              // SpYYUV
    320,
    128,
    2,
    YUV },

  {  // N7CXI, 2000
    (char *) "PD-90",
    (char *) "PD90",
    20e-3,
    2.08e-3,
    0e-3,
    0.532e-3,
    703.04e-3,
    320,
    128,
    2,
    YUV },

  {  // N7CXI, 2000
    (char *) "PD-120",
    (char *) "PD120",
    20e-3,
    2.08e-3,
    0e-3,
    0.19e-3,
    508.48e-3,
    640,
    496,
    1,
    YUV },

  {  // N7CXI, 2000
    (char *) "PD-160",
    (char *) "PD160",
    20e-3,
    2.08e-3,
    0e-3,
    0.382e-3,
    804.416e-3,
    512,
    400,
    1,
    YUV },

  {  // N7CXI, 2000
    (char *) "PD-180",
    (char *) "PD180",
    20e-3,
    2.08e-3,
    0e-3,
    0.286e-3,
    754.24e-3,
    640,
    496,
    1,
    YUV },

  {  // N7CXI, 2000
    (char *) "PD-240",
    (char *) "PD240",
    20e-3,
    2.08e-3,
    0e-3,
    0.382e-3,
    1000e-3,
    640,
    496,
    1,
    YUV },

  {  // N7CXI, 2000
    (char *) "PD-290",
    (char *) "PD290",
    20e-3,
    2.08e-3,
    0e-3,
    0.286e-3,
    937.28e-3,
    800,
    616,
    1,
    YUV },

  {  // N7CXI, 2000
    (char *) "Pasokon P3",
    (char *) "P3",
    5.208e-3,
    1.042e-3,
    1.042e-3,
    0.2083e-3,
    409.375e-3,
    640,
    496,
    1,
    RGB },

  {  // N7CXI, 2000
    (char *) "Pasokon P5",
    (char *) "P5",
    7.813e-3,
    1.563e-3,
    1.563e-3,
    0.3125e-3,
    614.065e-3,
    640,
    496,
    1,
    RGB },

{  // N7CXI, 2000
    (char *) "Pasokon P7",
    (char *) "P7",
    10.417e-3,
    2.083e-3,
    2.083e-3,
    0.4167e-3,
    818.747e-3,
    640,
    496,
    1,
    RGB }
};

/*
 * Mapping of 7-bit VIS codes to modes
 *
 * Reference: Dave Jones KB4YZ (1998): "List of SSTV Modes with VIS Codes".
 *            http://www.tima.com/~djones/vis.txt
 *
 */

//                 x0    x1    x2    x3   x4    x5    x6    x7    x8    x9    xA    xB   xC   xD   xE    xF

u1_t   VISmap[] = { 0,    0,    R8BW, 0,   R24,  0,    R12BW,0,    R36,  0,    R24BW,0,   R72, 0,   0,    0,     // 0x
                    0,    0,    0,    0,   0,    0,    0,    0,    0,    0,    0,    0,   0,   0,   0,    0,     // 1x
                    M4,   0,    0,    0,   M3,   0,    0,    0,    M2,   0,    0,    0,   M1,  0,   0,    0,     // 2x
                    0,    0,    0,    0,   0,    0,    0,    W2180,S2,   0,    0,    0,   S1,  0,   0,    W2120, // 3x
                    0,    0,    0,    0,   0,    0,    0,    0,    0,    0,    0,    0,   SDX, 0,   0,    0,     // 4x
                    0,    0,    0,    0,   0,    0,    0,    0,    0,    0,    0,    0,   0,   PD50,PD290,PD120, // 5x
                    PD180,PD240,PD160,PD90,0,    0,    0,    0,    0,    0,    0,    0,   0,   0,   0,    0,     // 6x
                    0,    P3,   P5,   P7,  0,    0,    0,    0,    0,    0,    0,    0,   0,   0,   0,    0 };   // 7x
