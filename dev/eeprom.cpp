/*
--------------------------------------------------------------------------------
This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public
License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.
This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.
You should have received a copy of the GNU Library General Public
License along with this library; if not, write to the
Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
Boston, MA  02110-1301, USA.
--------------------------------------------------------------------------------
*/

// Copyright (c) 2016-2023 John Seamons, ZL4VO/KF6VO

#include "types.h"
#include "config.h"
#include "kiwi.h"
#include "misc.h"
#include "str.h"
#include "peri.h"
#include "spi.h"
#include "coroutines.h"
#include "eeprom.h"
#include "fpga.h"
#include "printf.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

//#define EEPROM_TEST_BAD_PART
//#define EEPROM_TEST_VER_UPDATE

#define	SEQ_SERNO_FILE "/root/kiwi.config/seq_serno.txt"
#define	SEQ_START		"1014"
#define SERNO_MAX       99999

#ifdef TEST_FLAG_EEPROM
void eeprom_test()
{
    if (test_flag & 1) {
        eeprom_check();
        real_printf("r"); fflush(stdout);
    }
    
    // full eeprom write takes just under 2 secs, so only start write every 4 secs (called at 1 Hz)
    static int wcount;
    if ((test_flag & 2) && (wcount++ & 3) == 0) {
	    eeprom_write(SERNO_WRITE, 1006);
        real_printf("W"); fflush(stdout);
	}
}
#endif

int eeprom_next_serno(next_serno_e type, int set_serno)
{
	int n, next_serno = 0, serno;
	FILE *fp;

	system("cp " SEQ_SERNO_FILE " " SEQ_SERNO_FILE ".bak");

retry:
	if ((fp = fopen(SEQ_SERNO_FILE, "r+")) == NULL) {
		if (errno == ENOENT) {
			system("echo " SEQ_START " > " SEQ_SERNO_FILE);
		    mlprintf("EEPROM next: no file, resetting to serial_no %s\n", SEQ_START);
			goto retry;
		}
		mlprintf("EEPROM next: open %s %s\n", SEQ_SERNO_FILE, strerror(errno));
		return -1;
	}
	
	serno = -1;
	n = fscanf(fp, "%d\n", &serno);
	if (n != 1 || serno < 0 || serno > SERNO_MAX) {
	    // recover if the file contains bad data somehow
	    rewind(fp);
	    char s[64];
	    fgets(s, sizeof(s), fp);
	    int sl = strlen(s);
	    if (sl && s[sl-1] == '\n') s[sl-1] = '\0';
		mlprintf("EEPROM next: serial_no file bad scan: \"%s\"(%d)\n", s, sl);
        system("echo " SEQ_START " > " SEQ_SERNO_FILE);
		mlprintf("EEPROM next: resetting file to serial_no %s\n", SEQ_START);
        goto retry;
	}
	
	if (type == SERNO_READ)
		return serno;
	
	if (type == SERNO_WRITE)
		next_serno = serno = set_serno;
	
	if (type == SERNO_ALLOC)
		next_serno = serno + 1;
	
	rewind(fp);
	if ((n = fprintf(fp, "%d\n", next_serno)) <= 0) {
		mlprintf("EEPROM next: write %s\n", strerror(errno));
		return -1;
	}
	fclose(fp);
	
	mprintf("EEPROM next: new seq serno = %d\n", serno);
	return serno;
	
}

eeprom_t eeprom;
static bool debian7 = true;

#define EEPROM_DEV_DEBIAN7  "/sys/bus/i2c/devices/1-0054/eeprom"

#ifdef CPU_TDA4VM
 #define EEPROM_DEV     "/sys/bus/i2c/devices/5-0054/eeprom"
#elif defined(CPU_AM5729)
 #define EEPROM_DEV     "/sys/bus/i2c/devices/3-0054/eeprom"
#elif defined(CPU_BCM2837)
 #define EEPROM_DEV     "/sys/bus/i2c/devices/1-0050/eeprom"
#else
 #define EEPROM_DEV     "/sys/bus/i2c/devices/2-0054/eeprom"
#endif

