/*
 * prussdrv.c
 *
 * User space driver for PRUSS
 *
 * Copyright (C) 2012 Texas Instruments Incorporated - http://www.ti.com/
 *
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *    Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 *    Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the
 *    distribution.
 *
 *    Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
*/


/*
 * ============================================================================
 * Copyright (c) Texas Instruments Inc 2010-12
 *
 * Use of this software is controlled by the terms and conditions found in the
 * license agreement under which this software has been supplied or provided.
 * ============================================================================
 */

#include "misc.h"
#include <prussdrv.h>
#include "__prussdrv.h"
#include <pthread.h>
#include <stdio.h>

//#define __DEBUG
#ifdef __DEBUG
//#define DEBUG_PRINTF(FORMAT, ...) fprintf(stderr, FORMAT, ## __VA_ARGS__)
#define DEBUG_PRINTF(FORMAT, ...) lprintf(FORMAT, ## __VA_ARGS__)
#else
#define DEBUG_PRINTF(FORMAT, ...)
#endif

#define PRUSS_UIO_PRAM_PATH_LEN 128
#define PRUSS_UIO_PARAM_VAL_LEN 20
#define HEXA_DECIMAL_BASE 16

static tprussdrv prussdrv;

int __prussdrv_memmap_init(void)
{
    int i, fd;
    char hexstring[PRUSS_UIO_PARAM_VAL_LEN];

    if (prussdrv.mmap_fd == 0) {
        for (i = 0; i < NUM_PRU_HOSTIRQS; i++) {
            if (prussdrv.fd[i])
                break;
        }
        if (i == NUM_PRU_HOSTIRQS)
            return -1;
        else
            prussdrv.mmap_fd = prussdrv.fd[i];
    }
    
    for (i=0; i<UIO_OPEN_TIMEOUT; i++) {
		fd = open(PRUSS_UIO_DRV_PRUSS_BASE, O_RDONLY);
		if (fd >= 0) {
			read(fd, hexstring, PRUSS_UIO_PARAM_VAL_LEN);
			prussdrv.pruss_phys_base =
				strtoul(hexstring, NULL, HEXA_DECIMAL_BASE);
			close(fd);
			break;
		}
		sleep(1);
	}
	if (i==UIO_OPEN_TIMEOUT) {
		DEBUG_PRINTF("open %s: timeout\n", PRUSS_UIO_DRV_PRUSS_BASE);
		return -2;
	}
        
    fd = open(PRUSS_UIO_DRV_PRUSS_SIZE, O_RDONLY);
    if (fd >= 0) {
        read(fd, hexstring, PRUSS_UIO_PARAM_VAL_LEN);
        prussdrv.pruss_map_size =
            strtoul(hexstring, NULL, HEXA_DECIMAL_BASE);
        close(fd);
    } else
        return -3;

    prussdrv.pru0_dataram_base =
        mmap(0, prussdrv.pruss_map_size, PROT_READ | PROT_WRITE,
             MAP_SHARED, prussdrv.mmap_fd, PRUSS_UIO_MAP_OFFSET_PRUSS);
    prussdrv.version =
        __pruss_detect_hw_version((unsigned int *) prussdrv.pru0_dataram_base);

    switch (prussdrv.version) {
    case PRUSS_V1:
        {
            DEBUG_PRINTF(PRUSS_V1_STR "\n");
            prussdrv.pru0_dataram_phy_base = AM18XX_DATARAM0_PHYS_BASE;
            prussdrv.pru1_dataram_phy_base = AM18XX_DATARAM1_PHYS_BASE;
            prussdrv.intc_phy_base = AM18XX_INTC_PHYS_BASE;
            prussdrv.pru0_control_phy_base = AM18XX_PRU0CONTROL_PHYS_BASE;
            prussdrv.pru0_debug_phy_base = AM18XX_PRU0DEBUG_PHYS_BASE;
            prussdrv.pru1_control_phy_base = AM18XX_PRU1CONTROL_PHYS_BASE;
            prussdrv.pru1_debug_phy_base = AM18XX_PRU1DEBUG_PHYS_BASE;
            prussdrv.pru0_iram_phy_base = AM18XX_PRU0IRAM_PHYS_BASE;
            prussdrv.pru1_iram_phy_base = AM18XX_PRU1IRAM_PHYS_BASE;
        }
        break;
    case PRUSS_V2:
        {
            DEBUG_PRINTF(PRUSS_V2_STR "\n");
            prussdrv.pru0_dataram_phy_base = AM33XX_DATARAM0_PHYS_BASE;
            prussdrv.pru1_dataram_phy_base = AM33XX_DATARAM1_PHYS_BASE;
            prussdrv.intc_phy_base = AM33XX_INTC_PHYS_BASE;
            prussdrv.pru0_control_phy_base = AM33XX_PRU0CONTROL_PHYS_BASE;
            prussdrv.pru0_debug_phy_base = AM33XX_PRU0DEBUG_PHYS_BASE;
            prussdrv.pru1_control_phy_base = AM33XX_PRU1CONTROL_PHYS_BASE;
            prussdrv.pru1_debug_phy_base = AM33XX_PRU1DEBUG_PHYS_BASE;
            prussdrv.pru0_iram_phy_base = AM33XX_PRU0IRAM_PHYS_BASE;
            prussdrv.pru1_iram_phy_base = AM33XX_PRU1IRAM_PHYS_BASE;
            prussdrv.pruss_sharedram_phy_base =
                AM33XX_PRUSS_SHAREDRAM_BASE;
            prussdrv.pruss_cfg_phy_base = AM33XX_PRUSS_CFG_BASE;
            prussdrv.pruss_uart_phy_base = AM33XX_PRUSS_UART_BASE;
            prussdrv.pruss_iep_phy_base = AM33XX_PRUSS_IEP_BASE;
            prussdrv.pruss_ecap_phy_base = AM33XX_PRUSS_ECAP_BASE;
            prussdrv.pruss_miirt_phy_base = AM33XX_PRUSS_MIIRT_BASE;
            prussdrv.pruss_mdio_phy_base = AM33XX_PRUSS_MDIO_BASE;
        }
        break;
    default:
        DEBUG_PRINTF(PRUSS_UNKNOWN_STR "\n");
    }

#define ADDR_OFFSET(a,o) ((void*) ((char*) (a) + (o)))

    prussdrv.pru1_dataram_base = ADDR_OFFSET(prussdrv.pru0_dataram_base,
        prussdrv.pru1_dataram_phy_base - prussdrv.pru0_dataram_phy_base);
    prussdrv.intc_base = ADDR_OFFSET(prussdrv.pru0_dataram_base,
        prussdrv.intc_phy_base - prussdrv.pru0_dataram_phy_base);
    prussdrv.pru0_control_base = ADDR_OFFSET(prussdrv.pru0_dataram_base,
        prussdrv.pru0_control_phy_base - prussdrv.pru0_dataram_phy_base);
    prussdrv.pru0_debug_base = ADDR_OFFSET(prussdrv.pru0_dataram_base,
        prussdrv.pru0_debug_phy_base - prussdrv.pru0_dataram_phy_base);
    prussdrv.pru1_control_base = ADDR_OFFSET(prussdrv.pru0_dataram_base,
        prussdrv.pru1_control_phy_base - prussdrv.pru0_dataram_phy_base);
    prussdrv.pru1_debug_base = ADDR_OFFSET(prussdrv.pru0_dataram_base,
    	prussdrv.pru1_debug_phy_base - prussdrv.pru0_dataram_phy_base);
    prussdrv.pru0_iram_base = ADDR_OFFSET(prussdrv.pru0_dataram_base,
    	prussdrv.pru0_iram_phy_base - prussdrv.pru0_dataram_phy_base);
    prussdrv.pru1_iram_base = ADDR_OFFSET(prussdrv.pru0_dataram_base,
    	prussdrv.pru1_iram_phy_base - prussdrv.pru0_dataram_phy_base);
    if (prussdrv.version == PRUSS_V2) {
        prussdrv.pruss_sharedram_base = ADDR_OFFSET(prussdrv.pru0_dataram_base,
        	prussdrv.pruss_sharedram_phy_base - prussdrv.pru0_dataram_phy_base);
        prussdrv.pruss_cfg_base = ADDR_OFFSET(prussdrv.pru0_dataram_base,
        	prussdrv.pruss_cfg_phy_base - prussdrv.pru0_dataram_phy_base);
        prussdrv.pruss_uart_base = ADDR_OFFSET(prussdrv.pru0_dataram_base,
        	prussdrv.pruss_uart_phy_base - prussdrv.pru0_dataram_phy_base);
        prussdrv.pruss_iep_base = ADDR_OFFSET(prussdrv.pru0_dataram_base,
        	prussdrv.pruss_iep_phy_base - prussdrv.pru0_dataram_phy_base);
        prussdrv.pruss_ecap_base = ADDR_OFFSET(prussdrv.pru0_dataram_base,
        	prussdrv.pruss_ecap_phy_base - prussdrv.pru0_dataram_phy_base);
        prussdrv.pruss_miirt_base = ADDR_OFFSET(prussdrv.pru0_dataram_base,
        	prussdrv.pruss_miirt_phy_base - prussdrv.pru0_dataram_phy_base);
        prussdrv.pruss_mdio_base = ADDR_OFFSET(prussdrv.pru0_dataram_base,
        	prussdrv.pruss_mdio_phy_base - prussdrv.pru0_dataram_phy_base);
    }
#ifndef DISABLE_L3RAM_SUPPORT
    fd = open(PRUSS_UIO_DRV_L3RAM_BASE, O_RDONLY);
    if (fd >= 0) {
        read(fd, hexstring, PRUSS_UIO_PARAM_VAL_LEN);
        prussdrv.l3ram_phys_base =
            strtoul(hexstring, NULL, HEXA_DECIMAL_BASE);
        close(fd);
    } else
        return -4;


    fd = open(PRUSS_UIO_DRV_L3RAM_SIZE, O_RDONLY);
    if (fd >= 0) {
        read(fd, hexstring, PRUSS_UIO_PARAM_VAL_LEN);
        prussdrv.l3ram_map_size =
            strtoul(hexstring, NULL, HEXA_DECIMAL_BASE);
        close(fd);
    } else
        return -5;

    prussdrv.l3ram_base =
        mmap(0, prussdrv.l3ram_map_size, PROT_READ | PROT_WRITE,
             MAP_SHARED, prussdrv.mmap_fd, PRUSS_UIO_MAP_OFFSET_L3RAM);
#endif

    fd = open(PRUSS_UIO_DRV_EXTRAM_BASE, O_RDONLY);
    if (fd >= 0) {
        read(fd, hexstring, PRUSS_UIO_PARAM_VAL_LEN);
        prussdrv.extram_phys_base =
            strtoul(hexstring, NULL, HEXA_DECIMAL_BASE);
        close(fd);
    } else
        return -6;

    fd = open(PRUSS_UIO_DRV_EXTRAM_SIZE, O_RDONLY);
    if (fd >= 0) {
        read(fd, hexstring, PRUSS_UIO_PARAM_VAL_LEN);
        prussdrv.extram_map_size =
            strtoul(hexstring, NULL, HEXA_DECIMAL_BASE);
        close(fd);
    } else
        return -7;


    prussdrv.extram_base =
        mmap(0, prussdrv.extram_map_size, PROT_READ | PROT_WRITE,
             MAP_SHARED, prussdrv.mmap_fd, PRUSS_UIO_MAP_OFFSET_EXTRAM);

    return 0;

}

