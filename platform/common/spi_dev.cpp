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

#include "types.h"
#include "config.h"
#include "kiwi.h"
#include "misc.h"
#include "cfg.h"
#include "peri.h"
#include "coroutines.h"
#include "debug.h"
#include "spi.h"
#include "spi_dev.h"
#include "spi_pio.h"

#ifdef CPU_AM3359
 #include "spi_pio.h"
#endif

#include <fcntl.h>
#include <sys/mman.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>

#ifdef __linux__
    #include <linux/types.h>
    #include <linux/spi/spidev.h>
#else
    #include "devl_spidev.h"
#endif

#ifdef SPI_SHMEM_DISABLE
    static spi_shmem_t spi_shmem;
    spi_shmem_t *spi_shmem_p = &spi_shmem;
#endif

#define	NOT(bit)	0	// documentation aid

static int spi_fd = -1;
static bool init = false;
static int speed;

static int use_async;

#if defined(CPU_BCM2837)
    // RPI only supports 8 bit per word
    // It is fine to support SPI_32 FPGA firmware
    #undef SPI_BPW
    #define SPI_BPW 8

    #define SPI_INLINE_BUF_SIZE 2048
#endif

static void _spi_dev(SPI_SEL sel, SPI_MOSI *mosi, int tx_xfers, SPI_MISO *miso, int rx_xfers)
{
#if defined(CPU_BCM2837)
    SPI_T tx_buffer[SPI_INLINE_BUF_SIZE];
#endif

    SPI_T *txb = mosi->msg;
    SPI_T *rxb = miso->msg;

#if defined(CPU_BCM2837)
    if (tx_xfers < SPI_INLINE_BUF_SIZE) {
		for (int i = 0; i < tx_xfers; i++)
			tx_buffer[i] = __builtin_bswap32(txb[i]);
		txb = tx_buffer;
	}
	else
	{
		SPI_T *t = (SPI_T*)malloc(sizeof(SPI_T)* tx_xfers);

		for (int i = 0; i < tx_xfers; i++)
			t[i] = __builtin_bswap32(txb[i]);
		txb = t;
	}
#endif

	if (sel == SPI_BOOT) {
		GPIO_CLR_BIT(SPIn_CS1);
	}
	
	evSpiDev(EC_EVENT, EV_SPILOOP, -1, "spi_dev", evprintf("%s(%d) T%dx R%dx", (sel != SPI_HOST)? "BOOT" : cmds[mosi->data.cmd], mosi->data.cmd, tx_xfers, rx_xfers));

	if (use_spidev) {
		int spi_bytes = SPI_X2B(MAX(tx_xfers, rx_xfers));
		struct spi_ioc_transfer spi_tr;
		memset(&spi_tr, 0, sizeof spi_tr);
		spi_tr.tx_buf = (unsigned long) txb;
		spi_tr.rx_buf = (unsigned long) rxb;
		spi_tr.len = spi_bytes;
		spi_tr.delay_usecs = 0;
		spi_tr.speed_hz = speed;
		spi_tr.bits_per_word = SPI_BPW;		// zero also means 8-bits?
		spi_tr.cs_change = 0;
	
		int actual_rxbytes;
		if ((actual_rxbytes = ioctl(spi_fd, SPI_IOC_MESSAGE(1), &spi_tr)) < 0) sys_panic("SPI_IOC_MESSAGE");
		check(actual_rxbytes == spi_bytes);
		if (actual_rxbytes != spi_bytes) printf("actual_rxbytes %d spi_bytes %d\n", actual_rxbytes, spi_bytes);
	    //real_printf("SPI bytes=%d 0x%08x\n", actual_rxbytes, *rxb);
	} else {

	    #ifdef CPU_AM3359
	        spi_pio(mosi, tx_xfers, miso, rx_xfers);
        #else
            panic("SPI_PIO not supported by arch");
        #endif
	}

	if (sel == SPI_BOOT) {
		GPIO_SET_BIT(SPIn_CS1);
	}

#if defined(CPU_BCM2837)
	if (tx_xfers >= SPI_INLINE_BUF_SIZE)
	{
		free(txb);
	}
	for (int i = 0; i < rx_xfers; i++)
		rxb[i] = __builtin_bswap32(rxb[i]);
#endif
}