int eeprom_check(model_e *model)
{
	eeprom_t *e = &eeprom;
	const char *fn;
	FILE *fp;
	int n;
	if (model != NULL) *model = (model_e) -1;
	
	fn = EEPROM_DEV_DEBIAN7;
	fp = fopen(fn, "r");
	
	if (fp == NULL && errno == ENOENT) {
		fn = EEPROM_DEV;
		debian7 = false;
		fp = fopen(fn, "r");
	}
	
	if (fp == NULL) {
		mlprintf("EEPROM check: open %s %s\n", fn, strerror(errno));
		return -1;
	}

	memset(e, 0, sizeof(eeprom_t));
	if ((n = fread(e, 1, sizeof(eeprom_t), fp)) < 0) {
		mlprintf("EEPROM check: read %s\n", strerror(errno));
		return -1;
	}
	if (n < sizeof(eeprom_t)) {
		mlprintf("EEPROM check: WARNING short read %d\n", n);
		return -1;
	}
	fclose(fp);
	
	u4_t header = FLIP32(e->header);
	if (header != EE_HEADER) {
		mlprintf("EEPROM check: bad header, got 0x%08x want 0x%08x\n", header, EE_HEADER);
		return -1;
	}
	
	#ifdef EEPROM_TEST_BAD_PART
        if (test_flag & 1) memset(e, 0, sizeof(eeprom_t));
	    //return -1;
	#endif
	
    char serial_no[sizeof(e->serial_new) + SPACE_FOR_NULL];
    GET_CHARS(e->serial_new, serial_no);

    int serno = -1;
    n = sscanf(serial_no, "%d", &serno);
    if (n != 1 || serno <= 0 || serno > 99999999) {
        mlprintf("EEPROM check: read serial_no \"%s\" %d\n", serial_no, serno);
        mlprintf("EEPROM check: scan failed (serno)\n");
        return -1;
    }
    if (serno >= 10000 && serno <= 19999) {
        mlprintf("EEPROM check: old format serial_no compatibility %d => %d\n", serno, serno - 10000);
        serno -= 10000;
    }
	
	char part_no[sizeof(e->part_no) + SPACE_FOR_NULL];
	GET_CHARS(e->part_no, part_no);

	int _model = -1;
	if (strcmp(part_no, "KIWISDR10       ") == 0) {
	    _model = KiwiSDR_1;
	} else {
        n = sscanf(part_no, "KiwiSDR %d", &_model);
        if (n != 1 || _model <= 0) {
            mlprintf("EEPROM check: read model \"%s\" %d\n", part_no, _model);
            mlprintf("EEPROM check: scan failed (model)\n");
            return -1;
        }
    }
    mprintf("EEPROM check: read model KiwiSDR %d, serial_no %d\n", _model, serno);
	if (model != NULL) *model = (model_e) _model;
	
	return serno;
}

