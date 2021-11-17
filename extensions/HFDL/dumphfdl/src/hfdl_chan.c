/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <unistd.h>
#include <complex.h>
#include <math.h>
#ifdef DEBUG
#include "pthr.h"                   // pthr_mutex_*
#endif
#include <string.h>                 // memcpy
#include <sys/time.h>               // struct timeval
#include <liquid/liquid.h>
#include "hfdl_config.h"            // *_DEBUG
#ifndef HAVE_PTHREAD_BARRIERS
#include "pthread_barrier.h"
#endif
#include "block.h"                  // struct block, block_connection_is_shutdown_signaled
#include "dumpfile.h"               // dumpfile_*
#include "util.h"                   // NEW, XCALLOC, octet_string_new
#include "fastddc.h"                // fft_channelizer_create, fastddc_inv_cc
#include "libfec/hfdl_fec.h"        // viterbi27
#include "hfdl.h"                   // HFDL_SYMBOL_RATE, SPS
#include "metadata.h"               // struct metadata
#include "pdu.h"                    // pdu_decoder_queue_push, hfdl_pdu_metadata_create
#include "statsd.h"                 // statsd_*

#define PREKEY_LEN 448
#define A_LEN 127
#define M1_LEN 127
#define M2_LEN 15
#define M_SHIFT_CNT 8
#define T_LEN 15
#define EQ_LEN 15
#define DATA_FRAME_LEN 30
#define DATA_FRAME_CNT_SINGLE_SLOT 72
#define DATA_FRAME_CNT_DOUBLE_SLOT 168
#define DATA_SYMBOLS_CNT_MAX (DATA_FRAME_CNT_DOUBLE_SLOT * DATA_FRAME_LEN)
#define PREAMBLE_LEN (2 * A_LEN + M1_LEN + M2_LEN + 9 * T_LEN)
#define SINGLE_SLOT_FRAME_LEN (PREKEY_LEN + PREAMBLE_LEN + DATA_FRAME_CNT_SINGLE_SLOT * (DATA_FRAME_LEN + T_LEN))
#define CORR_THRESHOLD_A1 0.36f
#define CORR_THRESHOLD_A2 0.3f
#define CORR_THRESHOLD_M1 0.3f
#define MAX_SEARCH_RETRIES 3
#define HFDL_SSB_CARRIER_OFFSET_HZ 1440

typedef enum {
	SAMPLER_EMIT_BITS = 1,
	SAMPLER_EMIT_SYMBOLS = 2,
	SAMPLER_SKIP = 3
} sampler_state;

typedef enum {
	FRAMER_A1_SEARCH = 1,
	FRAMER_A2_SEARCH = 2,
	FRAMER_M1_SEARCH = 3,
	FRAMER_M2_SKIP = 4,
	FRAMER_EQ_TRAIN = 5,
	FRAMER_DATA_1 = 6,      // first half of data frame
	FRAMER_DATA_2 = 7       // second half
} framer_state;

// values correspond to numbers of bits per symbol (arity)
typedef enum {
	M_UNKNOWN = 0,
	M_BPSK = 1,
	M_PSK4 = 2,
	M_PSK8 = 3
} mod_arity;
#define MOD_ARITY_MAX M_PSK8
#define MODULATION_CNT 4

struct hfdl_params {
	mod_arity scheme;
	int32_t data_segment_cnt;
	int32_t code_rate;
	int32_t deinterleaver_push_column_shift;
};

static struct hfdl_params const hfdl_frame_params[M_SHIFT_CNT] = {
	// 300 bps, single slot
	[0] = {
		.scheme = M_BPSK,
		.data_segment_cnt = DATA_FRAME_CNT_SINGLE_SLOT,
		.code_rate = 4,
		.deinterleaver_push_column_shift = 17
	},
	// 600 bps, single slot
	[1] = {
		.scheme = M_BPSK,
		.data_segment_cnt = DATA_FRAME_CNT_SINGLE_SLOT,
		.code_rate = 2,
		.deinterleaver_push_column_shift = 17
	},
	// 1200 bps, single slot
	[2] = {
		.scheme = M_PSK4,
		.data_segment_cnt = DATA_FRAME_CNT_SINGLE_SLOT,
		.code_rate = 2,
		.deinterleaver_push_column_shift = 17
	},
	// 1800 bps, single slot
	[3] = {
		.scheme = M_PSK8,
		.data_segment_cnt = DATA_FRAME_CNT_SINGLE_SLOT,
		.code_rate = 2,
		.deinterleaver_push_column_shift = 17
	},
	// 300 bps, double slot
	[4] = {
		.scheme = M_BPSK,
		.data_segment_cnt = DATA_FRAME_CNT_DOUBLE_SLOT,
		.code_rate = 4,
		.deinterleaver_push_column_shift = 23
	},
	// 600 bps, double slot
	[5] = {
		.scheme = M_BPSK,
		.data_segment_cnt = DATA_FRAME_CNT_DOUBLE_SLOT,
		.code_rate = 2,
		.deinterleaver_push_column_shift = 23
	},
	// 1200 bps, double slot
	[6] = {
		.scheme = M_PSK4,
		.data_segment_cnt = DATA_FRAME_CNT_DOUBLE_SLOT,
		.code_rate = 2,
		.deinterleaver_push_column_shift = 23
	},
	// 1800 bps, double slot
	[7] = {
		.scheme = M_PSK8,
		.data_segment_cnt = DATA_FRAME_CNT_DOUBLE_SLOT,
		.code_rate = 2,
		.deinterleaver_push_column_shift = 23
	}
};

#define SYMSYNC_PFB_CNT 16          // number of filter in symsync polyphase filterbank
#define HFDL_MF_SYMBOL_DELAY 3      // delay introduced by matched filter (measured in symbols)
#define HFDL_MF_TAPS_CNT 19         // SPS * 3 symbols of delay * 2 + 1
static float hfdl_matched_filter[HFDL_MF_TAPS_CNT] = {
	-0.0170974647427123, 0.01148231492068473, 0.03138375667422348, 0.009454398851680437,
	-0.04161644170893816, -0.06451564801420356, -0.005495792933327306, 0.1316404671361545,
	0.2759693160697777, 0.3375901874933208, 0.2759693160697777, 0.1316404671361545,
	-0.005495792933327306, -0.06451564801420356, -0.04161644170893816, 0.009454398851680437,
	0.03138375667422348, 0.01148231492068473, -0.0170974647427123
};
static float hfdl_matched_filter_interp[HFDL_MF_TAPS_CNT];

