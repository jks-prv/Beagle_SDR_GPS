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
 * Format        Format of scanline (sync, porch, pixel data etc.)
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
 *
 *             bruxy.regnet.cz/web/hamradio/EN/a-look-into-sstv-mode
 */

ModeSpec_t ModeSpec[] = {
    {},     // UNKNOWN
    {},     // VISX

  {
    (char *) "Amiga Video Transceiver",
    (char *) "AVT",
    0,
    0,
    0e-3,
    0,
    0,
    320,
    256,
    1,
    RGB,
    FMT_DEFAULT,
    UNSUPPORTED },

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
    GBR,
    FMT_DEFAULT },

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
    GBR,
    FMT_DEFAULT },

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
    GBR,
    FMT_DEFAULT },

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
    GBR,
    FMT_DEFAULT },

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
    GBR,
    FMT_REV },

  {  // N7CXI, 2000
    (char *) "Scottie S2",
    (char *) "S2",
    9e-3,
    1.5e-3,
    1.5e-3,
    0.2752e-3,      // GBR each = 0.2752 * 320 = 88.064
    277.692e-3,     // ~71/256, but exactly s0s1Sp2 = 1.5 G 1.5 B 9 1.5 R
    320,
    256,
    1,
    GBR,
    FMT_REV },

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
    GBR,
    FMT_REV },


    // correct Robot color timings from: github.com/n5ac/mmsstv/blob/master/Main.cpp LineR[NN] routines
    
  {  // N7CXI, 2000
    (char *) "Robot 72",    // 4:2:2 format
    (char *) "R72",
    9e-3,
    3e-3,                   // porch
    6e-3,                   // 4.5 separator + 1.5 porch
    0.215625e-3,            // Tpixel 69/320, 138:69:69 ms, 0.215625 * 320 = 69
    300e-3,                 // Tline 72s/240line = 0.3 s/line; 200 LPM: 72/240 = 60/x(200), Sp00s1s2 = 9+3+138+6+69+6+69 = 300
    320,
    240,
    1,
    YUV,
    FMT_422 },

  {  // N7CXI, 2000
    (char *) "Robot 36",    // 4:2:0 format
    (char *) "R36",
    9e-3,
    3e-3,                   // porch
    6e-3,                   // 4.5 even/odd separator + 1.5 porch
    0.1375e-3,              // 88:44:0 ms
    150e-3,                 // 400 LPM
    320,
    240,
    1,
    YUV,
    FMT_420 },

  {  // N7CXI, 2000
    (char *) "Robot 24",    // 4:2:2 format
    (char *) "R24",
    6e-3,
    2e-3,
    4e-3,
    0.14375e-3,             // 46/320, 92:46:46 ms
    200e-3,                 // 24s/120line = 0.2s/line, 300 LPM
    320,
    120,
    2,
    YUV,
    FMT_422 },

  {  // N7CXI, 2000
    (char *) "Robot 24 B/W",
    (char *) "R24-BW",
    7e-3,
    0e-3,
    0e-3,
    0.291e-3,
    100e-3,
    320,
    240,
    1,
    BW,
    FMT_BW },

  {  // N7CXI, 2000
    (char *) "Robot 12 B/W",
    (char *) "R12-BW",
    7e-3,
    0e-3,
    0e-3,
    0.291e-3,
    100e-3,
    320,
    120,
    2,
    BW,
    FMT_BW },

  {  // N7CXI, 2000
    (char *) "Robot 8 B/W",
    (char *) "R8-BW",
    7e-3,
    0e-3,
    0e-3,
    0.188e-3,
    67e-3,
    320,
    120,
    2,
    BW,
    FMT_BW },

  { // KB4YZ, 1999
    (char *) "Wraase SC-2 60",      // 2:4:2 format
    (char *) "SC60",
    0,
    0,
    0e-3,
    0,
    0,
    320,
    256,
    1,
    RGB,
    FMT_242,
    UNSUPPORTED },

  { // KB4YZ, 1999
    (char *) "Wraase SC-2 120",     // 2:4:2 format
    (char *) "SC120",
    5e-3,
    1.0525e-3,
    0e-3,
    0.489039081e-3,     // Tpixel (fixme)156.5/2/320      // (LT-sync) / (320*3)
    475.530018e-3,                  // 126.175 LPM
    320,
    256,
    1,
    RGB,
    FMT_242 },

  {  // N7CXI, 2000
    (char *) "Wraase SC-2 180",
    (char *) "SC180",
    5.5225e-3,
    0.5e-3,
    0e-3,
    0.734532e-3,
    711.0225e-3,
    320,
    256,
    1,
    RGB,
    FMT_DEFAULT },

  {  // N7CXI, 2000
    (char *) "PD-50",
    (char *) "PD50",
    20e-3,
    2.08e-3,
    0e-3,
    0.286e-3,
    388.16e-3,      // SpYUVY
    320,
    128,
    2,
    YUV,
    FMT_DEFAULT },

  {  // N7CXI, 2000
    (char *) "PD-90",
    (char *) "PD90",
    20e-3,
    2.08e-3,
    0e-3,
    0.532e-3,       // Tpixel 170.24/320
    703.04e-3,      // Tline ~90/128, but exactly SpYUVY = 20+2.08+170.24+170.24+170.24+170.24 = 703.04
    320,
    128,
    2,
    YUV,
    FMT_DEFAULT },

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
    YUV,
    FMT_DEFAULT },

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
    YUV,
    FMT_DEFAULT },

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
    YUV,
    FMT_DEFAULT },

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
    YUV,
    FMT_DEFAULT },

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
    YUV,
    FMT_DEFAULT },

  {  // N7CXI, 2000
    (char *) "Pasokon P3",
    (char *) "P3",
    5.208333e-3,    // Tsync 25x Tpixel
    1.041666e-3,    // Tporch 5x Tpixel
    1.041666e-3,    // Tseptr 5x Tpixel
    1.0/4800.0,     // Tpixel i.e. 4800 Hz
    409.375e-3,     // Tline ~203/496, but exactly SpRsGsBs = (25+5+640+5+640+5+640+5)/4800 = 409.375
    640,
    496,
    1,
    RGB,
    FMT_DEFAULT },

  {  // N7CXI, 2000
    (char *) "Pasokon P5",
    (char *) "P5",
    7.813e-3,
    1.563e-3,
    1.563e-3,
    0.3125e-3,      // pixel i.e. 3200 Hz
    614.065e-3,
    640,
    496,
    1,
    RGB,
    FMT_DEFAULT },

