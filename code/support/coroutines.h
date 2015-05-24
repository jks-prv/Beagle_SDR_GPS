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

#ifndef	_COROUTINES_H_
#define	_COROUTINES_H_

#include "types.h"
#include "config.h"
#include "timing.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#define	HIGH_PRIORITY	1
#define	LOW_PRIORITY	0

typedef void (*func_t)();
typedef void (*funcP_t)(void *);

void TaskInit();
void NextTask();
void _NextTaskL(const char *s);
char *NextTaskM();
#define CreateTask(f, priority)			_CreateTask(f, #f, priority)
#define CreateTaskP(f, priority, param)	_CreateTaskP(f, #f, priority, param)
int _CreateTask(func_t entry, const char *name, int priority);
int _CreateTaskP(funcP_t entry, const char *name, int priority, void *param);
int TaskSleep(u4_t deadline);
void TaskWakeup(int id, bool check_waking, int wake_param);
void TaskRemove(int id);
void TaskParams(u4_t quanta);
void TaskDump();
u4_t TaskID();
const char *TaskName();
void StartSlice();

#ifdef DEBUG
 #define NextTaskL(s)	_NextTaskL(s);
#else
 #define NextTaskL(s)	_NextTaskL();
#endif

typedef struct {
	bool init;
	u4_t enter, leave;
	const char *name;
} lock_t;

#define lock_init(lock) _lock_init(lock, #lock)
void _lock_init(lock_t *lock, const char *name);
void lock_enter(lock_t *lock);
void lock_leave(lock_t *lock);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif
