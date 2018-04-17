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

// Copyright (c) 2014-2016 John Seamons, ZL/KF6VO

#ifndef	_COROUTINES_H_
#define	_COROUTINES_H_

#include "types.h"
#include "config.h"
#include "timing.h"
#include "peri.h"

#include <setjmp.h>

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#define	HIGHEST_PRIORITY	7

#define	SPIPUMP_PRIORITY	7
#define	DATAPUMP_PRIORITY	6
#define	SND_PRIORITY		5

// essentially round-robin all of these:
#define	EXT_PRIORITY		2
#define	WF_PRIORITY			2
#define ADMIN_PRIORITY		2
#define	WEBSERVER_PRIORITY	2
#define	GPS_PRIORITY		2
#define GPS_ACQ_PRIORITY	2
#define	MAIN_PRIORITY		2

#define	LOWEST_PRIORITY		0
#define	NUM_PRIORITY		(HIGHEST_PRIORITY+1)

#define	MISC_TASKS			6					// main, stats, spi, data pump, web server, sdr_hu
#define GPS_TASKS			(GPS_CHANS + 3)		// chan*n + search + solve + stat
#define	RX_TASKS			(RX_CHANS * 2)		// SND, W/F
#define	EXT_TASKS			RX_CHANS			// each extension server-side part runs as a separate task
#define	ADMIN_TASKS			4					// simultaneous admin connections
#define	EXTRA_TASKS			16
#define	MAX_TASKS           (MISC_TASKS + GPS_TASKS + RX_TASKS + EXT_TASKS + ADMIN_TASKS + EXTRA_TASKS)

typedef int tid_t;

void TaskInit();
void TaskCollect();

#define CTF_CHANNEL         0x000f
#define CTF_RX_CHANNEL		0x0010
#define CTF_BUSY_HELPER		0x0020
#define CTF_POLL_INTR		0x0040
#define CTF_FORK_CHILD		0x0080
#define CTF_PRIO_INVERSION  0x0100
#define CTF_NO_CHARGE		0x0200
#define CTF_TNAME_FREE		0x0400

int _CreateTask(funcP_t entry, const char *name, void *param, int priority, u4_t flags, int f_arg);
#define CreateTask(f, param, priority)				_CreateTask(f, #f, param, priority, 0, 0)
#define CreateTaskF(f, param, priority, flags, fa)	_CreateTask(f, #f, param, priority, flags, fa)
#define CreateTaskSF(f, s, param, priority, flags, fa)	_CreateTask(f, s, param, priority, flags, fa)

// usec == 0 means sleep until someone does TaskWakeup() on us
// usec > 0 is microseconds time in future (added to current time)
void *_TaskSleep(const char *reason, int usec);
#define TaskSleep()                 _TaskSleep("TaskSleep", 0)
#define TaskSleepUsec(us)           _TaskSleep("TaskSleep", us)
#define TaskSleepMsec(ms)           _TaskSleep("TaskSleep", MSEC_TO_USEC(ms))
#define TaskSleepSec(s)             _TaskSleep("TaskSleep", SEC_TO_USEC(s))
#define TaskSleepReason(r)          _TaskSleep(r, 0)
#define TaskSleepReasonUsec(r, us)  _TaskSleep(r, us)
#define TaskSleepReasonMsec(r, ms)  _TaskSleep(r, MSEC_TO_USEC(ms))
#define TaskSleepReasonSec(r, s)    _TaskSleep(r, SEC_TO_USEC(s))

void TaskSleepID(int id, int usec);
void TaskWakeup(int id, bool check_waking, void *wake_param);

enum ipoll_from_e {
	CALLED_FROM_INIT,
	CALLED_WITHIN_NEXTTASK,
	CALLED_FROM_LOCK,
	CALLED_FROM_SPI,
	CALLED_FROM_FASTINTR,
};

extern bool itask_run;
void TaskPollForInterrupt(ipoll_from_e from);
#define TaskFastIntr(s)			if (GPIO_READ_BIT(GPIO0_15)) TaskPollForInterrupt(CALLED_FROM_FASTINTR);

void TaskRemove(int id);
void TaskParams(u4_t quanta_us);
u4_t TaskFlags();
void TaskLastRun();
u4_t TaskID();
u4_t TaskPriority(int priority);
void TaskCheckStacks();
u64_t TaskStartTime();
void TaskForkChild();
bool TaskIsChild();

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

char *Task_s(int id);
char *Task_ls(int id);

#define	TSTAT_MASK		0x00ff
#define	TSTAT_NC		0
#define	TSTAT_SET		1
#define	TSTAT_INCR		2
#define	TSTAT_MAX		3

#define	TSTAT_LATCH		0x0f00
#define	TSTAT_ZERO		0x0100
#define	TSTAT_CMDS		0x0200

#define TSTAT_SPI_RETRY	0x1000

int TaskStatU(u4_t s1_func, int s1_val, const char *s1_units, u4_t s2_func, int s2_val, const char *s2_units);
#define TaskStat(f1, v1, f2, v2) TaskStatU(f1, v1, NULL, f2, v2, NULL)

#define	NT_NONE			0
#define	NT_BUSY_WAIT	1
#define	NT_LONG_RUN		2

#ifdef DEBUG
 void _NextTask(const char *s, u4_t param, u_int64_t pc);
#else
 void _NextTask(u4_t param);
#endif

#ifdef DEBUG
 #define NextTask(s)		NextTaskW(s, NT_NONE);
 #define NextTaskP(s,p)		NextTaskW(s, p);
 #define NextTaskW(s,p)		_NextTask(s, p, 0);
#else
 #define NextTask(s)		_NextTask(NT_NONE);
 #define NextTaskP(s,p)		_NextTask(p);
 #define NextTaskW(s,p)		_NextTask(p);
#endif

#define LOCK_MAGIC_B	0x10ccbbbb
#define LOCK_MAGIC_E	0x10cceeee

struct lock_t {
	u4_t magic_b;
	bool init;
	u4_t enter, leave;
	const char *name;
	char *enter_name;
	void *owner;
	void *users;                    // queue of lock users
	u4_t n_prio_swap, n_prio_inversion;
	u4_t timer_since_no_owner;
	u4_t magic_e;
};

void _lock_init(lock_t *lock, const char *name);
#define lock_init(lock) _lock_init(lock, #lock)
#define lock_initS(lock, name) _lock_init(lock, name)

void lock_register(lock_t *lock);
void lock_dump();
bool lock_check();
void lock_enter(lock_t *lock);
void lock_leave(lock_t *lock);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif
