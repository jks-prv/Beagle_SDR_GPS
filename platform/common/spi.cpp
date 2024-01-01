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

// Copyright (c) 2015 John Seamons, ZL4VO/KF6VO

#include "types.h"
#include "misc.h"
#include "spi.h"
#include "spi_dev.h"
#include "peri.h"
#include "coroutines.h"
#include "debug.h"

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sched.h>

#if 0
static u4_t last_cmd, ack_seq, rdy_seq;
static int last_rdy_seq=-1;

static void spi_seq(SPI_T status)
{
	int i, j;

	last_cmd = 0;
	for (i = SPI_NST-4, j=0; i > SPI_NST-4-8; i--, j++) {
		last_cmd |= (status & (1 << i))? (1 << j) : 0;
	}
	
	rdy_seq = 0;
	for (j=0; i > SPI_NST-4-8-10; i--, j++) {
		rdy_seq |= (status & (1 << i))? (1 << j) : 0;
	}

	ack_seq = 0;
	for (j=0; i > 0; i--, j++) {
		ack_seq |= (status & (1 << i))? (1 << j) : 0;
	}
}
#endif

spi_t spi;
static spi_mosi_data_t _CmdFlush = { CmdFlush, 0, 0, 0, 0 };

void spi_stats()
{
    #if 0
        int xfers_sec = spi.xfers / STATS_INTERVAL_SECS;
        int flush_sec = spi.flush / STATS_INTERVAL_SECS;
        int total_sec = xfers_sec + flush_sec;
        float MB_sec = (float) spi.bytes / 1e6 / STATS_INTERVAL_SECS;
        int spin_ms_sec = spi_delay * total_sec / 1000;
        printf("SPI: %5d + %5d = %5d xfers/s, %.3f MB/s, %3d msec/s spin(%d), retries: ",
            xfers_sec, flush_sec, total_sec, MB_sec, spin_ms_sec, spi_delay);
        for (int i = 0; i < NRETRY_HIST; i++) {
            if (spi.retry_hist[i])
                printf("%d:%d ", i, spi.retry_hist[i]);
            else
                printf("%d:_ ", i);
            spi.retry_hist[i] = 0;
        }
        printf("\n");
        spi.xfers = spi.flush = spi.bytes = 0;
    #endif
}

static int wait_avail(const char *where, SPI_CMD cmd)
{
    int wait = 0;
    
	for (int max = 0; !GPIO_READ_BIT(CMD_READY) && max < 1000; max++) {
	    wait++;
	    NextTask("wait_avail");
	}
	
	//if (wait) {
	if (0) {
	//if (1) {
	    real_printf("wait_avail %5d %26s %s\n", wait, where, cmds[cmd]);
	}
	
	return wait;
}

///////////////////////////////////////////////////////////////////////////////////////////////

static SPI_MISO *pingx;

void spi_pump(void *param)
{
	while(1) {
		#ifdef SPI_PUMP_CHECK
			pingx->word[0] = 0x1234;
			spi_get_noduplex(CmdPing, pingx, 10);
	
			#if 0
			spi_seq(pingx->status);
			evSpi(EC_EVENT, EV_SPILOOP, -1, "spi_pump", evprintf("PUMP CHECK %p: c=%d rs=%d as=%d cafe=0x%x sp=%d seq=0x%04x reenter=%d babe=0x%x",
				pingx, last_cmd, rdy_seq, ack_seq, pingx->word[0], pingx->word[1], pingx->word[2], pingx->word[3], pingx->word[4]));
			#else
			evSpi(EC_EVENT, EV_SPILOOP, -1, "spi_pump", evprintf("PUMP CHECK %p: cafe=0x%x sp=%d seq=0x%04x reenter=%d babe=0x%x",
				pingx, pingx->word[0], pingx->word[1], pingx->word[2], pingx->word[3], pingx->word[4]));
			#endif
			if (pingx->word[0] != 0xcafe || pingx->word[1] != 4 || pingx->word[4] != 0xbabe) {
				if (ev_dump) {
					evSpi(EC_EVENT, EV_SPILOOP, ev_dump, "spi_pump", "NOPE! expecting: 0xcafe, sp=4, 0xbabe ------------------------------------------------");
				} else {
					printf("."); fflush(stdout);
				}
			}
		#else
			spi_set(CmdPumpFlush);
		#endif
		TaskStat(TSTAT_INCR|TSTAT_ZERO, 0, "pmp");
		TaskSleep();    // woken up by special code in _NextTask() because we're marked as the CTF_BUSY_HELPER task
	}
}

