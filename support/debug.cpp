#include "kiwi.h"
#include "printf.h"
#include "debug.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#ifdef EV_MEAS

typedef struct {
	int prev_valid, free_s2, event, cmd;
	const char *s, *s2, *task;
	int rx_chan;
	u4_t tprio, tid, tlast, tepoch, depoch, trig1, trig2, trig3;
	u4_t tseq;      // time since last ev print
	u4_t ttask;     // task time for this quanta
	u4_t trig_realtime, trig_accum;
	bool dump_point;
} ev_t;

static u4_t ev_trig_realtime, ev_trig_accum[MAX_TASKS];

static int evc, ev_wrapped;

#define NEV 8192
//#define NEV 64
ev_t evs[NEV+1024];

const char *evcmd[NECMD] = {
	"Event", "Dump", "DumpCont", "Task", "Trig1", "Trig2", "Trig3", "Real", "Acc1", "Acc0"
};

const char *evn[NEVT] = {
	"NextTask", "SPI", "WF", "SND", "GPS", "DataPump", "Printf", "Ext", "RX", "WebSrvr"
};

enum evdump_e { REG, SUMMARY };

static void evdump(evdump_e type, int lo, int hi)
{
	ev_t *e;
	u4_t printf_type = bg? PRINTF_LOG : PRINTF_REAL;
	//u4_t printf_type = (ev_dump == -1)? PRINTF_LOG : PRINTF_REAL;
	
	if (type == SUMMARY) {
        lfprintf(printf_type, "task summary:\n");
        for (int i=lo; i<hi; i++) {
            assert(i >= 0 && i < NEV);
            e = &evs[i];
            if (e->cmd == EC_TASK_SWITCH) {
                lfprintf(printf_type, "%7.3f %16s:P%d:T%02d ", e->ttask/1e3, e->task, e->tprio, e->tid);
                if (e->rx_chan >= 0)
                    lfprintf(printf_type, "ch%d ", e->rx_chan);
                else
                    lfprintf(printf_type, "    ");
                lfprintf(printf_type, "%s\n", (e->ttask > 6000)? "==============================":"");
            }
        }
        return;
	}

	for (int i=lo; i<hi; i++) {
		assert(i >= 0 && i < NEV);
		e = &evs[i];

		#if 0
            lfprintf(printf_type, "%4d %5s %8s %7.3f %10.6f %7.3f %7.3f %7.3f %16s:P%d:T%02d %-10s | %s\n", i, evcmd[e->cmd], evn[e->event],
                /*(float) e->tlast/1e3,*/ (float) e->tseq/1e3, (float) e->tepoch/1e6,
                (float) e->trig1/1e3, (float) e->trig2/1e3, (float) e->trig3/1e3,
                e->task, e->tprio, e->tid, e->s, e->s2);
		#else
		    // 12345 12345678 1234567 1234567 1234567890
		    //   cmd    event    tseq   ttask     tepoch
		    //                     ms      ms        sec
            lfprintf(printf_type, "%5s %8s %7.3f %7.3f %10.6f ", evcmd[e->cmd], evn[e->event],
                (float) e->tseq/1e3, (float) e->ttask/1e3, (float) e->tepoch/1e6);
            if (e->trig_accum) {
                //lfprintf(printf_type, "%7.3f%c ", (float) e->trig3/1e3, (e->trig3 > 15000)? '$':' ');
                lfprintf(printf_type, "%7.3f%c ", (float) e->trig_accum/1e3, (e->cmd == EC_TRIG_ACCUM_OFF)? '$':' ');
            } else {
                lfprintf(printf_type, "-------  ");
            }
            lfprintf(printf_type, "%16s:P%d:T%02d ", e->task, e->tprio, e->tid);
            if (e->rx_chan >= 0)
                lfprintf(printf_type, "ch%d ", e->rx_chan);
            else
                lfprintf(printf_type, "    ");
            lfprintf(printf_type, "%-10s | %s\n", e->s, e->s2);
		#endif

		if (e->cmd == EC_TASK_SWITCH) {
		    lfprintf(printf_type, "                       -------\n");
		}
		if (e->cmd == EC_DUMP || e->cmd == EC_DUMP_CONT || e->dump_point)
			lfprintf(printf_type, "*** DUMP *** DUMP *** DUMP *** DUMP *** DUMP *** DUMP *** DUMP *** DUMP *** DUMP *** DUMP *** DUMP *** DUMP ***\n");
	}

#ifndef EVENT_DUMP_WHILE_RUNNING
	//if (lo == 0) lfprintf(printf_type, "12345678 xxx.xxx xxx.xxx xxx.xxxxxx xxx.xxx\n");
	  if (lo == 0) lfprintf(printf_type, "*** DUMP  seq ms task ms        sec trg3 ms\n");
#endif
}

#define EV_MALLOCED 1   // assumes malloc() won't return an odd-value pointer

static u64_t ev_epoch;
static u4_t ev_dump_ms, ev_dump_expire;
static bool ev_dump_continue;
static bool ev_already_dumped = false;
static u4_t last_time, tlast[NEVT], triggered, ev_trig1, ev_trig2, ev_trig3;
//static u4_t ev_trig3[256];