static float complex T_seq[2][T_LEN] = {
	[0] = { 1.f, 1.f, 1.f, -1.f, 1.f, 1.f, -1.f, -1.f, 1.f, -1.f, 1.f, -1.f, -1.f, -1.f, -1.f },
	[1] = { -1.f, -1.f, -1.f, 1.f, -1.f, -1.f, 1.f, 1.f, -1.f, 1.f, -1.f, 1.f, 1.f, 1.f, 1.f }
};

#ifdef DEBUG
    static struct {
        int32_t A1_found, A2_found, M1_found;
        int32_t train_bits_total, train_bits_bad;
        float A1_corr_total, A2_corr_total, M1_corr_total;
    } S;

    static pthr_mutex_t S_lock;

    #define STATS_UPDATE(what) \
    do { \
        pthr_mutex_lock(&S_lock); \
        what; \
        pthr_mutex_unlock(&S_lock); \
    } while(0);
#else
    #define STATS_UPDATE(what) nop()
#endif // DEBUG

static uint32_t T = 0x9AF;      // training sequence

static bsequence A_bs, M1[M_SHIFT_CNT], M2[M_SHIFT_CNT];

/**********************************
 * Forward declarations
 **********************************/

typedef struct costas *costas;
typedef struct deinterleaver *deinterleaver;
typedef struct descrambler *descrambler;
struct hfdl_channel;

static void *hfdl_decoder_thread(void *ctx);
static int32_t match_sequence(bsequence *templates, size_t template_cnt, bsequence bits, float *result_corr);
static void compute_train_bit_error_cnt(struct hfdl_channel *c);
static void decode_user_data(struct hfdl_channel *c);
static void dispatch_pdu(struct hfdl_channel *c, uint8_t *buf, size_t len);
static void sampler_reset(struct hfdl_channel *c);
static void framer_reset(struct hfdl_channel *c);

struct hfdl_channel {
	struct block block;
	fft_channelizer channelizer;
	msresamp_crcf resampler;
	agc_crcf agc;
	costas loop;
	firfilt_crcf mf;
	eqlms_cccf eq;
	modem m[MODULATION_CNT];
	symsync_crcf ss;
	bsequence bits;
	bsequence user_data;
	cbuffercf training_symbols;
	cbuffercf data_symbols;
	cbuffercf current_buffer;
	descrambler descrambler;
	deinterleaver deinterleaver[M_SHIFT_CNT];
	void *viterbi_ctx[M_SHIFT_CNT];
	uint64_t symbol_cnt, sample_cnt;
	float resamp_rate;
	sampler_state s_state;
	framer_state fr_state;
	mod_arity data_mod_arity;
	mod_arity current_mod_arity;
	int32_t chan_freq;
	int32_t resampler_delay;
	int32_t symbols_wanted;
	int32_t search_retries;
	int32_t eq_train_seq_cnt;
	int32_t data_segment_cnt;
	int32_t train_bits_total;
	int32_t train_bits_bad;
	int32_t T_idx;
	int32_t M1;
	uint32_t bitmask;
	uint32_t symsync_out_idx;
	// PDU metadata
	struct timeval pdu_timestamp;
	float freq_err_hz;
	float signal_level;
	float noise_floor;
};

/**********************************
 * Costas loop
 **********************************/

struct costas {
	float alpha, beta, phi, dphi, err;
};

static costas costas_cccf_create() {
	NEW(struct costas, c);
	c->alpha = 0.1f;
	c->beta = 0.047f * c->alpha * c->alpha;
	return c;
}

static void costas_cccf_destroy(costas c) {
	XFREE(c);
}

static void costas_cccf_execute(costas c, float complex in, float complex *out) {
	*out = in * cexpf(-I * c->phi);
}

static inline float branchless_limit(float x, float limit) {
	float x1 = fabsf(x + limit);
	float x2 = fabsf(x - limit);
	x1 -= x2;
	return 0.5 * x1;
}

static void costas_cccf_adjust(costas c, float err) {
	c->err = err;
	c->err = branchless_limit(c->err, 1.0f);
	c->phi += c->alpha * c->err;
	c->dphi += c->beta * c->err;
}

static void costas_cccf_step(costas c) {
	c->phi += c->dphi;
	if(c->phi > M_PI) {
		c->phi -= 2.0 * M_PI;
	} else if(c->phi < -M_PI) {
		c->phi += 2.0 * M_PI;
	}
}

static void costas_cccf_reset(costas c) {
	c->dphi = c->phi = 0.f;
}

/**********************************
 * Descrambler
 **********************************/

#define LFSR_LEN 15
#define LFSR_GENPOLY 0x8003u
#define LFSR_INIT 0x6959u
#define DESCRAMBLER_LEN 120

struct descrambler {
	msequence ms;
	uint32_t len;
	uint32_t pos;
};

static descrambler descrambler_create(uint32_t numbits, uint32_t genpoly, uint32_t init, uint32_t seq_len) {
	NEW(struct descrambler, d);
	d->ms = msequence_create(numbits, genpoly, init);
	d->len = seq_len;
	d->pos = 0;
	return d;
}

static void descrambler_destroy(descrambler d) {
	if(d != NULL) {
		msequence_destroy(d->ms);
		XFREE(d);
	}
}

static uint32_t descrambler_advance(descrambler d) {
	ASSERT(d);
	if(d->pos == d->len) {
		d->pos = 0;
		msequence_reset(d->ms);
	}
	d->pos++;
	return msequence_advance(d->ms);
}

/**********************************
 * Deinterleaver
 **********************************/

struct deinterleaver {
	uint8_t **table;
	int32_t row, col;
	int32_t column_cnt;
	int32_t push_column_shift;
};

#define DEINTERLEAVER_ROW_CNT 40
#define DEINTERLEAVER_POP_ROW_SHIFT 9