// Why is there an SPI lock?
// The spi_lock allows the local buffers to be held across the NextTask() that occurs if
// the SPI request has to be retried due to the eCPU being busy.

lock_t spi_lock;
static SPI_MISO *junk, *prev;

void spi_init()
{
	assert(CmdCheckLast == NUM_CMDS);
	assert(ARRAY_LEN(cmds) == (NUM_CMDS + NSPI_CMD_PSEUDO));

	lock_init(&spi_lock);
	lock_register(&spi_lock);
	assert(SPI_SHMEM != NULL);
    pingx = &SPI_SHMEM->pingx_miso;
	junk = &SPI_SHMEM->spi_junk_miso;
	prev = junk;
	junk->status = SPI_BUSY;
	CreateTaskF(spi_pump, 0, SPIPUMP_PRIORITY, CTF_BUSY_HELPER);
}

static void spi_scan(int wait, SPI_MOSI *mosi, int tbytes=0, SPI_MISO *miso=junk, int rbytes=0) {
	int i;
	
	assert(rbytes <= SPIBUF_B);
	
	int tx_bytes = tbytes? (sizeof(mosi->data.cmd) + tbytes) : sizeof(mosi->data);
    int tx_xfers = SPI_B2X(tx_bytes);
    
    int rx_bytes = sizeof(miso->status) + rbytes;
    miso->len_bytes = rx_bytes;
    miso->len_xfers = SPI_B2X(rx_bytes);
    miso->tid = TaskID();

    int prx_xfers = MAX(tx_xfers, prev->len_xfers);
    
    if (mosi->data.cmd == CmdPumpFlush) mosi->data.cmd = CmdFlush;
    miso->cmd = mosi->data.cmd;

	if (mosi->data.cmd != CmdFlush) {
	    ecpu_cmds++; //TaskStat(TSTAT_CMDS, 0, "tcm");
	    spi.xfers++;
	} else {
	    spi.flush++;
	}
	ecpu_tcmds++;
	
	int bytes = MAX(tx_bytes, prev->len_bytes);
	spi.bytes += bytes;
	
	#if 0
        if (mosi->data.cmd == CmdPing || mosi->data.cmd == CmdPing2)
            real_printf("%s: tx%d(%dX)|Prx%d(%dX)=T%d(%dX) Crx%d(%dX) ", &cmds[mosi->data.cmd][3],
                tx_bytes, tx_xfers, prev->len_bytes, SPI_B2X(prev->len_bytes), bytes, SPI_B2X(bytes),
                rx_bytes, SPI_B2X(rx_bytes)); fflush(stdout);
    #endif
	
	evSpiCmd(EC_EVENT, EV_SPILOOP, -1, "spi_scan", evprintf("ENTER %s(%d) mosi %p:%dx miso %p%s:%dB prev %p%s:%dx",
		cmds[mosi->data.cmd], mosi->data.cmd, mosi, tx_xfers,
		miso, (miso == junk)? " (junk)":"", rbytes,
		prev, (prev == junk)? " (junk)":"", prx_xfers));

	// also note in some cases it is possible for: miso == prev == junk on entry
	miso->status = SPI_BUSY;	// for loop in spi_get()
	prev->status = SPI_BUSY;	// for loop below

	int retries=0;
    for (i=0; i < 32; i++) {
        #ifdef EV_MEAS_SPI_CMD
            u4_t orig = prev->status & SPI_BUSY_MASK;
        #endif
		assert((prev->status & SPI_BUSY_MASK) == SPI_BUSY);
        spi_dev(SPI_HOST,
            mosi, tx_xfers,   // MOSI: new request
            prev, prx_xfers);  // MISO: response to previous caller's request

		// fixme: understand why is this needed (hangs w/o it) [still?]
        #ifdef EV_MEAS_SPI_CMD
            u4_t before = prev->status & SPI_BUSY_MASK;
        #endif
        if (spi_delay > 0) spin_us(spi_delay); else
        if (spi_delay < 0) kiwi_usleep(-spi_delay);
        u4_t after = prev->status & SPI_BUSY_MASK;
        //if ((prev->status & SPI_BUSY_MASK) != SPI_BUSY) break; // new request accepted?
        
        if (after != SPI_BUSY) break; // new request accepted?

        #ifdef EV_MEAS_SPI_CMD
            int r = TaskStat(TSTAT_SPI_RETRY, 0, "rty");
        #endif
        evSpiCmd(EC_EVENT, EV_SPILOOP, -1, "spi_scan", evprintf("RETRY %d: orig %x before %x after %x", r, orig, before, after));
        spi.retry++;
        retries++;
        NextTask("spi_scan RETRY"); // wait (still holding the spi_lock!) and try again
    }
    if (i == 32)
		evSpiCmd(EC_DUMP, EV_SPILOOP, -1, "spi_scan", "NO REPLY FROM eCPU!");
	
	if (retries)
	    spi.retry_hist[MIN(retries, NRETRY_HIST-1)]++;

	#if 0
        spi_seq(prev->status);
        evSpi(EC_EVENT, EV_SPILOOP, -1, "spi_scan", evprintf("DONE prev %p: c=%d rs=%d as=%d 0x%x 0x%x 0x%x",
            prev, last_cmd, rdy_seq, ack_seq, prev->word[0], prev->word[1], prev->word[2]));
        if (last_rdy_seq == -1) last_rdy_seq = rdy_seq; else last_rdy_seq++;
        if (last_rdy_seq != rdy_seq) {
            evSpi(EC_EVENT, EV_SPILOOP, -1, "spi_scan", evprintf("MISMATCH last_rdy_seq %d rdy_seq %d",
                last_rdy_seq, rdy_seq));
            last_rdy_seq = rdy_seq;
        }
	#else
        evSpiCmd(EC_EVENT, EV_SPILOOP, -1, "spi_scan", evprintf(">>> DONE %s(%d) %d retries, PREV %s(%d) %s %p: 0x%x 0x%x 0x%x",
            cmds[mosi->data.cmd], mosi->data.cmd, retries,
            cmds[prev->cmd], prev->cmd, Task_s(prev->tid),
            prev, prev->word[0], prev->word[1], prev->word[2]));
	#endif
    prev = miso; // next caller collects this for us
}

