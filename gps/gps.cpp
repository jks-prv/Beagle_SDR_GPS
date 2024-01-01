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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "kiwi.h"
#include "gps.h"
#include "spi.h"
#include "printf.h"

///////////////////////////////////////////////////////////////////////////////////////////////

unsigned bin(char *s, int n) {
	unsigned u = *s;
	while (--n) u += u + *++s;
	return u;
}

///////////////////////////////////////////////////////////////////////////////////////////////

void gps_main(int argc, char *argv[])
{
    // verilog limitations, see:
    //      gps.v: "cmd_chan"
    //      ipcore_bram_gps_4k_12b

    assert(gps_chans <= GPS_MAX_CHANS);

	printf("GPS starting..\n");
    SearchParams(argc, argv);

    // some configs (e.g. rx14wf0) only support a reduced number of GPS channels
    // due to FPGA space limitations
    spi_set(CmdSetChans, gps_chans-1);      // NB: -1 because of how to_loop[2] insn works
	SearchInit();

    for(int i=0; i<gps_chans; i++) {
    	char *tname;
    	asprintf(&tname, "GPSchan-%02d", i+1);
    	CreateTaskSF(ChanTask, tname, TO_VOID_PARAM(i), GPS_PRIORITY, CTF_TNAME_FREE, 0);
    }

    CreateTask(SolveTask, 0, GPS_PRIORITY);

    if (!background_mode && (print_stats & (STATS_GPS | STATS_GPS_SOLN))) CreateTask(StatTask, 0, GPS_PRIORITY);
}
