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
	"NextTask", "SPI", "WF", "SND", "GPS", "DataPump", "Printf", "Ext", "RX"
};

enum evdump_e { REG, SUMMARY };

static void evdump(evdump_e type, int lo, int hi)
{
	ev_t *e;
	
	if (type == SUMMARY) {
        real_printf("task summary:\n");
        for (int i=lo; i<hi; i++) {
            assert(i >= 0 && i < NEV);
            e = &evs[i];
            if (e->cmd == EC_TASK_SWITCH) {
                real_printf("%7.3f %16s:P%d:T%02d ", e->ttask/1e3, e->task, e->tprio, e->tid);
                if (e->rx_chan >= 0)
                    real_printf("ch%d ", e->rx_chan);
                else
                    real_printf("    ");
                real_printf("%s\n", (e->ttask > 5000)? "==============================":"");
            }
        }
        return;
	}

	for (int i=lo; i<hi; i++) {
		assert(i >= 0 && i < NEV);
		e = &evs[i];

		#if 0
            real_printf("%4d %5s %8s %7.3f %10.6f %7.3f %7.3f %7.3f %16s:P%d:T%02d %-10s | %s\n", i, evcmd[e->cmd], evn[e->event],
                /*(float) e->tlast/1e3,*/ (float) e->tseq/1e3, (float) e->tepoch/1e6,
                (float) e->trig1/1e3, (float) e->trig2/1e3, (float) e->trig3/1e3,
                e->task, e->tprio, e->tid, e->s, e->s2);
		#else
		    // 12345 12345678 1234567 1234567 1234567890
		    //   cmd    event    tseq   ttask     tepoch
		    //                     ms      ms        sec
            real_printf("%5s %8s %7.3f %7.3f %10.6f ", evcmd[e->cmd], evn[e->event],
                (float) e->tseq/1e3, (float) e->ttask/1e3, (float) e->tepoch/1e6);
            if (e->trig_accum) {
                //real_printf("%7.3f%c ", (float) e->trig3/1e3, (e->trig3 > 15000)? '$':' ');
                real_printf("%7.3f%c ", (float) e->trig_accum/1e3, (e->cmd == EC_TRIG_ACCUM_OFF)? '$':' ');
            } else {
                real_printf("-------  ");
            }
            real_printf("%16s:P%d:T%02d ", e->task, e->tprio, e->tid);
            if (e->rx_chan >= 0)
                real_printf("ch%d ", e->rx_chan);
            else
                real_printf("    ");
            real_printf("%-10s | %s\n", e->s, e->s2);
		#endif

		if (e->cmd == EC_TASK_SWITCH) {
		    real_printf("                       -------\n");
		}
		if (e->cmd == EC_DUMP || e->cmd == EC_DUMP_CONT || e->dump_point)
			real_printf("*** DUMP *** DUMP *** DUMP *** DUMP *** DUMP *** DUMP *** DUMP *** DUMP *** DUMP *** DUMP *** DUMP *** DUMP ***\n");
	}

#ifndef EVENT_DUMP_WHILE_RUNNING
	//if (lo == 0) real_printf("12345678 xxx.xxx xxx.xxx xxx.xxxxxx xxx.xxx\n");
	  if (lo == 0) real_printf("*** DUMP  seq ms task ms        sec trg3 ms\n");
#endif
}

static u64_t ev_epoch;
static u4_t ev_dump_ms, ev_dump_expire;
static bool ev_dump_continue;
static u4_t last_time, tlast[NEVT], triggered, ev_trig1, ev_trig2, ev_trig3;
//static u4_t ev_trig3[256];

void ev(int cmd, int event, int param, const char *s, const char *s2)
{
	int i, id = param;
	int tid = TaskID();
	ev_t *e;
	u4_t now_ms = timer_ms();
	u64_t now_us = timer_us64();
	int free_s2 = ((u64_t) s2 & 1)? 1:0;
	s2 = (char*) ((u64_t) s2 & ~1);
	
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
	if ((ev_dump == -1) || ((cmd == EC_DUMP || cmd == EC_DUMP_CONT) && param <= 0) || (ev_dump_ms && (now_ms > ev_dump_expire))) {
	//if (cmd == EC_DUMP) {
		e->tlast = 0;
		if (ev_wrapped) evdump(REG, evc+1, NEV);
		evdump(REG, 0, evc);
		if (ev_wrapped) evdump(SUMMARY, evc+1, NEV);
		evdump(SUMMARY, 0, evc);
		if (ev_dump_ms) printf("expiration of %.3f sec dump time\n", ev_dump_ms/1000.0);
		dump();
		if (cmd == EC_DUMP_CONT || ev_dump_continue) {
		    // reset
		    ev_dump_ms = ev_dump_expire = 0;
		    ev_dump_continue = false;
		    return;
		}
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
	assert(((u64_t) ret & 1) == 0);
	return (char*) ((u64_t) ret | 1);
}

#endif
