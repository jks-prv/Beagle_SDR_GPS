/************************************************************************************
 **                                                                                 **
 **                               UHSDR Firmware Project                            **
 **                                                                                 **
 **---------------------------------------------------------------------------------**
 **                                                                                 **
 **  Licence:		GNU GPLv3                                                       **
 ************************************************************************************/
//*********************************************************************************
//**
//** Project.........: Read Hand Sent Morse Code (tolerant of considerable jitter)
//**
//** Copyright (c) 2016  Loftur E. Jonasson  (tf3lj [at] arrl [dot] net)
//**
//** This program is free software: you can redistribute it and/or modify
//** it under the terms of the GNU General Public License as published by
//** the Free Software Foundation, either version 3 of the License, or
//** (at your option) any later version.
//**
//** This program is distributed in the hope that it will be useful,
//** but WITHOUT ANY WARRANTY; without even the implied warranty of
//** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//** GNU General Public License for more details.
//**
//** The GNU General Public License is available at
//** http://www.gnu.org/licenses/
//**
//** Substantive portions of the methodology used here to decode Morse Code are found in:
//**
//** "MACHINE RECOGNITION OF HAND-SENT MORSE CODE USING THE PDP-12 COMPUTER"
//** by Joel Arthur Guenther, Air Force Institute of Technology,
//** Wright-Patterson Air Force Base, Ohio
//** December 1973
//** http://www.dtic.mil/dtic/tr/fulltext/u2/786492.pdf
//**
//** Platform........: Teensy 3.1 / 3.2 and the Teensy Audio Shield
//**
//** Initial version.: 0.00, 2016-01-25  Loftur Jonasson, TF3LJ / VE2LJX
//**
//*********************************************************************************

#include "config.h"
#include "coroutines.h"
#include "misc.h"
#include "mem.h"
#include "rx_util.h"
#include "uhsdr_cw_decoder.h"

#include <limits.h>

//#define SIGNAL_TAU			0.01
#define SIGNAL_TAU			    0.1
#define	ONEM_SIGNAL_TAU         (1.0 - SIGNAL_TAU)
#define AUTO_WEIGHT_LINEAR      32000   // ~45 dB
#define AUTO_THRESHOLD_LINEAR   15849   // ~42 dB

#define TRAINING_STABLE     32
#define CW_TIMEOUT			3  // Time, in seconds, to trigger display of last Character received
#define ERROR_TIMEOUT       8
#define ONE_SECOND			(snd_rate / cw->blocksize) // sample rate / decimation rate / block size

#define CW_ONE_BIT_SAMPLE_COUNT (ONE_SECOND / 58.3) // = 6.4 works ! standard word PARIS has 14 pulses & 14 spaces, assumed: 25WPM

#define CW_SPIKECANCEL_MAX_DURATION        8  // Cancel transients/spikes/drops that have max duration of number chosen.
// Typically 4 or 8 to select at time periods of 4 or 8 times 2.9ms.
// 0 to deselect.

#define CW_SIG_BUFSIZE      256  // Size of a circular buffer of decoded input levels and durations
#define CW_DATA_BUFSIZE      40  // Size of a buffer of accumulated dot/dash information. Max is DATA_BUFSIZE-2
// Needs to be significantly longer than longest symbol 'sos'= ~30.

typedef struct
{
	int a;
	float32_t b;
	float32_t sin;
	float32_t cos;
	float32_t r;
	float32_t b0, b1, b2;
} Goertzel;

typedef struct {
	unsigned state :1; // Pulse or space (sample buffer) OR Dot or Dash (data buffer)
	unsigned time :31; // Time duration
} sigbuf_t;

typedef struct
{
	unsigned initialized :1;// Do we have valid time duration measurements?
	unsigned dash :1;       // Dash flag
	unsigned wspace :1;     // Word Space flag
	unsigned timeout :1;    // Timeout flag
	unsigned overload :1;   // Overload flag
	unsigned track :1;      // Track timing changes continuously
} bflags_t;

typedef struct
{
	float32_t pulse_avg; // CW timing variables - pulse_avg is a composite value
	float32_t dot_avg;
	float32_t dash_avg;            // Dot and Dash Space averages
	float32_t symspace_avg;
	float32_t cwspace_avg; // Intra symbol Space and Character-Word Space
	int32_t w_space;                      // Last word space time
} cw_times_t;

typedef struct {
    int rx_chan;
    bool process_samples;
    u4_t wpm_fixed;
    u4_t wpm_update;
    int wsc;
    int err_cnt, err_timeout;
    
	float32_t sampling_freq;
	float32_t target_freq;
	uint8_t speed;
	bool isAutoThreshold;
	uint32_t weight_linear;
	uint32_t threshold_linear;
	uint8_t blocksize;

	uint8_t noisecancel_enable;
	uint8_t spikecancel;
#define CW_SPIKECANCEL_MODE_OFF 0
#define CW_SPIKECANCEL_MODE_SPIKE 1
#define CW_SPIKECANCEL_MODE_SHORT 2

    float32_t raw_signal_buffer[CW_DECODER_BLOCKSIZE_MAX];  // audio signal buffer
    Goertzel goertzel;

    sigbuf_t sig[CW_SIG_BUFSIZE]; // A circular buffer of decoded input levels and durations, input from
    int32_t sig_lastrx;         // Circular buffer in pointer, updated by SignalSampler
    int32_t sig_incount;        // Circular buffer in pointer, copy of sig_lastrx, used by CW Decode functions
    int32_t sig_outcount;       // Circular buffer out pointer, used by CW Decode functions
    int32_t sig_timer;          // Elapsed time of current signal state, dependent

    sigbuf_t data[CW_DATA_BUFSIZE]; // Buffer containing decoded dot/dash and time information for assembly into a character
    uint8_t data_len;               // Length of incoming character data
    uint32_t code;                  // Decoded dot/dash info in pairs of bits, - is encoded as 11, and . is encoded as 10

    bool state;     // Current decoded signal state
    bflags_t b;     // Various Operational state flags
    cw_times_t times;

    // on sample rate, decimation factor and CW_DECODE_BLOCK_SIZE
    // 48ksps & decimation-by-4 equals 12ksps
    // if CW_DECODE_BLOCK_SIZE == 32, then we have 12000/32 = 375 blocks per second, which means
    // one Goertzel magnitude is calculated 375 times a second, which means 2.67ms per timer_stepsize
    // this is very similar to the original 2.9ms (when using FFT256 in the Teensy 3 original sketch)
    // DD4WH 2017_09_08

    int32_t timer_stepsize;     // equivalent to 2.67ms, see above
    int32_t cur_time;           // copy of sig_timer
    int32_t cur_outcount;       // Basically same as sig_outcount, for Error Correction functionality
    int32_t last_outcount;      // sig_outcount for previous character, used for Error Correction func
    
    // formerly statics: CW_Decode_exe()
	float32_t CW_env;
	float32_t CW_mag;
	float32_t CW_noise;
	float32_t old_siglevel;
	float32_t speed_wpm_avg;
	bool prevstate; 				// Last recorded state of signal input (mark or space)
    bool noisecancel_change;

    // formerly statics: various places
    uint16_t sample_counter;
    int16_t startpos, progress; // Progress counter, size = SIG_BUFSIZE
    bool initializing;          // Bool for first time init of progress counter
    bool spike;
    bool processed;
    int training_interval;
    
} cw_decoder_t;

