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

// Copyright (c) 2015-2022 John Seamons, ZL4VO/KF6VO

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
#include "fpga.h"
#include "leds.h"

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <ctype.h>

u2_t ctrl_get()
{
	SPI_MISO *ctrl = get_misc_miso();
	
	spi_get_noduplex(CmdCtrlGet, ctrl, sizeof(ctrl->word[0]));
	u2_t rv = ctrl->word[0];
	release_misc_miso();
	return rv;
}

static void stat_dump(const char *id)
{
    stat_reg_t stat = stat_get();
    printf("%s stat_get=0x%04x %4s|%3s|%5s|%s\n", id, stat.word,
        (stat.word & 0x80)? "READ" : "",
        (stat.word & 0x40)? "CLK" : "",
        (stat.word & 0x20)? "SHIFT" : "",
        (stat.word & 0x10)? "D1" : "D0"
        );
}

// NB: the eCPU maintains a latched shadow value of SET_CTRL[]
// This means if a bit is given in the set parameter it persists until given in the clr parameter.
// But it doesn't have to be given in subsequent set parameters to persist due to the latching.
void ctrl_clr_set(u2_t clr, u2_t set)
{
	spi_set_noduplex(CmdCtrlClrSet, clr, set);
	//printf("ctrl_clr_set(0x%04x, 0x%04x) ctrl_get=0x%04x ", clr, set, ctrl_get());
	//stat_dump("SET");
}

void ctrl_positive_pulse(u2_t bits)
{
    //printf("ctrl_positive_pulse 0x%x\n", bits);
	spi_set_noduplex(CmdCtrlClrSet, bits, bits);
	//printf("ctrl_positive_pulse(0x%04x, 0x%04x) ctrl_get=0x%04x ", bits, bits, ctrl_get());
	//stat_dump("RISE");
	spi_set_noduplex(CmdCtrlClrSet, bits, 0);
	//printf("ctrl_positive_pulse(0x%04x, 0x%04x) ctrl_get=0x%04x ", bits, 0,    ctrl_get());
	//stat_dump("FALL");
}

void ctrl_set_ser_dev(u2_t ser_dev)
{
    //printf("ctrl_set_ser_dev 0x%x\n", ser_dev);
    ctrl_clr_set(CTRL_SER_MASK, ser_dev & CTRL_SER_MASK);
}

void ctrl_clr_ser_dev()
{
    //printf("ctrl_clr_ser_dev\n");
    ctrl_clr_set(CTRL_SER_MASK, CTRL_SER_NONE);
}

stat_reg_t stat_get()
{
    SPI_MISO *status = get_misc_miso();
    stat_reg_t stat;
    
    spi_get_noduplex(CmdGetStatus, status, sizeof(stat));
	release_misc_miso();
    stat.word = status->word[0];

    return stat;
}

u2_t getmem(u2_t addr)
{
	SPI_MISO *mem = get_misc_miso();
	
	memset(mem->word, 0x55, sizeof(mem->word));
	spi_get_noduplex(CmdGetMem, mem, 4, addr);
	release_misc_miso();
	assert(addr == mem->word[1]);
	
	return mem->word[0];
}

void setmem(u2_t addr, u2_t data)
{
	spi_set_noduplex(CmdSetMem, addr, data);
}

void printmem(const char *str, u2_t addr)
{
	printf("%s %04x: %04x\n", str, addr, (int) getmem(addr));
}

void fpga_panic(int code, const char *s)
{
    lprintf("FPGA panic: code=%d %s\n", code, s);
    led_clear(0);
    
    for (int i = 0; i < 5; i++) {
        for (int j = 0; j < 5; j++) {
            led_set(1,0,1,0, 500);
            led_set(0,1,0,1, 500);
        }
        led_clear(1000);
        #define LB(c,v) (((c) & (v))? 1:0)
        led_set(LB(code,8), LB(code,4), LB(code,2), LB(code,1), 5000);
        led_clear(1000);
    }
    
    led_flash_all(32);
    led_clear(1000);
    led_set_debian();
    panic("FPGA panic");
}


////////////////////////////////
// FPGA DNA
////////////////////////////////

u64_t fpga_dna()
{
    #define CTRL_DNA_CLK    CTRL_SER_CLK
    #define CTRL_DNA_READ   CTRL_SER_LE_CSN
    #define CTRL_DNA_SHIFT  CTRL_SER_DATA

    ctrl_set_ser_dev(CTRL_SER_DNA);
        ctrl_clr_set(CTRL_DNA_CLK | CTRL_DNA_SHIFT, CTRL_DNA_READ);
        ctrl_positive_pulse(CTRL_DNA_CLK);
        ctrl_clr_set(CTRL_DNA_CLK | CTRL_DNA_READ, CTRL_DNA_SHIFT);
        u64_t dna = 0;
        for (int i=0; i < 64; i++) {
            stat_reg_t stat = stat_get();
            dna = (dna << 1) | ((stat.word & STAT_DNA_DATA)? 1ULL : 0ULL);
            ctrl_positive_pulse(CTRL_DNA_CLK);
        }
        ctrl_clr_set(CTRL_DNA_CLK | CTRL_DNA_READ | CTRL_DNA_SHIFT, 0);
    ctrl_clr_ser_dev();
    return dna;
}


