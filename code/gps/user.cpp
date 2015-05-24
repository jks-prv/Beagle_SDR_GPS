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
#include "LiquidCrystal.h"
#include "gps.h"
#include "spi.h"

static bool ready = FALSE;

typedef struct {
	int prn;
	int snr, snr2;
	int rssi, gain;
	int wdog, ca_unlocked;
	int hold;
	int sub;
	int lo_dop, ca_dop;
	int lo_dop2, ca_dop2;
	int pe, pp, pl;
	double lo, f_lo, s_lo, l_lo, h_lo, d_lo;
	double ca, f_ca, s_ca, l_ca, h_ca, d_ca;
	char dir;
	int to;
	double dbug_d1, dbug_d2;
	int dbug, dbug_i1, dbug_i2, dbug_i3;
} stats_t;

stats_t stats[GPS_CHANS];
int stats_chans, stats_fixes, stats_fft = -1;
unsigned stats_ttff;


///////////////////////////////////////////////////////////////////////////////////////////////

enum {
    LCD_D4=0, LCD_D5=1, LCD_D6=2, LCD_D7=3,
    LCD_EN=4,
    LCD_RS=5
};

///////////////////////////////////////////////////////////////////////////////////////////////

void Print::digitalWrite(int pin, int state) {
    static int reg;
    reg&=~(1<<pin);
    reg|=state<<pin;
    //if (pin==LCD_EN) spi_set(CmdSetLCD, reg);
}

void Print::delayMicroseconds(int n) {
    if (n>1) usleep(n);
}

///////////////////////////////////////////////////////////////////////////////////////////////

struct DISPLAY : LiquidCrystal {
    DISPLAY () : LiquidCrystal(LCD_RS, LCD_EN, LCD_D4, LCD_D5, LCD_D6, LCD_D7) {
        begin(16, 2);
        createBars();
    }

    void drawForm(int);
    void drawData(int);
    void createBars();
    void writeAt(int x, int y, const char *s);
};

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

static int    StatBars[GPS_CHANS];

static double StatSNR, StatSec, StatLat, StatLon, StatAlt;
static int    StatPRN, StatDay, StatNS,  StatEW,  StatChans;
static int decim, min_sig;
static float fft_msec;

///////////////////////////////////////////////////////////////////////////////////////////////