static void spi_check_wakeup(SPI_CMD cmd)
{
	TaskPollForInterrupt(CALLED_FROM_SPI);
}

///////////////////////////////////////////////////////////////////////////////////////////////

#ifdef STACK_CHECK

struct stack_entry_t {
	u2_t v, c;
};

struct stack_check_t {
	u2_t sp_beef;
	stack_entry_t sp_ready;
	stack_entry_t sp_rx;
	stack_entry_t sp_gps[GPS_MAX_CHANS];
	stack_entry_t sp_cmds[NUM_CMDS];
	u2_t sp_seq;
	u2_t sp_8bad;
	u2_t sp_reenter;
	u2_t sp_feed;
};

static int nope;

static void stack_nope(const char *what, int off, stack_entry_t *stk, int expected)
{
	if (ev_dump) {
		evSpi(EC_EVENT, EV_SPILOOP, ev_dump, "stack_check", evprintf("NOPE! %s(%s,%d) last sp 0x%04x expected sp 0x%04x count %d ------------------------------------------------",
			what, !strcmp(what, "sp_cmds")? cmds[off] : "-", off, stk->v, expected, stk->c));
	} else {
		printf("X %d %d\n", nope++, spi.retry);
	}
}

static void stack_nope2(u4_t expected)
{
	if (ev_dump) {
		evSpi(EC_EVENT, EV_SPILOOP, ev_dump, "stack_check", evprintf("NOPE! expecting 0x%x ------------------------------------------------", expected));
	} else {
		printf("X %d %d\n", nope++, spi.retry);
	}
}

