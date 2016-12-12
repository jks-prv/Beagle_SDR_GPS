#include "types.h"
#include "config.h"
#include "kiwi.h"
#include "misc.h"
#include "web.h"
#include "peri.h"
#include "spi_dev.h"
#include "gps.h"
#include "coroutines.h"
#include "pru_realtime.h"
#include "debug.h"

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <ctype.h>

bool background_mode = FALSE;
bool adc_clock_enable = FALSE;
double adc_clock_nom, adc_clock, adc_clock_offset;

static SPI_MOSI code, zeros;
static SPI_MISO readback;
static u1_t bbuf[2048];
static u2_t code2[4096];

void fpga_init() {

    FILE *fp;
    int n, i, j;
    static SPI_MISO ping;

	gpio_setup(FPGA_PGM, GPIO_DIR_OUT, 1, PMUX_OUT_PU, 0);		// i.e. FPGA_PGM is an INPUT, active LOW
	gpio_setup(FPGA_INIT, GPIO_DIR_BIDIR, GPIO_HIZ, PMUX_IO_PU, 0);

#ifdef LOAD_FPGA
	spi_dev_init(spi_clkg, spi_speed);

    // Reset FPGA
	GPIO_WRITE_BIT(FPGA_PGM, 0);	// assert FPGA_PGM LOW
	for (i=0; i < 1*M && GPIO_READ_BIT(FPGA_INIT) == 1; i++)	// wait for FPGA_INIT to acknowledge init process
		;
	if (i == 1*M) panic("FPGA_INIT never went LOW");
	spin_us(100);
	GPIO_WRITE_BIT(FPGA_PGM, 1);	// de-assert FPGA_PGM
	for (i=0; i < 1*M && GPIO_READ_BIT(FPGA_INIT) == 0; i++)	// wait for FPGA_INIT to complete
		;
	if (i == 1*M) panic("FPGA_INIT never went HIGH");

	const char *config = background_mode? "/usr/local/bin/KiwiSDRd.bit" : "./KiwiSDR.bit";
    fp = fopen(config, "rb");		// FPGA configuration bitstream
    if (!fp) panic("fopen config");

	// byte-swap config data to match ended-ness of SPI
    while (1) {
    #ifdef SPI_8
        n = fread(code.bytes, 1, 2048, fp);
    #endif
    #ifdef SPI_16
        n = fread(bbuf, 1, 2048, fp);
    	for (i=0; i < 2048; i += 2) {
    		code.bytes[i+0] = bbuf[i+1];
    		code.bytes[i+1] = bbuf[i+0];
    	}
    #endif
    #ifdef SPI_32
        n = fread(bbuf, 1, 2048, fp);
    	for (i=0; i < 2048; i += 4) {
    		code.bytes[i+0] = bbuf[i+3];
    		code.bytes[i+1] = bbuf[i+2];
    		code.bytes[i+2] = bbuf[i+1];
    		code.bytes[i+3] = bbuf[i+0];
    	}
    #endif
        if (n <= 0) break;
        spi_dev(SPI_FPGA, &code, SPI_B2X(n), &readback, SPI_B2X(n));
    }

	// keep clocking until config/startup finishes
	n = sizeof(zeros.bytes);
    spi_dev(SPI_FPGA, &zeros, SPI_B2X(n), &readback, SPI_B2X(n));

    fclose(fp);

	if (GPIO_READ_BIT(FPGA_INIT) == 0)
		panic("FPGA config CRC error");
	
	spin_ms(100);
#endif

	// download embedded CPU program binary
	const char *aout = background_mode? "/usr/local/bin/kiwid.aout" : "e_cpu/kiwi.aout";
    fp = fopen(aout, "rb");
    if (!fp) panic("fopen aout");

	// download first 1k words via SPI hardware boot (NSPI_RX limit)
	n = 2048;
    n = fread(code.msg, 1, n, fp);

    spi_dev(SPI_BOOT, &code, SPI_B2X(n), &readback, SPI_B2X(sizeof(readback.status) + n));
    
	spin_ms(100);
	printf("ping..\n");
	memset(&ping, 0, sizeof(ping));
	spi_get_noduplex(CmdPing, &ping, 2);
	if (ping.word[0] != 0xcafe) {
		lprintf("FPGA not responding: 0x%04x\n", ping.word[0]);
		evSpi(EC_DUMP, EV_SPILOOP, -1, "main", "dump");
		xit(-1);
	}

	// download second 1k words via program command transfers
    j = n;
    n = fread(code2, 1, 4096-n, fp);
    if (n < 0) panic("fread");
    if (n) {
		printf("load second 1K CPU RAM n=%d(%d) n+2048=%d(%d)\n", n, n/2, n+2048, (n+2048)/2);
		for (i=0; i<n; i+=2) {
			u2_t insn = code2[i/2];
			u4_t addr = j+i;
			spi_set_noduplex(CmdLoad, insn, addr);
			u2_t readback = getmem(addr);
			if (insn != readback) {
				printf("%04x:%04x:%04x\n", addr, insn, readback);
				readback = getmem(addr+2);
				printf("%04x:%04x:%04x\n", addr+2, insn, readback);
				readback = getmem(addr+4);
				printf("%04x:%04x:%04x\n", addr+4, insn, readback);
				readback = getmem(addr+6);
				printf("%04x:%04x:%04x\n", addr+6, insn, readback);
			}
			assert(insn == readback);
		}
		//printf("\n");
	}
    fclose(fp);

	printf("ping2..\n");
	spi_get_noduplex(CmdPing2, &ping, 2);
	if (ping.word[0] != 0xbabe) {
		lprintf("FPGA not responding: 0x%04x\n", ping.word[0]);
		evSpi(EC_DUMP, EV_SPILOOP, -1, "main", "dump");
		xit(-1);
	}

	spi_get_noduplex(CmdGetStatus, &ping, 2);
	union {
		u2_t word;
		struct {
			u2_t fpga_id:4, clock_id:4, fpga_ver:4, fw_id:3, ovfl:1;
		};
	} stat;
	stat.word = ping.word[0];

	if (stat.fpga_id != FPGA_ID) {
		lprintf("FPGA ID %d, expecting %d\n", stat.fpga_id, FPGA_ID);
		panic("mismatch");
	}

	if (stat.fw_id != (FW_ID >> 12)) {
		lprintf("eCPU firmware ID %d, expecting %d\n", stat.fw_id, FW_ID >> 12);
		panic("mismatch");
	}

	lprintf("FPGA version %d\n", stat.fpga_ver);
	if (stat.fpga_ver != FPGA_VER) {
		lprintf("\tbut expecting %d\n", FPGA_VER);
		panic("mismatch");
	}
	
	switch (stat.clock_id) {
	
	case 0:
		adc_clock_nom = ADC_CLOCK_66M_NOM;
		adc_clock = ADC_CLOCK_66M_TYP;
		adc_clock_enable = TRUE;
		break;

	default:
		panic("FPGA returned bad clock select");
	}
}