static cw_decoder_t cw_decoder[MAX_RX_CHANS];

static void CW_Decode(cw_decoder_t *cw);

#define N_CW_ELEM   8

typedef struct {
    uint8_t c;
    const char elems[N_CW_ELEM + SPACE_FOR_NULL];
    const char *prosign;
    u4_t code;
} cw_code_t;

static cw_code_t cw_code[] = {
    {  0,   ".-.-",     "<aa>" },
    {  1,   ".-.-.",    "<ar>" },
    {  2,   ".-...",    "<as>" },
    {  3,   "-...-.-",  "<bk>" },
    {  4,   "-...-",    "<bt>" },
    {  5,   "-.-..-..", "<cl>" },
    {  6,   "-.-.-",    "<ct>" },
    {  7,   "........", "<hh>" },
    {  8,   "-.--.",    "<kn>" },
    {  9,   "-..---",   "<nj>" },
    { 10,   "...-.-",   "<sk>" },
    { 11,   "...-.",    "<sn>" },
    { 'E',  "." },
    { 'T',  "-" },
    { 'I',  ".." },
    { 'A',  ".-" },
    { 'N',  "-." },
    { 'M',  "--" },
    { 'S',  "..." },
    { 'U',  "..-" },
    { 'R',  ".-." },
    { 'W',  ".--" },
    { 'D',  "-.." },
    { 'K',  "-.-" },
    { 'G',  "--." },
    { 'O',  "---" },
    { 'H',  "...." },
    { 'V',  "...-" },
    { 'F',  "..-." },
    { 'L',  ".-.." },
    { 'P',  ".--." },
    { 'J',  ".---" },
    { 'B',  "-..." },
    { 'X',  "-..-" },
    { 'C',  "-.-." },
    { 'Y',  "-.--" },
    { 'Z',  "--.." },
    { 'Q',  "--.-" },
    { '5',  "....." },
    { '4',  "....-" },
    { '3',  "...--" },
    { '2',  "..---" },
    { '1',  ".----" },
    { '6',  "-...." },
    { '=',  "-...-" },
    { '/',  "-..-." },
    { '7',  "--..." },
    { '8',  "---.." },
    { '9',  "----." },
    { '0',  "-----" },
    { '?',  "..--.." },
    { '"',  ".-..-." }, // '_'
    { '.',  ".-.-.-" },
    { '@',  ".--.-." },
    { '\'', ".----." },
    { '-',  "-....-" },
    { ',',  "--..--" },
    { ':',  "---..." },
};

#define N_CW_CODE   ARRAY_LEN(cw_code)

#define DIT     2
#define DAH     3

static void cw_code_init()
{
    static bool init;
    if (init) return;
    init = true;
    
	for (int i = 0; i < N_CW_CODE; i++) {
	    cw_code_t *c = &cw_code[i];
	    u4_t elems = 0;
	    for (int el = 0; el < N_CW_ELEM; el++) {
	        if (c->elems[el] == '\0') break;
	        elems = (elems << 2) | ((c->elems[el] == '.')? DIT : DAH);
	    }
	    c->code = elems;
	    #if 0
            if (c->c < ' ')
                printf("%s %d\n", c->prosign, c->code);
            else
                printf("%c %d\n", c->c, c->code);
        #endif
	}
}

static uint8_t CwGen_CharacterIdFunc(uint32_t code)
{
	uint8_t out = 0xff; // 0xff selected to indicate ERROR

	// Should never happen - Empty, spike suppression or similar
	if (code == 0) return 0xfe;

	for (int i = 0; i < N_CW_CODE; i++) {
	    cw_code_t *c = &cw_code[i];
		if (code == c->code) {
			out = c->c;
			break;
		}
	}

	return out;
}

static void AudioFilter_CalcGoertzel(Goertzel* g, float32_t freq, const uint32_t size, const float goertzel_coeff, float32_t samplerate)
{
	printf("AudioFilter_CalcGoertzel f=%f sr=%f\n", freq, samplerate);
    g->a = (0.5 + (freq * goertzel_coeff) * size/samplerate);
    g->b = (2*K_PI*g->a)/size;
    g->sin = sinf(g->b);
    g->cos = cosf(g->b);
    g->r = 2.0 * g->cos;
	g->b0 = g->b1 = g->b2 = 0;
}

static void AudioFilter_GoertzelInput(Goertzel* g, float32_t in)
{
	g->b0 = g->r * g->b1 - g->b2 + in;
	g->b2 = g->b1;
	g->b1 = g->b0;
}

static float32_t AudioFilter_GoertzelEnergy(Goertzel* g)
{
    #if 1
        float32_t re = (g->b1 - (g->b2 * g->cos));   // calculate energy at frequency
        float32_t im = (g->b2 * g->sin);
        float32_t mag_sq = re*re + im*im;
    #else
        float32_t mag_sq = (g->b1 * g->b1) + (g->b2 * g->b2) - (g->b1 * g->b2 * g->r);
    #endif
	g->b0 = g->b1 = g->b2 = 0;
	return sqrtf(mag_sq);
}

static float32_t decayavg(float32_t average, float32_t input, int weight)
{ // adapted from https://github.com/ukhas/dl-fldigi/blob/master/src/include/misc.h
	float32_t retval;
	if (weight <= 1)
	{
		retval = input;
	}
	else
	{
		retval = ( ( input - average ) / (float32_t) weight ) + average ;
	}
	return retval;
}