int prussdrv_init(void)
{
    memset(&prussdrv, 0, sizeof(prussdrv));
    return 0;

}

int prussdrv_open(unsigned int host_interrupt)
{
	int i, fd;
	
    char name[PRUSS_UIO_PRAM_PATH_LEN];
    if (!prussdrv.fd[host_interrupt]) {
        kiwi_snprintf_buf(name, "/dev/uio%d", host_interrupt);
        for (i=0; i<UIO_OPEN_TIMEOUT; i++) {
        	if ((fd = open(name, O_RDWR | O_SYNC)) >= 0)
        		break;
        	sleep(1);
        }
        if (i==UIO_OPEN_TIMEOUT) {
			DEBUG_PRINTF("open %s: timeout\n", name);
        	return -1;
        }
        prussdrv.fd[host_interrupt] = fd;
        return __prussdrv_memmap_init();
    } else {
        return -1;

    }
}

int prussdrv_version() {
    return prussdrv.version;
}

const char * prussdrv_strversion(int version) {
    switch (version) {
        case PRUSS_V1:
            return PRUSS_V1_STR;
        case PRUSS_V2:
            return PRUSS_V2_STR;
        default:
            return PRUSS_UNKNOWN_STR;
    }
}

int prussdrv_pru_reset(unsigned int prunum)
{
    unsigned int *prucontrolregs;
    if (prunum == 0)
        prucontrolregs = (unsigned int *) prussdrv.pru0_control_base;
    else if (prunum == 1)
        prucontrolregs = (unsigned int *) prussdrv.pru1_control_base;
    else
        return -1;
    *prucontrolregs = 0;
    return 0;
}

