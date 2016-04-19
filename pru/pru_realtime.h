#ifndef _PRU_REALTIME_H_
#define _PRU_REALTIME_H_

#define	PRU_DONE			0
#define	PRU_PING			1


// no -I path when using pasm
#include "../arch/sitara/sitara.h"

#define PRU_COM_SIZE	(64*4)		// 64 entries max in a PRU assembler .struct
#define PRU_COM_MEM		0
#define PRU_COM_MEM2	(PRU_COM_MEM + PRU_COM_SIZE)

#ifndef _PASM_

#include "misc.h"

// shared memory used to communicate with PRU
// layout must match pru_realtime.p
typedef volatile struct com_s {
	u4_t cmd;
	u4_t count;
	u4_t watchdog;
	u4_t p[4];
} com_t;

extern com_t *pru;

typedef volatile struct com2_s {
	u4_t m2_offset;
} com2_t;

extern com2_t *pru2;

void pru_start();

#endif

#endif