// called by:
//  "SET cw_start="
//  "SET cw_train="
//  "SET cw_wpm=" => CwDecode_wpm()
void CwDecode_Init(int rx_chan, int wpm, int training_interval)
{
    cw_code_init();
    
    cw_decoder_t *cw = &cw_decoder[rx_chan];
    printf("CwDecode_Init wpm=%d %s\n", wpm, wpm? "FIXED":"AUTO");
    memset(cw, 0, sizeof(cw_decoder_t));
    cw->rx_chan = rx_chan;
    cw->wsc = 1;
    cw->old_siglevel = 0.001;
    cw->timer_stepsize = 1;
    cw->sampling_freq = ext_update_get_sample_rateHz(rx_chan);
    cw->isAutoThreshold = false;
    cw->blocksize = CW_DECODER_BLOCKSIZE_DEFAULT;
    cw->noisecancel_enable = 1;
    cw->spikecancel = 0;
    
    if (wpm != 0) {
        cw->speed_wpm_avg = wpm;
	    float32_t blk_per_ms = cw->sampling_freq / 1e3 / CW_DECODER_BLOCKSIZE_DEFAULT;
	    //#define N_ELEM_PARIS 46
	    #define N_ELEM_PARIS 50
	    float32_t ms_per_elem = 60000.0 / ((float32_t) wpm * N_ELEM_PARIS);
		cw->times.dot_avg = blk_per_ms * ms_per_elem;
		cw->times.dash_avg = cw->times.dot_avg * 3;
		cw->times.cwspace_avg = cw->times.dot_avg * 4.2;
		cw->times.symspace_avg = cw->times.dot_avg * 0.93;
        cw->times.pulse_avg = (cw->times.dot_avg / 4 + cw->times.dash_avg) / 2.0;

	    float32_t spdcalc = 10.0 * cw->times.dot_avg + 4.0 * cw->times.dash_avg + 9.0 * cw->times.symspace_avg + 5.0 * cw->times.cwspace_avg;
		float32_t speed_ms_per_word = spdcalc * 1000.0 / (cw->sampling_freq / (float32_t)cw->blocksize);
		float32_t speed_wpm_raw = (0.5 + 60000.0 / speed_ms_per_word); // calculate words per minute
        printf("CW WPM FIXED INIT %.0f|%.1f spdcalc %.3f MSPW %.3f pulse %.1f dot %.1f dash %.1f space %.1f sym %.1f wsc %d threshold %s %d (%.1f dB)\n",
            cw->speed_wpm_avg, speed_wpm_raw, spdcalc, speed_ms_per_word,
            cw->times.pulse_avg, cw->times.dot_avg, cw->times.dash_avg, cw->times.cwspace_avg, cw->times.symspace_avg, cw->wsc,
            cw->isAutoThreshold? "AUTO" : "FIXED", cw->threshold_linear, dB_fast((float) cw->threshold_linear));

	    cw->b.track = FALSE;
		cw->b.initialized = TRUE;   // Indicate we're done and return
		cw->initializing = FALSE;   // Allow for correct setup of progress if cw_train() is invoked a second time
        ext_send_msg(cw->rx_chan, false, "EXT cw_train=0");
    } else {
        cw->training_interval = training_interval;
	    cw->b.track = TRUE;
    }
    
    // CwDecode_pboff() needs to be called before starting to process samples
}

void CwDecode_pboff(int rx_chan, u4_t pboff)
{
    cw_decoder_t *cw = &cw_decoder[rx_chan];

	// set Goertzel parameters for CW decoding
	//printf("CwDecode_pboff %d\n", pboff);
	AudioFilter_CalcGoertzel(&cw->goertzel, pboff, cw->blocksize, 1.0, cw->sampling_freq);
	cw->process_samples = true;
}

#define ring_idx_wrap_upper(value,size) (((value) >= (size)) ? (value) - (size) : (value))
#define ring_idx_wrap_zero(value,size) (((value) < (0)) ? (value) + (size) : (value))

/*
 * @brief adjust index "value" by "change" while keeping it in the ring buffer size limits of "size"
 * @returns the value changed by adding change to it and doing a modulo operation on it for the ring buffer size. So return value is always 0 <= result < size
 */
#define ring_idx_change(value,change,size) \
    (change > 0? \
        ring_idx_wrap_upper((value)+(change), (size)) : \
        ring_idx_wrap_zero((value)+(change), (size)) \
    )

#define ring_idx_increment(value,size) ((value+1) == (size)? 0:(value+1))
#define ring_idx_decrement(value,size) ((value) == 0? (size)-1:(value)-1)

// Determine number of states waiting to be processed
#define ring_distanceFromTo(from,to) (((to) < (from))? ((CW_SIG_BUFSIZE + (to)) - ((from) )) : (to - from))