static deinterleaver deinterleaver_create(int32_t M1) {
	deinterleaver d = XCALLOC(DEINTERLEAVER_ROW_CNT, sizeof(struct deinterleaver));
	d->column_cnt = hfdl_frame_params[M1].data_segment_cnt * DATA_FRAME_LEN
		* hfdl_frame_params[M1].scheme / DEINTERLEAVER_ROW_CNT;
	d->table = XCALLOC(DEINTERLEAVER_ROW_CNT, sizeof(uint8_t *));
	for(int32_t i = 0; i < DEINTERLEAVER_ROW_CNT; i++) {
		d->table[i] = XCALLOC(d->column_cnt, sizeof(uint8_t));
	}
	d->push_column_shift = hfdl_frame_params[M1].deinterleaver_push_column_shift;
	debug_print(D_FRAME, "M1: %d column_cnt: %d total_size: %d column_shift: %d\n",
			M1, d->column_cnt, d->column_cnt * DEINTERLEAVER_ROW_CNT, d->push_column_shift);
	return d;
}

static void deinterleaver_destroy(deinterleaver d) {
	if(d) {
		for(int32_t i = 0; i < DEINTERLEAVER_ROW_CNT; i++) {
			XFREE(d->table[i]);
		}
		XFREE(d->table);
		XFREE(d);
	}
}

static void deinterleaver_push(deinterleaver d, uint8_t val) {
	debug_print(D_FRAME_DETAIL, "push:%d:%d:%hhu\n", d->row, d->col, val);
	d->table[d->row][d->col] = val;
	d->row++;
	if(d->row == DEINTERLEAVER_ROW_CNT) {
		d->row = 0;
		d->col++;
	}
	d->col -= d->push_column_shift;
	if(d->col < 0) {
		d->col += d->column_cnt;
	}
}

static uint8_t deinterleaver_pop(deinterleaver d) {
	uint8_t ret = d->table[d->row][d->col];
	debug_print(D_FRAME_DETAIL, "pop:%d:%d:%hhu\n", d->row, d->col, ret);
	d->row = (d->row + DEINTERLEAVER_POP_ROW_SHIFT) % DEINTERLEAVER_ROW_CNT;
	if(d->row == 0) {
		d->col++;
	}
	return ret;
}

static void deinterleaver_reset(deinterleaver d) {
	d->row = d->col = 0;
}

/**********************************
 * HFDL public routines
 **********************************/

void hfdl_init_globals(void) {
	uint8_t A_octets[] = {
		0b01011011,
		0b10111100,
		0b01110100,
		0b01010111,
		0b00000011,
		0b11011001,
		0b10001001,
		0b00111001,
		0b11110010,
		0b00001000,
		0b11010101,
		0b00110110,
		0b10010100,
		0b00101100,
		0b00110010,
		0b11111110
	};
	A_bs = bsequence_create(A_LEN);
	bsequence_init(A_bs, A_octets);

	uint32_t M1_bits[M1_LEN] = {
		0,1,1,1,0,1,1,0,1,1,1,1,0,1,0,0,0,1,0,1,1,0,0,
		1,0,1,1,1,1,1,0,0,0,1,0,0,0,0,0,0,1,1,0,0,1,1,0,1,1,
		0,0,0,1,1,1,0,0,1,1,1,0,1,0,1,1,1,0,0,0,0,1,0,0,1,1,
		0,0,0,0,0,1,0,1,0,1,0,1,1,0,1,0,0,1,0,0,1,0,1,0,0,1,
		1,1,1,0,0,1,0,0,0,1,1,0,1,0,1,0,0,0,0,1,1,1,1,1,1,1
	};

	size_t M_shifts[M_SHIFT_CNT] = { 72, 82, 113, 123, 61, 103, 93, 9 };
	for(int32_t shift = 0; shift < M_SHIFT_CNT; shift++) {
		M1[shift] = bsequence_create(M1_LEN);
		M2[shift] = bsequence_create(M2_LEN);
		for(int32_t j = 0; j < M1_LEN; j++) {
			bsequence_push(M1[shift], M1_bits[(M_shifts[shift]+j) % M1_LEN]);
		}
		for(int32_t j = 0; j < M2_LEN; j++) {
			bsequence_push(M2[shift], M1_bits[(M_shifts[shift]+j) % M1_LEN]);
		}
	}
	// symsync uses interpolator internally, so it needs MF filter taps
	// multipled by SPS
	// XXX: doesn't work correctly for a PFB with 16 filters - gain still 2x too low
	for(int32_t i = 0; i < HFDL_MF_TAPS_CNT; i++) {
		hfdl_matched_filter_interp[i] = hfdl_matched_filter[i] * SPS;
	}
	
#ifdef DEBUG
	pthr_mutex_init("S_lock", &S_lock, NULL);
#endif
}