void spi_dev_func(int param)
{
    assert(SPI_SHMEM != NULL);
    spi_dev_ipc_t *ipc = &SPI_SHMEM->spi_dev_ipc;
    _spi_dev(ipc->sel, ipc->mosi, ipc->tx_xfers, ipc->miso, ipc->rx_xfers);
}

void spi_dev(SPI_SEL sel, SPI_MOSI *mosi, int tx_xfers, SPI_MISO *miso, int rx_xfers)
{
    assert(init);

    if (use_async && sel == SPI_HOST) {
        //kiwi_backtrace("spi_dev");
        assert(SPI_SHMEM != NULL);
        spi_dev_ipc_t *ipc = &SPI_SHMEM->spi_dev_ipc;
        ipc->sel = sel;
        ipc->mosi = mosi;
        ipc->tx_xfers = tx_xfers;
        ipc->miso = miso;
        ipc->rx_xfers = rx_xfers;

        //real_printf("PARENT spi_dev %d start...\n", seq);
        shmem_ipc_invoke(SIG_IPC_SPI);
    } else {
        _spi_dev(sel, mosi, tx_xfers, miso, rx_xfers);
    }
}

void spi_dev_mode(int spi_mode)
{
    u1_t mode = spi_mode | NOT(SPI_CS_HIGH) | NOT(SPI_NO_CS) | NOT(SPI_LSB_FIRST), check_mode;
    if (ioctl(spi_fd, SPI_IOC_WR_MODE, &mode) < 0) sys_panic("SPI_IOC_WR_MODE");
    if (ioctl(spi_fd, SPI_IOC_RD_MODE, &check_mode) < 0) sys_panic("SPI_IOC_RD_MODE");
    lprintf("SPIDEV: spi_mode 0x%x|0x%x\n", mode, check_mode);
}

static void _spi_dev_init(int spi_clkg, int spi_speed)
{
	// if not overridden in command line, set SPI speed according to configuration param
	if (spi_speed == SPI_48M) {
		bool error;
		int spi_clock = cfg_int("SPI_clock", &error, CFG_OPTIONAL);
		if (error || spi_clock == SPI_48M)
			spi_speed = SPI_48M;
		else
			spi_speed = SPI_24M;
	}
    
	if (use_spidev) {
		if (spi_fd != -1) close(spi_fd);
	
	    const char *spi_devname;
        #if defined(CPU_AM5729)
            spi_devname = "/dev/spidev1.0";
        #elif defined(CPU_AM3359)
            if (debian_ver <= 9)
                spi_devname = "/dev/spidev1.0";
            else
                spi_devname = "/dev/spidev0.0";
        #else
            spi_devname = "/dev/spidev0.0";
        #endif
		lprintf("### open SPI_DEV %s\n", spi_devname);
	
		spi_fd = open(spi_devname, O_RDWR);
		if (spi_fd < 0) {
            #if defined(CPU_TDA4VM)
                spi_devname = "/dev/spidev9.0";     // name changed with a kernel update around 11/2022
		        lprintf("### open SPI_DEV %s\n", spi_devname);
		        spi_fd = open(spi_devname, O_RDWR);
		        if (spi_fd < 0)
		            sys_panic("open spidev");
            #else
		        sys_panic("open spidev");
            #endif
        }
	
		u4_t max_speed = 0, check_speed;
		if (spi_speed == SPI_48M) max_speed = 48000000; else
		if (spi_speed == SPI_24M) max_speed = 24000000; else
		if (spi_speed == SPI_12M) max_speed = 12000000; else
		if (spi_speed == SPI_6M) max_speed = 6000000; else
		if (spi_speed == SPI_3M) max_speed = 3000000; else
		if (spi_speed == SPI_1_5M) max_speed = 1500000; else
			panic("unknown spi_speed");
		speed = max_speed;
		if (ioctl(spi_fd, SPI_IOC_WR_MAX_SPEED_HZ, &max_speed) < 0) sys_panic("SPI_IOC_WR_MAX_SPEED_HZ");
		check_speed = 0;
		if (ioctl(spi_fd, SPI_IOC_RD_MAX_SPEED_HZ, &check_speed) < 0) sys_panic("SPI_IOC_RD_MAX_SPEED_HZ");
		check(max_speed == check_speed);

		char bpw = SPI_BPW, check_bpw;
		if (ioctl(spi_fd, SPI_IOC_WR_BITS_PER_WORD, &bpw) < 0) sys_panic("SPI_IOC_WR_BITS_PER_WORD");
		check_bpw = -1;
		if (ioctl(spi_fd, SPI_IOC_RD_BITS_PER_WORD, &check_bpw) < 0) sys_panic("SPI_IOC_RD_BITS_PER_WORD");
		check(bpw == check_bpw);
		lprintf("SPIDEV: max_speed %d bpw %d\n", check_speed, check_bpw);

        spi_dev_mode(SPI_SETUP_MODE);
	} else {

	    #ifdef CPU_AM3359
	        spi_pio_init(spi_clkg, spi_speed);
        #else
            panic("SPI_PIO not supported by arch");
        #endif
	}

	spi_init();
	init = true;
}