static void stack_check(SPI_MISO *miso)
{
	int i;
	stack_check_t *s = (stack_check_t *) &miso->word[0];
	
	#if 0
	spi_seq(miso->status);
	evSpi(EC_EVENT, EV_SPILOOP, -1, "stack_check", evprintf("STACK CHECK %p: c=%d rs=%d as=%d beef=0x%x seq=0x%04x 8bad=0x%x reenter=%d feed=0x%x",
		miso, last_cmd, rdy_seq, ack_seq, s->sp_beef, s->sp_seq, s->sp_8bad, s->sp_reenter, s->sp_feed));
	#else
	evSpi(EC_EVENT, EV_SPILOOP, -1, "stack_check", evprintf("STACK CHECK %p: beef=0x%x seq=0x%04x 8bad=0x%x reenter=%d feed=0x%x",
		miso, s->sp_beef, s->sp_seq, s->sp_8bad, s->sp_reenter, s->sp_feed));
	#endif
	if (s->sp_beef != 0xbeef) {
		stack_nope2(0xbeef);
	}
	if (s->sp_8bad != 0x8bad) {
		stack_nope2(0x8bad);
	}
	if (s->sp_feed != 0xfeed) {
		stack_nope2(0xfeed);
	}
	if (s->sp_ready.c) stack_nope("sp_ready", 0, &s->sp_ready, 0);
	if (s->sp_rx.c) stack_nope("sp_rx", 0, &s->sp_rx, 13);
	for (i=0; i < gps_chans; i++) {
		if (s->sp_gps[i].c) stack_nope("sp_gps", i, &s->sp_gps[i], 12-i);
	}
	for (i=0; i < NUM_CMDS; i++) {
		if (s->sp_cmds[i].c) stack_nope("sp_cmds", i, &s->sp_cmds[i], 0);
	}
}

static spi_mosi_data_t _CmdUploadStackCheck = { CmdUploadStackCheck, 0, 0, 0, 0 };

void spi_stack_check(int wait, SPI_MOSI *tx)
{
    tx->data = _CmdUploadStackCheck;
    spi_scan(wait, tx, 0, pingx, sizeof(stack_check_t));
    tx->data = _CmdFlush;
    spi_scan(wait, tx);
    stack_check(pingx);
}

#endif

///////////////////////////////////////////////////////////////////////////////////////////////

void _spi_set(SPI_CMD cmd, uint16_t wparam, uint32_t lparam) {
    lock_enter(&spi_lock);
        int wait = wait_avail("_spi_set", cmd);
		SPI_MOSI *tx = &SPI_SHMEM->spi_tx[0];
		tx->data.cmd = cmd; tx->data.wparam = wparam;
		tx->data.lparam_lo = lparam & 0xffff; tx->data.lparam_hi = lparam >> 16;
		evSpiCmd(EC_EVENT, EV_SPILOOP, -1, "spi_set", evprintf("ENTER %s(%d) %d %d", cmds[cmd], cmd, wparam, lparam));
		spi_scan(wait, tx);
		evSpiCmd(EC_EVENT, EV_SPILOOP, -1, "spi_set", "DONE");
    lock_leave(&spi_lock);
    spi_check_wakeup(cmd);		// must be done outside the lock
}

void spi_set3(SPI_CMD cmd, uint16_t wparam, uint32_t lparam, uint16_t w2param) {
    lock_enter(&spi_lock);
        int wait = wait_avail("spi_set3", cmd);
		SPI_MOSI *tx = &SPI_SHMEM->spi_tx[1];
		tx->data.cmd = cmd; tx->data.wparam = wparam;
		tx->data.lparam_lo = lparam & 0xffff; tx->data.lparam_hi = lparam >> 16;
		tx->data.w2param = w2param;
		evSpiCmd(EC_EVENT, EV_SPILOOP, -1, "spi_set", evprintf("ENTER %s(%d) %d %d", cmds[cmd], cmd, wparam, lparam));
		spi_scan(wait, tx);
		evSpiCmd(EC_EVENT, EV_SPILOOP, -1, "spi_set", "DONE");
    lock_leave(&spi_lock);
    spi_check_wakeup(cmd);		// must be done outside the lock
}

void spi_set_noduplex(SPI_CMD cmd, uint16_t wparam, uint32_t lparam) {
	lock_enter(&spi_lock);		// block other threads
        int wait = wait_avail("spi_set_noduplex", cmd);
		SPI_MOSI *tx = &SPI_SHMEM->spi_tx[2];
		tx->data.cmd = cmd; tx->data.wparam = wparam;
		tx->data.lparam_lo = lparam & 0xffff; tx->data.lparam_hi = lparam >> 16;
		evSpiCmd(EC_EVENT, EV_SPILOOP, -1, "spi_setND", evprintf("ENTER %s(%d) %d %d", cmds[cmd], cmd, wparam, lparam));
		spi_scan(wait, tx);				// Send request
		
#ifdef STACK_CHECK
        spi_stack_check(wait, tx);
#else
		tx->data = _CmdFlush;
		evSpiCmd(EC_EVENT, EV_SPILOOP, -1, "spi_setND", evprintf("CmdNoDuplex self-response"));
        wait = wait_avail("spi_set_noduplex RESPONSE", cmd);
		spi_scan(wait, tx);				// Collect response to our own request
#endif
		evSpiCmd(EC_EVENT, EV_SPILOOP, -1, "spi_setND", "DONE");
    lock_leave(&spi_lock);		// release block
    spi_check_wakeup(cmd);		// must be done outside the lock
}

