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

#include <unistd.h>
#include <stdio.h>
#include <math.h>

#include "misc.h"
#include "gps.h"
#include "spi.h"

static bool ready = FALSE;

typedef struct {
	int prn;
	int snr, snr2;
	int rssi, gain;
	int wdog, ca_unlocked;
	int hold;
	int sub, sub_next;
	int lo_dop, ca_dop;
	int lo_dop2, ca_dop2;
	int pe, pp, pl;
	double lo, f_lo, s_lo, l_lo, h_lo, d_lo;
	double ca, f_ca, s_ca, l_ca, h_ca, d_ca;
	char dir;
	int to;
	double dbug_d1, dbug_d2;
	int dbug, dbug_i1, dbug_i2, dbug_i3;
	int novfl;
} stats_t;

stats_t stats[GPS_CHANS];

gps_stats_t gps;

int stats_fft = -1;
unsigned stats_ttff;


///////////////////////////////////////////////////////////////////////////////////////////////

const char *Week[] = {
    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

///////////////////////////////////////////////////////////////////////////////////////////////

struct UMS {
    int u, m;
    double fm, s;
    UMS(double x) {
        u = trunc(x); x = (x-u)*60; fm = x;
        m = trunc(x); s = (x-m)*60;
    }
};

///////////////////////////////////////////////////////////////////////////////////////////////

static double StatSec, StatLat, StatLon, StatAlt;
static int    StatDay=-1, StatNS,  StatEW;
static int decim, min_sig;
static float fft_msec;

///////////////////////////////////////////////////////////////////////////////////////////////

void UserStat(STAT st, double d, int i, int j, int k, int m, double d2) {
	stats_t *s;
	
	switch(st) {
        case STAT_PARAMS:
            decim = i;
            min_sig = j;
            gps.acquiring = k? true:false;
            if (!gps.acquiring) stats_fft = -1;
            ready = TRUE;
            break;
        case STAT_PRN:
			if (i < 0 || i >= GPS_CHANS) return;
			s = &stats[i];
            s->prn = j;
            s->snr = (int) d;
            stats_fft = k? i:-1;
            if (m) fft_msec = (float)m/1000000.0;
			s->l_lo = s->l_ca = 99999999; s->h_lo = s->h_ca = -99999999;
			s->dir = ' '; s->to = 0;
            break;
        case STAT_POWER:
			if (i < 0 || i >= GPS_CHANS) return;
			s = &stats[i];
        	s->rssi = (int) sqrt(d);
        	s->gain = j;
        	if (d == 0) {
        		s->prn = s->snr = s->snr2 = s->wdog = s->ca_unlocked = s->hold = s->sub = s->sub_next = 0;
        	}
            break;
        case STAT_WDOG:
			if (i < 0 || i >= GPS_CHANS) return;
			s = &stats[i];
            s->wdog = j;
            s->hold = k;
            s->ca_unlocked = m;
            break;
        case STAT_SUB:
			if (i < 0 || i >= GPS_CHANS) return;
			s = &stats[i];
        	if (j <= 0 || j > PARITY) break;
        	j--;
        	if (s->sub & 1<<j) {
        		s->sub_next |= 1<<j;
            	s->sub &= ~(1<<j);
        	} else {
            	s->sub |= 1<<j;
            }
            break;
        case STAT_LAT:
            StatLat = fabs(d);
            StatNS = d<0?'S':'N';
            break;
        case STAT_LON:
            StatLon = fabs(d);
            StatEW = d<0?'W':'E';
            break;
        case STAT_ALT:
            StatAlt = d;
            break;
        case STAT_TIME:
        	if (d == 0) return;
            StatDay = d/(60*60*24);
            if (StatDay < 0 || StatDay >= 7) { StatDay = -1; break; }
            StatSec = d-(60*60*24)*StatDay;
            break;
        case STAT_TTFF:
        	stats_ttff = (unsigned) i;
            break;
        case STAT_DOP:
			if (i < 0 || i >= GPS_CHANS) return;
			s = &stats[i];
        	s->lo_dop = j;
        	s->ca_dop = k;
            break;
        case STAT_DOP2:
			if (i < 0 || i >= GPS_CHANS) return;
			s = &stats[i];
            s->snr2 = (int) d;
        	s->lo_dop2 = j;
        	s->ca_dop2 = k;
            break;
        case STAT_EPL:
			if (i < 0 || i >= GPS_CHANS) return;
			s = &stats[i];
        	s->pe = j;
        	s->pp = k;
        	s->pl = m;
            break;
        case STAT_LO:
			if (i < 0 || i >= GPS_CHANS) return;
			s = &stats[i];
            s->d_lo = d - s->lo;
            s->lo = d;
            break;
        case STAT_CA:
			if (i < 0 || i >= GPS_CHANS) return;
			s = &stats[i];
            s->d_ca = d - s->ca;
            s->ca = d;
            break;
        case STAT_NOVFL:
			if (i < 0 || i >= GPS_CHANS) return;
			s = &stats[i];
        	s->novfl++;
        	break;
        case STAT_DEBUG:
			if (i < 0 || i >= GPS_CHANS) return;
			s = &stats[i];
        	s->dbug=1;
            s->dbug_d1 = d;
            s->dbug_d2 = d2;
            s->dbug_i1 = j;
            s->dbug_i2 = k;
            s->dbug_i3 = m;
            break;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////

#define show3(c, v) if (s->c) printf("%3d ", s->v); else printf("    ");
#define show4(c, v) if (s->c) printf("%4d ", s->v); else printf("     ");
#define show5(c, v) if (s->c) printf("%5d ", s->v); else printf("      ");
#define show6(c, v) if (s->c) printf("%6d ", s->v); else printf("       ");
#define show7(c, v) if (s->c) printf("%7d ", s->v); else printf("        ");
#define showf7_1(c, v) if (s->c) printf("%7.1f ", s->v); else printf("        ");
#define showf7_4(c, v) if (s->c) printf("%7.4f ", s->v); else printf("        ");

void StatTask() {

	int i, j, prn;
	unsigned start = timer_ms();

	while (!ready) TaskSleep(1000000);
	
	while (1) {
		UMS lat(StatLat), lon(StatLon);
		UMS hms(StatSec/60/60);

		TaskSleep(1000000);
		
		// only print solutions
		if (print_stats == 2) {
			static int fixes;
			if (gps.fixes > fixes) {
				fixes = gps.fixes;
				if (StatLat) printf("wikimapia.org/#lang=en&lat=%9.6f&lon=%9.6f&z=18&m=b\n",
					(StatNS=='S')? -StatLat:StatLat, (StatEW=='W')? -StatLon:StatLon);
			}
			continue;
		}

		printf("\n\n\n\n\n\n");
#if DECIM_CMP
		printf("   CH    PRN    SNR     CA    ERR   RSSI   GAIN   BITS   WDOG     SUB");
#else
		//      12345 * 1234 123456 123456 123456 123456 123456 Up12345 123456 #########
		printf("   CH    PRN    SNR   RSSI   GAIN   BITS   WDOG     SUB  NOVFL");
#endif
		//printf("  LS    CS      LO     SLO     DLO      CA     SCA     DCA");
		printf("\n");

		for (i=0; i<gps_chans; i++) {
			stats_t *s = &stats[i];
			char c1, c2;
			double snew;
			printf("%5d %c ", i+1, (stats_fft == i)? '*':' ');
			show4(prn, prn);
			show6(snr, snr);
#if DECIM_CMP
			show6(snr, ca_dop);
			printf("       ");
#endif
			show6(rssi, rssi);
			show6(rssi, gain);
			show6(hold, hold);
			show6(rssi, wdog);
			
			printf("%c", s->ca_unlocked? 'U':' ');
			printf("%c", (s->sub & (1<<(PARITY-1)))? 'p':' ');
			s->sub &= ~(1<<(PARITY-1));		// clear parity
			for (j=4; j>=0; j--) {
				printf("%c", (s->sub & (1<<j))? '1'+j:' ');
				if (s->sub_next & (1<<j)) {
					s->sub |= 1<<j;
					s->sub_next &= ~(1<<j);
				}
			}

			show7(novfl, novfl);
			printf(" ");
			
#if 0
			if (s->rssi) printf("%6d:E %6d:P %6d:L ", s->pe/1000, s->pp/1000, s->pl/1000);
#endif
#if 0
			show3(rssi, lo_dop);
			show5(rssi, ca_dop);
			if (s->dir == ' ') s->f_lo=s->lo, s->f_ca=s->ca;
			snew = fabs(s->f_lo-s->lo);
			if (snew > s->s_lo) s->s_lo = snew;
			snew = fabs(s->f_ca-s->ca);
			if (snew > s->s_ca) s->s_ca = snew;
			showf7_1(lo, lo);
			showf7_1(s_lo, s_lo);
			showf7_1(d_lo, d_lo);
			showf7_4(ca, ca);
			showf7_4(s_ca, s_ca);
			showf7_4(d_ca, d_ca);
			c1 = c2 = '_';
			if (s->rssi) {
				s->to++;
				if (s->lo < s->l_lo) { s->l_lo=s->lo; s->dir='v'; s->to=0; c1='v'; } else
				if (s->lo > s->h_lo) { s->h_lo=s->lo; s->dir='^'; s->to=0; c1='^'; };
				if (s->ca < s->l_ca) { s->l_ca=s->ca; s->to=0; c2='v'; } else
				if (s->ca > s->h_ca) { s->h_ca=s->ca; s->to=0; c2='^'; };
			}
			printf("%c%c ", c1, c2);
			if (s->rssi) printf("%3d%c ", s->to, s->dir); else printf("     ");
#endif
			//if (s->dbug) printf("%9.6f %9.6f %6d %6d %6d ",
			//	s->dbug_d1, s->dbug_d2, s->dbug_i1, s->dbug_i2, s->dbug_i3);
			printf("  ");
			for (j=0; j < s->rssi*50/3000; j++) printf("#");
			printf ("\n");
#if DECIM_CMP
			printf("             ");
			show6(snr2, snr2);
			show6(snr2, ca_dop2);
			show6(snr2, ca_dop-s->ca_dop2);
			printf ("\n");
#endif
		}
		printf("\n");

		NextTask("stat1");

		printf(" SATS ");
			if (gps.tracking) printf("tracking %d", gps.tracking);
			if (gps.good) printf(", good %d", gps.good);
			printf("\n");
		printf("  LAT ");
			if (StatLat) printf("%9.5fd %c    %3dd %2dm %6.3fs %c    ", StatLat, StatNS, lat.u, lat.m, lat.s, StatNS);
			if (StatLat) printf("%3dd %6.3fm %c", lat.u, lat.fm, StatNS);
			printf("\n");
		printf("  LON ");
			if (StatLat) printf("%9.5fd %c    %3dd %2dm %6.3fs %c    ", StatLon, StatEW, lon.u, lon.m, lon.s, StatEW);
			if (StatLat) printf("%3dd %6.3fm %c", lon.u, lon.fm, StatEW);
			printf("\n");
		printf("  ALT ");
			if (StatLat) printf("%1.0f m", StatAlt);
			printf("\n");
		printf(" TIME ");
			if (StatDay != -1) printf("%s %02d:%02d:%02.0f GPST", Week[StatDay], hms.u, hms.m, hms.s);
			printf("\n");
		printf("FIXES ");
			if (gps.fixes) printf("%d", gps.fixes);
			printf("\n");
		printf(" TTFF ");
			if (stats_ttff) printf("%d:%02d", stats_ttff / 60, stats_ttff % 60);
			printf("\n");
		printf("  RUN ");
			unsigned r = (timer_ms() - start)/1000;
			if (r >= 3600) printf("%02d:", r / 3600);
			printf("%02d:%02d", (r / 60) % 60, r % 60);
			printf("\n");
		printf("  MAP ");
			if (StatLat) printf("wikimapia.org/#lang=en&lat=%9.6f&lon=%9.6f&z=18&m=b",
				(StatNS=='S')? -StatLat:StatLat, (StatEW=='W')? -StatLon:StatLon);
			printf("\n");
		printf(" ECPU ");
			printf("%4.1f%% cmds %d/%d", ecpu_use(), ecpu_cmds, ecpu_tcmds);
			ecpu_cmds = ecpu_tcmds = 0;
			printf("\n");
		int offset = (int)(adc_clock - adc_clock_nom);
		printf("  DECIM: %d  FFT: %d -> %d  CCF: %5.3fs  MIN_SIG: %d  ADC_CLK: %.6f %s%d (%d)  ACQ: %d",
			decim, FFT_LEN, FFT_LEN/decim, fft_msec, min_sig,
			adc_clock/1000000.0, (offset >= 0)? "+":"", offset, gps.adc_clk_corr, gps.acquiring);
			printf("\n");

		printf("\n");

		NextTask("stat2");		
		TaskDump();
	}
}
