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

#include "gps.h"

// FIXME: update periodically as new sats are commissioned

SATELLITE Sats[] = {

    // Navstar
    // www.navcen.uscg.gov/?Do=constellationstatus
    { 1,  2,  6, Navstar},
    { 2,  3,  7, Navstar},
    { 3,  4,  8, Navstar},
    { 4,  5,  9, Navstar},
    { 5,  1,  9, Navstar},
    { 6,  2, 10, Navstar},
    { 7,  1,  8, Navstar},
    { 8,  2,  9, Navstar},
    { 9,  3, 10, Navstar},
    {10,  2,  3, Navstar},
    {11,  3,  4, Navstar},
    {12,  5,  6, Navstar},
    {13,  6,  7, Navstar},
    {14,  7,  8, Navstar},
    {15,  8,  9, Navstar},
    {16,  9, 10, Navstar},
    {17,  1,  4, Navstar},
    {18,  2,  5, Navstar},
    {19,  3,  6, Navstar},
    {20,  4,  7, Navstar},
    {21,  5,  8, Navstar},
    {22,  6,  9, Navstar},
    {23,  1,  3, Navstar},
    {24,  4,  6, Navstar},
    {25,  5,  7, Navstar},
    {26,  6,  8, Navstar},
    {27,  7,  9, Navstar},
    {28,  8, 10, Navstar},
    {29,  1,  6, Navstar},
    {30,  2,  7, Navstar},
    {31,  3,  8, Navstar},
    {32,  4,  9, Navstar},
    
    // Sats that specify PRN with G2 delay (not used, doc only)
    // and G2 init value (octal) instead of as taps on G2

/*
    // WAAS (USA SBAS)
    {131, 1012, 00551, SBAS},
    {133,  603, 01731, SBAS},
    {135,  359, 01216, SBAS},
    {138,  386, 00450, SBAS},
    {140,  456, 01653, SBAS},

    // EGNOS (Europe SBAS)
    {120,  145, 01106, SBAS},
    {123,   21, 00232, SBAS},
    {136,  595, 00740, SBAS},

    // GATBP (AUS SBAS)
    {122,   52, 00267, SBAS},

    // MSAS (Japan SBAS)
    {129,  762, 01250, SBAS},
    {137,   68, 01007, SBAS},
*/

    // QZSS (Japan) prn(saif) = 183++, prn(std) = 193++
    // last checked: 9-Sep-2020
    // sys.qzss.go.jp/dod/en/constellation.html [PNT L1 C/A entries]
    {193, 339, 01050, QZSS},   // J01, QZS-1
    {194, 208, 01607, QZSS},   // J02, QZS-2
    {195, 711, 01747, QZSS},   // J04, QZS-4
//  {196, 189, 01305, QZSS},   // (not scheduled until 2023)
//  {197, 263, 00540, QZSS},   //
//  {198, 537, 01363, QZSS},   //
    {199, 663, 00727, QZSS},   // J03, QZS-3
//  {200, 942, 00147},
//  {201, 173, 01206},
//  {202, 900, 01045},

    // Galileo E1B
    // last checked: 9-Sep-2020
    // www.gsc-europa.eu/system-status/Constellation-Information
    // en.wikipedia.org/wiki/List_of_Galileo_satellites
    { 1, 0, 0, E1B},    // gsat0210 "Galileo 13"
    { 2, 0, 0, E1B},    // gsat0211 "Galileo 14"
    { 3, 0, 0, E1B},    // gsat0212 "Galileo 16"
    { 4, 0, 0, E1B},    // gsat0213 "Galileo 17"
    { 5, 0, 0, E1B},    // gsat0214 "Galileo 18"

    { 7, 0, 0, E1B},    // gsat0207 "Galileo 15"
    { 8, 0, 0, E1B},    // gsat0208 "Galileo 11"
    { 9, 0, 0, E1B},    // gsat0209 "Galileo 12"

    {11, 0, 0, E1B},    // gsat0101 "Galileo 1, IOV PFM"
    {12, 0, 0, E1B},    // gsat0102 "Galileo 2, IOV FM2"

    {19, 0, 0, E1B},    // gsat0103 "Galileo 3, IOV FM3"
    {24, 0, 0, E1B},    // gsat0205 "Galileo 9"
    {26, 0, 0, E1B},    // gsat0203 "Galileo 7"
    {30, 0, 0, E1B},    // gsat0206 "Galileo 10"

    {14, 0, 0, E1B},    // gsat0202 "Galileo 6", testing
    {18, 0, 0, E1B},    // gsat0201 "Galileo 5", testing
    {21, 0, 0, E1B},    // gsat0215 "Galileo 19"
    {25, 0, 0, E1B},    // gsat0216 "Galileo 20"
    {27, 0, 0, E1B},    // gsat0217 "Galileo 21"
    {31, 0, 0, E1B},    // gsat0218 "Galileo 22"

    {36, 0, 0, E1B},    // gsat0219 "Galileo 23"
    {13, 0, 0, E1B},    // gsat0220 "Galileo 24"
    {15, 0, 0, E1B},    // gsat0221 "Galileo 25"
    {33, 0, 0, E1B},    // gsat0222 "Galileo 26"

//  {20, 0, 0, E1B},    // gsat0104 "Galileo 4", not available
//  {22, 0, 0, E1B},    // gsat0224 "Galileo 8", not usable
    
    {-1}
};
