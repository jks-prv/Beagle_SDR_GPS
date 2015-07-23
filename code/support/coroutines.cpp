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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <setjmp.h>
#include <sched.h>
#include <sys/time.h>

#include "types.h"
#include "wrx.h"
#include "config.h"
#include "misc.h"
#include "coroutines.h"

#define	STACK_ALIGN	0xff
#define STACK_SIZE	(64*K)

struct TASK {
	int id;
	TASK *next, *prev;
	u4_t priority;
	u4_t deadline;
	u4_t run;
	funcP_t funcP;
	const char *name;
	void *param;
	bool valid, stopped, wakeup;
	int wake_param;
	u4_t usec, quanta;
    int stk2[STACK_SIZE];		// fixme remove!
    int stk[STACK_SIZE];
    union {
        jmp_buf jb;
        struct {
 #if defined(__x86_64__)
			u_int64_t x1, fp, sp, x2[4], pc;
 #endif
 #if defined(__ARM_EABI__)
            u_int32_t v[6], sl, fp, sp, pc;
 #endif
        };
    };
};

static TASK Tasks[MAX_TASKS];
static TASK *cur_task, *cur_low, *prio_high, *prio_low;
static u4_t epoch;

struct TSLICE {
	int id;
	u4_t usec;
	int wakeup;
	u4_t twoke;
	const char *s;
	char msg[32];
};

#define NSLICES	(1*1024)
static TSLICE slices[NSLICES];
static int slice;

// print per-task accumulated usec runtime since last dump
void TaskDump()
{
	int i;
	TASK *t;
	u4_t now = timer_us();
	u4_t elapsed = now - epoch;
	float f_elapsed = ((float) elapsed) / 1e6;
	float f_sum = 0;
	
	for (i=0; i<MAX_TASKS; i++) {
		t = Tasks + i;
		if (t->valid) {
			float f_usec = ((float) t->usec) / 1e6;
			f_sum += f_usec;
			lprintf("T%02d %c%c%c %.3fuS %4.1f%% %6.3fM %7.3fdl %s\n", i,
				(t->priority == HIGH_PRIORITY)? 'H':'L',
				t->stopped? 'S':'R',
				t->quanta? 'q':' ',
				f_usec, f_usec/f_elapsed*100, (float) t->run / 1e6,
				t->deadline? ((float) (t->deadline-now) / 1e3) : 0, t->name);
			t->usec = 0;
		}
	}
	float f_remain = f_elapsed - f_sum;
	if (f_remain > 0) lprintf("   %.3f %4.1f%% (unaccounted)\n", f_remain, f_remain/f_elapsed*100);
	epoch = timer_us();
}

static volatile u4_t start_us, start_ms;

static void task_init(TASK *t, int id, funcP_t funcP, void *param, const char *name, u4_t priority)
{
	t->id = id;
	t->funcP = funcP;
	t->param = param;
	t->name = name;
	t->priority = priority;
	t->deadline = 0;
	t->stopped = FALSE;
	t->wakeup = FALSE;
	t->usec = 0;
	t->quanta = 0;
	t->valid = TRUE;
}

void TaskInit()
{
	static bool init;
    TASK *t;
	
	//printf("MAX_TASKS %d\n", MAX_TASKS);
	epoch = start_us = timer_us();
	start_ms = timer_ms();
	t = Tasks;
	cur_task = cur_low = t;
	prio_low = t;
	t->next = t->prev = NULL;
	task_init(t, 0, NULL, NULL, "main", LOW_PRIORITY);
}

void TaskRemove(int id)
{
    TASK *t = Tasks + id;
    t->valid = FALSE;
    
    if (t->prev) {
    	t->prev->next = t->next;
    } else {
    	if (t == prio_high) {
    		prio_high = t->next;
    	} else {
    		assert(t == prio_low);
    		prio_low = t->next;
    	}
    	if (t->next) t->next->prev = 0;
    }
    	
    NextTask();
	if (t == cur_task && !t->valid) panic("shouldn't return");
}