struct block *hfdl_channel_create(int32_t sample_rate, int32_t pre_decimation_rate,
		float transition_bw, int32_t centerfreq, int32_t frequency) {
	NEW(struct hfdl_channel, c);
	c->resamp_rate = (float)(HFDL_SYMBOL_RATE * SPS) / ((float)sample_rate / (float)pre_decimation_rate);
	c->resampler = msresamp_crcf_create(c->resamp_rate, 60.0f);
	c->resampler_delay = (int32_t)ceilf(msresamp_crcf_get_delay(c->resampler));
    debug_print("hfdl_channel_create: re_srate=%f srate=%d pre_decim_rate=%d cf=%d f=%d\n",
        c->resamp_rate, sample_rate, pre_decimation_rate, centerfreq, frequency);

	c->chan_freq = frequency;
	float freq_shift = (float)(centerfreq - (frequency + HFDL_SSB_CARRIER_OFFSET_HZ)) / (float)sample_rate;
	debug_print(D_DSP, "create: centerfreq=%d frequency=%d freq_shift=%f\n",
			centerfreq, frequency, freq_shift);

	c->channelizer = fft_channelizer_create(pre_decimation_rate, transition_bw, freq_shift);
    debug_print("fft_channelizer_create: transition_bw=%f freq_shift: (%d - (%d - %d)) / %d = %d/%d = %f\n",
        transition_bw, centerfreq, frequency, HFDL_SSB_CARRIER_OFFSET_HZ, sample_rate,
    centerfreq - (frequency + HFDL_SSB_CARRIER_OFFSET_HZ), sample_rate, freq_shift);
	if(c->channelizer == NULL) {
		goto fail;
	}

	c->agc = agc_crcf_create();
	agc_crcf_set_bandwidth(c->agc, 0.005f);
	agc_crcf_set_bandwidth(c->agc, 0.01f);
	// Set the initial noise estimate to a very high value for faster convergence
	// (which is designed to be faster in downwards direction than upwards)
	c->noise_floor = 1.0f;

	c->loop = costas_cccf_create();

	c->mf = firfilt_crcf_create(hfdl_matched_filter, HFDL_MF_TAPS_CNT);
	c->eq = eqlms_cccf_create_lowpass(EQ_LEN, 0.45f);
	eqlms_cccf_set_bw(c->eq, 0.1f);

	c->m[M_BPSK] = modem_create(LIQUID_MODEM_BPSK);
	c->m[M_PSK4] = modem_create(LIQUID_MODEM_PSK4);
	c->m[M_PSK8] = modem_create(LIQUID_MODEM_PSK8);

	//c->ss = symsync_crcf_create(SPS, SYMSYNC_PFB_CNT, hfdl_matched_filter_interp, HFDL_MF_TAPS_CNT);
	c->ss = symsync_crcf_create_kaiser(SPS, HFDL_MF_SYMBOL_DELAY, 0.9f, SYMSYNC_PFB_CNT);
	symsync_crcf_set_lf_bw(c->ss, 0.001f);
	symsync_crcf_set_output_rate(c->ss, 2);

	c->bits = bsequence_create(M1_LEN);
	c->training_symbols = cbuffercf_create(T_LEN);
	c->data_symbols = cbuffercf_create(DATA_SYMBOLS_CNT_MAX);
	c->descrambler = descrambler_create(LFSR_LEN, LFSR_GENPOLY, LFSR_INIT, DESCRAMBLER_LEN);
	for(int32_t i = 0; i < M_SHIFT_CNT; i++) {
		c->deinterleaver[i] = deinterleaver_create(i);
		struct hfdl_params const p = hfdl_frame_params[i];
		int32_t user_data_bits_cnt = p.data_segment_cnt * DATA_FRAME_LEN * p.scheme / p.code_rate;
		debug_print(D_DSP, "user_data_bits_cnt[%d]: %d\n", i, user_data_bits_cnt);
		c->viterbi_ctx[i] = create_viterbi27(user_data_bits_cnt);
	}

	c->user_data = bsequence_create(DATA_SYMBOLS_CNT_MAX * MOD_ARITY_MAX);

	framer_reset(c);

	struct producer producer = { .type = PRODUCER_NONE };
	struct consumer consumer = { .type = CONSUMER_MULTI, .min_ru = 0 };
	c->block.producer = producer;
	c->block.consumer = consumer;
	c->block.thread_routine = hfdl_decoder_thread;

	return &c->block;
fail:
	XFREE(c);
	return NULL;

}

void hfdl_channel_destroy(struct block *channel_block) {
	if(channel_block == NULL) {
		return;
	}
	struct hfdl_channel *c = container_of(channel_block, struct hfdl_channel, block);
	msresamp_crcf_destroy(c->resampler);
	fft_channelizer_destroy(c->channelizer);
	agc_crcf_destroy(c->agc);
	costas_cccf_destroy(c->loop);
	firfilt_crcf_destroy(c->mf);
	eqlms_cccf_destroy(c->eq);
	modem_destroy(c->m[M_BPSK]);
	modem_destroy(c->m[M_PSK4]);
	modem_destroy(c->m[M_PSK8]);
	symsync_crcf_destroy(c->ss);
	bsequence_destroy(c->bits);
	cbuffercf_destroy(c->training_symbols);
	cbuffercf_destroy(c->data_symbols);
	descrambler_destroy(c->descrambler);
	for(int32_t i = 0; i < M_SHIFT_CNT; i++) {
		deinterleaver_destroy(c->deinterleaver[i]);
		delete_viterbi27(c->viterbi_ctx[i]);
	}
	bsequence_destroy(c->user_data);
	XFREE(c);

#ifdef DEBUG
	pthr_mutex_destroy(&S_lock);
#endif
}

void hfdl_print_summary(void) {
#ifdef DEBUG
	fprintf(stderr, "A1_found:\t\t%d\nA2_found:\t\t%d\nM1_found:\t\t%d\n",
			S.A1_found, S.A2_found, S.M1_found);
	fprintf(stderr, "A1_corr_avg:\t\t%4.3f\n", S.A1_found > 0 ? S.A1_corr_total / (float)S.A1_found : 0);
	fprintf(stderr, "A2_corr_avg:\t\t%4.3f\n", S.A2_found > 0 ? S.A2_corr_total / (float)S.A2_found : 0);
	fprintf(stderr, "M1_corr_avg:\t\t%4.3f\n", S.M1_found > 0 ? S.M1_corr_total / (float)S.M1_found : 0);
	fprintf(stderr, "train_bits_bad/total:\t%d/%d (%f%%)\n", S.train_bits_bad, S.train_bits_total,
			(float)S.train_bits_bad / (float)S.train_bits_total * 100.f);
#endif
}

/**********************************
 * HFDL private routines
 **********************************/

#define chan_debug(fmt, ...) { \
	debug_print(D_DSP, "%d: " fmt, c->chan_freq / 1000, ##__VA_ARGS__); \
}
#define LEVEL_TO_DB(level) (20.0f * log10f(level))

