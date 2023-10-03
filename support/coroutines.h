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

// Copyright (c) 2014-2016 John Seamons, ZL4VO/KF6VO

#ifndef	_COROUTINES_H_
#define	_COROUTINES_H_

#include "types.h"
#include "config.h"
#include "timing.h"

#include <setjmp.h>

#define	HIGHEST_PRIORITY	7

#define	SPIPUMP_PRIORITY	7
#define	DATAPUMP_PRIORITY	6

#define	SND_PRIORITY		5

extern u4_t task_medium_priority;
#define TASK_MED_PRI_NEW    3
#define TASK_MED_PRI_OLD    2

// an experiment to favor SSTV (an EXT) over WF
#define	TASK_MED_PRIORITY   -1
#define	EXT_PRIORITY		task_medium_priority
#define WEBSERVER_PRIORITY  task_medium_priority
#define ADMIN_PRIORITY		task_medium_priority
#define	GPS_PRIORITY		task_medium_priority

#define EXT_PRIORITY_LOW    2
#define	WF_PRIORITY			2
#define GPS_ACQ_PRIORITY	2
#define SERVICES_PRIORITY   2
#define	MAIN_PRIORITY		2

#define CAT_PRIORITY        1

#define	LOWEST_PRIORITY		0
#define	NUM_PRIORITY		(HIGHEST_PRIORITY+1)

#define	MISC_TASKS			7					// main, stats, spi pump, data pump, web server, registration, SNR meas
#define GPS_TASKS			(GPS_MAX_CHANS + 3) // chan*n + search + solve + stat
#define SND_EXT_TASKS       (MAX_RX_CHANS * (2 + N_CAMP))   // W/F handled below
#define	EXTRA_TASKS			(MAX_RX_CHANS * 4)  // additional tasks created by extensions etc.

#define	REG_STACK_TASKS     (MISC_TASKS + GPS_TASKS + SND_EXT_TASKS + N_QUEUERS + N_CONN_ADMIN + EXTRA_TASKS)
#define MED_STACK_TASKS     (MAX_RX_CHANS + 1)  // WF (+ reload slop)
#define LARGE_STACK_TASKS   (1 + 1)             // DRM (+ reload slop)

#define	MAX_TASKS           (REG_STACK_TASKS + MED_STACK_TASKS + LARGE_STACK_TASKS)

typedef int tid_t;
#define TID_MAIN    0
#define TID_FIRST   (TID_MAIN + 1)

void TaskInit();
void TaskInitCfg();
void TaskCollect();

#define CTF_CHANNEL         (MAX_RX_CHANS - 1)
#define CTF_RX_CHANNEL		0x0010
#define CTF_BUSY_HELPER		0x0020
#define CTF_POLL_INTR		0x0040
#define CTF_FORK_CHILD		0x0080
#define CTF_PRIO_INVERSION  0x0100
#define CTF_NO_CHARGE		0x0200
#define CTF_TNAME_FREE		0x0400
#define CTF_NO_PRIO_INV		0x0800

#define CTF_STACK_SIZE		0x3000
#define CTF_STACK_REG		0x0000
#define CTF_STACK_MED		0x1000
#define CTF_STACK_LARGE		0x2000

#define CTF_SOFT_FAIL		0x4000
#define CTF_NO_LOG		    0x8000

C_LINKAGE(int _CreateTask(funcP_t entry, const char *name, void *param, int priority, u4_t flags, int f_arg));
#define CreateTask(f, param, priority)				    _CreateTask(f, #f, param, priority, 0, 0)
#define CreateTaskF(f, param, priority, flags)          _CreateTask(f, #f, param, priority, flags, 0)
#define CreateTaskFA(f, param, priority, flags, fa)     _CreateTask(f, #f, param, priority, flags, fa)
#define CreateTaskSF(f, s, param, priority, flags, fa)	_CreateTask(f, s, param, priority, flags, fa)

// usec == 0 means sleep until someone does TaskWakeup() on us
// usec > 0 is microseconds time in future (added to current time)
C_LINKAGE(void *_TaskSleep(const char *reason, u64_t usec, u4_t *wakeup_test));
#define TaskSleep()                 _TaskSleep("TaskSleep", 0, NULL)
#define TaskSleepUsec(us)           _TaskSleep("TaskSleep", us, NULL)
#define TaskSleepMsec(ms)           _TaskSleep("TaskSleep", MSEC_TO_USEC(ms), NULL)
#define TaskSleepSec(s)             _TaskSleep("TaskSleep", SEC_TO_USEC(s), NULL)
#define TaskSleepReason(r)          _TaskSleep(r, 0, NULL)
#define TaskSleepReasonUsec(r, us)  _TaskSleep(r, us, NULL)
#define TaskSleepReasonMsec(r, ms)  _TaskSleep(r, MSEC_TO_USEC(ms), NULL)
#define TaskSleepReasonSec(r, s)    _TaskSleep(r, SEC_TO_USEC(s), NULL)
#define TaskSleepWakeupTest(r, wu)  _TaskSleep(r, 0, wu)