{  // N7CXI, 2000
    (char *) "Pasokon P7",
    (char *) "P7",
    10.417e-3,
    2.083e-3,
    2.083e-3,
    0.4167e-3,      // pixel i.e. 2400 Hz
    818.747e-3,
    640,
    496,
    1,
    RGB,
    FMT_DEFAULT },

// MP is like PD: Yodd U V Yeven
{
    (char *) "MMSSTV MP73",
    (char *) "MP73",
    9.0e-3,         // Tsync
    1.0e-3,         // Tporch
    0e-3,           // Tseptr
    0.4375e-3,      // Tpixel 140/320
    570.0e-3,       // Tline ~73/128, but exactly SpYUVY = 9+1+140+140+140+140 = 570
    320,
    128,
    2,
    YUV,
    FMT_DEFAULT },

{
    (char *) "MMSSTV MP115",
    (char *) "MP115",
    9.0e-3,         // Tsync
    1.0e-3,         // Tporch
    0e-3,           // Tseptr
    0.696875e-3,    // Tpixel 223/320
    902.0e-3,       // Tline ~115/128, but exactly SpYUVY = 9+1+223+223+223+223 = 902
    320,
    128,
    2,
    YUV,
    FMT_DEFAULT },

{
    (char *) "MMSSTV MP140",
    (char *) "MP140",
    9.0e-3,         // Tsync
    1.0e-3,         // Tporch
    0e-3,           // Tseptr
    0.84375e-3,     // Tpixel 270/320
    1090.0e-3,      // Tline ~140/128, but exactly SpYUVY = 9+1+270+270+270+270 = 1090
    320,
    128,
    2,
    YUV,
    FMT_DEFAULT },

{
    (char *) "MMSSTV MP175",
    (char *) "MP175",
    9.0e-3,         // Tsync
    1.0e-3,         // Tporch
    0e-3,           // Tseptr
    1.0625e-3,      // Tpixel 340/320
    1370.0e-3,      // Tline ~175/128, but exactly SpYUVY = 9+1+340+340+340+340 = 1370
    320,
    128,
    2,
    YUV,
    FMT_DEFAULT },

// MR mode line timings are not Sp00s1s2 (as implied by FMT_422) but rather Sp00s1s2s (note trailing "s")
{
    (char *) "MMSSTV MR73",
    (char *) "MR73",
    9.0e-3,         // Tsync
    1.0e-3,         // Tporch
    0.1e-3,         // Tseptr
    0.215625e-3,    // Tpixel 138/2/320
    286.3e-3,       // Tline ~73/256, but exactly Sp00s1s2s = 9+1+138+.1+138/2+.1+138/2+.1 = 286.3
    320,
    256,
    1,
    YUV,
    FMT_422 },

{
    (char *) "MMSSTV MR90",
    (char *) "MR90",
    9.0e-3,         // Tsync
    1.0e-3,         // Tporch
    0.1e-3,         // Tseptr
    0.2671875e-3,   // Tpixel 171/2/320
    352.3e-3,       // Tline ~90/256, but exactly Sp00s1s2s = 9+1+171+.1+171/2+.1+171/2+.1 = 352.3
    320,
    256,
    1,
    YUV,
    FMT_422 },

{
    (char *) "MMSSTV MR115",
    (char *) "MR115",
    9.0e-3,         // Tsync
    1.0e-3,         // Tporch
    0.1e-3,         // Tseptr
    0.34375e-3,     // Tpixel 220/2/320
    450.3e-3,       // Tline ~115/256, but exactly Sp00s1s2s = 9+1+220+.1+220/2+.1+220/2+.1 = 450.3
    320,
    256,
    1,
    YUV,
    FMT_422 },