static void CW_Decode_exe(cw_decoder_t *cw)
{
	bool newstate;
	float32_t CW_clipped = 0.0;
	float32_t siglevel;                	// signal level from Goertzel calculation

	//    1.) get samples
	// these are already in raw_signal_buffer

	//    2.) calculate Goertzel
	for (uint16_t index = 0; index < cw->blocksize; index++)
	{
		AudioFilter_GoertzelInput(&cw->goertzel, cw->raw_signal_buffer[index]);
	}
	
	//    3.) AGC (not used)

	//    4.) signal averaging/smoothing

	float32_t magnitudeSquared = AudioFilter_GoertzelEnergy(&cw->goertzel);
    siglevel = magnitudeSquared;

	//    4b.) automatic threshold correction
	if (cw->isAutoThreshold)
	{
        cw->CW_mag = siglevel;

        cw->CW_env = decayavg(
                        cw->CW_env,     // average
                        cw->CW_mag,     // input
                        (cw->CW_mag > cw->CW_env)?      // weight
                            (cw->weight_linear /1000 / 4)   // (CW_ONE_BIT_SAMPLE_COUNT / 4)
                        :
                            (cw->weight_linear /1000 * 16)  // (CW_ONE_BIT_SAMPLE_COUNT * 16)
                    );
    
        cw->CW_noise = decayavg(
                            cw->CW_noise,   // average
                            cw->CW_mag,     // input
                            (cw->CW_mag < cw->CW_noise)?    // weight
                                (cw->weight_linear /1000 / 4)   // (CW_ONE_BIT_SAMPLE_COUNT / 4)
                            :
                                (cw->weight_linear /1000 * 48)  // (CW_ONE_BIT_SAMPLE_COUNT * 48)
                        );
    
        CW_clipped = CLAMP(cw->CW_env, cw->CW_noise, cw->CW_mag);
    
        //float32_t CW_env_to_noise = cw->CW_env - cw->CW_noise;
        //float32_t v1 = (CW_clipped - cw->CW_noise) * CW_env_to_noise -
        //                0.8 * (CW_env_to_noise * CW_env_to_noise);

        float32_t CW_env_to_noise = CW_clipped - cw->CW_noise;
        float32_t v1 = (CW_clipped - cw->CW_noise) * CW_env_to_noise -
                        0.8 * (CW_env_to_noise * CW_env_to_noise);
        v1 = sqrtf(fabsf(v1)) * ((v1 < 0)? -1:1);
    
        // lowpass
        siglevel = v1 * SIGNAL_TAU + ONEM_SIGNAL_TAU * cw->old_siglevel;
        cw->old_siglevel = v1;
        //newstate = (siglevel >= 0)? true:false;
		newstate = (siglevel >= cw->threshold_linear)? true:false;
	}

	//    4c.) fixed threshold
	else
	{
	    // lowpass
		siglevel = siglevel * SIGNAL_TAU + ONEM_SIGNAL_TAU * cw->old_siglevel;
		cw->old_siglevel = magnitudeSquared;
		newstate = (siglevel >= cw->threshold_linear)? true:false;
	}

	//    5.) signal state determination
	//----------------
	// Signal State sampling

	// noise cancel requires at least two consecutive samples to be
	// of same (changed state) to accept change (i.e. a single sample change is ignored).
	if (cw->noisecancel_enable)
	{
		if (cw->noisecancel_change == TRUE)     // needs to be the same to confirm a true change
		{
			cw->state = newstate;
			cw->noisecancel_change = FALSE;
		}
		else if (newstate != cw->state)
		{
			cw->noisecancel_change = TRUE;
		}

	}
	else
	{   // No noise canceling
		cw->state = newstate;
	}
	
	static int slowdown;
	if (slowdown++ == 2) {
	    float sig = cw->isAutoThreshold? fabsf(siglevel): siglevel;
	    float dB = dB_fast(sig);
	    dB = CLAMP(dB, 0, 100.0);
	    float threshold_log = dB_fast(cw->threshold_linear);
        ext_send_msg(cw->rx_chan, false, "EXT cw_plot=%.2f,%d,%.2f", dB, siglevel >= 0, threshold_log);
        //real_printf("%.0f|%.0f ", sig, dB); fflush(stdout);
        slowdown = 0;
    }

	#if 0
        static void *sample_state;
        static int sample_idx;
        print_max_min_stream_f(&sample_state, P_MAX_MIN_DEMAND, "cw_samp", sample_idx, 1, siglevel);
        sample_idx++;
        if (sample_idx == 137) {
            print_max_min_stream_f(&sample_state, P_MAX_MIN_DUMP, "cw_samp");
            kiwi_ifree(sample_state, "cw_samp");
            sample_state = NULL;
            sample_idx = 0;
        }
    #endif

	//    6.) fill into circular buffer
	//----------------
	// Record state changes and durations onto circular buffer
	if (cw->state != cw->prevstate)
	{
		// Enter the type and duration of the state change into the circular buffer
		cw->sig[cw->sig_lastrx].state = cw->prevstate;
		cw->sig[cw->sig_lastrx].time = cw->sig_timer;

		// Zero circular buffer when at max
		cw->sig_lastrx = ring_idx_increment(cw->sig_lastrx, CW_SIG_BUFSIZE);

		cw->sig_timer = 0;              // Zero the signal timer.
		cw->prevstate = cw->state;      // Update state
	}

	//----------------
	// Count signal state timer upwards based on which sampling rate is in effect
	cw->sig_timer = cw->sig_timer + cw->timer_stepsize;

	if (cw->sig_timer > ONE_SECOND * CW_TIMEOUT)
	{
		cw->sig_timer = ONE_SECOND * CW_TIMEOUT;    // Impose a MAXTIME second boundary for overflow time
	}

	cw->sig_incount = cw->sig_lastrx;   // Current Incount pointer
	cw->cur_time = cw->sig_timer;

	//    7.) CW Decode
	CW_Decode(cw);      // Do all the heavy lifting

	// calculation of speed of the received morse signal on basis of the standard "PARIS"
	float32_t spdcalc = 10.0 * cw->times.dot_avg + 4.0 * cw->times.dash_avg + 9.0 * cw->times.symspace_avg + 5.0 * cw->times.cwspace_avg;

	// update only if initialized and prevent division by zero
	if (cw->b.initialized == true && spdcalc > 0)
	{
		// Convert to Milliseconds per Word
		float32_t speed_ms_per_word = spdcalc * 1000.0 / (cw->sampling_freq / (float32_t)cw->blocksize);
		float32_t speed_wpm_raw = (0.5 + 60000.0 / speed_ms_per_word); // calculate words per minute
		cw->speed_wpm_avg = speed_wpm_raw * 0.3 + 0.7 * cw->speed_wpm_avg; // a little lowpass filtering

        #if 0
            static int cnt;
            if ((cnt & 0xff) == 0) {
                printf("CW WPM %s %.0f|%.1f spdcalc %.3f MSPW %.3f pulse %.1f dot %.1f dash %.1f space %.1f sym %.1f %s %d (%.1f dB)"
                    " %.0f %.0f|(%.0f,%.0f)|%.0f\n",
                    cw->b.track? "TRACK" : "FIXED",
                    cw->speed_wpm_avg, speed_wpm_raw, spdcalc, speed_ms_per_word,
                    cw->times.pulse_avg, cw->times.dot_avg, cw->times.dash_avg, cw->times.cwspace_avg, cw->times.symspace_avg,
                    cw->isAutoThreshold? "AUTO" : "FIXED", cw->threshold_linear, dB_fast((float) cw->threshold_linear),
                    cw->old_siglevel, cw->CW_noise, CW_clipped, cw->CW_env, cw->CW_mag
                );
            }
            cnt++;
        #endif
	}
	else
	{
	    //printf("b.init=%d (spd>0)=%d\n", cw->b.initialized, spdcalc > 0);
		cw->speed_wpm_avg = 0; // we have no calculated speed, i.e. not synchronized to signal
	}

	cw->speed = cw->speed_wpm_avg; // for external use, 0 indicates no signal condition
}

void CwDecode_RxProcessor(int rx_chan, int chan, int nsamps, TYPEMONO16 *samps)
{
    cw_decoder_t *cw = &cw_decoder[rx_chan];
    
    if (!cw->process_samples) return;

	for (uint16_t idx = 0; idx < nsamps; idx++) {
		//cw->raw_signal_buffer[cw->sample_counter] = (float32_t) samps[idx];
		cw->raw_signal_buffer[cw->sample_counter] = (float32_t) samps[idx] / 4;

		cw->sample_counter++;
        if (cw->sample_counter >= cw->blocksize) {
            CW_Decode_exe(cw);
            cw->sample_counter = 0;
        }
	}
}


void CwDecode_wpm(int rx_chan, int wpm, int training_interval)
{
    cw_decoder_t *cw = &cw_decoder[rx_chan];
    cw->wpm_fixed = wpm;
    CwDecode_Init(rx_chan, wpm, training_interval);
}


//------------------------------------------------------------------
//
// Initialization Function (non-blocking-style)
// Determine Pulse, Dash, Dot and initial
// Character-Word time averages
//
// Input is the circular buffer sig[], including in and out counters
// Output is variables containing dot dash and space averages
//
//------------------------------------------------------------------
static void cw_train(cw_decoder_t *cw)
{
	int16_t processed;      // Number of states that have been processed
	float32_t t;            // We do timing calculations in floating point

	// to gain a little bit of precision when low
	// sampling rate
	// Set up progress counter at beginning of initialize
	if (cw->initializing == FALSE)
	{
		cw->startpos = cw->sig_outcount;        // We start at last processed mark/space
		cw->progress = cw->sig_outcount;
		cw->initializing = TRUE;
		cw->times.pulse_avg = 0;                // Reset CW timing variables to 0
		cw->times.dot_avg = 0;
		cw->times.dash_avg = 0;
		cw->times.symspace_avg = 0;
		cw->times.cwspace_avg = 0;
		cw->times.w_space = 0;
	}

	if (cw->b.track == FALSE)       // no training if fixed WPM
	    return;

	// Determine number of states waiting to be processed
	processed = ring_distanceFromTo(cw->startpos, cw->progress);
    ext_send_msg(cw->rx_chan, false, "EXT cw_train=%d", processed >= cw->training_interval? 0 : processed);

	if (processed >= cw->training_interval)
	{
		cw->b.initialized = TRUE;   // Indicate we're done and return
		cw->initializing = FALSE;   // Allow for correct setup of progress if cw_train() is invoked a second time

		#if 0
        printf("pulse %.1f dot %.1f dash %.1f space %.1f sym %.1f\n",
            cw->times.pulse_avg, cw->times.dot_avg, cw->times.dash_avg, cw->times.cwspace_avg, cw->times.symspace_avg);
	    #endif
	}
	
	if (cw->progress != cw->sig_incount)            // Do we have a new state?
	{
		t = cw->sig[cw->progress].time;

		if (cw->sig[cw->progress].state)            // Is it a pulse?
		{
			if (processed > TRAINING_STABLE)        // More than TRAINING_STABLE, getting stable
			{
				if (t > cw->times.pulse_avg)
				{
					cw->times.dash_avg = cw->times.dash_avg + (t - cw->times.dash_avg) / 4.0;    // (e.q. 4.5)
				}
				else
				{
					cw->times.dot_avg = cw->times.dot_avg + (t - cw->times.dot_avg) / 4.0;       // (e.q. 4.4)
				}
			}
			else                           // Less than TRAINING_STABLE, still quite unstable
			{
				if (t > cw->times.pulse_avg)
				{
					cw->times.dash_avg = (t + cw->times.dash_avg) / 2.0;               // (e.q. 4.2)
				}
				else
				{
					cw->times.dot_avg = (t + cw->times.dot_avg) / 2.0;                 // (e.q. 4.1)
				}
			}
			cw->times.pulse_avg = (cw->times.dot_avg / 4 + cw->times.dash_avg) / 2.0; // Update pulse_avg (e.q. 4.3)
		}
		else          // Not a pulse - determine character_word space avg
		{
			if (processed > TRAINING_STABLE)
			{
				if (t > cw->times.pulse_avg)                              // Symbol space?
				{
					cw->times.cwspace_avg = cw->times.cwspace_avg + (t - cw->times.cwspace_avg) / 4.0; // (e.q. 4.8)
				}
				else
				{
					cw->times.symspace_avg = cw->times.symspace_avg + (t - cw->times.symspace_avg) / 4.0; // New EQ, to assist calculating Rate
				}
			}
		}

		cw->progress = ring_idx_increment(cw->progress, CW_SIG_BUFSIZE);    // Increment progress counter
	}
}

//------------------------------------------------------------------
//
// Spike Cancel function
//
// Optionally selectable in CWReceive.h, used by Data Recognition
// function to identify and ignore spikes of short duration.
//
//------------------------------------------------------------------

static bool CwDecoder_IsSpike(cw_decoder_t *cw, uint32_t t)
{
	bool retval = false;

	if (cw->spikecancel == CW_SPIKECANCEL_MODE_SPIKE) // SPIKE CANCEL // Squash spikes/transients of short duration
	{
		retval = t <= CW_SPIKECANCEL_MAX_DURATION;
	}
	else if (cw->spikecancel == CW_SPIKECANCEL_MODE_SHORT) // SHORT CANCEL // Squash spikes shorter than 1/3rd dot duration
	{
		retval = (3 * t < cw->times.dot_avg) && (cw->b.initialized == TRUE); // Only do this if we are not initializing dot/dash periods
	}
	return retval;
}


static float32_t spikeCancel(cw_decoder_t *cw, float32_t t)
{
	if (cw->spikecancel != CW_SPIKECANCEL_MODE_OFF)
	{
		if (CwDecoder_IsSpike(cw, t) == true)
		{
			cw->spike = TRUE;
			cw->sig_outcount = ring_idx_increment(cw->sig_outcount, CW_SIG_BUFSIZE); // If short, then do nothing
			t = 0.0;
		}
		else if (cw->spike == TRUE) // Check if last state was a short Spike or Drop
		{
			cw->spike = FALSE;
			// Add time of last three states together.
			t =		t
					+ cw->sig[ring_idx_change(cw->sig_outcount, -1, CW_SIG_BUFSIZE)].time
					+ cw->sig[ring_idx_change(cw->sig_outcount, -2, CW_SIG_BUFSIZE)].time;
		}
	}

	return t;
}