void spi_set4_noduplex(SPI_CMD cmd, uint16_t wparam, uint16_t w2param, uint16_t w3param, uint16_t w4param) {
	lock_enter(&spi_lock);		// block other threads
        int wait = wait_avail("spi_set_noduplex", cmd);
		SPI_MOSI *tx = &SPI_SHMEM->spi_tx[3];
		tx->data2.cmd = cmd; tx->data2.wparam = wparam; tx->data2.w2param = w2param; tx->data2.w3param = w3param; tx->data2.w4param = w4param;
		evSpiCmd(EC_EVENT, EV_SPILOOP, -1, "spi_setND", evprintf("ENTER %s(%d) %d %d", cmds[cmd], cmd, wparam, lparam));
		spi_scan(wait, tx);				// Send request
		
#ifdef STACK_CHECK
        spi_stack_check(wait, tx);
#else
		tx->data = _CmdFlush;
		evSpiCmd(EC_EVENT, EV_SPILOOP, -1, "spi_setND", evprintf("CmdNoDuplex self-response"));
        wait = wait_avail("spi_set_noduplex RESPONSE", cmd);
		spi_scan(wait, tx);				// Collect response to our own request
#endif
		evSpiCmd(EC_EVENT, EV_SPILOOP, -1, "spi_setND", "DONE");
    lock_leave(&spi_lock);		// release block
    spi_check_wakeup(cmd);		// must be done outside the lock
}

void spi_set_buf_noduplex(SPI_CMD cmd, SPI_MOSI *tx, int bytes) {
	lock_enter(&spi_lock);		// block other threads
        int wait = wait_avail("spi_set_buf_noduplex", cmd);
		tx->data.cmd = cmd;
		spi_scan(wait, tx, bytes);    // Send request
		
		tx->data = _CmdFlush;
        wait_avail("spi_set_buf_noduplex RESPONSE", cmd);
		spi_scan(wait, tx);           // Collect response to our own request

    lock_leave(&spi_lock);		// release block
    spi_check_wakeup(cmd);		// must be done outside the lock
}

///////////////////////////////////////////////////////////////////////////////////////////////

void _spi_get(SPI_CMD cmd, SPI_MISO *rx, int bytes, uint16_t wparam, uint32_t lparam) {
	lock_enter(&spi_lock);
        int wait = wait_avail("_spi_get", cmd);
		SPI_MOSI *tx = &SPI_SHMEM->spi_tx[4];
		tx->data.cmd = cmd; tx->data.wparam = wparam;
		tx->data.lparam_lo = lparam & 0xffff; tx->data.lparam_hi = lparam >> 16;
		evSpiCmd(EC_EVENT, EV_SPILOOP, -1, "spi_get", evprintf("ENTER %s(%d) rx %dB %d %d", cmds[cmd], cmd, bytes, wparam, lparam));
		spi_scan(wait, tx, 0, rx, bytes);
    lock_leave(&spi_lock);
    
    spi_check_wakeup(cmd);		// must be done outside the lock
    
    // wait for some future SPI from another task to make our reply valid
	int busy = 0;
    #ifdef EV_MEAS_SPI_CMD
	    int tid = TaskID();
	#endif
    while ((rx->status & SPI_BUSY_MASK) == SPI_BUSY) {
    	busy++;
		evSpiCmd(EC_EVENT, EV_SPILOOP, -1, "spi_get", evprintf(">>>> BUSY WAIT for DONE #%d %s(%d) %s miso %p",
			busy, cmds[cmd], cmd, Task_s(tid), rx));
    	if (busy > 2) {
			if (ev_dump) evSpi(EC_EVENT, EV_SPILOOP, ev_dump, "spi_get", evprintf("NT_BUSY_WAIT / BUSY_HELPER failed? %s(%d) %s miso %p ------------------------------------------------",
				cmds[cmd], cmd, Task_s(tid), rx));
    	}
    	
    	// use NT_BUSY_WAIT to indicate NextTask needs to run the spi_pump() task to help us receive our reply
    	NextTaskP("spi_get busy wait", NT_BUSY_WAIT); // wait for response
    }
	evSpiCmd(EC_EVENT, EV_SPILOOP, -1, "spi_get", evprintf("BUSY WAIT is DONE %s(%d) %s miso %p",  cmds[cmd], cmd, Task_s(tid), rx));
	evSpiCmd(EC_EVENT, EV_SPILOOP, -1, "spi_get", "DONE");
}

