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

// Copyright (c) 2016 John Seamons, ZL/KF6VO

#include "types.h"
#include "config.h"
#include "kiwi.h"
#include "misc.h"
#include "peri.h"
#include "spi.h"
#include "coroutines.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#define	SEQ_SERNO_FILE "/root/kiwi.config/seq_serno.txt"

int eeprom_next_serno(next_serno_e type, int set_serno)
{
	int n, next_serno, serno = -1;
	FILE *fp;

	system("cp " SEQ_SERNO_FILE " " SEQ_SERNO_FILE ".bak");

	if ((fp = fopen(SEQ_SERNO_FILE, "r+")) == NULL) {
		sys_panic("eeprom_write fopen " SEQ_SERNO_FILE);
	}
	if ((n = fscanf(fp, "seq_serno %d\n", &serno)) != 1) {
		mprintf("eeprom_write fscanf %d %s\n", n, SEQ_SERNO_FILE);
		sys_panic("eeprom_write fscanf " SEQ_SERNO_FILE);
	}
	
	if (type == SERNO_READ)
		return serno;
	
	if (type == SERNO_WRITE)
		next_serno = serno = set_serno;
	
	if (type == SERNO_ALLOC)
		next_serno = serno + 1;
	
	rewind(fp);
	if ((n = fprintf(fp, "seq_serno %d\n", next_serno)) <= 0) {
		mprintf("eeprom_write fwrite %d %s\n", n, SEQ_SERNO_FILE);
		sys_panic("eeprom_write fwrite " SEQ_SERNO_FILE);
	}
	fclose(fp);
	
	mprintf("EEPROM: new seq serno = %d\n", serno);
	return serno;
	
}

struct eeprom_t {
	#define	EE_HEADER	0xAA5533EE
	u4_t header;
	
	#define	EE_FMT_REV	"A1"
	char fmt_rev[2];
	
	char board_name[32];
	char version[4];
	char mfg[16];
	char part_no[16];
	char week[2];
	char year[2];
	char assembly[4];
	char serial_no[4];
	
	u2_t n_pins;
	u2_t io_pins[EE_NPINS];
	
	#define	EE_MA_3V3	250
	#define	EE_MA_5INT	0
	#define	EE_MA_5EXT	0
	#define	EE_MA_DC	1500
	u2_t mA_3v3, mA_5int, mA_5ext, mA_DC;
	
	u1_t free[1];
} __attribute__((packed));

static eeprom_t eeprom;

//#define EEPROM_DEV			"/sys/bus/i2c/devices/1-0054/eeprom"
//#define EEPROM_DEV_WRMODE	"r+"

#define EEPROM_DEV			"/root/eeprom.bin"
#define EEPROM_DEV_WRMODE	"w+"

int eeprom_check()
{
	eeprom_t *e = &eeprom;
	FILE *fp;
	int n;
	
	if ((fp = fopen(EEPROM_DEV, "r")) == NULL) {
		if (errno == ENOENT) {
			mprintf("EEPROM: no file %s\n", EEPROM_DEV);
			return -1;
		}
		sys_panic("fopen " EEPROM_DEV);
	}
	memset(e, 0, sizeof(eeprom_t));
	if ((n = fread(e, 1, sizeof(eeprom_t), fp)) < 0) {
		sys_panic("fread " EEPROM_DEV);
	}
	if (n < sizeof(eeprom_t)) {
		printf("fread warning, read short n=%d\n", n);
	}
	printf("EEPROM read n=%d %s\n", n, EEPROM_DEV);
	fclose(fp);
	
	u4_t header = FLIP32(e->header);
	if (header != EE_HEADER) {
		mprintf("EEPROM: bad header, got 0x%08x want 0x%08x\n", header, EE_HEADER);
		return -1;
	}
	
	char serial_no[sizeof(e->serial_no)+1];
	GET_CHARS(e->serial_no, serial_no);
	int serno = -1;
	n = sscanf(serial_no, "%d", &serno);
	mprintf("EEPROM: serial_no %d 0x%08x <%s>\n", serno, serial_no, serial_no);
	if (n != 1) {
		mprintf("EEPROM: scan failed\n");
		return -1;
	}
	return serno;
}

void eeprom_write()
{
	int n, next_serno = eeprom_next_serno(SERNO_ALLOC, 0);
	FILE *fp;
	
	eeprom_t *e = &eeprom;
	
	e->header = FLIP32(EE_HEADER);
	SET_CHARS(e->fmt_rev, EE_FMT_REV, ' ');
	
	SET_CHARS(e->board_name, "KiwiSDR", ' ');
	SET_CHARS(e->version, "v0.5", ' ');		// fixme
	SET_CHARS(e->mfg, "Seed.cc", ' ');
	SET_CHARS(e->part_no, "(fixme part_no)", ' ');

	SET_CHARS(e->week, "WW", ' ');
	SET_CHARS(e->year, "YY", ' ');
	SET_CHARS(e->assembly, "&&&&", ' ');
	sprintf(e->serial_no, "%4d", next_serno);	// caution: leaves '\0' at start of next field (n_pins)
	
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
	
	if ((fp = fopen(EEPROM_DEV, EEPROM_DEV_WRMODE)) == NULL)
		sys_panic("eeprom_write fopen " EEPROM_DEV);
	if (fwrite(e, 1, sizeof(eeprom_t), fp) != sizeof(eeprom_t))
		sys_panic("eeprom_write fwrite " EEPROM_DEV);
	fclose(fp);
}