int prussdrv_pru_enable(unsigned int prunum)
{
    unsigned int *prucontrolregs;
    if (prunum == 0)
        prucontrolregs = (unsigned int *) prussdrv.pru0_control_base;
    else if (prunum == 1)
        prucontrolregs = (unsigned int *) prussdrv.pru1_control_base;
    else
        return -1;

    *prucontrolregs = 2;
    return 0;

}

int prussdrv_pru_disable(unsigned int prunum)
{
    unsigned int *prucontrolregs;
    if (prunum == 0)
        prucontrolregs = (unsigned int *) prussdrv.pru0_control_base;
    else if (prunum == 1)
        prucontrolregs = (unsigned int *) prussdrv.pru1_control_base;
    else
        return -1;
    *prucontrolregs = 1;
    return 0;

}

int prussdrv_pru_write_memory(unsigned int pru_ram_id,
                              unsigned int wordoffset,
                              const unsigned int *memarea,
                              unsigned int bytelength)
{
    unsigned int *pruramarea, i, wordlength;
    switch (pru_ram_id) {
    case PRUSS0_PRU0_IRAM:
        pruramarea = (unsigned int *) prussdrv.pru0_iram_base;
        break;
    case PRUSS0_PRU1_IRAM:
        pruramarea = (unsigned int *) prussdrv.pru1_iram_base;
        break;
    case PRUSS0_PRU0_DATARAM:
        pruramarea = (unsigned int *) prussdrv.pru0_dataram_base;
        break;
    case PRUSS0_PRU1_DATARAM:
        pruramarea = (unsigned int *) prussdrv.pru1_dataram_base;
        break;
    case PRUSS0_SHARED_DATARAM:
        if (prussdrv.version != PRUSS_V2)
            return -1;
        pruramarea = (unsigned int *) prussdrv.pruss_sharedram_base;
        break;
    default:
        return -1;
    }


    wordlength = (bytelength + 3) >> 2; //Adjust length as multiple of 4 bytes
    for (i = 0; i < wordlength; i++) {
        *(pruramarea + i + wordoffset) = *(memarea + i);
    }
    return wordlength;

}


