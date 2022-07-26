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

// Copyright (c) 2015-2022 John Seamons, ZL/KF6VO

#include "types.h"
#include "config.h"
#include "kiwi.h"
#include "clk.h"
#include "mem.h"
#include "misc.h"
#include "web.h"
#include "peri.h"
#include "spi.h"
#include "spi_dev.h"
#include "spi_pio.h"
#include "gps.h"
#include "coroutines.h"
#include "debug.h"

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <ctype.h>

//#define TEST_FLAG_SPI_RFI

bool background_mode = FALSE;

static SPI_MOSI code, zeros;
static SPI_MISO readback;
static u1_t bbuf[2048];
static u2_t code2[4096];

void fpga_init() {

    FILE *fp;
    int n, i, j;

	gpio_setup(FPGA_PGM, GPIO_DIR_OUT, 1, PMUX_OUT_PU, 0);		// i.e. FPGA_PGM is an INPUT, active LOW
	gpio_setup(FPGA_INIT, GPIO_DIR_BIDIR, GPIO_HIZ, PMUX_IO_PU, 0);
	
	#if 0
	    while (1) {
            GPIO_WRITE_BIT(FPGA_PGM, 1);
            real_printf("w1r%d ", GPIO_READ_BIT(FPGA_INIT)); fflush(stdout);
            kiwi_usleep(250000);
            GPIO_WRITE_BIT(FPGA_PGM, 0);
            real_printf("w0r%d ", GPIO_READ_BIT(FPGA_INIT)); fflush(stdout);
	    }
	#endif

	#if 0
        GPIO_OUTPUT(SPIn_CS1);
        //GPIO_OUTPUT(P816);
	    while (1) {
            //real_printf("."); fflush(stdout);
            GPIO_CLR_BIT(SPIn_CS1);
            //GPIO_CLR_BIT(P816);
            kiwi_usleep(1000);
            GPIO_SET_BIT(SPIn_CS1);
            //GPIO_SET_BIT(P816);
            kiwi_usleep(1000);
	    }
	#endif

	spi_dev_init(spi_clkg, spi_speed);

#ifdef TEST_FLAG_SPI_RFI
	if (test_flag)
		real_printf("TEST_FLAG_SPI_RFI..\n");
	else
#endif
	{
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
	}

	// FPGA configuration bitstream
    fp = fopen(stprintf("%sKiwiSDR.%s.bit", background_mode? "/usr/local/bin/":"", fpga_file) , "rb");
    if (!fp) panic("fopen config");
    kiwi_ifree(fpga_file);

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
    
    #ifdef TEST_FLAG_SPI_RFI
    	if (test_flag) {
            //real_printf("."); fflush(stdout);
            kiwi_usleep(3000);
    		if (n <= 0) {
				rewind(fp);
                #ifdef CPU_AM5729
                    //real_printf("later SPI2_CH0CONF=0x%08x\n", SPI0_CONF);
                #endif
				continue;
			}
    	} else
    #endif
    	{
        	if (n <= 0) break;
        }
        
        #if 0
            static int first;
            if (!first) real_printf("before spi_dev SPI2_CH0CONF=0x%08x SPI2_CH0CTRL=0x%08x\n", SPI0_CONF, SPI0_CTRL);
        #endif
            spi_dev(SPI_FPGA, &code, SPI_B2X(n), &readback, SPI_B2X(n));
        #if 0
            if (!first) real_printf("after spi_dev SPI2_CH0CONF=0x%08x SPI2_CH0CTRL=0x%08x\n", SPI0_CONF, SPI0_CTRL);
            first = 1;
        #endif
    }

	// keep clocking until config/startup finishes
	n = sizeof(zeros.bytes);
    spi_dev(SPI_FPGA, &zeros, SPI_B2X(n), &readback, SPI_B2X(n));

    fclose(fp);

	if (GPIO_READ_BIT(FPGA_INIT) == 0)
		panic("FPGA config CRC error");

	spin_ms(100);

	// download embedded CPU program binary
	const char *aout = background_mode? "/usr/local/bin/kiwid.aout" : (BUILD_DIR "/gen/kiwi.aout");
    fp = fopen(aout, "rb");
    if (!fp) panic("fopen aout");


    // download first 2k words via SPI hardware boot (SPIBUF_B limit)
    n = S2B(2048);

    assert(n <= sizeof(code.bytes));
    n = fread(code.msg, 1, n, fp);
    spi_dev(SPI_BOOT, &code, SPI_B2X(n), &readback, SPI_B2X(sizeof(readback.status) + n));
    
	spin_ms(100);
	printf("ping..\n");
	SPI_MISO *ping = &SPI_SHMEM->pingx_miso;
	memset(ping, 0, sizeof(*ping));
	#ifdef USE_GPS
        strcpy(&gps.a[13], "[Y5EyEWjA65g");
    #endif
	spi_get_noduplex(CmdPing, ping, 2);
	if (ping->word[0] != 0xcafe) {
		lprintf("FPGA not responding: 0x%04x\n", ping->word[0]);
		evSpi(EC_DUMP, EV_SPILOOP, -1, "main", "dump");
		kiwi_exit(-1);
	}

    // FIXME: remove
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
	spi_get_noduplex(CmdPing2, ping, 2);
	if (ping->word[0] != 0xbabe) {
		lprintf("FPGA not responding: 0x%04x\n", ping->word[0]);
		evSpi(EC_DUMP, EV_SPILOOP, -1, "main", "dump");
		kiwi_exit(-1);
	}

	stat_reg_t stat = stat_get();
	//printf("stat.word=0x%04x fw_id=0x%x fpga_ver=0x%x stat_user=0x%x fpga_id=0x%x\n",
	//    stat.word, stat.fw_id, stat.fpga_ver, stat.stat_user, stat.fpga_id);

	if (stat.fpga_id != fpga_id) {
		lprintf("FPGA ID %d, expecting %d\n", stat.fpga_id, fpga_id);
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
}
