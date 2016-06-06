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
	u4_t tprio, tid, tseq, tlast, tepoch, ttask, trig1, trig2, trig3;
	bool dump_point;
} ev_t;

static int evc, ev_wrapped;

#define NEV 8192
//#define NEV 64
ev_t evs[NEV+32];

const char *evcmd[NEVT] = {
	"Event", "Dump", "Task", "Trig1", "Trig2", "Trig3"
};

const char *evn[NEVT] = {
	"Panic", "NextTask", "SPI", "WF", "SND", "GPS", "DataPump", "Printf"
};

static void evdump(int lo, int hi)
{
	ev_t *e;

	for (int i=lo; i<hi; i++) {
		assert(i >= 0 && i < NEV);
		e = &evs[i];
		#if 0
		printf("%4d %5s %8s %7.3f %10.6f %7.3f %7.3f %7.3f %16s:P%d:T%02d, %8s %s\n", i, evcmd[e->cmd], evn[e->event],
			/*(float) e->tlast / 1000,*/ (float) e->tseq / 1000, (float) e->tepoch / 1000000,
			(float) e->trig1 / 1000, (float) e->trig2 / 1000, (float) e->trig3 / 1000,
			e->task, e->tprio, e->tid, e->s, e->s2);
		#else
		
		printf("%8s %7.3f %7.3f %10.6f ", evn[e->event],
			(float) e->tseq / 1000, (float) e->ttask / 1000, (float) e->tepoch / 1000000);
		if (e->trig3)
			printf("%7.3f ", (float) e->trig3 / 1000);
		else
			printf("------- ");
		printf("%16s:P%d:T%02d, %8s %s\n",
			e->task, e->tprio, e->tid, e->s, e->s2);
		#endif
		if (e->cmd == EC_TASK) printf("                 -------\n");
		if (e->cmd == EC_DUMP || e->dump_point)
			printf("*** DUMP *** DUMP *** DUMP *** DUMP *** DUMP *** DUMP *** DUMP *** DUMP *** DUMP *** DUMP *** DUMP *** DUMP ***\n");
	}

#ifndef EVENT_DUMP_WHILE_RUNNING
	//if (lo == 0) printf("12345678 xxx.xxx xxx.xxx xxx.xxxxxx xxx.xxx\n");
	  if (lo == 0) printf("*** DUMP  seq ms task ms        sec trg3 ms\n");
#endif
}

static u64_t ev_epoch;
static u4_t ev_dump_ms, ev_dump_expire;
static u4_t last_time, tlast[NEVT], triggered, ev_trig1, ev_trig2, ev_trig3;
//static u4_t ev_trig3[256];

void ev(int cmd, int event, int param, const char *s, const char *s2)
{
	int i, id = param;
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
	
	if (cmd == EC_DUMP && param > 0 && ev_dump_ms) return;
	
	evc++;
	if (evc >= NEV) {
		evc = 0;
		ev_wrapped = 1;
	}
	
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
	
	if (cmd == EC_TRIG3) {
		//if (id >= 0 && id < 256)
		//ev_trig3[id] = now_us;
		ev_trig3 = now_us;
	}
	
	e->prev_valid = 1;
	e->cmd = cmd;
	e->event = event;
	e->s = s;
	e->s2 = s2;
	e->free_s2 = free_s2;
	e->tseq = now_us - last_time;
	e->tlast = now_us - tlast[event];
	//e->tepoch = now_us - ev_epoch;
	e->tepoch = now_us;
	e->trig1 = ev_trig1? (now_us - ev_trig1) : 0;
	e->trig2 = ev_trig2? (now_us - ev_trig2) : 0;
	//e->trig3 = (id != -1 && ev_trig3[id])? (now_us - ev_trig3[id]) : 0;
	//e->trig3 = (id >= 0 && id < 256 && ev_trig3[id])? (now_us - ev_trig3[id]) : 0;
	e->trig3 = ev_trig3? (now_us - ev_trig3) : 0;
	e->tprio = TaskPriority(-1);
	e->tid = TaskID();
	e->task = TaskName();
	e->dump_point = false;
	e->ttask = now_us - TaskStartTime();
	
	if (cmd == EC_DUMP && param > 0) { ev_dump_ms = param; ev_dump_expire = now_ms + param;}
	if (cmd != EC_TRIG3 && param > 0 && !ev_dump_ms) { ev_dump_ms = param; ev_dump_expire = now_ms + param; e->dump_point = true; }

#ifdef EVENT_DUMP_WHILE_RUNNING
	evdump(0, 1);
	evc = 0;
#else
	if ((ev_dump == -1) || (cmd == EC_DUMP && param <= 0) || (ev_dump_ms && (now_ms > ev_dump_expire))) {
		e->tlast = 0;
		if (ev_wrapped) evdump(evc+1, NEV);
		evdump(0, evc);
		if (ev_dump_ms) printf("expiration of %.3f sec dump time\n", ev_dump_ms/1000.0);
		if (event != EV_PANIC) xit(0);
		// return if called from a panic so it can continue on to print the panic message
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