void eeprom_write(next_serno_e type, int serno, int model)
{
	int n;
	const char *fn;
	FILE *fp;
	
	#ifdef EEPROM_TEST_BAD_PART
	    if (test_flag & 1) return;
	#endif
	
	eeprom_t *e = &eeprom;
	memset(e, 0, sizeof(eeprom_t));		// v1.1 fix: zero unused e->io_pins

	e->header = FLIP32(EE_HEADER);
	SET_CHARS(e->fmt_rev, EE_FMT_REV, ' ');
	
	SET_CHARS(e->board_name, "KiwiSDR", ' ');

	#ifdef EEPROM_TEST_VER_UPDATE
	if (test_flag) {
		SET_CHARS(e->version, "v0.9", ' ');
	} else
	#endif

	SET_CHARS(e->version, "v1.1", ' ');

	SET_CHARS(e->mfg, "kiwisdr.com", ' ');
	
	SET_CHARS(e->part_no, stprintf("KiwiSDR %d", model), ' ');

	// we use the WWYY fields as "date of last EEPROM write" rather than "date of production"
	time_t t = utc_time();
	struct tm tm; gmtime_r(&t, &tm);
	int ord_date = tm.tm_yday + 1;		// convert 0..365 to 1..366
	int weekday = tm.tm_wday? tm.tm_wday : 7;	// convert Sunday = 0 to = 7

	// formula from en.wikipedia.org/wiki/ISO_week_date#Calculating_the_week_number_of_a_given_date
	int ww = (ord_date - weekday + 10) / 7;
	if (ww < 1) ww = 1;
	if (ww > 52) ww = 52;
	kiwi_snprintf_buf_plus_space_for_null(e->week, "%2d", ww);	// caution: leaves '\0' at start of next field (year)

	int yy = tm.tm_year - 100;
	kiwi_snprintf_buf_plus_space_for_null(e->year, "%2d", yy);	// caution: leaves '\0' at start of next field (assembly)

	SET_CHARS(e->assembly, "0001", ' ');

	if (type == SERNO_ALLOC)
		serno = eeprom_next_serno(SERNO_ALLOC, 0);

    // 4 to 8-digit serno migration: just write the specified value into the 8-digit field
	//kiwi_snprintf_buf_plus_space_for_null(e->serial_old, "%4d", serno);	// caution: leaves '\0' at start of next field (n_pins)
    kiwi_snprintf_buf_plus_space_for_null(e->serial_new, "%8d", serno);	// caution: leaves '\0' at start of next field (n_pins)
	
	e->n_pins = FLIP16(2*26);
	int pin;
	for (pin = 0; pin < EE_NPINS; pin++) {
		pin_t *p = &eeprom_pins[pin];
		u4_t off = EE_PINS_OFFSET_BASE + pin*2;
		if (p->gpio.eeprom_off) {
			e->io_pins[pin] = FLIP16(p->attrs);
			assert(p->gpio.eeprom_off == off);
			//printf("EEPROM %3d 0x%x %s-%02d 0x%04x\n", off, off,
			//	(p->gpio.pin & P9)? "P9":"P8", p->gpio.pin & PIN_BITS, p->attrs);
		} else {
			//printf("EEPROM %3d 0x%x not used\n", off, off);
		}
	}
	
	e->mA_3v3 = FLIP16(EE_MA_3V3);
	e->mA_5int = FLIP16(EE_MA_5INT);
	e->mA_5ext = FLIP16(EE_MA_5INT);
	e->mA_DC = FLIP16(EE_MA_DC);

    // NB: have to change WP before fopen() and after fclose() because writes don't
    // fully complete (flush) until after fclose() returns!
	ctrl_clr_set(CTRL_EEPROM_WP, 0);    // takes effect about 600 us before last write
	kiwi_msleep(1);

	fn = debian7? EEPROM_DEV_DEBIAN7 : EEPROM_DEV;

	if ((fp = fopen(fn, "r+")) == NULL) {
		mlprintf("EEPROM write: open %s %s\n", fn, strerror(errno));
		return;
	}

	if ((n = fwrite(e, 1, sizeof(eeprom_t), fp)) != sizeof(eeprom_t)) {
		mlprintf("EEPROM write WARNING: write %s\n", strerror(errno));
	}

	mprintf("EEPROM write: wrote %d bytes\n", n);
	fclose(fp);
	kiwi_msleep(1);
	ctrl_clr_set(0, CTRL_EEPROM_WP);    // takes effect about 200 us after last write
}

void eeprom_update()
{
	int n, serno;
	eeprom_t *e = &eeprom;

	if ((serno = eeprom_check()) == -1) {
		printf("eeprom_update: BAD serno\n");
		return;
	}
	
	char v[sizeof(e->version) + SPACE_FOR_NULL];
	GET_CHARS(e->version, v);
	if (v[0] != 'v' || !isdigit(v[1]) || v[2] != '.' || !isdigit(v[3])) {
		printf("eeprom_update: BAD version string \"%s\"\n", v);
		return;
	}
	
	float vf = 0;
	n = sscanf(v, "v%f", &vf);
	if (n != 1 || vf < 0.5 || vf >= 3.0) {
		printf("eeprom_update: version scan failed \"%s\"\n", v);
		return;
	}
	
	#ifdef EEPROM_TEST_VER_UPDATE
	if (test_flag)
		vf = 0.9;
	#endif

	if (vf >= 1.1) {
		//printf("eeprom_update: OKAY v%.1f\n", vf);
		return;
	}

	printf("UPDATING EEPROM from v%.1f to v1.1\n", vf);
	eeprom_write(SERNO_WRITE, serno);
}
