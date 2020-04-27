/******************************************************************************\
 * British Broadcasting Corporation
 * Copyright (c) 2001-2014
 *
 * Author(s):
 *  Julian Cable
 *
 * Description:
 *  DRM Propagation Parameters
 *
 ******************************************************************************
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
\******************************************************************************/

#include "TableDRMGlobal.h"
/*
 * Alexander Kurpiers said:
 * If you look at table 47, you can calculate the FFT size.
 * E.g. for mode A: at 48kHz 24ms (T_u) is 1152 samples. Thus the FFT size
 * is 1152. And you see this is correct: 48kHz/1152=41 2/3Hz, which is the
 * carrier spacing. For another sample rate you would of course get another
 * FFT size.
 */

PropagationParams propagationParams[NUM_ROBUSTNESS_MODES]= {
    { {24, 1}, {1, 9}, 15},
    { {64, 3}, {1, 4}, 15},
    { {44, 3}, {4, 11}, 20},
    { {28, 3}, {11, 14}, 24},
    { {9, 4}, {1, 9}, 40}
};