void ev(int cmd, int event, int param, const char *s, const char *s2)
{
	int i, id = param;
	int tid = TaskID();
	ev_t *e;
	u4_t now_ms = timer_ms();
	u64_t now_us = timer_us64();
	int free_s2 = 0;
	
	// Using -O3 constant strings can get allocated on off byte boundaries
	// causing our scheme of using the low-order bit to mark malloced pointers to fail.
	// It's not really possible to access the "_end" symbol in cpp it seems.
	// And the advent of ASLR makes it moot in any case.
	// But we observe that low addresses always seem to be used for string constants.
	// So we use an empirical value. We'll get a double-free detect failure if this number isn't correct.
	
	#define EV_HIGHEST_ADDR_CONST_STRINGS ((char *) 0x100000)
    if (s2 > EV_HIGHEST_ADDR_CONST_STRINGS && ((u64_t) s2 & EV_MALLOCED)) {
        free_s2 = EV_MALLOCED;
        s2 = (char*) ((u64_t) s2 & ~EV_MALLOCED);
    }
	
	assert(event >= 0 && event < NEVT);
	
	static bool init;
	if (!init) {
		ev_epoch = now_us;
		last_time = now_us;
		for (i=0; i<NEVT; i++) tlast[event] = now_us;
		init = true;
	}
	
	assert(evc >= 0 && evc < NEV);
	e = &evs[evc];

	// keep memory from filling up with un-freed vasprintf()s from evprintf()
	if (e->prev_valid && e->free_s2) {
		free((void*) e->s2);
	}
	memset(e, 0, sizeof(*e));
	
	//if (cmd == EC_DUMP && param > 0 && ev_dump_ms) return;
	
	evc++;
	if (evc >= NEV) {
		evc = 0;
		ev_wrapped = 1;
	}
	
	/*
	if (cmd == EC_TRIG1) {
		ev_trig1 = ev_trig2 = now_us;
	}
	
	if (cmd == EC_TRIG2) {
		if (param >= 0 && (now_us - ev_trig2) > param)
			//ev_dump = -1;
			;
		else
			ev_trig2 = 0;
	}
	*/
	
	if (cmd == EC_TRIG_REALTIME) {
		ev_trig_realtime = now_us;
	}
	
	if (cmd == EC_TRIG_ACCUM_ON) {
		ev_trig_accum[tid] = 1;      // bootstrap
	}
	
	e->prev_valid = 1;
	e->cmd = cmd;
	e->event = event;
	e->s = s;
	e->s2 = s2;
	e->free_s2 = free_s2;
	e->tseq = now_us - last_time;
	e->tlast = now_us - tlast[event];
	e->depoch = now_us - ev_epoch;
	e->tepoch = now_us;
	e->tprio = TaskPriority(-1);
	e->tid = TaskID();
	e->task = TaskName();
	e->dump_point = false;
	e->ttask = now_us - TaskStartTime();
	
	e->trig1 = ev_trig1? (now_us - ev_trig1) : 0;
	e->trig2 = ev_trig2? (now_us - ev_trig2) : 0;
	//e->trig3 = (id != -1 && ev_trig3[id])? (now_us - ev_trig3[id]) : 0;
	//e->trig3 = (id >= 0 && id < 256 && ev_trig3[id])? (now_us - ev_trig3[id]) : 0;

	e->trig_realtime = ev_trig_realtime? (now_us - ev_trig_realtime) : 0;
	if (ev_trig_accum[tid])
	    ev_trig_accum[tid] += e->tseq;
	e->trig_accum = ev_trig_accum[tid];
	
	u4_t flags = TaskFlags();
	e->rx_chan = (flags & CTF_RX_CHANNEL)? (flags & CTF_CHANNEL) : -1;
	
	// dump at a point in the future
	if ((cmd == EC_DUMP || cmd == EC_DUMP_CONT) && param > 0) {
	    ev_dump_ms = param;
	    ev_dump_expire = now_ms + param;
	    if (cmd == EC_DUMP_CONT) ev_dump_continue = true;
	    return;
	}
	
	//if (cmd != EC_TRIG3 && param > 0 && !ev_dump_ms) { ev_dump_ms = param; ev_dump_expire = now_ms + param; e->dump_point = true; }

	if (cmd == EC_TRIG_ACCUM_OFF) {
		ev_trig_accum[tid] = 0;
	}
	
#ifdef EVENT_DUMP_WHILE_RUNNING
	evdump(REG, 0, 1);
	evc = 0;
#else
	if (
	    //(ev_dump == -1) ||      // dump immediately with no delay
	    ((cmd == EC_DUMP || cmd == EC_DUMP_CONT) && (param <= 0 || bg)) ||      // dump on command + param
	    (ev_dump_ms && (now_ms > ev_dump_expire))) {        // dump after a delay specified by ev_dump
	//if (cmd == EC_DUMP) {
	    if (ev_already_dumped) return;
		e->tlast = 0;
		
		if (bg) {
		//if (ev_dump == -1) {
		    int start = MAX(evc-32, 0);
            evdump(REG, start, evc);
		} else {
            if (ev_wrapped) evdump(REG, evc+1, NEV);
            evdump(REG, 0, evc);
            if (ev_wrapped) evdump(SUMMARY, evc+1, NEV);
            evdump(SUMMARY, 0, evc);
        }
        
		if (ev_dump_ms) printf("expiration of %.3f sec dump time\n", ev_dump_ms/1000.0);
		if (!bg) dump();
		//if (ev_dump != -1) dump();
		if (cmd == EC_DUMP_CONT || ev_dump_continue) {
		    // reset
		    ev_dump_ms = ev_dump_expire = 0;
		    ev_dump_continue = false;
		    return;
		}
		ev_already_dumped = true;
		panic("evdump");
	}
#endif
	
	tlast[event] = last_time = now_us;
}

char *evprintf(const char *fmt, ...)
{
	char *ret;
	va_list ap;
	
	va_start(ap, fmt);
	vasprintf(&ret, fmt, ap);
	va_end(ap);
	
	// hack: mark malloced string for later free
	assert(((u64_t) ret & EV_MALLOCED) == 0);
	return (char*) ((u64_t) ret | EV_MALLOCED);
}

#endif