static void *hfdl_decoder_thread(void *ctx) {
	ASSERT(ctx != NULL);
	struct block *block = ctx;
	struct hfdl_channel *c = container_of(block, struct hfdl_channel, block);

	// FIXME: post_input_size / post_decimation_rate ?
	float complex *channelizer_output = XCALLOC(c->channelizer->ddc->post_input_size, sizeof(float complex));
	size_t resampled_size = (c->channelizer->ddc->post_input_size + c->resampler_delay + 10) * c->resamp_rate;
	float complex *resampled = XCALLOC(resampled_size, sizeof(float complex));
	uint32_t resampled_cnt = 0;
	uint32_t noise_floor_sampling_clk = 0;
	float complex r, s;
	float frame_symbol_cnt = 0.0f;      // float because it's used only in float calculations
	float complex symbols[3];
	uint32_t symbols_produced = 0;
	uint32_t bits = 0;
	int32_t M1_match = -1;
	float corr_A1 = 0.f;
	float corr_A2 = 0.f;
	float corr_M1 = 0.f;
	static size_t const max_symbols_without_frame = 13 * SINGLE_SLOT_FRAME_LEN;
	c->s_state = SAMPLER_EMIT_BITS;
	c->fr_state = FRAMER_A1_SEARCH;
#ifdef COSTAS_DEBUG
	dumpfile_rf32 f_costas_dphi = dumpfile_rf32_open("f_costas_dphi.rf32", NAN);
	dumpfile_rf32 f_costas_err = dumpfile_rf32_open("f_costas_err.rf32", NAN);
	dumpfile_cf32 f_costas_out = dumpfile_cf32_open("f_costas_out.cf32", NAN);
#endif
#ifdef SYMSYNC_DEBUG
	dumpfile_cf32 f_symsync_out = dumpfile_cf32_open("f_symsync_out.cf32", NAN);
#endif
#ifdef CHAN_DEBUG
	dumpfile_cf32 f_chan_out = dumpfile_cf32_open("f_chan_out.cf32", NAN);
#endif
#ifdef MF_DEBUG
	dumpfile_cf32 f_mf_out = dumpfile_cf32_open("f_mf_out.cf32", NAN);
#endif
#ifdef AGC_DEBUG
	dumpfile_cf32 f_agc_out = dumpfile_cf32_open("f_agc_out.cf32", NAN);
	dumpfile_rf32 f_agc_gain = dumpfile_rf32_open("f_agc_gain.rf32", NAN);
	dumpfile_rf32 f_agc_rssi = dumpfile_rf32_open("f_agc_rssi.rf32", NAN);
	dumpfile_rf32 f_noise_floor = dumpfile_rf32_open("f_noise_floor.rf32", NAN);
	dumpfile_rf32 f_sig_level = dumpfile_rf32_open("f_sig_level.rf32", NAN);
	float gain, rssi;
#endif
#ifdef EQ_DEBUG
	dumpfile_cf32 f_eq_out = dumpfile_cf32_open("f_eq_out.cf32", NAN);
#endif
#ifdef CORR_DEBUG
	dumpfile_rf32 f_corr_A1 = dumpfile_rf32_open("f_corr_A1.rf32", 0.f);
	dumpfile_rf32 f_corr_A2 = dumpfile_rf32_open("f_corr_A2.rf32", 0.f);
#endif
#ifdef DUMP_CONST
	uint64_t frame_id;
	FILE *consts;
	if(hfdl->Config.datadumps == true) {
		consts = fopen("const.m", "w");
		ASSERT(consts);
	}
#endif
#ifdef DUMP_FFT
	dumpfile_cf32 f_fft_out = dumpfile_cf32_open("f_fft_out.cf32");
#endif
	struct shared_buffer *input = &block->consumer.in->shared_buffer;
	static struct timeval ts_correction = {
		.tv_sec = 0,
		.tv_usec = (PREKEY_LEN + 2 * A_LEN) * 1000000UL / HFDL_SYMBOL_RATE
	};

	while(true) {
		pthr_barrier_wait(input->consumers_ready);
		pthr_barrier_wait(input->data_ready);
		if(block_connection_is_shutdown_signaled(block->consumer.in)) {
			debug_print(D_MISC, "channel %d: Exiting (ordered shutdown)\n", c->chan_freq);
			break;
		}
#ifdef DUMP_FFT
		// XXX: Does not work now due to missing sample clock
		//dumpfile_cf32_write_block(f_fft_out, input->buf, c->channelizer->ddc->fft_size);
#endif
		// FIXME: pass c->channelizer pointer to this function
		c->channelizer->shift_status = fastddc_inv_cc(input->buf, channelizer_output, c->channelizer->ddc,
				c->channelizer->inv_plan, c->channelizer->filtertaps_fft, c->channelizer->shift_status);
		msresamp_crcf_execute(c->resampler, channelizer_output, c->channelizer->shift_status.output_size,
				resampled, &resampled_cnt);
		if(resampled_cnt < 1) {
			debug_print(D_DSP, "ERROR: resampled_cnt is 0\n");
			continue;
		}
#ifdef CHAN_DEBUG
		dumpfile_cf32_write_block(f_chan_out, c->sample_cnt, resampled, resampled_cnt);
#endif
		for(size_t k = 0; k < resampled_cnt; k++, c->sample_cnt++) {
			agc_crcf_execute(c->agc, resampled[k], &r);
#ifdef AGC_DEBUG
			gain = agc_crcf_get_gain(c->agc);
			rssi = agc_crcf_get_rssi(c->agc);
			dumpfile_rf32_write_value(f_agc_gain, c->sample_cnt, gain);
			dumpfile_rf32_write_value(f_agc_rssi, c->sample_cnt, rssi);
			dumpfile_cf32_write_value(f_agc_out, c->sample_cnt, r);
#endif
			firfilt_crcf_push(c->mf, r);
			firfilt_crcf_execute(c->mf, &s);
#ifdef MF_DEBUG
			dumpfile_cf32_write_value(f_mf_out, c->sample_cnt, s);
#endif
			// update noise floor estimate - every 255 samples, only when we aren't inside a frame
			if(c->fr_state == FRAMER_A1_SEARCH && (++noise_floor_sampling_clk & 0xFFu) == 0xFFu) {
				c->noise_floor = 0.65f * c->noise_floor +
					0.35f * fminf(c->noise_floor, agc_crcf_get_signal_level(c->agc)) + 1e-6f;
            //printf("%g %g\n", c->noise_floor, agc_crcf_get_signal_level(c->agc));
#ifdef AGC_DEBUG
				dumpfile_rf32_write_value(f_noise_floor, c->sample_cnt, c->noise_floor);
#endif
			}
			symsync_crcf_execute(c->ss, &s, 1, symbols, &symbols_produced);
			for(size_t i = 0; i < symbols_produced; i++, c->symsync_out_idx++) {
				costas_cccf_step(c->loop);
				costas_cccf_execute(c->loop, symbols[i], &r);
				if(UNLIKELY(fabsf(c->loop->dphi) > 0.25f && c->fr_state == FRAMER_A1_SEARCH)) {
					chan_debug("costas_dphi: %f, resetting control loops\n", c->loop->dphi);
					costas_cccf_reset(c->loop);
					symsync_crcf_reset(c->ss);
				}

				eqlms_cccf_push(c->eq, r);
				if(!(c->symsync_out_idx & 1)) {
					continue;
				}
#ifdef SYMSYNC_DEBUG
				dumpfile_cf32_write_value(f_symsync_out, c->sample_cnt, symbols[i]);
#endif
#ifdef COSTAS_DEBUG
				dumpfile_rf32_write_value(f_costas_dphi, c->sample_cnt, c->loop->dphi);
				dumpfile_rf32_write_value(f_costas_err, c->sample_cnt, c->loop->err);
				dumpfile_cf32_write_value(f_costas_out, c->sample_cnt, r);
#endif
				eqlms_cccf_execute(c->eq, &s);
				if(c->fr_state == FRAMER_EQ_TRAIN) {
					eqlms_cccf_step(c->eq, T_seq[c->bitmask & 1][c->T_idx], s);
					c->T_idx++;
				}
#ifdef EQ_DEBUG
				dumpfile_cf32_write_value(f_eq_out, c->sample_cnt, s);
#endif
				modem_demodulate(c->m[c->current_mod_arity], s, &bits);
				costas_cccf_adjust(c->loop, modem_get_demodulator_phase_error(c->m[c->current_mod_arity]));
#ifdef DUMP_CONST
				if(c->fr_state >= FRAMER_EQ_TRAIN && hfdl->Config.datadumps == true) {
					fprintf(consts, "frame%lu(end+1,1)=%f+%f*i;\n", frame_id,
							crealf(s), cimagf(s));
				}
#endif
				c->symbol_cnt++;
				if(UNLIKELY(c->symbol_cnt >= max_symbols_without_frame && c->fr_state == FRAMER_A1_SEARCH)) {
					chan_debug("Too long without a good frame (%" PRIu64 " symbols), resetting control loops\n",
							c->symbol_cnt);
					c->symbol_cnt = 0;
					costas_cccf_reset(c->loop);
					symsync_crcf_reset(c->ss);
				}

				if(c->s_state == SAMPLER_EMIT_BITS) {
					bits ^= c->bitmask;
					for(uint32_t b = 0; b < c->current_mod_arity; b++, bits >>= 1) {
						bsequence_push(c->bits, bits);
					}
				} else if(c->s_state == SAMPLER_EMIT_SYMBOLS) {
					ASSERT(cbuffercf_space_available(c->current_buffer) != 0);
					cbuffercf_push(c->current_buffer, s);
				} else {    // SKIP
							// NOOP
				}
				// Update signal level estimate - only when inside a frame
				if(c->fr_state > FRAMER_A1_SEARCH) {
					// Approximate averaging
					c->signal_level = (c->signal_level * frame_symbol_cnt + agc_crcf_get_signal_level(c->agc)) / (frame_symbol_cnt + 1.0f);
					frame_symbol_cnt += 1.0f;
#ifdef AGC_DEBUG
					dumpfile_rf32_write_value(f_sig_level, c->sample_cnt, c->signal_level);
#endif
				}
				if(c->symbols_wanted > 1) {
					c->symbols_wanted--;
					continue;
				}

				switch(c->fr_state) {
				case FRAMER_A1_SEARCH:
					corr_A1 = 2.0f * (float)bsequence_correlate(A_bs, c->bits) / (float)A_LEN - 1.0f;
#ifdef CORR_DEBUG
					dumpfile_rf32_write_value(f_corr_A1, c->sample_cnt, corr_A1);
#endif
					if(fabsf(corr_A1) > CORR_THRESHOLD_A1) {
						STATS_UPDATE(S.A1_found++);
						STATS_UPDATE(S.A1_corr_total += fabsf(corr_A1));
						c->bitmask = corr_A1 > 0.f ? 0 : ~0;
						c->signal_level = agc_crcf_get_signal_level(c->agc);
						frame_symbol_cnt = 1.0f;
						c->symbols_wanted = A_LEN;
						c->search_retries = 0;
						c->fr_state++;
#ifdef DUMP_CONST
						frame_id = c->sample_cnt;
#endif
					}
					break;
				case FRAMER_A2_SEARCH:
					corr_A2 = 2.0f * (float)bsequence_correlate(A_bs, c->bits) / (float)A_LEN - 1.0f;
#ifdef CORR_DEBUG
					dumpfile_rf32_write_value(f_corr_A2, c->sample_cnt, corr_A2);
#endif
					if(fabsf(corr_A2) > CORR_THRESHOLD_A2) {
						// Save the current timestamp and go back by the length
						// of the prekey and two A sequences, so that the timestamp
						// points at the start of the frame.
						gettimeofday(&c->pdu_timestamp, NULL);
						timersub(&c->pdu_timestamp, &ts_correction, &c->pdu_timestamp);
						chan_debug("A2 sequence found at sample %" PRIu64 " (corr=%f retry=%d costas_dphi=%f)\n",
								c->sample_cnt, corr_A2, c->search_retries, c->loop->dphi);
						c->freq_err_hz = c->loop->dphi * HFDL_SYMBOL_RATE / (2.0 * M_PI);
						STATS_UPDATE(S.A2_found++);
						STATS_UPDATE(S.A2_corr_total += fabsf(corr_A2));
						c->symbols_wanted = M1_LEN;
						c->search_retries = 0;
						c->fr_state = FRAMER_M1_SEARCH;
						statsd_increment_per_channel(c->chan_freq, "demod.preamble.A2_found");
					} else if(++c->search_retries >= MAX_SEARCH_RETRIES) {
						framer_reset(c);
					}
					break;
				case FRAMER_M1_SEARCH:
					M1_match = match_sequence(M1, M_SHIFT_CNT, c->bits, &corr_M1);
					if(fabsf(corr_M1) > CORR_THRESHOLD_M1) {
						chan_debug("M1 match at sample %" PRIu64 ": %d (corr=%f, costas_dphi=%f)\n",
								c->sample_cnt, M1_match, corr_M1, c->loop->dphi);
						statsd_increment_per_channel(c->chan_freq, "demod.preamble.M1_found");
						STATS_UPDATE(S.M1_found++);
						STATS_UPDATE(S.M1_corr_total += fabsf(corr_M1));
						c->data_segment_cnt = hfdl_frame_params[M1_match].data_segment_cnt;
						c->data_mod_arity = hfdl_frame_params[M1_match].scheme;
						c->M1 = M1_match;
						c->symbols_wanted = M2_LEN;
						c->search_retries = 0;
						c->fr_state = FRAMER_M2_SKIP;
						c->s_state = SAMPLER_SKIP;
					} else {
						chan_debug("M1 sequence unreliable (val=%d corr=%f)\n", M1_match, corr_M1);
						statsd_increment_per_channel(c->chan_freq, "demod.preamble.errors.M1_not_found");
						framer_reset(c);
					}
					break;
				case FRAMER_M2_SKIP:
					cbuffercf_reset(c->training_symbols);
					c->symbols_wanted = T_LEN;
					c->eq_train_seq_cnt = 9;
					c->fr_state = FRAMER_EQ_TRAIN;
					c->s_state = SAMPLER_EMIT_SYMBOLS;
#ifdef DUMP_CONST
					if(hfdl->Config.datadumps == true) {
						fprintf(consts, "frame%lu = [];\n", frame_id);
					}
#endif
					break;
				case FRAMER_EQ_TRAIN:
					ASSERT(cbuffercf_size(c->training_symbols) == T_LEN);
					compute_train_bit_error_cnt(c);
					cbuffercf_reset(c->training_symbols);
					if(c->eq_train_seq_cnt > 1) {               // next frame is training sequence
						c->eq_train_seq_cnt--;
						c->symbols_wanted = T_LEN;
						c->T_idx = 0;
					} else if(c->data_segment_cnt > 0) {        // next frame is data frame
						c->symbols_wanted = DATA_FRAME_LEN / 2;
						c->fr_state = FRAMER_DATA_1;
						c->current_mod_arity = c->data_mod_arity;
						c->current_buffer = c->data_symbols;
					} else {                                    // end of frame
						chan_debug("train_bits_bad: %d/%d (%f%%)\n",
								c->train_bits_bad, c->train_bits_total,
								(float)c->train_bits_bad / (float)c->train_bits_total * 100.f);
						decode_user_data(c);
						framer_reset(c);
						c->symbol_cnt = 0;
					}
					break;
				case FRAMER_DATA_1:
					c->symbols_wanted = DATA_FRAME_LEN / 2;
					c->fr_state = FRAMER_DATA_2;
					break;
				case FRAMER_DATA_2:
					c->data_segment_cnt--;
					c->current_mod_arity = M_BPSK;
					c->current_buffer = c->training_symbols;
					c->fr_state = FRAMER_EQ_TRAIN;
					c->eq_train_seq_cnt = 1;
					c->symbols_wanted = T_LEN;
					c->T_idx = 0;
					break;
				}
			}
		}
	}
#ifdef COSTAS_DEBUG
	dumpfile_rf32_destroy(f_costas_dphi);
	dumpfile_rf32_destroy(f_costas_err);
	dumpfile_cf32_destroy(f_costas_out);
#endif
#ifdef SYMSYNC_DEBUG
	dumpfile_cf32_destroy(f_symsync_out);
#endif
#ifdef CHAN_DEBUG
	dumpfile_cf32_destroy(f_chan_out);
#endif
#ifdef MF_DEBUG
	dumpfile_cf32_destroy(f_mf_out);
#endif
#ifdef AGC_DEBUG
	dumpfile_cf32_destroy(f_agc_out);
	dumpfile_rf32_destroy(f_agc_gain);
	dumpfile_rf32_destroy(f_agc_rssi);
	dumpfile_rf32_destroy(f_sig_level);
	dumpfile_rf32_destroy(f_noise_floor);
#endif
#ifdef EQ_DEBUG
	dumpfile_cf32_destroy(f_eq_out);
#endif
#ifdef CORR_DEBUG
	dumpfile_rf32_destroy(f_corr_A1);
	dumpfile_rf32_destroy(f_corr_A2);
#endif
#ifdef DUMP_CONST
	if(hfdl->Config.datadumps == true) {
		fclose(consts);
	}
#endif
#ifdef DUMP_FFT
	dumpfile_cf32_destroy(f_fft_out);
#endif
	XFREE(channelizer_output);
	XFREE(resampled);
	block->running = false;
	return NULL;
}