void UserStat(STAT st, double d, int i, int j, int k, int m, double d2) {

	stats_t *s = &stats[i];
	switch(st) {
        case STAT_PARAMS:
            decim = i;
            min_sig = j;
            ready = TRUE;
            break;
        case STAT_PRN:
            s->prn = j;
            s->snr = (int) d;
            stats_fft = k? i:-1;
            if (m) fft_msec = (float)m/1000000.0;
			s->l_lo = s->l_ca = 99999999; s->h_lo = s->h_ca = -99999999;
			s->dir = ' '; s->to = 0;
            break;
        case STAT_POWER:
        	s->rssi = (int) sqrt(d);
        	s->gain = j;
        	if (d == 0) {
        		s->prn = s->snr = s->snr2 = s->wdog = s->ca_unlocked = s->hold = s->sub = 0;
        	}
            break;
        case STAT_WDOG:
            s->wdog = j;
            s->hold = k;
            s->ca_unlocked = m;
            break;
        case STAT_SUB:
            s->sub |= 1<<(j-1);
            if (j != 6) s->sub &= ~(1<<(6-1));	// clear parity
            break;
        case STAT_CHANS:
            stats_chans = i;
            break;
        case STAT_LAT:
            StatLat = fabs(d);
            StatNS = d<0?'S':'N';
            stats_fixes++;
            break;
        case STAT_LON:
            StatLon = fabs(d);
            StatEW = d<0?'W':'E';
            break;
        case STAT_ALT:
            StatAlt = d;
            StatChans = i;
            EventRaise(EVT_POS);
            break;
        case STAT_TIME:
            StatDay = d/(60*60*24);
            StatSec = d-(60*60*24)*StatDay;
            break;
        case STAT_TTFF:
        	stats_ttff = (unsigned) i;
            break;
        case STAT_DOP:
        	s->lo_dop = j;
        	s->ca_dop = k;
            break;
        case STAT_DOP2:
            s->snr2 = (int) d;
        	s->lo_dop2 = j;
        	s->ca_dop2 = k;
            break;
        case STAT_EPL:
        	s->pe = j;
        	s->pp = k;
        	s->pl = m;
            break;
        case STAT_LO:
            s->d_lo = d - s->lo;
            s->lo = d;
            break;
        case STAT_CA:
            s->d_ca = d - s->ca;
            s->ca = d;
            break;
        case STAT_DEBUG:
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

void DISPLAY::writeAt(int x, int y, const char *s) {
    setCursor(x, y);
    while(*s) write(*s++);
}

///////////////////////////////////////////////////////////////////////////////////////////////

void DISPLAY::createBars() {
    const char *bars[8] = {
        "\x00\x00\x00\x00\x00\x00\x00\x1f",
        "\x00\x00\x00\x00\x00\x00\x1f\x1f",
        "\x00\x00\x00\x00\x00\x1f\x1f\x1f",
        "\x00\x00\x00\x00\x1f\x1f\x1f\x1f",
        "\x00\x00\x00\x1f\x1f\x1f\x1f\x1f",
        "\x00\x00\x1f\x1f\x1f\x1f\x1f\x1f",
        "\x00\x1f\x1f\x1f\x1f\x1f\x1f\x1f",
        "\x1f\x1f\x1f\x1f\x1f\x1f\x1f\x1f"
    };

    for (int i=0; i<8; i++)
        createChar(i, (uint8_t*) bars[i]);
 }

///////////////////////////////////////////////////////////////////////////////////////////////

void DISPLAY::drawForm(int page) {
    clear();
    switch(page) {
        case -2:
            writeAt(0, 0, "  Homemade GPS  ");
            writeAt(0, 1, "A.Holme May 2013");
            break;
        case -1:
            writeAt(0, 0, "Shutdown");
            break;
        case 0:
            writeAt(0, 0, "PRN __ ___");
            writeAt(0, 1, "____________");
            break;
        case 1:
            writeAt(0, 0, "_     __._____ _");
            writeAt(0, 1, "_     __._____ _");
            break;
        case 2:
            writeAt(0, 0, "__\xDF __\xDF __.___ _");
            writeAt(0, 1, "__\xDF __\xDF __.___ _");
            break;
        case 3:
            break;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////

void DISPLAY::drawData(int page) {
    char s[80];
    switch(page) {
        case 0:
            if (EventCatch(EVT_PRN)) {
                sprintf(s, "%2d %3.0f", StatPRN, StatSNR);
                writeAt(4, 0, s);
            }
            if (EventCatch(EVT_BARS)) {
                setCursor(0, 1);
                for (int i=0; i<GPS_CHANS; i++) write(StatBars[i]);
            }
            break;
        case 1:
            if (EventCatch(EVT_POS)) {
                sprintf(s, "%-5d %8.5f %c", StatChans, StatLat, StatNS);
                writeAt(0, 0, s);
                sprintf(s, "%-5.0f %8.5f %c", StatAlt, StatLon, StatEW);
                writeAt(0, 1, s);
            }
            break;
        case 2:
            if (EventCatch(EVT_POS)) {
                UMS lat(StatLat), lon(StatLon);
                sprintf(s, "%2d\xDF%3d\xDF%7.3f %c", lat.u, lat.m, lat.s, StatNS);
                writeAt(0, 0, s);
                sprintf(s, "%2d\xDF%3d\xDF%7.3f %c", lon.u, lon.m, lon.s, StatEW);
                writeAt(0, 1, s);
            }
            break;
        case 3:
            if (EventCatch(EVT_TIME)) {
                UMS hms(StatSec/60/60);
                sprintf(s, "%s %02d:%02d:%02.0f", Week[StatDay], hms.u, hms.m, hms.s);
                writeAt(0, 0, s);
            }
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////

#define show3(c, v) if (s->c) printf("%3d ", s->v); else printf("    ");
#define show4(c, v) if (s->c) printf("%4d ", s->v); else printf("     ");
#define show5(c, v) if (s->c) printf("%5d ", s->v); else printf("      ");
#define show6(c, v) if (s->c) printf("%6d ", s->v); else printf("       ");
#define showf7_1(c, v) if (s->c) printf("%7.1f ", s->v); else printf("        ");
#define showf7_4(c, v) if (s->c) printf("%7.4f ", s->v); else printf("        ");

void UserTask() {

	int i, j, prn;
	unsigned start = timer_ms();

	while (1) {
		UMS lat(StatLat), lon(StatLon);
		UMS hms(StatSec/60/60);

		while (!ready) NextTask();

#if DECIM_CMP
		printf("\n\n\n\n\n\n   CH    PRN    SNR     CA    ERR   RSSI   GAIN   BITS   WDOG     SUB");
#else
		printf("\n\n\n\n\n\n   CH    PRN    SNR   RSSI   GAIN   BITS   WDOG     SUB");
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
			printf("%c", (s->sub & (1<<5))? 'p':' ');
			for (j=4; j>=0; j--) printf("%c", (s->sub & (1<<j))? '1'+j:' ');
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
		printf("\n GOOD ");
			if (stats_chans) printf("%d %s", stats_chans, (stats_chans==1)? "sat":"sats");
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
			if (StatLat) printf("%s %02d:%02d:%02.0f GPST", Week[StatDay], hms.u, hms.m, hms.s);
			printf("\n");
		printf("FIXES ");
			if (stats_fixes) printf("%d", stats_fixes);
			printf("\n");
		printf(" TTFF ");
			if (stats_ttff) printf("%d:%02d", stats_ttff / 60, stats_ttff % 60);
			printf("\n");
		printf("  RUN ");
			unsigned r = (timer_ms() - start)/1000;
			if (r > 3600) printf("%02d:", r / 3600);
			printf("%02d:%02d", (r / 60) % 60, r % 60);
			printf("\n");
		printf("  MAP ");
			if (StatLat) printf("wikimapia.org/#lang=en&lat=%9.6f&lon=%9.6f&z=18&m=b",
				(StatNS=='S')? -StatLat:StatLat, (StatEW=='W')? -StatLon:StatLon);
			printf("\n");
		printf(" ECPU ");
			printf("%4.1f%%", ecpu_use());
			printf("\n");
		printf("  DECIM: %d  FFT: %d -> %d  CCF: %5.3fs  MIN_SIG: %d",
			decim, FFT_LEN, FFT_LEN/decim, fft_msec, min_sig);
			printf("\n");
		//TaskDump();

		nonSim_TimerWait(1000);
	}
}

static unsigned Signals;

void EventRaise(unsigned sigs) {
    Signals |= sigs;
}

unsigned EventCatch(unsigned sigs) {
    sigs &= Signals;
    Signals -= sigs;
    return sigs;
}