// pipelined: don't need to wait for reply to be valid
void spi_get_pipelined(SPI_CMD cmd, SPI_MISO *rx, int bytes, uint16_t wparam, uint32_t lparam) {
	lock_enter(&spi_lock);
        int wait = wait_avail("spi_get_pipelined", cmd);
		SPI_MOSI *tx = &SPI_SHMEM->spi_tx[5];
		tx->data.cmd = cmd; tx->data.wparam = wparam;
		tx->data.lparam_lo = lparam & 0xffff; tx->data.lparam_hi = lparam >> 16;
		evSpiCmd(EC_EVENT, EV_SPILOOP, -1, "spi_getPIPE", evprintf("ENTER %s(%d) rx %dB %d %d", cmds[cmd], cmd, bytes, wparam, lparam));
		spi_scan(wait, tx, 0, rx, bytes);
		evSpiCmd(EC_EVENT, EV_SPILOOP, -1, "spi_getPIPE", "DONE");
    lock_leave(&spi_lock);
    
    spi_check_wakeup(cmd);		// must be done outside the lock
}

// no duplexing: send a second cmd (flush) to force reply to our original cmd
void spi_get_noduplex(SPI_CMD cmd, SPI_MISO *rx, int bytes, uint16_t wparam, uint32_t lparam) {
	lock_enter(&spi_lock);		// block other threads
        int wait = wait_avail("spi_get_noduplex", cmd);
		SPI_MOSI *tx = &SPI_SHMEM->spi_tx[6];
		tx->data.cmd = cmd; tx->data.wparam = wparam;
		tx->data.lparam_lo = lparam & 0xffff; tx->data.lparam_hi = lparam >> 16;
		evSpiCmd(EC_EVENT, EV_SPILOOP, -1, "spi_getND", evprintf("ENTER %s(%d) rx %dB %d %d", cmds[cmd], cmd, bytes, wparam, lparam));
		spi_scan(wait, tx, 0, rx, bytes);	// Send request

#ifdef STACK_CHECK
        spi_stack_check(wait, tx);
#else
		tx->data = _CmdFlush;
		evSpiCmd(EC_EVENT, EV_SPILOOP, -1, "spi_getND", evprintf("CmdNoDuplex self-response"));
        wait = wait_avail("spi_get_noduplex RESPONSE", cmd);
		spi_scan(wait, tx);				// Collect response to our own request
#endif
		evSpi(EC_EVENT, EV_SPILOOP, -1, "spi_getND", "DONE");
    lock_leave(&spi_lock);		// release block
    spi_check_wakeup(cmd);		// must be done outside the lock
}

// spi_get_noduplex() but with 3 uint16_t parameters
void spi_get3_noduplex(SPI_CMD cmd, SPI_MISO *rx, int bytes, uint16_t wparam, uint16_t w2param, uint16_t w3param) {
	lock_enter(&spi_lock);		// block other threads
        int wait = wait_avail("spi_get3_noduplex", cmd);
		SPI_MOSI *tx = &SPI_SHMEM->spi_tx[7];
		tx->data.cmd = cmd; tx->data.wparam = wparam;
		tx->data.lparam_lo = w2param; tx->data.lparam_hi = w3param;
		evSpiCmd(EC_EVENT, EV_SPILOOP, -1, "spi_getND", evprintf("ENTER %s(%d) rx %dB %d %d", cmds[cmd], cmd, bytes, wparam, lparam));
		spi_scan(wait, tx, 0, rx, bytes);	// Send request

#ifdef STACK_CHECK
        spi_stack_check(wait, tx);
#else
		tx->data = _CmdFlush;
		evSpiCmd(EC_EVENT, EV_SPILOOP, -1, "spi_getND", evprintf("CmdNoDuplex self-response"));
        wait = wait_avail("spi_get3_noduplex RESPONSE", cmd);
		spi_scan(wait, tx);				// Collect response to our own request
#endif
		evSpi(EC_EVENT, EV_SPILOOP, -1, "spi_getND", "DONE");
    lock_leave(&spi_lock);		// release block
    spi_check_wakeup(cmd);		// must be done outside the lock
}