{
    (char *) "MMSSTV MR140",
    (char *) "MR140",
    9.0e-3,         // Tsync
    1.0e-3,         // Tporch
    0.1e-3,         // Tseptr
    0.4203125e-3,   // Tpixel 269/2/320
    548.3e-3,       // Tline ~140/256, but exactly Sp00s1s2s = 9+1+269+.1+269/2+.1+269/2+.1 = 548.3
    320,
    256,
    1,
    YUV,
    FMT_422 },

{
    (char *) "MMSSTV MR175",
    (char *) "MR175",
    9.0e-3,         // Tsync
    1.0e-3,         // Tporch
    0.1e-3,         // Tseptr
    0.5265625e-3,   // Tpixel 337/2/320
    684.3e-3,       // Tline ~175/256, but exactly Sp00s1s2s = 9+1+337+.1+337/2+.1+337/2+.1 = 684.3
    320,
    256,
    1,
    YUV,
    FMT_422 },

  {
    (char *) "MMSSTV ML180",
    (char *) "ML180",
    9.0e-3,         // Tsync
    1.0e-3,         // Tporch
    0.1e-3,         // Tseptr
    0,
    0,
    640,
    496,
    1,
    YUV,
    FMT_422,
    UNSUPPORTED },

  {
    (char *) "MMSSTV ML240",
    (char *) "ML240",
    9.0e-3,         // Tsync
    1.0e-3,         // Tporch
    0.1e-3,         // Tseptr
    0,
    0,
    640,
    496,
    1,
    YUV,
    FMT_422,
    UNSUPPORTED },

  {
    (char *) "MMSSTV ML280",
    (char *) "ML280",
    9.0e-3,         // Tsync
    1.0e-3,         // Tporch
    0.1e-3,         // Tseptr
    0,
    0,
    640,
    496,
    1,
    YUV,
    FMT_422,
    UNSUPPORTED },

  {
    (char *) "MMSSTV ML320",
    (char *) "ML320",
    9.0e-3,         // Tsync
    1.0e-3,         // Tporch
    0.1e-3,         // Tseptr
    0,
    0,
    640,
    496,
    1,
    YUV,
    FMT_422,
    UNSUPPORTED }

};

/*
 * Mapping of 7-bit VIS codes to modes
 *
 * Reference: Dave Jones KB4YZ (1998): "List of SSTV Modes with VIS Codes".
 *            http://www.tima.com/~djones/vis.txt
 *
 */

u1_t   VISmap[] = {
//  x0     x1     x2     x3    x4     x5     x6     x7     x8     x9     xA     xB    xC    xD    xE     xF
    0,     0,     R8BW,  0,    R24,   0,     R12BW, 0,     R36,   0,     R24BW, 0,    R72,  0,    0,     0,     // 0x
    0,     0,     0,     0,    0,     0,     0,     0,     0,     0,     0,     0,    0,    0,    0,     0,     // 1x
    M4,    0,     0,     VISX, M3,    0,     0,     0,     M2,    0,     0,     0,    M1,   0,    0,     0,     // 2x
    0,     0,     0,     0,    0,     0,     0,     SC180, S2,    0,     0,     SC60, S1,   0,    0,     SC120, // 3x
    0,     0,     0,     0,    AVT,   0,     0,     0,     0,     0,     0,     0,    SDX,  0,    0,     0,     // 4x
    0,     0,     0,     0,    0,     0,     0,     0,     0,     0,     0,     0,    0,    PD50, PD290, PD120, // 5x
    PD180, PD240, PD160, PD90, 0,     0,     0,     0,     0,     0,     0,     0,    0,    0,    0,     0,     // 6x
    0,     P3,    P5,    P7,   0,     0,     0,     0,     0,     0,     0,     0,    0,    0,    0,     0      // 7x
};

/*
 * MMSSTV extended VIS codes
 */
 
u1_t   VISXmap[] = {
//  x0    x1    x2    x3   x4    x5     x6     x7    x8     x9     xA     xB   xC     xD   xE    xF
    0,    0,    0,    0,   0,    ML180, ML240, 0,    0,     ML280, ML320, 0,   0,     0,   0,    0,     // 0x
    0,    0,    0,    0,   0,    0,     0,     0,    0,     0,     0,     0,   0,     0,   0,    0,     // 1x
    0,    0,    0,    0,   0,    MP73,  0,     0,    0,     MP115, MP140, 0,   MP175, 0,   0,    0,     // 2x
    0,    0,    0,    0,   0,    0,     0,     0,    0,     0,     0,     0,   0,     0,   0,    0,     // 3x
    0,    0,    0,    0,   0,    MR73,  MR90,  0,    0,     MR115, MR140, 0,   MR175, 0,   0,    0,     // 4x
    0,    0,    0,    0,   0,    0,     0,     0,    0,     0,     0,     0,   0,     0,   0,    0,     // 5x
    0,    0,    0,    0,   0,    0,     0,     0,    0,     0,     0,     0,   0,     0,   0,    0,     // 6x
    0,    0,    0,    0,   0,    0,     0,     0,    0,     0,     0,     0,   0,     0,   0,    0,     // 7x
};