char *NextTaskM()
{
	if (slice) {
    	TSLICE *ts = &slices[slice-1];
    	return ts->msg;
	} else {
		return NULL;
	}
}

#ifdef DEBUG
 void _NextTaskL(const char *s)
#else
 void _NextTaskL()
#endif
{
	int i;
    TASK *t, *tn;
    u4_t now, quanta;

	t = cur_task;

    // don't switch until quanta expired (if any)
    if (t->quanta && ((timer_ms() - start_ms) < t->quanta))
    	return;
    
    ev(EV_NEXTTASK, "NextTask", "");
    
    now = timer_us();
    quanta = now - start_us;
    t->usec += quanta;
	
    if (slice) {
    	TSLICE *ts = &slices[slice-1];
    	ts->id = t->id;
    	ts->usec = quanta;
    	ts->wakeup = rx0_wakeup;
    	ts->twoke = now - rx0_twoke;
    	
    	#ifdef DEBUG
    		ts->s = s;
    	#endif
    	
    	slice++;
    	
    	if (slice == NSLICES) {
    		u4_t maxv=0; int maxi=0;
    		for (i=0; i<NSLICES; i++) {
    			ts = &slices[i];
    			if (ts->usec > maxv) { maxv = ts->usec; maxi = i; }
    		}
    		for (i=0; i<NSLICES; i++) {
    			ts = &slices[i];
    			printf("%4d T%02d %7.3f %7.3f w%d %s %s %s %s\n",
    				i, ts->id, ((float) ts->usec) / 1e3, ((float) ts->twoke) / 1e3,
    				ts->wakeup, Tasks[ts->id].name,
    				ts->s? ts->s : "", ts->msg,
    				(i==maxi)? "############################################": (
    				(ts->usec>=2000.0)? "------------------------------------------":"") );
    		}
    		xit(0);
    		slice = 0;
    	}
    }
    
    if (setjmp(t->jb)) {
    	return;
    }
    
	//printf("P%d-%s %.3f ms pc %p\n", t->id, t->name, ((float) quanta) / 1e3, (void *) t->pc); fflush(stdout);
    
    for (i=0; i<MAX_TASKS; i++) {
    	TASK *tp = Tasks + i;
    	if (tp->valid && tp->deadline && (tp->deadline < now)) {
    		tp->deadline = 0;
    		tp->stopped = FALSE;
    		tp->wakeup = TRUE;
//printf("wake T%02d\n", tp->id);
    	}
    }
    
    if (t->priority == HIGH_PRIORITY) {
		do {	// after a high priority finishes run next high priority
			t = t->next;
		} while (t && t->stopped);
		
    	if (!t) {	// no other high priority, so run next low priority
    		t = cur_low;
    		do {
    			t = t->next;
    			if (!t) t = prio_low;
    		} while (t->stopped);
    	}
    } else {
    	// run _all_ high priority after each low priority finishes
		for (t = prio_high; t && t->stopped; t = t->next)
			;
    	if (!t) {	// no high priority so run next low priority
    		t = cur_task;
    		do {
    			t = t->next;
    			if (!t) t = prio_low;
    		} while (t->stopped);
    	}
    }

    cur_task = t;
    if (t->priority == LOW_PRIORITY) cur_low = t;
    t->run++;

	//printf("N%d-%s pc %p\n", t->id, t->name, (void *) t->pc); fflush(stdout);
	start_us = now;
	if (t->quanta) start_ms = timer_ms();
    longjmp(t->jb, 1);
}

void NextTask()
{
	_NextTaskL(NULL);
}

static int create_task(func_t entry, funcP_t funcP, const char *name, int priority, void *param)
{
	int i;
    TASK *t;
    
	for (i=0; i<MAX_TASKS; i++) {
		t = Tasks + i;
		if (!t->valid) break;
	}
	if (i == MAX_TASKS) panic("create_task");
	
	task_init(t, i, funcP, param, name, priority);
    
    if (priority == HIGH_PRIORITY) {
    	t->next = prio_high;
    	prio_high = t;
    } else {
    	t->next = prio_low;
    	prio_low = t;
    }
    if (t->next) t->next->prev = t;
    t->prev = NULL;
    
	//printf("C%d-%s entry %p func %p param %p\n", t->id, name, entry, funcP, param); fflush(stdout);
    setjmp(t->jb);
    t->pc = (u_int64_t) entry;
	t->sp = ((u_int64_t) (t->stk + STACK_SIZE) & ~STACK_ALIGN) + 8;	// +8 for proper alignment moving doubles to stack

	return t->id;
}

static void trampoline()
{
    TASK *t = cur_task;
	(t->funcP)(t->param);
}

int _CreateTask(func_t entry, const char *name, int priority)
{
	return create_task(entry, (funcP_t) 0, name, priority, 0);
}

int _CreateTaskP(funcP_t entry, const char *name, int priority, void *param)
{
	return create_task(&trampoline, entry, name, priority, param);
}

int TaskSleep(u4_t usec)
{
    TASK *t = cur_task;

//printf("sleep T%02d %d usec\n", t->id, usec);
    t->deadline = usec? (timer_us() + usec) : 0;
    t->stopped = TRUE;
    do { NextTaskL("TaskSleep"); } while (!t->wakeup);
//printf("woke T%02d %d usec\n", t->id, usec);
//if (t->id==4 || t->id==6) printf("wo%d ", t->id); fflush(stdout);
    t->deadline = 0;
    t->stopped = FALSE;
	t->wakeup = FALSE;
	return t->wake_param;
}

void TaskWakeup(int id, bool check_waking, int wake_param)
{
    TASK *t = Tasks + id;
    
    if (!t->valid) return;
    t->deadline = 0;
    t->stopped = FALSE;
	if (check_waking) assert(!t->wakeup);
//printf("wa%d ", t->id); fflush(stdout);
    t->wakeup = TRUE;
    t->wake_param = wake_param;
}

u4_t TaskID()
{
	return cur_task->id;
}

const char *TaskName()
{
	return cur_task->name;
}

void TaskParams(u4_t quanta)
{
    TASK *t = cur_task;

	t->quanta = quanta;
}

void StartSlice()
{
	printf("start slice\n");
	slice = 1;
}

// Critical section "first come, first served"

void _lock_init(lock_t *lock, const char *name)
{
	memset(lock, 0, sizeof(*lock));
	lock->name = name;
	lock->init = true;
	lock->magic_b = LOCK_MAGIC_B;
	lock->magic_e = LOCK_MAGIC_E;
}

#define check_lock() \
	if (lock->magic_b != LOCK_MAGIC_B || lock->magic_e != LOCK_MAGIC_E) \
		lprintf("*** BAD LOCK MAGIC 0x%x 0x%x\n", lock->magic_b, lock->magic_e);


void lock_enter(lock_t *lock)
{
	if (!lock->init) return;
	check_lock();
    int token = lock->enter++;
    //bool dbg = (strcmp(lock->name, "&waterfall_hw_lock") == 0);
    bool dbg = false;
    bool waiting = false; 
    while (token > lock->leave) {
		if (dbg && !waiting) {
			printf("LOCK t%d %s WAIT %s\n", TaskID(), TaskName(), lock->name);
			waiting = true;
		}
    	NextTaskL("lock enter");
		check_lock();
    }
	if (dbg) printf("LOCK t%d %s ACQUIRE %s\n", TaskID(), TaskName(), lock->name);
}

void lock_leave(lock_t *lock)
{
	if (!lock->init) return;
	check_lock();
    //bool dbg = (strcmp(lock->name, "&waterfall_hw_lock") == 0);
    bool dbg = false;
    lock->leave++;
	if (dbg) printf("LOCK t%d %s RELEASE %s\n", TaskID(), TaskName(), lock->name);
}