int prussdrv_pruintc_init(const tpruss_intc_initdata *prussintc_init_data)
{
    unsigned int *pruintc_io = (unsigned int *) prussdrv.intc_base;
    unsigned int i, mask1, mask2;

    pruintc_io[PRU_INTC_SIPR1_REG >> 2] = 0xFFFFFFFF;
    pruintc_io[PRU_INTC_SIPR2_REG >> 2] = 0xFFFFFFFF;

    for (i = 0; i < (NUM_PRU_SYS_EVTS + 3) >> 2; i++)
        pruintc_io[(PRU_INTC_CMR1_REG >> 2) + i] = 0;
    for (i = 0;
         ((prussintc_init_data->sysevt_to_channel_map[i].sysevt != -1)
          && (prussintc_init_data->sysevt_to_channel_map[i].channel !=
              -1)); i++) {
        __prussintc_set_cmr(pruintc_io,
                            prussintc_init_data->sysevt_to_channel_map[i].
                            sysevt,
                            prussintc_init_data->sysevt_to_channel_map[i].
                            channel);
    }
    for (i = 0; i < (NUM_PRU_HOSTS + 3) >> 2; i++)
        pruintc_io[(PRU_INTC_HMR1_REG >> 2) + i] = 0;
    for (i = 0;
         ((prussintc_init_data->channel_to_host_map[i].channel != -1)
          && (prussintc_init_data->channel_to_host_map[i].host != -1));
         i++) {

        __prussintc_set_hmr(pruintc_io,
                            prussintc_init_data->channel_to_host_map[i].
                            channel,
                            prussintc_init_data->channel_to_host_map[i].
                            host);
    }

    pruintc_io[PRU_INTC_SITR1_REG >> 2] = 0x0;
    pruintc_io[PRU_INTC_SITR2_REG >> 2] = 0x0;


    mask1 = mask2 = 0;
    for (i = 0; prussintc_init_data->sysevts_enabled[i] != -1; i++) {
        if (prussintc_init_data->sysevts_enabled[i] < 32) {
            mask1 =
                mask1 + (1 << (prussintc_init_data->sysevts_enabled[i]));
        } else if (prussintc_init_data->sysevts_enabled[i] < 64) {
            mask2 =
                mask2 +
                (1 << (prussintc_init_data->sysevts_enabled[i] - 32));
        } else {
            DEBUG_PRINTF("Error: SYS_EVT%d out of range\n",
			 prussintc_init_data->sysevts_enabled[i]);
            return -1;
        }
    }
    pruintc_io[PRU_INTC_ESR1_REG >> 2] = mask1;
    pruintc_io[PRU_INTC_SECR1_REG >> 2] = mask1;
    pruintc_io[PRU_INTC_ESR2_REG >> 2] = mask2;
    pruintc_io[PRU_INTC_SECR2_REG >> 2] = mask2;

    for (i = 0; i < MAX_HOSTS_SUPPORTED; i++)
        if (prussintc_init_data->host_enable_bitmask & (1 << i)) {
            pruintc_io[PRU_INTC_HIEISR_REG >> 2] = i;
        }

    pruintc_io[PRU_INTC_GER_REG >> 2] = 0x1;

    // Stash a copy of the intc settings
    memcpy( &prussdrv.intc_data, prussintc_init_data,
            sizeof(prussdrv.intc_data) );

    return 0;
}