void TaskSleepID(int id, u64_t usec);

#define TWF_NONE                0x0000
#define TWF_CHECK_WAKING        0x0001
#define TWF_CANCEL_DEADLINE     0x0002

C_LINKAGE(void _TaskWakeup(int id, u4_t flags, void *wake_param));
#define TaskWakeup(id)          _TaskWakeup(id, TWF_NONE, 0);
#define TaskWakeupF(id,f)       _TaskWakeup(id, f, 0);
#define TaskWakeupFP(id,f,p)    _TaskWakeup(id, f, p);

typedef enum {
	CALLED_FROM_INIT,
	CALLED_WITHIN_NEXTTASK,
	CALLED_FROM_LOCK,
	CALLED_FROM_SPI,
	CALLED_FROM_FASTINTR,
} ipoll_from_e;

extern bool itask_run;
void TaskPollForInterrupt(ipoll_from_e from);
#define TaskFastIntr(s)			if (GPIO_READ_BIT(SND_INTR)) TaskPollForInterrupt(CALLED_FROM_FASTINTR);

void TaskRemove(int id);
void TaskMinRun(u4_t minrun_us);
u4_t TaskFlags();
void TaskSetFlags(u4_t flags);
void TaskLastRun();
u4_t TaskPriority(int priority);
void TaskCheckStacks(bool report);
u64_t TaskStartTime();
void TaskForkChild();
bool TaskIsChild();

C_LINKAGE(u4_t TaskID());
C_LINKAGE(void *TaskGetUserParam());
C_LINKAGE(void TaskSetUserParam(void *param));

// don't collide with PRINTF_FLAGS
#define	TDUMP_PRINTF    0x00ff
#define	TDUMP_REG       0x0000
#define	TDUMP_LOG       0x0100		// shorter lines suitable for /dump URL
#define	TDUMP_HIST      0x0200		// include runtime histogram
#define	TDUMP_CLR_HIST  0x0400		// clear runtime histogram
void TaskDump(u4_t flags);

const char *_TaskName(const char *name, bool free_name);
#define TaskName()          _TaskName(NULL, false)
#define TaskNameS(name)     _TaskName(name, false)
#define TaskNameSFree(name) _TaskName(name, true)

C_LINKAGE(char *Task_s(int id));
C_LINKAGE(char *Task_ls(int id));

#define	TSTAT_MASK		0x00ff
#define	TSTAT_NC		0
#define	TSTAT_SET		1
#define	TSTAT_INCR		2
#define	TSTAT_MIN		3
#define	TSTAT_MAX		4

#define	TSTAT_LATCH		0x0f00
#define	TSTAT_ZERO		0x0100
#define	TSTAT_CMDS		0x0200

#define TSTAT_SPI_RETRY	0x1000

int TaskStat(u4_t s1_func, int s1_val, const char *s1_units, u4_t s2_func DEF_0, int s2_val DEF_0, const char *s2_units DEF_NULL);
#define TaskStat2(f, v, u) TaskStat(0, 0, NULL, f, v, u);

#define	NT_NONE			0
#define	NT_BUSY_WAIT	1
#define	NT_LONG_RUN		2
#define NT_FAST_CHECK   3

C_LINKAGE(void _NextTask(const char *s, u4_t param, u_int64_t pc));
#define NextTaskW(s,p)  _NextTask(s, p, 0);
#define NextTask(s)		NextTaskW(s, NT_NONE);
#define NextTaskP(s,p)  NextTaskW(s, p);
#define NextTaskFast(s) NextTaskW(s, NT_FAST_CHECK);

#define LOCK_MAGIC_B	0x10ccbbbb
#define LOCK_MAGIC_E	0x10cceeee

typedef struct {
	u4_t magic_b;
	bool init;
	u4_t enter, leave;
	const char *name;
	#define LEN_ENTER_NAME  32
	char enter_name[LEN_ENTER_NAME];
	void *owner;
	void *users;                    // queue of lock users
	u4_t n_prio_swap, n_prio_inversion;
	u4_t timer_since_no_owner;
	u4_t magic_e;
} lock_t;

#define lock_init(lock) _lock_init(lock, #lock)
#define lock_initS(lock, name) _lock_init(lock, name)

C_LINKAGE(void _lock_init(lock_t *lock, const char *name));
C_LINKAGE(void lock_register(lock_t *lock));
C_LINKAGE(void lock_unregister(lock_t *lock));
C_LINKAGE(void lock_dump());
C_LINKAGE(bool lock_check());
C_LINKAGE(void lock_enter(lock_t *lock));
C_LINKAGE(void lock_leave(lock_t *lock));

#endif