//------------------------------------------------------------------
//
// Data Recognition Function (non-blocking-style)
// Decode dots, dashes and spaces and group together
// into a character.
//
// Input is the circular buffer sig[], including in and out counters
// Variables containing dot, dash and space averages are maintained, and
// output is a data[] buffer containing decoded dot/dash information, a
// data_len variable containing length of incoming character data.
// The function returns TRUE when further calls will not yield a change or a complete new character has been decoded.
// The bool variable the parameter points to is set to true if a new character has been decoded
// In addition, b.wspace flag indicates whether long (word) space after char
//
//------------------------------------------------------------------
static bool cw_DataRecognition(cw_decoder_t *cw, bool* new_char_p)
{
	bool not_done = FALSE;                  // Return value

	*new_char_p = FALSE;

	//-----------------------------------
	// Do we have a new state to process?
	if (cw->sig_outcount != cw->sig_incount)
	{
		not_done = true;
		cw->b.timeout = FALSE;           // Mainly used by Error Correction Function

		const float32_t t = spikeCancel(cw, cw->sig[cw->sig_outcount].time); // Get time of the new state
		// Squash spikes/transients if enabled
		// Attention: Side Effect -> sig_outcount has been be incremented inside spikeCancel if result == 0, because of this we increment only if not 0

		if (t > 0) // not a spike (or spike processing not enabled)
		{
			const bool is_markstate = cw->sig[cw->sig_outcount].state;

			cw->sig_outcount = ring_idx_increment(cw->sig_outcount, CW_SIG_BUFSIZE); // Update process counter
			//-----------------------------------
			// Is it a Mark (keydown)?
			if (is_markstate == true)
			{
				cw->processed = FALSE; // Indicate that incoming character is not processed

				// Determine if Dot or Dash (e.q. 4.10)
				if ((cw->times.pulse_avg - t) >= 0)         // It is a Dot
				{
					cw->b.dash = FALSE;                     // Clear Dash flag
					cw->data[cw->data_len].state = 0;       // Store as Dot
					if (cw->b.track)
					    cw->times.dot_avg = cw->times.dot_avg + (t - cw->times.dot_avg) / 8.0; // Update cw->times.dot_avg (e.q. 4.6)
				}
				//-----------------------------------
				// Is it a Dash?
				else
				{
					cw->b.dash = TRUE;                      // Set Dash flag
					cw->data[cw->data_len].state = 1;       // Store as Dash
					if (t <= 5 * cw->times.dash_avg)        // Store time if not stuck key
					{
					    if (cw->b.track)
						    cw->times.dash_avg = cw->times.dash_avg + (t - cw->times.dash_avg) / 8.0; // Update dash_avg (e.q. 4.7)
					}
				}

				cw->data[cw->data_len].time = (uint32_t) t;     // Store associated time
				cw->data_len++;                                 // Increment by one dot/dash
                if (cw->b.track)
				    cw->times.pulse_avg = (cw->times.dot_avg / 4 + cw->times.dash_avg) / 2.0; // Update pulse_avg (e.q. 4.3)
			}

			//-----------------------------------
			// Is it a Space?
			else
			{
				bool full_char_detected = true;
				if (cw->b.dash == TRUE)                // Last character was a dash
				{
				    cw->b.dash = false;
				    float32_t eq4_12 = t
				            - (cw->times.pulse_avg
				                    - ((uint32_t) cw->data[cw->data_len - 1].time
				                            - cw->times.pulse_avg) / 4.0); // (e.q. 4.12, corrected)
				    if (eq4_12 < 0) // Return on symbol space - not a full char yet
				    {
					    if (cw->b.track)
				            cw->times.symspace_avg = cw->times.symspace_avg + (t - cw->times.symspace_avg) / 8.0; // New EQ, to assist calculating Rat
				        full_char_detected = false;
				    }
				    else if (t <= 10 * cw->times.dash_avg) // Current space is not a timeout
				    {
				        float32_t eq4_14 = t
				                - (cw->times.cwspace_avg
				                        - ((uint32_t) cw->data[cw->data_len - 1].time
				                                - cw->times.pulse_avg) / 4.0); // (e.q. 4.14)
				        if (eq4_14 >= 0)                   // It is a Word space
				        {
				            cw->times.w_space = t;
				            cw->b.wspace = TRUE;
				        }
				    }
				}
				else                                 // Last character was a dot
				{
					// (e.q. 4.11)
					if ((t - cw->times.pulse_avg) < 0) // Return on symbol space - not a full char yet
					{
					    if (cw->b.track)
						    cw->times.symspace_avg = cw->times.symspace_avg + (t - cw->times.symspace_avg) / 8.0; // New EQ, to assist calculating Rate
						full_char_detected = false;
					}
					else if (t <= 10 * cw->times.dash_avg) // Current space is not a timeout
					{
					    if (cw->b.track)
						    cw->times.cwspace_avg = cw->times.cwspace_avg + (t - cw->times.cwspace_avg) / 8.0; // (e.q. 4.9)

						// (e.q. 4.13)
						if ((t - cw->times.cwspace_avg) >= 0)        // It is a Word space
						{
							cw->times.w_space = t;
							cw->b.wspace = TRUE;
						}
					}
				}
				// Process the character
				if (full_char_detected == true && cw->processed == FALSE)
				{
					*new_char_p = TRUE; // Indicate there is a new char to be processed
				}
			}
		}
	}
	//-----------------------------------
	// Long key down or key up
	else if (cw->cur_time > (10 * cw->times.dash_avg))
	{
		// If current state is Key up and Long key up then  Char finalized
		if (cw->sig[cw->sig_incount].state == false && cw->processed == false)
		{
			cw->processed = TRUE;
			cw->b.wspace = TRUE;
			cw->b.timeout = TRUE;
			*new_char_p = TRUE;                         // Process the character
		}
	}

	if (cw->data_len > CW_DATA_BUFSIZE - 2)
	{
		cw->data_len = CW_DATA_BUFSIZE - 2; // We're receiving garble, throw away
	}

	if (*new_char_p)       // Update circular buffer pointers for Error function
	{
		cw->last_outcount = cw->cur_outcount;
		cw->cur_outcount = cw->sig_outcount;
	}
	return not_done;  // FALSE if all data processed or new character, else TRUE
}

//------------------------------------------------------------------
//
// The Code Generation Function converts the received
// character to a string code[] of dots and dashes
//
//------------------------------------------------------------------
static void CodeGenFunc(cw_decoder_t *cw)
{
	uint8_t a;
	cw->code = 0;

	for (a = 0; a < cw->data_len; a++)
	{
		cw->code <<= 2;
		cw->code |= cw->data[a].state? DAH : DIT;
	}
	cw->data_len = 0;                               // And make ready for a new Char
}