short prussdrv_get_event_to_channel_map( unsigned int eventnum )
{
    unsigned int i;
    for (i = 0; i < NUM_PRU_SYS_EVTS &&
                prussdrv.intc_data.sysevt_to_channel_map[i].sysevt  !=-1 &&
                prussdrv.intc_data.sysevt_to_channel_map[i].channel !=-1; ++i) {
        if ( eventnum == prussdrv.intc_data.sysevt_to_channel_map[i].sysevt )
            return prussdrv.intc_data.sysevt_to_channel_map[i].channel;
    }
    return -1;
}

short prussdrv_get_channel_to_host_map( unsigned int channel )
{
    unsigned int i;
    for (i = 0; i < NUM_PRU_CHANNELS &&
                prussdrv.intc_data.channel_to_host_map[i].channel != -1 &&
                prussdrv.intc_data.channel_to_host_map[i].host    != -1; ++i) {
        if ( channel == prussdrv.intc_data.channel_to_host_map[i].channel )
            /** -2 is because first two host interrupts are reserved
             * for PRU0 and PRU1 */
            return prussdrv.intc_data.channel_to_host_map[i].host - 2;
    }
    return -1;
}

short prussdrv_get_event_to_host_map( unsigned int eventnum )
{
    short ans = prussdrv_get_event_to_channel_map( eventnum );
    if (ans < 0) return ans;
    return prussdrv_get_channel_to_host_map( ans );
}

int prussdrv_pru_send_event(unsigned int eventnum)
{
    unsigned int *pruintc_io = (unsigned int *) prussdrv.intc_base;
    if (eventnum < 32)
        pruintc_io[PRU_INTC_SRSR1_REG >> 2] = 1 << eventnum;
    else
        pruintc_io[PRU_INTC_SRSR2_REG >> 2] = 1 << (eventnum - 32);
    return 0;
}

unsigned int prussdrv_pru_wait_event(unsigned int host_interrupt)
{
    unsigned int event_count;
    read(prussdrv.fd[host_interrupt], &event_count, sizeof(int));
    return event_count;
}

