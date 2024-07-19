#pragma once

#include "types.h"
#include "datatypes.h"      // TYPESTEREO16
#include "coroutines.h"     // tid_t

#define DRM_CHECKING
#ifdef DRM_CHECKING
    #define drm_array_dim(d,l) assert_array_dim(d,l)
    #define DRM_CHECK(x) x
    #define DRM_CHECK_ALT(x,y) x
#else
    #define drm_array_dim(d,l)
    #define DRM_CHECK(x)
    #define DRM_CHECK_ALT(x,y) y
#endif

enum { DRM_DAT_IQ=0 } drm_dat_e;

typedef struct {
    #define N_DRM_OBUF 32
    #define N_DRM_OSAMPS 9600
    u4_t out_wr_pos, out_rd_pos;
    int out_pos, out_samps;
    TYPESTEREO16 out_samples[N_DRM_OBUF][N_DRM_OSAMPS];
} drm_buf_t;

typedef struct {
    int drm_chan;
    
    s2_t *s2p_start1, *s2p_end1;
    s2_t *s2p_start2, *s2p_end2;
    u4_t tsamps1, tsamps2;
} drm_info_t;

typedef enum { DRM_MSG_STATUS, DRM_MSG_SERVICE, DRM_MSG_JOURNALINE, DRM_MSG_SLIDESHOW } drm_msg_e;

typedef struct {
    DRM_CHECK(u4_t magic1;)
	bool init;
	int rx_chan;
	tid_t tid;
	int iq_rd_pos, iq_bpos, remainingIQ;
	bool monitor, reset;
	int run;
	u4_t debug;
	bool dbgUs;
	int p_i[4];
	int use_LPF;
	drm_info_t *info;
	
	u4_t i_epoch;
	u4_t i_samples, i_tsamples;
	
	u4_t MeasureTime[16];
	
	int audio_service;
	bool send_iq;
	
	int journaline_objID;
	bool journaline_objSet;

    int test;
    s2_t *s2p;
    u4_t tsamp;

    // stats
    u4_t no_input;
    u4_t sent_silence;

    // DRM_msg_encoded()
    #define N_MSGBUF 4
    #define L_MSGCMD 32
    #define L_MSGBUF 4096
    char msg_cmd[N_MSGBUF][L_MSGCMD];
    char msg_buf[N_MSGBUF][L_MSGBUF];
    u4_t msg_tx_seq[N_MSGBUF], msg_rx_seq[N_MSGBUF];

    // DRM_data()
    u4_t data_tx_seq, data_rx_seq;
    u4_t data_nbuf;
    #define N_DATABUF (4 + 64*2 + 256*2 + 2048*2)
    u1_t data_buf[N_DATABUF];
    u1_t data_cmd;
    
    DRM_CHECK(u4_t magic2;)
} drm_t;

#ifndef DRM_MAX_RX
    #define DRM_MAX_RX 4
#endif

#define DRM_NREG_CHANS_DEFAULT 3

typedef struct {
    drm_t drm[DRM_MAX_RX];
    drm_buf_t drm_buf[DRM_MAX_RX];
} drm_shmem_t;

#include "shmem_config.h"

#ifdef DRM
    #ifdef MULTI_CORE
        //#define DRM_SHMEM_DISABLE_TEST
        #ifdef DRM_SHMEM_DISABLE_TEST
            #warning do not forget to remove DRM_SHMEM_DISABLE_TEST
            #define DRM_SHMEM_DISABLE
            #define RX_SHMEM_DISABLE
        #else
            // shared memory enabled
        #endif
    #else
        // normally shared memory disabled
        // but could be enabled for testing
        #define DRM_SHMEM_DISABLE
        #define RX_SHMEM_DISABLE
    #endif
#else
    #define DRM_SHMEM_DISABLE
#endif

#include "shmem.h"

#ifdef DRM_SHMEM_DISABLE
    extern drm_shmem_t *drm_shmem_p;
    #define DRM_SHMEM drm_shmem_p

    #define DRM_YIELD() NextTask("drm Y");
    #define DRM_YIELD_LOWER_PRIO() TaskSleepReasonUsec("drm YLP", 1000);
#else
    #define DRM_SHMEM (&shmem->drm_shmem)
    
    #ifdef MULTI_CORE
        // Processes run in parallel simultaneously and communicate w/ shmem mechanism.
        // But sleep a little bit to reduce cpu busy looping waiting for updates to shmem.
        // But don't sleep _too_ much else insufficient throughput.
        #define DRM_YIELD() kiwi_usleep(30000);
        //#define DRM_YIELD() kiwi_usleep(10000);
        #define DRM_YIELD_LOWER_PRIO() kiwi_usleep(1000);
    #else
        // experiment with shmem mechanism on uni-processors
        #define DRM_YIELD() kiwi_usleep(100);  // force process switch
    #endif
#endif

int DRM_rx_chan();
void DRM_msg_encoded(drm_msg_e msg_type, const char *cmd, kstr_t *ks);
void DRM_data(u1_t cmd, u1_t *data, u4_t nbuf);