static void cw_print(cw_decoder_t *cw, const char *s)
{
    ext_send_msg_encoded(cw->rx_chan, false, "EXT", "cw_chars", (char *) s);
}

static void PrintCharFunc(cw_decoder_t *cw, uint8_t c)
{
	//--------------------------------------
	// Prosigns

	const char *s;
	char s2[2] = { '\0', '\0' };
	
    if (c >= 0x7f) {
        s = "[err]";
    } else
    if (c < ' ') {
        s = cw_code[c].prosign;
    } else {
        s2[0] = c;
        s = (const char *) s2;
    }
    cw_print(cw, s);
	
	u4_t now = timer_sec() / 4;
	if (cw->wpm_update != now) {
		ext_send_msg(cw->rx_chan, false, "EXT cw_wpm=%d", cw->speed);
	    cw->wpm_update = now;
	}
}

//------------------------------------------------------------------
//
// The Word Space Function takes care of Word Spaces.
// Word Space Correction is applied if certain characters, which
// are less likely to be at the end of a word, are received
// The characters tested are applicable to the English language
//
//------------------------------------------------------------------
static void WordSpaceFunc(cw_decoder_t *cw, uint8_t c)
{
	if (cw->b.wspace == TRUE)                             // Print word space
	{
		cw->b.wspace = FALSE;

		// Word space correction routine - longer space required if certain characters
		if (cw->wsc && ((c == 'I') || (c == 'J') || (c == 'Q') || (c == 'U') || (c == 'V') || (c == 'Z')))
		{
			int16_t x = (cw->times.cwspace_avg + cw->times.pulse_avg) - cw->times.w_space;      // (e.q. 4.15)
			if (x < 0)
			{
				cw_print(cw, " ");
			}
		}
		else
		{
            cw_print(cw, " ");
		}
	}
}

void CwDecode_wsc(int rx_chan, int wsc)
{
    cw_decoder_t *cw = &cw_decoder[rx_chan];
    cw->wsc = wsc;
}

void CwDecode_threshold(int rx_chan, int type, int param)
{
    cw_decoder_t *cw = &cw_decoder[rx_chan];
    
    if (type == 0) {
        cw->isAutoThreshold = param? true : false;
        if (cw->isAutoThreshold) {
            cw->weight_linear = AUTO_WEIGHT_LINEAR;
            cw->threshold_linear = AUTO_THRESHOLD_LINEAR;
        }
        printf("CW (type=0) isAutoThreshold=%d\n", param);
    } else {
        cw->threshold_linear = param;
        printf("CW (type=1) threshold_linear=%d (%.1f dB)\n", param, dB_fast((float) param));
    }
}

//------------------------------------------------------------------
//
// Error Correction Function has three parts
// 1) Exits with Error if character is too long (DATA_BUFSIZE-2)
// 2) If a dot duration is determined to be less than half expected,
// then this dot is eliminated by adding it and the two spaces on
// either side to for a new space duration, then new code is generated
// for pattern parsing.
// 3) If not 2) then separate two run-on characters caused by
// a short character space - Extend the char space and reprocess
//
// If not able to resolve anything, then return FALSE
// Return TRUE if something was resolved.
//
//------------------------------------------------------------------
static bool ErrorCorrectionFunc(cw_decoder_t *cw)
{
	bool result = FALSE; // Result of Error resolution - FALSE if nothing resolved

	if (cw->data_len >= CW_DATA_BUFSIZE - 2)     // Too long char received
	{
		PrintCharFunc(cw, 0xff);    // Print Error
		WordSpaceFunc(cw, 0xff);    // Print Word Space
	}

	else
	{
		cw->b.wspace = FALSE;
		//-----------------------------------------------------
		// Find the location of pulse with shortest duration
		// and the location of symbol space of longest duration
		int32_t temp_outcount = cw->last_outcount; // Grab a copy of endpos for last successful decode
		int32_t slocation = cw->last_outcount; // Long symbol space duration and location
		int32_t plocation = cw->last_outcount; // Short pulse duration and location
		uint32_t pduration = UINT_MAX; // Very high number to decrement for min pulse duration
		uint32_t sduration = 0; // and a zero to increment for max symbol space duration

		// if cur_outcount is < CW_SIG_BUFSIZE, loop must terminate after CW_SIG_BUFSIZE -1 steps
        // "while !=" is okay because temp_outcount increments and wraps to zero
		while (temp_outcount != cw->cur_outcount)
		{
			//-----------------------------------------------------
			// Find shortest pulse duration. Only test key-down states
			if (cw->sig[temp_outcount].state)
			{
				bool is_shortest_pulse = cw->sig[temp_outcount].time < pduration;
				// basic test -> shorter than all previously seen ones

				bool is_not_spike = CwDecoder_IsSpike(cw, cw->sig[temp_outcount].time) == false;

				if (is_shortest_pulse == true && is_not_spike == true)
				{
					pduration = cw->sig[temp_outcount].time;
					plocation = temp_outcount;
				}
			}

			//-----------------------------------------------------
			// Find longest symbol space duration. Do not test first state
			// or last state and only test key-up states
			if ((temp_outcount != cw->last_outcount)
					&& (temp_outcount != (cw->cur_outcount - 1))
					&& (!cw->sig[temp_outcount].state))
			{
				if (cw->sig[temp_outcount].time > sduration)
				{
					sduration = cw->sig[temp_outcount].time;
					slocation = temp_outcount;
				}
			}

			temp_outcount = ring_idx_increment(temp_outcount,CW_SIG_BUFSIZE);
		}

		uint8_t decoded[] = { 0xff, 0xff };

		//-----------------------------------------------------
		// Take corrective action by dropping shortest pulse
		// if shorter than half of cw->times.dot_avg
		// This can result in one or more valid characters - or Error
		if ((pduration < cw->times.dot_avg / 2) && (plocation != temp_outcount))
		{
			// Add up duration of short pulse and the two spaces on either side,
			// as space at pulse location + 1
			cw->sig[ring_idx_change(plocation, +1, CW_SIG_BUFSIZE)].time =
					cw->sig[ring_idx_change(plocation, -1, CW_SIG_BUFSIZE)].time
					+ cw->sig[plocation].time
					+ cw->sig[ring_idx_change(plocation, +1, CW_SIG_BUFSIZE)].time;

			// Shift the preceding data forward accordingly
			temp_outcount = ring_idx_change(plocation, -2 ,CW_SIG_BUFSIZE);

			// if last_outcount is < CW_SIG_BUFSIZE, loop must terminate after CW_SIG_BUFSIZE -1 steps
            // "while !=" is okay because temp_outcount decrements and wraps to CW_SIG_BUFSIZE-1
			while (temp_outcount != cw->last_outcount)
			{
				cw->sig[ring_idx_change(temp_outcount, +2, CW_SIG_BUFSIZE)].time =
						cw->sig[temp_outcount].time;

				cw->sig[ring_idx_change(temp_outcount, +2, CW_SIG_BUFSIZE)].state =
						cw->sig[temp_outcount].state;


				temp_outcount = ring_idx_decrement(temp_outcount,CW_SIG_BUFSIZE);
			}
			// And finally shift the startup pointer similarly
			cw->sig_outcount = ring_idx_change(cw->last_outcount, +2,CW_SIG_BUFSIZE);
			//
			// Now we reprocess
			//
			// Pull out a character, using the adjusted sig[] buffer
			// Process character delimited by character or word space
			bool dummy;
			int i;
			for (i=0; i < 1024 && cw_DataRecognition(cw, &dummy); i++)
			{
			    NextTask("cw-loop");
			}
			if (i == 1024) printf("CW LOOP T/O #1\n");

			CodeGenFunc(cw);                 // Generate a dot/dash pattern string
			decoded[0] = CwGen_CharacterIdFunc(cw->code); // Convert dot/dash data into a character
			//if (decoded[0] != 0xff)
			if (decoded[0] < 0xfe)
			{
				PrintCharFunc(cw, decoded[0]);
				result = TRUE;                // Error correction had success.
			}
			else
			{
				PrintCharFunc(cw, 0xff);
			}
		}
		//-----------------------------------------------------
		// Take corrective action by converting the longest symbol space to character space
		// This will result in two valid characters - or Error
		else
		{
			// Split char in two by adjusting time of longest sym space to a char space
			cw->sig[slocation].time =
					((cw->times.cwspace_avg - 1) >= 1 ? cw->times.cwspace_avg - 1 : 1); // Make sure it is always larger than 0
			cw->sig_outcount = cw->last_outcount; // Set circ buffer reference to the start of previous failed decode
			//
			// Now we reprocess
			//
			// Debug - If timing is out of whack because of noise, with rate
			// showing at >99 WPM, then cw_DataRecognition() occasionally fails.
			// Not found out why, but millis() is used to guards against it.

			// Process first character delimited by character or word space
			bool dummy;
			int i;
			for (i=0; i < 1024 && cw_DataRecognition(cw, &dummy); i++)
			{
			    NextTask("cw-loop");
			}
			if (i == 1024) printf("CW LOOP T/O #2\n");

			CodeGenFunc(cw);                 // Generate a dot/dash pattern string
			decoded[0] = CwGen_CharacterIdFunc(cw->code); // Convert dot/dash pattern into a character
			// Process second character delimited by character or word space

			for (i=0; i < 1024 && cw_DataRecognition(cw, &dummy); i++)
			{
			    NextTask("cw-loop");
			}
			if (i == 1024) printf("CW LOOP T/O #3\n");

			CodeGenFunc(cw);                 // Generate a dot/dash pattern string
			decoded[1] = CwGen_CharacterIdFunc(cw->code); // Convert dot/dash pattern into a character

			//if ((decoded[0] != 0xff) && (decoded[1] != 0xff)) // If successful error resolution
			if ((decoded[0] < 0xfe) && (decoded[1] < 0xfe)) // If successful error resolution
			{
				PrintCharFunc(cw, decoded[0]);
				PrintCharFunc(cw, decoded[1]);
				result = TRUE;                // Error correction had success.
			}
			else
			{
				PrintCharFunc(cw, 0xff);
			}
		}
	}
	return result;
}

//------------------------------------------------------------------
//
// CW Decode manages all the decode Functions.
// It establishes dot/dash/space periods through the Initialization
// function, and when initialized (or if excessive time when not fully
// initialized), then it runs DataRecognition, CodeGen and CharacterId
// functions to decode any incoming data.  If not successful decode
// then ErrorCorrection is attempted, and if that fails, then
// Initialization is re-performed.
//
//------------------------------------------------------------------
static void CW_Decode(cw_decoder_t *cw)
{
	//-----------------------------------
	// Initialize pulse_avg, dot_avg, dash_avg, symspace_avg, cwspace_avg

	if (cw->b.initialized == FALSE)
	{
		cw_train(cw);
	}

	//-----------------------------------
	// Process the works once initialized - or if timeout

	if ((cw->b.initialized == TRUE) || (cw->cur_time >= ONE_SECOND * CW_TIMEOUT))
	{
		bool received;                          // True on a symbol received
		cw_DataRecognition(cw, &received);      // True if new character received
		if (received && (cw->data_len > 0))     // also make sure it is not a spike
		{
			CodeGenFunc(cw);                    // Generate a dot/dash pattern string

			uint8_t decoded = CwGen_CharacterIdFunc(cw->code);
			// Identify the Character
			// 0xff if char not recognized

			if (decoded < 0xfe)        // 0xfe = spike suppression, 0xff = error
			{
				PrintCharFunc(cw, decoded);     // Print character
				WordSpaceFunc(cw, decoded);     // Print Word Space
			}
			else if (decoded == 0xff)           // Attempt Error Correction
			{
				// If Error Correction function cannot resolve, then reinitialize speed
				if (ErrorCorrectionFunc(cw) == FALSE && cw->b.track)
				{
				    cw->err_cnt++;
				    cw->err_timeout = timer_sec() + ERROR_TIMEOUT;
                    ext_send_msg(cw->rx_chan, false, "EXT cw_train=%d", -(cw->err_cnt));
                    if (cw->err_cnt > 3) {
                        // re-train
					    cw->b.initialized = FALSE;
					    cw->err_cnt = 0;
					    cw->err_timeout = 0;
					}
				}
			}
			
			// timeout error counting after a while
			if (cw->b.track && cw->err_timeout && timer_sec() >= cw->err_timeout) {
                cw->err_cnt = 0;
                cw->err_timeout = 0;
                ext_send_msg(cw->rx_chan, false, "EXT cw_train=0");
			}
		}
	}
}
