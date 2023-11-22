// ----------------------------------------------------------------------------
// interleave.cxx  --  MFSK (de)interleaver
//
// Copyright (C) 2006-2008
//		Dave Freese, W1HKJ
//
// This file is part of fldigi.  Adapted from code contained in gmfsk source code
// distribution.
//  gmfsk Copyright (C) 2001, 2002, 2003
//  Tomi Manninen (oh2bns@sral.fi)
//
// Fldigi is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Fldigi is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with fldigi.  If not, see <http://www.gnu.org/licenses/>.
// ----------------------------------------------------------------------------

#include <config.h>

#include <cstring>

#include "interleave.h"

// ----------------------------------------------------------------------

interleave::interleave (int _size, int _depth, int dir)
{
	size = _size;
	depth = _depth;
	direction = dir;
	len = size * size * depth;
	table = new unsigned char [len];
	flush();
}

interleave::~interleave ()
{
	delete [] table;
}

void interleave::init()
{
	if(table) {
		len = size * size * depth;
		flush();
	}
}

void interleave::symbols(unsigned char *psyms)
{
	int i, j, k;

	for (k = 0; k < depth; k++) {
		for (i = 0; i < size; i++)
			for (j = 0; j < size - 1; j++)
				*tab(k, i, j) = *tab(k, i, j + 1);

		for (i = 0; i < size; i++)
			*tab(k, i, size - 1) = psyms[i];

		for (i = 0; i < size; i++) {
			if (direction == INTERLEAVE_FWD)
				psyms[i] = *tab(k, i, size - i - 1);
			else
				psyms[i] = *tab(k, i, i);
		}
	}
}

void interleave::bits(unsigned int *pbits)
{
	unsigned char syms[size];
	int i;

	for (i = 0; i < size; i++)
		syms[i] = (*pbits >> (size - i - 1)) & 1;

	symbols(syms);

	for (*pbits = i = 0; i < size; i++)
		*pbits = (*pbits << 1) | syms[i];
}

void interleave::flush(void)
{
// Fill entire RX interleaver with punctures or 0 depending on whether
// Rx or Tx
	if (direction == INTERLEAVE_REV)
		memset(table, PUNCTURE, len);
	else
		memset(table, 0, len);
}


// ----------------------------------------------------------------------