////////////////////////////////
// FPGA init
////////////////////////////////

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
		if (i == 1*M) fpga_panic(1, "FPGA_INIT never went LOW");
		spin_us(100);
		GPIO_WRITE_BIT(FPGA_PGM, 1);	// de-assert FPGA_PGM
		for (i=0; i < 1*M && GPIO_READ_BIT(FPGA_INIT) == 0; i++)	// wait for FPGA_INIT to complete
			;
		if (i == 1*M) fpga_panic(2, "FPGA_INIT never went HIGH");
	}

	// FPGA configuration bitstream
	char *file;
	asprintf(&file, "%sKiwiSDR.%s.bit", background_mode? "/usr/local/bin/":"", fpga_file);
    char *sum = non_blocking_cmd_fmt(NULL, "sum %s", file);
    lprintf("firmware: %s %.5s\n", file, kstr_sp(sum));
    fp = fopen(file, "rb");
    if (!fp) fpga_panic(3, "fopen config");
    kstr_free(sum);
    kiwi_asfree(file);
    kiwi_asfree(fpga_file);

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
		fpga_panic(4, "FPGA config CRC error");

	spin_ms(100);

	// download embedded CPU program binary
	const char *aout = background_mode? "/usr/local/bin/kiwid.aout" : (BUILD_DIR "/gen/kiwi.aout");
    fp = fopen(aout, "rb");
    if (!fp) fpga_panic(5, "fopen aout");


    // download first 2k words via SPI hardware boot (SPIBUF_B limit)
    n = S2B(2048);

    assert(n <= sizeof(code.bytes));
    n = fread(code.msg, 1, n, fp);
    spi_dev(SPI_BOOT, &code, SPI_B2X(n), &readback, SPI_B2X(sizeof(readback.status) + n));
    SPI_MISO *ping = &SPI_SHMEM->pingx_miso;

    #ifdef PLATFORM_beaglebone
        // more favorable timing for kiwi.v:BBB_MISO
        if (spi_mode == -1)
            spi_mode = (kiwi.model == KiwiSDR_1)? SPI_KIWISDR_1_MODE : SPI_KIWISDR_2_MODE;
        for (i = 0; i < 10; i++) {
            spin_ms(100);
            spi_dev_mode(spi_mode);
            spin_ms(100);
            printf("ping..\n");
            memset(ping, 0, sizeof(*ping));
            spi_get_noduplex(CmdPing, ping, 2);
            if (ping->word[0] != 0xcafe) {
                lprintf("FPGA not responding: 0x%04x\n", ping->word[0]);
                evSpi(EC_DUMP, EV_SPILOOP, -1, "main", "dump");
                spi_mode = (spi_mode == SPI_KIWISDR_1_MODE)? SPI_KIWISDR_2_MODE : SPI_KIWISDR_1_MODE;
            } else
                break;
        }
        if (i == 10)
            fpga_panic(6, "FPGA not responding to ping1");
    #else
        spin_ms(100);
        printf("ping..\n");
        memset(ping, 0, sizeof(*ping));
        spi_get_noduplex(CmdPing, ping, 2);
        if (ping->word[0] != 0xcafe) {
            lprintf("FPGA not responding: 0x%04x\n", ping->word[0]);
            evSpi(EC_DUMP, EV_SPILOOP, -1, "main", "dump");
            fpga_panic(6, "FPGA not responding to ping1");
        }
    #endif

	// download second 1k words via program command transfers
    // NB: not needed now that SPI buf is same size as eCPU code memory (2k words / 4k bytes)
    /*
    j = n;
    n = fread(code2, 1, 4096-n, fp);
    if (n < 0) panic("fread");
    if (n) {
		printf("load second 1K CPU RAM n=%d(%d) n+2048=%d(%d)\n", n, n/2, n+2048, (n+2048)/2);
		for (i=0; i<n; i+=2) {
			u2_t insn = code2[i/2];
			u4_t addr = j+i;
			spi_set_noduplex(CmdSetMem, insn, addr);
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
	*/
    fclose(fp);

	printf("ping2..\n");
	spi_get_noduplex(CmdPing2, ping, 2);
	if (ping->word[0] != 0xbabe) {
		lprintf("FPGA not responding: 0x%04x\n", ping->word[0]);
		evSpi(EC_DUMP, EV_SPILOOP, -1, "main", "dump");
        fpga_panic(7, "FPGA not responding to ping2");
	}

	#ifdef USE_GPS
        strcpy(&gps.a[13], "[Y5EyEWjA65g");
    #endif

	stat_reg_t stat = stat_get();
	//printf("stat.word=0x%04x fw_id=0x%x fpga_ver=0x%x stat_user=0x%x fpga_id=0x%x\n",
	//    stat.word, stat.fw_id, stat.fpga_ver, stat.stat_user, stat.fpga_id);

	if (stat.fpga_id != fpga_id) {
		lprintf("FPGA ID %d, expecting %d\n", stat.fpga_id, fpga_id);
		fpga_panic(8, "mismatch");
	}

	if (stat.fw_id != (FW_ID >> 12)) {
		lprintf("eCPU firmware ID %d, expecting %d\n", stat.fw_id, FW_ID >> 12);
		fpga_panic(9, "mismatch");
	}

	lprintf("FPGA version %d\n", stat.fpga_ver);
	if (stat.fpga_ver != FPGA_VER) {
		lprintf("\tbut expecting %d\n", FPGA_VER);
		fpga_panic(10, "mismatch");
	}

    spi_dev_init2();
}