static int32_t match_sequence(bsequence *templates, size_t template_cnt, bsequence bits, float *result_corr) {
	float max_corr = 0.f;
	int32_t max_idx = -1;
	int32_t seq_len = bsequence_get_length(bits);
	for(size_t idx = 0; idx < template_cnt; idx++) {
		float corr = fabsf(2.0f * (float)bsequence_correlate(templates[idx], bits) / (float)seq_len - 1.0f);
		if(corr > max_corr) {
			max_corr = corr;
			max_idx = idx;
		}
	}
	*result_corr = max_corr;
	return max_idx;
}

static void compute_train_bit_error_cnt(struct hfdl_channel *c) {
	uint32_t T_seq = 0, bit = 0;
	float complex s;
	for(int32_t i = 0; i < T_LEN; i++) {
		cbuffercf_pop(c->training_symbols, &s);
		modem_demodulate(c->m[M_BPSK], s, &bit);
		bit ^= (c->bitmask & 1);
		T_seq = (T_seq << 1) | bit;
	}
	int32_t error_cnt = count_bit_errors(T, T_seq);
	STATS_UPDATE(S.train_bits_total += T_LEN);
	STATS_UPDATE(S.train_bits_bad += error_cnt);
	c->train_bits_total += T_LEN;
	c->train_bits_bad += error_cnt;
}

