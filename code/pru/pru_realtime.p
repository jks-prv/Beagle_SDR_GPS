//
// Code that runs on the Sitara PRU. Not used presently.
//

.origin 0
.entrypoint start

#include "pru_realtime.hp"

	// shared memory used to communicate with CPU
	// layout must match pru_realtime.h
	.struct	m
		.u32	cmd
		.u32	count
		.u32	watchdog
		
		.u32	p0
		.u32	p1
		.u32	p2
		.u32	p3
	.ends

	.struct	m2
		.u32	m2_offset
	.ends

	// register aliases
	.struct	regs
		.u32	m
		.u32	m2
		.u32	gpio0
		.u32	gpio1
		.u32	gpio2
		.u32	gpio3
		.u32	gpio_in
		.u32	gpio_oe
		.u32	gpio_set
		.u32	gpio_clr
		.u32	timer
		.u32	timer_tclr
		.u32	timer_tcrr
		.u32	p0
		.u32	p1
		.u32	p2
		.u32	p3
		.u32	p4
		.u32	p5
		.u32	n0_ovfl
		.u32	n3_ovfl
		.u32	ovfl_none
		.u16	ra1
		.u16	ra2
		.u16	ra3
		.u16	ra4
		.u32	io30
		.u32	io31
	.ends
	
	.assign	regs, r6, r31, r			// r0-r5 are temps, avoid r30-r31

start:
    // enable OCP master ports so we can access gpio & timer
    lbco	r1, c4, 4, 4				// SYSCFG register (C4 in constant table)
    clr		r1, r1, 4					// clear standby_init bit
    sbco	r1, c4, 4, 4

	// address pointers (too big for immediate field in most cases)
	mov		r.m, PRU_COM_MEM
	mov		r.m2, PRU_COM_MEM2

	mov		r1, 0
	eput	r1, r.m, m.watchdog

cmd_done:
	mov		r1, PRU_DONE
    eput	r1, r.m, m.cmd
    
cmd_get:
	eget	r1, r.m, m.cmd
	qbeq	cmd_ping, r1, PRU_PING
	jmp		cmd_get


// establish we're running by answering a ping
cmd_ping:
	eget	r1, r.m, m.p0
	eget	r2, r.m, m.p1
	add		r1, r1, r2
    eput	r1, r.m, m.p2
    eget	r1, r.m2, m2.m2_offset
    eput	r1, r.m, m.p3
	jmp 	cmd_done