void spi_dev_init(int spi_clkg, int spi_speed)
{
    #ifdef CPU_AM5729
        #if 0
            real_printf("before SPI2_MODULCTRL=0x%08x\n", SPI_MODULE);
    
            // IS DPE1 DPE0
            //  1    1    0 hw reset    0x00060000  rx=d1 d1tx=no  d0tx=yes (DPE sense is inverted)
            //  0    0    1 linux       0x20010fc4  rx=d0 d1tx=yes d0tx=no
            //                          0x20010fc4  clkg=1(ok) wl=0x1f(32-bits) epol=1(spien_L) clkd=1(div=2)
            u4_t conf = SPI0_CONF;
            real_printf("before anything SPI2_CH0CONF=0x%08x\n", conf);
            conf |=  0x00060000;  // ~IS ~DPE1
            conf &= ~0x00010000;  // DPE0
            SPI0_CONF = conf;
            real_printf("after force SPI2_CH0CONF=0x%08x\n", SPI0_CONF);
        #endif
    #endif

        _spi_dev_init(spi_clkg, spi_speed);

    #ifdef CPU_AM5729
        #if 0
            real_printf("after spi_dev_init SPI2_CHxCONF: 0x%08x 0x%08x 0x%08x 0x%08x\n",
                SPIn_CONF(0), SPIn_CONF(1), SPIn_CONF(2), SPIn_CONF(3));
            real_printf("after spi_dev_init SPI2_CHxCTRL: 0x%08x 0x%08x 0x%08x 0x%08x\n",
                SPIn_CTRL(0), SPIn_CTRL(1), SPIn_CTRL(2), SPIn_CTRL(3));
            conf = SPI0_CONF;
            conf |=  0x00060000;  // ~IS ~DPE1
            conf &= ~0x00010000;  // DPE0
            SPI0_CONF = conf;
            real_printf("after force2 SPI2_CH0CONF=0x%08x\n", SPI0_CONF);
        #endif
    #endif
}

// NB: start of SPI async task MUST be delayed until fpga init has
// finished with all calls to spi_dev_mode()
void spi_dev_init2()
{
    #ifdef SPI_SHMEM_DISABLE
        printf("SPI: CPU_%s SPI_SHMEM_DISABLE\n", ARCH_CPU_S);
    #else
        #ifdef CPU_AM3359
            if (use_spidev) {
                use_async = 1;  // using 2nd process ipc on uni-processor makes sense due to spi async stall problem
            }
        #endif

        #ifdef CPU_BCM2837
            use_spidev = 1;
            use_async = 1;
        #endif

        #ifdef CPU_AM5729
            use_spidev = 1;
            use_async = 1;
        #endif

        #ifdef CPU_TDA4VM
            use_spidev = 1;
            use_async = 1;
        #endif
    #endif

        if (use_async) {
            shmem_ipc_setup("kiwi.spi", SIG_IPC_SPI, spi_dev_func);
        }
}