static void sampler_reset(struct hfdl_channel *c) {
	symsync_crcf_reset(c->ss);
	c->s_state = SAMPLER_EMIT_BITS;
	c->bitmask = 0;
}

static void framer_reset(struct hfdl_channel *c) {
	c->fr_state = FRAMER_A1_SEARCH;
	c->symbols_wanted = 1;
	c->search_retries = 0;
	c->current_mod_arity = M_BPSK;
	c->train_bits_total = c->train_bits_bad = 0;
	c->T_idx = 0;
	c->current_buffer = c->training_symbols;
	agc_crcf_unlock(c->agc);
	eqlms_cccf_reset(c->eq);
	cbuffercf_reset(c->data_symbols);
	cbuffercf_reset(c->training_symbols);
	bsequence_reset(c->user_data);
	for(int32_t i = 0; i < M_SHIFT_CNT; i++) {
		deinterleaver_reset(c->deinterleaver[i]);
	}
	sampler_reset(c);
}

static void decode_user_data(struct hfdl_channel *c) {
#define deinterleaver_table_size(d) (uint32_t)((d)->column_cnt * DEINTERLEAVER_ROW_CNT)
	static float const phase_flip[2] = { [0] = 1.0f, [1] = -1.0f };
	int32_t M1 = c->M1;
	uint32_t num_symbols = hfdl_frame_params[M1].data_segment_cnt * DATA_FRAME_LEN;
	ASSERT(num_symbols == cbuffercf_size(c->data_symbols));
	uint32_t num_encoded_bits = num_symbols * c->data_mod_arity;
	chan_debug("got %d user data symbols, deinterleaver table size: %u bitmask: 0x%x\n", num_symbols,
			deinterleaver_table_size(c->deinterleaver[M1]), c->bitmask);
	ASSERT(num_encoded_bits == deinterleaver_table_size(c->deinterleaver[M1]));
	uint32_t bits = 0;
	uint32_t descrambler_bit = 0;
	float complex symbol;
	uint8_t soft_bits[c->data_mod_arity];
	modem data_modem = c->m[c->data_mod_arity];
	for(uint32_t i = 0; i < num_symbols; i++) {
		cbuffercf_pop(c->data_symbols, &symbol);
		descrambler_bit = descrambler_advance(c->descrambler);
		// Flip symbol phase by M_PI when descrambler outputs 1
		// Flip symbol phase by M_PI when Costas loop synced in an opposite phase
		modem_demodulate_soft(data_modem, symbol * phase_flip[descrambler_bit] * phase_flip[c->bitmask & 1],
				&bits, soft_bits);
		for(uint32_t j = 0; j < c->data_mod_arity; j++) {
			deinterleaver_push(c->deinterleaver[M1], soft_bits[j]);
		}
	}
#define CONV_CODE_RATE 2
	uint32_t viterbi_input_len = num_encoded_bits;
	// When FEC rate is 1/4, every chip is transmitted twice, so we take mean value of them
	if(hfdl_frame_params[M1].code_rate == 4) {
		viterbi_input_len /= 2;
	}
	uint8_t viterbi_input[viterbi_input_len];
	if(hfdl_frame_params[M1].code_rate == 4) {
		uint8_t a, b;
		for(uint32_t i = 0; i < viterbi_input_len; i++) {
			a = deinterleaver_pop(c->deinterleaver[M1]);
			b = deinterleaver_pop(c->deinterleaver[M1]);
			// Average without overflow (http://aggregate.org/MAGIC/#Average%20of%20Integers)
			viterbi_input[i] = (a & b) + ((a ^ b) >> 1);
		}
	} else {    // code_rate == 2
		for(uint32_t i = 0; i < viterbi_input_len; i++) {
			viterbi_input[i] = deinterleaver_pop(c->deinterleaver[M1]);
		}
	}
	debug_print_buf_hex(D_FRAME_DETAIL, viterbi_input, viterbi_input_len, "viterbi_input:\n");

	void *v = c->viterbi_ctx[M1];
	uint32_t viterbi_output_len = viterbi_input_len / CONV_CODE_RATE;
	uint32_t viterbi_output_len_octets = viterbi_output_len / 8 + (viterbi_output_len % 8 != 0 ? 1 : 0);
	uint8_t viterbi_output[viterbi_output_len_octets];
	init_viterbi27(v, 0);
	update_viterbi27_blk(v, viterbi_input, viterbi_output_len);
	chainback_viterbi27(v, viterbi_output, viterbi_output_len, 0);
	debug_print(D_FRAME, "code_rate: 1/%d num_encoded_bits: %u viterbi_input_len: %u viterbi_output_len: %u, viterbi_output_len_octets: %u\n",
			hfdl_frame_params[M1].code_rate, num_encoded_bits, viterbi_input_len, viterbi_output_len, viterbi_output_len_octets);
	debug_print_buf_hex(D_FRAME_DETAIL, viterbi_output, viterbi_output_len_octets, "viterbi_output:\n");
	for(uint32_t i = 0; i < viterbi_output_len_octets; i++) {
		viterbi_output[i] = REVERSE_BYTE(viterbi_output[i]);
	}
	debug_print_buf_hex(D_FRAME_DETAIL, viterbi_output, viterbi_output_len_octets, "viterbi_output (reversed):\n");
	dispatch_pdu(c, viterbi_output, viterbi_output_len_octets);
}

