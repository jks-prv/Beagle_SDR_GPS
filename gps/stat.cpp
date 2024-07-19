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

#include "kiwi.h"
#include "mode.h"
#include "clk.h"
#include "misc.h"
#include "gps.h"
#include "spi.h"
#include "printf.h"
#include "services.h"
#include "rx_util.h"

typedef struct {
	int lo_dop, ca_sft;
	int lo_dop2, ca_dop2;
	int pe, pp, pl;
	double lo, f_lo, s_lo, l_lo, h_lo, d_lo;
	double ca, f_ca, s_ca, l_ca, h_ca, d_ca;
	char dir;
	int to;
	double dbug_d1, dbug_d2;
	int dbug, dbug_i1, dbug_i2, dbug_i3;
} stats_t;

stats_t stats[GPS_MAX_CHANS];

gps_t gps;

static bool ready = FALSE;


///////////////////////////////////////////////////////////////////////////////////////////////

const char *Week[] = {
    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

///////////////////////////////////////////////////////////////////////////////////////////////

static int decim, min_sig;
static float fft_msec;

///////////////////////////////////////////////////////////////////////////////////////////////

void GPSstat_init() {
    for (int n=0; n < gps_chans; n++) gps.ch[n].sat = -1;
}

void GPSstat(STAT st, double d, int i, int j, int k, int m, double d2) {
	stats_t *s;
	gps_chan_t *c;
	
	switch(st) {
        case STAT_PARAMS:
            decim = i;
            min_sig = j;
            gps.FFTch = gps.StatDay = -1;
			gps.start = timer_ms();
            ready = TRUE;
            break;
            
        case STAT_ACQUIRE:
            gps.acquiring = i? true:false;
            if (!gps.acquiring) gps.FFTch = -1;
            break;
            
        case STAT_SAT:
			if (i < 0 || i >= gps_chans) return;
			s = &stats[i];
			c = &gps.ch[i];
            c->sat = j;
            c->snr = (int) d;
            gps.FFTch = k? i:-1;
            if (m) fft_msec = (float)m/1e6;
			s->l_lo = s->l_ca = 99999999; s->h_lo = s->h_ca = -99999999;
			s->dir = ' '; s->to = 0;
            break;
            
        case STAT_POWER:
			if (i < 0 || i >= gps_chans) return;
			c = &gps.ch[i];
        	
        	// signal lost
        	if (d < 0) {
        	    c->rssi = 0;
        	    c->sat = -1;
        		c->snr = c->wdog = c->ca_unlocked = c->hold = c->sub = c->sub_renew = 0;
        		c->has_soln = c->novfl = c->az = c->el = c->frames = c->par_errs = 0;
        		c->age[0] = '\0';
        	} else {
                c->rssi = (int) sqrt(d);
                c->gain = j;
        	}
            break;
            
        case STAT_WDOG:
			if (i < 0 || i >= gps_chans) return;
			c = &gps.ch[i];
            c->wdog = j;
            c->hold = k;
            c->ca_unlocked = m;
            break;
            
        case STAT_SUB:
			if (i < 0 || i >= gps_chans) return;
			c = &gps.ch[i];
        	if (j > PARITY) break;
        	if (j <= 0) {
        	    c->sub = c->sub_renew = 0;
        	} else
        	if (j == PARITY) {
				c->parity = 1;
				c->par_errs++;
        	} else {
				j--;
				if (c->sub & 1<<j) {
					// already on, blink it
					c->sub_renew |= 1<<j;
					c->sub &= ~(1<<j);
				} else {
					c->sub |= 1<<j;
				}
			    c->frames++;
			}
			c->alert = k;
            break;
            
        case STAT_NOVFL:
			if (i < 0 || i >= gps_chans) return;
			c = &gps.ch[i];
        	c->novfl++;
        	break;

        case STAT_LAT:
        	gps.sgnLat = d;
            gps.StatLat = fabs(d);
            gps.StatNS = d<0?'S':'N';
            
            if (!gps.ttff) {
            	gps.ttff = (timer_ms() - gps.start)/1000;
            	
            	// run kiwisdr.com registration so kiwi.gps.json gets updated
            	wakeup_reg_kiwisdr_com(WAKEUP_REG);
            }
            break;
        case STAT_LON:
        	gps.sgnLon = d;

            if (kiwi.test_marine_mobile) {
                static int delta, adj;
                if (delta == 0) delta = -3;
                gps.sgnLon += adj;
                adj += delta;
                if (gps.sgnLon < 10) delta = 3;
                if (gps.sgnLon > 170) delta = -3;
                latLon_t loc;
                char grid6[6 + SPACE_FOR_NULL];
                loc.lat = gps.sgnLat;
                loc.lon = gps.sgnLon;
                latLon_to_grid6(&loc, grid6);
                printf("TEST_MM %f %f %s\n", gps.sgnLat, gps.sgnLon, grid6);
            }
            
            gps.StatLon = fabs(d);
            gps.StatEW = d<0?'W':'E';
            on_GPS_solution();
            break;
        case STAT_ALT:
            gps.StatAlt = d;
            break;
        case STAT_TIME:
        	if (d == 0) return;
            gps.StatDay = d/(60*60*24);
            if (gps.StatDay < 0 || gps.StatDay >= 7) { gps.StatDay = -1; break; }
            gps.StatDaySec = d-(60*60*24)*gps.StatDay;
            gps.StatWeekSec = d;
            break;
        case STAT_DOP:
			if (i < 0 || i >= gps_chans) return;
			s = &stats[i];
        	s->lo_dop = j;
        	s->ca_sft = k;
            break;
        case STAT_EPL:
			if (i < 0 || i >= gps_chans) return;
			s = &stats[i];
        	s->pe = j;
        	s->pp = k;
        	s->pl = m;
            break;
        case STAT_LO:
			if (i < 0 || i >= gps_chans) return;
			s = &stats[i];
            s->d_lo = d - s->lo;
            s->lo = d;
            break;
        case STAT_CA:
			if (i < 0 || i >= gps_chans) return;
			s = &stats[i];
            s->d_ca = d - s->ca;
            s->ca = d;
            break;
        case STAT_DEBUG:
			if (i < 0 || i >= gps_chans) return;
			s = &stats[i];
        	s->dbug=1;
            s->dbug_d1 = d;
            s->dbug_d2 = d2;
            s->dbug_i1 = j;
            s->dbug_i2 = k;
            s->dbug_i3 = m;
            break;
        case STAT_SOLN:
            gps.soln_type = i;
            for (int ch = 0; ch < gps_chans; ch++) {
                gps.ch[ch].has_soln = j & (1 << ch);
            }
            break;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////

#define show3(p, c, v) if (p->c) printf("%3d ", p->v); else printf("    ");
#define show4(p, c, v) if (p->c) printf("%4d ", p->v); else printf("     ");
#define show5(p, c, v) if (p->c) printf("%5d ", p->v); else printf("      ");
#define show6(p, c, v) if (p->c) printf("%6d ", p->v); else printf("       ");
#define show66(p1, c, p2, v) if (p1->c) printf("%6d ", p2->v); else printf("       ");
#define show7(p, c, v) if (p->c) printf("%7d ", p->v); else printf("        ");
#define showf7_1(p, c, v) if (p->c) printf("%7.1f ", p->v); else printf("        ");
#define showf7_4(p, c, v) if (p->c) printf("%7.4f ", p->v); else printf("        ");

void StatTask(void *param) {

	int i, j;

	while (!ready) TaskSleepMsec(1000);
	
	while (1) {
		UMS lat(gps.StatLat), lon(gps.StatLon);
		UMS hms(gps.StatDaySec/60/60);

		TaskSleepMsec(1000);
		
		// only print solutions
		if (print_stats & STATS_GPS_SOLN) {
			static u4_t fixes;
			if (gps.fixes > fixes) {
				fixes = gps.fixes;
				if (gps.StatLat) printf("wikimapia.org/#lang=en&lat=%9.6f&lon=%9.6f&z=18&m=b\n",
					gps.sgnLat, gps.sgnLon);
			}
			continue;
		}

		printf("\n\n\n\n\n\n");
#define SHOW_DOP_CA
#ifdef SHOW_DOP_CA
		printf("   CH     PRN    SNR    DOP     CA   RSSI   GAIN   BITS   WDOG     SUB  NOVFL");
		//              55555 666666 666666 666666 666666 666666 666666 666666
#else
		//      12345 * 12345 123456 123456 123456 123456 123456 Up12345 123456 #########
		printf("   CH     PRN    SNR   RSSI   GAIN   BITS   WDOG     SUB  NOVFL");
#endif
		printf("\n");

		for (i=0; i < gps_chans; i++) {
			stats_t *s = &stats[i];
			gps_chan_t *c = &gps.ch[i];
			char c1, c2;
			double snew;
			
			printf("%5d %c ", i+1, (gps.FFTch == i)? '*':' ');
			if (c->sat >= 0) {
			    printf("  %4s ", PRN(c->sat));
			} else printf("       ");
			show6(c, snr, snr);
#ifdef SHOW_DOP_CA
			show66(c, snr, s, lo_dop);
			show66(c, snr, s, ca_sft);
#endif
			show6(c, rssi, rssi);
			show6(c, rssi, gain);
			show6(c, hold, hold);
			show6(c, rssi, wdog);
			
			printf("%c", c->ca_unlocked? 'U':' ');
			printf("%c", c->parity? 'p':' ');
			c->parity = 0;
			for (j=4; j>=0; j--) {
				printf("%c", (c->sub & (1<<j))? '1'+j:' ');
				if (c->sub_renew & (1<<j)) {
					c->sub |= 1<<j;
					c->sub_renew &= ~(1<<j);
				}
			}

			show7(c, novfl, novfl);
			printf(" ");
			
#if 0
			if (c->rssi) printf("%6d:E %6d:P %6d:L ", s->pe/1000, s->pp/1000, s->pl/1000);
#endif
#if 0
			show3(rssi, lo_dop);
			show5(rssi, ca_sft);
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
			for (j=0; j < c->rssi*50/3000; j++) printf("#");
			printf ("\n");
		}
		printf("\n");

		NextTask("stat1");

		printf(" SATS ");
			if (gps.tracking) printf("tracking %d", gps.tracking);
			if (gps.good) printf(", good %d", gps.good);
			printf("\n");
		printf("  LAT ");
			if (gps.StatLat) printf("%9.5fd %c    %3dd %2dm %6.3fs %c    ", gps.StatLat, gps.StatNS, lat.u, lat.m, lat.s, gps.StatNS);
			if (gps.StatLat) printf("%3dd %6.3fm %c", lat.u, lat.fm, gps.StatNS);
			printf("\n");
		printf("  LON ");
			if (gps.StatLat) printf("%9.5fd %c    %3dd %2dm %6.3fs %c    ", gps.StatLon, gps.StatEW, lon.u, lon.m, lon.s, gps.StatEW);
			if (gps.StatLat) printf("%3dd %6.3fm %c", lon.u, lon.fm, gps.StatEW);
			printf("\n");
		printf("  ALT ");
			if (gps.StatLat) printf("%1.0f m", gps.StatAlt);
			printf("\n");
		printf(" TIME ");
			if (gps.StatDay != -1) printf("%s %02d:%02d:%02.0f GPST", Week[gps.StatDay], hms.u, hms.m, hms.s);
			printf("\n");
		printf("FIXES ");
			if (gps.fixes) printf("%d", gps.fixes);
			printf("\n");
		printf(" TTFF ");
			if (gps.ttff) printf("%d:%02d", gps.ttff / 60, gps.ttff % 60);
			printf("\n");
		printf("  RUN ");
			unsigned r = (timer_ms() - gps.start)/1000;
			if (r >= 3600) printf("%02d:", r / 3600);
			printf("%02d:%02d", (r / 60) % 60, r % 60);
			printf("\n");
		printf("  MAP ");
			if (gps.StatLat) printf("wikimapia.org/#lang=en&lat=%9.6f&lon=%9.6f&z=18&m=b",
				gps.sgnLat, gps.sgnLon);
			printf("\n");
		printf(" ECPU ");
			printf("%4.1f%% cmds %d/%d", ecpu_use(), ecpu_cmds, ecpu_tcmds);
			ecpu_cmds = ecpu_tcmds = 0;
			printf("\n");
		int offset = (int)(adc_clock_system() - ADC_CLOCK_NOM);
		printf("  DECIM: %d  FFT: %d  CCF: %5.3fs  MIN_SIG: %d  ADC_CLK: %.6f %s%d (%d)  ACQ: %d",
			decim, FFT_LEN, fft_msec, min_sig,
			adc_clock_system()/1e6, (offset >= 0)? "+":"", offset, clk.adc_gps_clk_corrections, gps.acquiring);
			printf("\n");

		printf("\n");

		NextTask("stat2");		
		if (print_stats & STATS_TASK) TaskDump(PRINTF_REG);
	}
}