int prussdrv_pru_clear_event(unsigned int host_interrupt, unsigned int sysevent)
{
    unsigned int *pruintc_io = (unsigned int *) prussdrv.intc_base;
    if (sysevent < 32)
        pruintc_io[PRU_INTC_SECR1_REG >> 2] = 1 << sysevent;
    else
        pruintc_io[PRU_INTC_SECR2_REG >> 2] = 1 << (sysevent - 32);

    // Re-enable the host interrupt.  Note that we must do this _after_ the
    // system event has been cleared so as to not re-tigger the interrupt line.
    // See Section 6.4.9 of Reference manual about HIEISR register.
    // The +2 is because the first two host interrupts are reserved for
    // PRU0 and PRU1.
    pruintc_io[PRU_INTC_HIEISR_REG >> 2] = host_interrupt+2;
    return 0;
}

int prussdrv_pru_send_wait_clear_event(unsigned int send_eventnum,
                                       unsigned int host_interrupt,
                                       unsigned int ack_eventnum)
{
    prussdrv_pru_send_event(send_eventnum);
    prussdrv_pru_wait_event(host_interrupt);
    prussdrv_pru_clear_event(host_interrupt, ack_eventnum);
    return 0;

}


int prussdrv_map_l3mem(void **address)
{
    *address = prussdrv.l3ram_base;
    return 0;
}



int prussdrv_map_extmem(void **address)
{

    *address = prussdrv.extram_base;
    return 0;

}

unsigned int prussdrv_extmem_size(void)
{
    return prussdrv.extram_map_size;
}

int prussdrv_map_prumem(unsigned int pru_ram_id, void **address)
{
    switch (pru_ram_id) {
    case PRUSS0_PRU0_DATARAM:
        *address = prussdrv.pru0_dataram_base;
        break;
    case PRUSS0_PRU1_DATARAM:
        *address = prussdrv.pru1_dataram_base;
        break;
    case PRUSS0_SHARED_DATARAM:
        if (prussdrv.version != PRUSS_V2)
            return -1;
        *address = prussdrv.pruss_sharedram_base;
        break;
    default:
        *address = 0;
        return -1;
    }
    return 0;
}

int prussdrv_map_peripheral_io(unsigned int per_id, void **address)
{
    if (prussdrv.version != PRUSS_V2)
        return -1;

    switch (per_id) {
    case PRUSS0_CFG:
        *address = prussdrv.pruss_cfg_base;
        break;
    case PRUSS0_UART:
        *address = prussdrv.pruss_uart_base;
        break;
    case PRUSS0_IEP:
        *address = prussdrv.pruss_iep_base;
        break;
    case PRUSS0_ECAP:
        *address = prussdrv.pruss_ecap_base;
        break;
    case PRUSS0_MII_RT:
        *address = prussdrv.pruss_miirt_base;
        break;
    case PRUSS0_MDIO:
        *address = prussdrv.pruss_mdio_base;
        break;
    default:
        *address = 0;
        return -1;
    }
    return 0;
}

unsigned int prussdrv_get_phys_addr(const void *address)
{
    unsigned int retaddr = 0;
    if ((address >= prussdrv.pru0_dataram_base)
        && ((char*) address <
            (char*) prussdrv.pru0_dataram_base + prussdrv.pruss_map_size)) {
        retaddr =
            ((unsigned int) ((char*) address - (char*) prussdrv.pru0_dataram_base) +
             prussdrv.pru0_dataram_phy_base);
    } else if ((address >= prussdrv.l3ram_base)
               && ((char*) address <
                   (char*) prussdrv.l3ram_base + prussdrv.l3ram_map_size)) {
        retaddr =
            ((unsigned int) ((char*) address - (char*) prussdrv.l3ram_base) +
             prussdrv.l3ram_phys_base);
    } else if ((address >= prussdrv.extram_base)
               && ((char*) address <
                   (char*) prussdrv.extram_base + prussdrv.extram_map_size)) {
        retaddr =
            ((unsigned int) ((char*) address - (char*) prussdrv.extram_base) +
             prussdrv.extram_phys_base);
    }
    return retaddr;

}

void *prussdrv_get_virt_addr(unsigned int phyaddr)
{
    void *address = 0;
    if ((phyaddr >= prussdrv.pru0_dataram_phy_base)
        && (phyaddr <
            prussdrv.pru0_dataram_phy_base + prussdrv.pruss_map_size)) {
        address =
            (void *) ((char*)  prussdrv.pru0_dataram_base +
                      (phyaddr - prussdrv.pru0_dataram_phy_base));
    } else if ((phyaddr >= prussdrv.l3ram_phys_base)
               && (phyaddr <
                   prussdrv.l3ram_phys_base + prussdrv.l3ram_map_size)) {
        address =
            (void *) ((char*)  prussdrv.l3ram_base +
                      (phyaddr - prussdrv.l3ram_phys_base));
    } else if ((phyaddr >= prussdrv.extram_phys_base)
               && (phyaddr <
                   prussdrv.extram_phys_base + prussdrv.extram_map_size)) {
        address =
            (void *) ((char*)  prussdrv.extram_base +
                      (phyaddr - prussdrv.extram_phys_base));
    }
    return address;

}


#if 0
int prussdrv_exit()
{
    int i;
    munmap(prussdrv.pru0_dataram_base, prussdrv.pruss_map_size);
    munmap(prussdrv.l3ram_base, prussdrv.l3ram_map_size);
    munmap(prussdrv.extram_base, prussdrv.extram_map_size);
    for (i = 0; i < NUM_PRU_HOSTIRQS; i++) {
        if (prussdrv.fd[i])
            close(prussdrv.fd[i]);
        if (prussdrv.irq_thread[i])
            pthread_join(prussdrv.irq_thread[i], NULL);
    }
    return 0;
}
#endif

int prussdrv_exec_program(int prunum, const char *filename)
{
    FILE *fPtr;
    unsigned char fileDataArray[PRUSS_MAX_IRAM_SIZE];
    int fileSize = 0;
    unsigned int pru_ram_id;

    if (prunum == 0)
        pru_ram_id = PRUSS0_PRU0_IRAM;
    else if (prunum == 1)
        pru_ram_id = PRUSS0_PRU1_IRAM;
    else
        return -1;

    // Open an File from the hard drive
    fPtr = fopen(filename, "rb");
    if (fPtr == NULL) {
        DEBUG_PRINTF("File %s open failed\n", filename);
	return -1;
    } else {
        DEBUG_PRINTF("File %s open passed\n", filename);
    }
    // Read file size
    fseek(fPtr, 0, SEEK_END);
    fileSize = ftell(fPtr);

    if (fileSize == 0) {
        DEBUG_PRINTF("File read failed.. Closing program\n");
        fclose(fPtr);
        return -1;
    }

    fseek(fPtr, 0, SEEK_SET);

    if (fileSize !=
        fread((unsigned char *) fileDataArray, 1, fileSize, fPtr)) {
        DEBUG_PRINTF("WARNING: File Size mismatch\n");
	fclose(fPtr);
	return -1;
    }

    fclose(fPtr);

    // Make sure PRU sub system is first disabled/reset
    prussdrv_pru_disable(prunum);
    prussdrv_pru_write_memory(pru_ram_id, 0,
                              (unsigned int *) fileDataArray, fileSize);
    prussdrv_pru_enable(prunum);

    return 0;
}

#if 0
int prussdrv_start_irqthread(unsigned int host_interrupt, int priority,
                             prussdrv_function_handler irqhandler)
{
    pthread_attr_t pthread_attr;
    struct sched_param sched_param;
    pthread_attr_init(&pthread_attr);
    if (priority != 0) {
        pthread_attr_setinheritsched(&pthread_attr,
                                     PTHREAD_EXPLICIT_SCHED);
        pthread_attr_setschedpolicy(&pthread_attr, SCHED_FIFO);
        sched_param.sched_priority = priority;
        pthread_attr_setschedparam(&pthread_attr, &sched_param);
    }

    pthread_create(&prussdrv.irq_thread[host_interrupt], &pthread_attr,
                   irqhandler, NULL);

    pthread_attr_destroy(&pthread_attr);

    return 0;
}
#endif