static void dispatch_pdu(struct hfdl_channel *c, uint8_t *buf, size_t len) {
	struct metadata *m = hfdl_pdu_metadata_create();
	struct hfdl_pdu_metadata *hm = container_of(m, struct hfdl_pdu_metadata, metadata);
	hm->version = 1;
	hm->freq = c->chan_freq;
	hm->freq_err_hz = c->freq_err_hz;
	hm->rssi = LEVEL_TO_DB(c->signal_level);
	hm->noise_floor = LEVEL_TO_DB(c->noise_floor);
    //printf("SIG=%g(%.1f) NF=%g(%.1f)\n", c->signal_level, hm->rssi, c->noise_floor, hm->noise_floor);
	m->rx_timestamp.tv_sec = c->pdu_timestamp.tv_sec;
	m->rx_timestamp.tv_usec = c->pdu_timestamp.tv_usec;

	ASSERT(c->M1 >= 0);
	ASSERT(c->M1 < M_SHIFT_CNT);
	struct hfdl_params const *p = &hfdl_frame_params[c->M1];
	hm->bit_rate = HFDL_SYMBOL_RATE * p->scheme / p->code_rate *
		DATA_FRAME_LEN / (DATA_FRAME_LEN + T_LEN);
	hm->slot = p->data_segment_cnt == DATA_FRAME_CNT_SINGLE_SLOT ? 'S' : 'D';

	uint32_t flags = 0;
	uint8_t *copy = XCALLOC(len, sizeof(uint8_t));
	memcpy(copy, buf, len);
	pdu_decoder_queue_push(m, octet_string_new(copy, len), flags);
}
