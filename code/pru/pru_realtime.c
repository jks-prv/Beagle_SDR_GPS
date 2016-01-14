/*
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

/*****************************************************************************
*
* The PRU reads and stores values into the PRU Data RAM memory. PRU Data RAM 
* memory has an address in both the local data memory map and global memory 
* map. The example accesses the local Data RAM using both the local address 
* through a register pointed base address and the global address pointed by 
* entries in the constant table. 
*
******************************************************************************/

#include "types.h"
#include "config.h"
#include "kiwi.h"
#include "misc.h"
#include "pru_realtime.h"
#include <stdio.h>

#include <prussdrv.h>
#include <pruss_intc_mapping.h>

#define PRU_NUM 	0

static void *pruDataMem;

com_t *pru;
com2_t *pru2;

void pru_start()
{
    unsigned int ret;
    const char *bin;
    
    tpruss_intc_initdata pruss_intc_initdata = PRUSS_INTC_INITDATA;

    prussdrv_init();		
    if (prussdrv_open(PRU_EVTOUT_0)) panic("prussdrv_open");
    if (prussdrv_pruintc_init(&pruss_intc_initdata)) panic("prussdrv_pruintc_init");

	if (prussdrv_map_prumem(PRUSS0_PRU0_DATARAM, &pruDataMem)) panic("prussdrv_map_prumem");
    pru = (com_t *) pruDataMem;
    pru2 = (com2_t *) ((char*) pruDataMem + PRU_COM_SIZE);

	bin = background_mode? "/usr/local/bin/kiwid_realtime.bin" : "pru/pru_realtime.bin";
    if (prussdrv_exec_program(PRU_NUM, bin)) panic("prussdrv_exec_program");
    
    u4_t key1 = timer_us();
    u4_t key2 = key1 >> 8;
    pru->p[0] = key1;
    pru->p[1] = key2;
    pru->p[2] = 0;
    pru->p[3] = 0;
    pru2->m2_offset = 0xbeefcafe;
    pru->cmd = PRU_PING;
    while (pru->cmd != PRU_DONE);		// fixme: get latest version of this from 5370 project
    if (pru->p[2] != (key1+key2)) panic("PRU didn't start");
    if (pru->p[3] != 0xbeefcafe) panic("PRU com2_t at wrong offset?");
    lprintf("PRU started\n");

#if 0
    prussdrv_pru_wait_event(PRU_EVTOUT_0);		// wait for halt
    prussdrv_pru_clear_event(PRU_EVTOUT_0, PRU0_ARM_INTERRUPT);
    prussdrv_pru_disable(PRU_NUM);
    prussdrv_exit();
#endif
}
