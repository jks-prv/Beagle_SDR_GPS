#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_RX_CHANS        16

#define TO_VOID_PARAM(p)    ((void *) (long) (p))
#define FROM_VOID_PARAM(p)  ((long) (p))

#define	STRINGIFY(x) #x

bool hfdl_msgs(char *msg, int rx_chan);
int ext_send_msg_encoded(int rx_chan, bool debug, const char *dst, const char *cmd, const char *fmt, ...);

uint32_t TaskID();

#ifdef OPT_PTHR
#else

void *TaskGetUserParam();
void TaskSetUserParam(void *param);

void _NextTask(const char *s, unsigned int param, unsigned long long pc);
#define NextTask(s) _NextTask(s, 0, 0);

void *_TaskSleep(const char *reason, unsigned long long usec, unsigned int *wakeup_test);
#define TaskSleepReason(reason)         _TaskSleep(reason, 0, NULL)
#define TaskSleepReasonMsec(reason, ms) _TaskSleep(reason, (ms)*1000, NULL)

#endif

#ifdef __cplusplus
}
#endif

int asprintf(char **strp, const char *fmt, ...);    // can't seem to get this from system includes for some reason
const char *aspf(const char *fmt, ...);
