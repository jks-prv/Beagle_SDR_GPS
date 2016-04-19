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

#include <memory.h>

struct CACODE {
   char g1[11], g2[11], *tap[2];

   CACODE(int t0, int t1) {
      tap[0] = g2+t0;
      tap[1] = g2+t1;
      memset(g1+1, 1, 10);
      memset(g2+1, 1, 10);
   }

   int Chip() {
      return g1[10] ^ *tap[0] ^ *tap[1];
   }

   void Clock() {
      g1[0] = g1[3] ^ g1[10];
      g2[0] = g2[2] ^ g2[3] ^ g2[6] ^ g2[8] ^ g2[9] ^ g2[10];
      memmove(g1+1, g1, 10);
      memmove(g2+1, g2, 10);
   }

   bool Epoch() {
      return g1[10] & g1[9] & g1[8] & g1[7] & g1[6] & g1[5] & g1[4] & g1[3] & g1[2] & g1[1];
   }

   unsigned GetG1() {
      unsigned ret=0;
      for (int bit=0; bit<10; bit++) ret += ret + g1[10-bit];
      return ret;
   }
};
