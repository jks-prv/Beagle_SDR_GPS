#include "monitor.h"
#include <common/common.h>

#define LOG_LEVEL LOG_WARN
#include <ft8/debug_ft8.h>

#include <stdlib.h>

static float hann_i(int i, int N)
{
    float x = sinf((float)M_PI * i / N);
    return x * x;
}

// static float hamming_i(int i, int N)
// {
//     const float a0 = (float)25 / 46;
//     const float a1 = 1 - a0;

//     float x1 = cosf(2 * (float)M_PI * i / N);
//     return a0 - a1 * x1;
// }

// static float blackman_i(int i, int N)
// {
//     const float alpha = 0.16f; // or 2860/18608
//     const float a0 = (1 - alpha) / 2;
//     const float a1 = 1.0f / 2;
//     const float a2 = alpha / 2;

//     float x1 = cosf(2 * (float)M_PI * i / N);
//     float x2 = 2 * x1 * x1 - 1; // Use double angle formula

//     return a0 - a1 * x1 + a2 * x2;
// }

static void waterfall_init(ftx_waterfall_t* me, int max_blocks, int num_bins, int time_osr, int freq_osr)
{
    size_t mag_size = max_blocks * time_osr * freq_osr * num_bins * sizeof(me->mag[0]);
    me->max_blocks = max_blocks;
    me->num_blocks = 0;
    me->num_bins = num_bins;
    me->time_osr = time_osr;
    me->freq_osr = freq_osr;
    me->block_stride = (time_osr * freq_osr * num_bins);
    me->mag = (WF_ELEM_T*)malloc(mag_size);
    LOG(LOG_DEBUG, "Waterfall size = %zu\n", mag_size);
}

static void waterfall_free(ftx_waterfall_t* me)
{
    free(me->mag);
}

void monitor_init(monitor_t* me, const monitor_config_t* cfg)
{
    float slot_time = (cfg->protocol == FTX_PROTOCOL_FT4) ? FT4_SLOT_TIME : FT8_SLOT_TIME;
    float symbol_period = (cfg->protocol == FTX_PROTOCOL_FT4) ? FT4_SYMBOL_PERIOD : FT8_SYMBOL_PERIOD;
    // Compute DSP parameters that depend on the sample rate
    me->block_size = (int)(cfg->sample_rate * symbol_period); // samples corresponding to one FSK symbol
    me->subblock_size = me->block_size / cfg->time_osr;
    me->nfft = me->block_size * cfg->freq_osr;
    me->fft_norm = 2.0f / me->nfft;
    // const int len_window = 1.8f * me->block_size; // hand-picked and optimized

    me->window = (float*)malloc(me->nfft * sizeof(me->window[0]));
    for (int i = 0; i < me->nfft; ++i)
    {
        // window[i] = 1;
        me->window[i] = me->fft_norm * hann_i(i, me->nfft);
        // me->window[i] = blackman_i(i, me->nfft);
        // me->window[i] = hamming_i(i, me->nfft);
        // me->window[i] = (i < len_window) ? hann_i(i, len_window) : 0;
    }
    me->last_frame = (float*)calloc(me->nfft, sizeof(me->last_frame[0]));

    LOG(LOG_INFO, "Block size = %d\n", me->block_size);
    LOG(LOG_INFO, "Subblock size = %d\n", me->subblock_size);

    #ifdef KISS_FFT
        size_t fft_work_size = 0;
        kiss_fftr_alloc(me->nfft, 0, 0, &fft_work_size);
        me->fft_work = malloc(fft_work_size);
        me->fft_cfg = kiss_fftr_alloc(me->nfft, 0, me->fft_work, &fft_work_size);
    
        LOG(LOG_INFO, "N_FFT = %d\n", me->nfft);
        LOG(LOG_DEBUG, "FFT work area = %zu\n", fft_work_size);
    
        #ifdef WATERFALL_USE_PHASE
            me->nifft = 64; // Gives 200 Hz sample rate for FT8 (160ms symbol period)
        
            size_t ifft_work_size = 0;
            kiss_fft_alloc(me->nifft, 1, 0, &ifft_work_size);
            me->ifft_work = malloc(ifft_work_size);
            me->ifft_cfg = kiss_fft_alloc(me->nifft, 1, me->ifft_work, &ifft_work_size);
        
            LOG(LOG_INFO, "N_iFFT = %d\n", me->nifft);
            LOG(LOG_DEBUG, "iFFT work area = %zu\n", ifft_work_size);
        #endif
    #else
    #endif

    // Allocate enough blocks to fit the entire FT8/FT4 slot in memory
    const int max_blocks = (int)(slot_time / symbol_period);
    // Keep only FFT bins in the specified frequency range (f_min/f_max)
    me->min_bin = (int)(cfg->f_min * symbol_period);
    me->max_bin = (int)(cfg->f_max * symbol_period) + 1;
    const int num_bins = me->max_bin - me->min_bin;

    waterfall_init(&me->wf, max_blocks, num_bins, cfg->time_osr, cfg->freq_osr);
    me->wf.protocol = cfg->protocol;

    me->symbol_period = symbol_period;

    me->max_mag = -120.0f;
    
    #ifdef KISS_FFT
        me->timedata = (kiss_fft_scalar *) malloc(sizeof(kiss_fft_scalar) * me->nfft);
        me->freqdata = (kiss_fft_cpx *) malloc(sizeof(kiss_fft_cpx) * (me->nfft / 2 + 1));
    #else
        me->timedata = (float *) fftwf_malloc(sizeof(float) * me->nfft);
        me->freqdata = (fftwf_complex *) fftwf_malloc(sizeof(fftwf_complex) * (me->nfft / 2 + 1));
        me->fft_plan = fftwf_plan_dft_r2c_1d(me->nfft, me->timedata, me->freqdata, FFTW_ESTIMATE);
    #endif
}

void monitor_free(monitor_t* me)
{
    waterfall_free(&me->wf);
    free(me->last_frame);
    free(me->window);

    #ifdef KISS_FFT
        free(me->fft_work);
        free(me->timedata);
        free(me->freqdata);

        #ifdef WATERFALL_USE_PHASE
            ...
        #endif
    #else
        fftwf_free(me->timedata);
        fftwf_free(me->freqdata);
        fftwf_destroy_plan(me->fft_plan);
    #endif
    
    memset(me, 0, sizeof(*me));
}

void monitor_reset(monitor_t* me)
{
    me->wf.num_blocks = 0;
    me->max_mag = -120.0f;
}

// Compute FFT magnitudes (log wf) for a frame in the signal and update waterfall data
void monitor_process(monitor_t* me, const float* frame)
{
    // Check if we can still store more waterfall data
    if (me->wf.num_blocks >= me->wf.max_blocks)
        return;

    int offset = me->wf.num_blocks * me->wf.block_stride;
    int frame_pos = 0;

    // Loop over block subdivisions
    for (int time_sub = 0; time_sub < me->wf.time_osr; ++time_sub)
    {
        //kiss_fft_scalar timedata[me->nfft];
        //kiss_fft_cpx freqdata[me->nfft / 2 + 1];

        // Shift the new data into analysis frame
        for (int pos = 0; pos < me->nfft - me->subblock_size; ++pos)
        {
            me->last_frame[pos] = me->last_frame[pos + me->subblock_size];
        }
        for (int pos = me->nfft - me->subblock_size; pos < me->nfft; ++pos)
        {
            me->last_frame[pos] = frame[frame_pos];
            ++frame_pos;
        }

        // Do DFT of windowed analysis frame
        for (int pos = 0; pos < me->nfft; ++pos)
        {
            me->timedata[pos] = me->window[pos] * me->last_frame[pos];
        }
        
        #ifdef KISS_FFT
            kiss_fftr(me->fft_cfg, me->timedata, me->freqdata);
        #else
            fftwf_execute(me->fft_plan);
        #endif

        // Loop over possible frequency OSR offsets
        for (int freq_sub = 0; freq_sub < me->wf.freq_osr; ++freq_sub)
        {
            for (int bin = me->min_bin; bin < me->max_bin; ++bin)
            {
                int src_bin = (bin * me->wf.freq_osr) + freq_sub;
                
                #ifdef KISS_FFT
                    float mag2 = (me->freqdata[src_bin].i * me->freqdata[src_bin].i) + (me->freqdata[src_bin].r * me->freqdata[src_bin].r);
                #else
                    float mag2 = (me->freqdata[src_bin][0] * me->freqdata[src_bin][0]) + (me->freqdata[src_bin][1] * me->freqdata[src_bin][1]);
                #endif
                
                float db = 10.0f * log10f(1E-12f + mag2);

#ifdef WATERFALL_USE_PHASE
                // Save the magnitude in dB and phase in radians
                float phase = atan2f(me->freqdata[src_bin].i, me->freqdata[src_bin].r);
                me->wf.mag[offset].mag = db;
                me->wf.mag[offset].phase = phase;
#else
                // Scale decibels to unsigned 8-bit range and clamp the value
                // Range 0-240 covers -120..0 dB in 0.5 dB steps
                int scaled = (int)(2 * db + 240);
                me->wf.mag[offset] = (scaled < 0) ? 0 : ((scaled > 255) ? 255 : scaled);
#endif
                ++offset;

                if (db > me->max_mag)
                    me->max_mag = db;
            }
        }
    }

    ++me->wf.num_blocks;
}

#ifdef WATERFALL_USE_PHASE
void monitor_resynth(const monitor_t* me, const candidate_t* candidate, float* signal)
{
    const int num_ifft = me->nifft;
    const int num_shift = num_ifft / 2;
    const int taper_width = 4;
    const int num_tones = 8;

    // Starting offset is 3 subblocks due to analysis buffer loading
    int offset = 1;                          // candidate->time_offset;
    offset = (offset * me->wf.time_osr) + 1; // + candidate->time_sub;
    offset = (offset * me->wf.freq_osr);     // + candidate->freq_sub;
    offset = (offset * me->wf.num_bins);     // + candidate->freq_offset;

    WF_ELEM_T* el = me->wf.mag + offset;

    // DFT frequency data - initialize to zero
    //kiss_fft_cpx freqdata[num_ifft];
    for (int i = 0; i < num_ifft; ++i)
    {
        me->freqdata[i].r = 0;
        me->freqdata[i].i = 0;
    }

    int pos = 0;
    for (int num_block = 1; num_block < me->wf.num_blocks; ++num_block)
    {
        // Extract frequency data around the selected candidate only
        for (int i = candidate->freq_offset - taper_width - 1; i < candidate->freq_offset + 8 + taper_width - 1; ++i)
        {
            if ((i >= 0) && (i < me->wf.num_bins))
            {
                int tgt_bin = (me->wf.freq_osr * (i - candidate->freq_offset) + num_ifft) % num_ifft;
                float weight = 1.0f;
                if (i < candidate->freq_offset)
                {
                    weight = ((i - candidate->freq_offset) + taper_width) / (float)taper_width;
                }
                else if (i > candidate->freq_offset + 7)
                {
                    weight = ((candidate->freq_offset + 7 - i) + taper_width) / (float)taper_width;
                }

                // Convert (dB magnitude, phase) to (real, imaginary)
                float mag = powf(10.0f, el[i].mag / 20) / 2 * weight;
                me->freqdata[tgt_bin].r = mag * cosf(el[i].phase);
                me->freqdata[tgt_bin].i = mag * sinf(el[i].phase);

                int i2 = i + me->wf.num_bins;
                tgt_bin = (tgt_bin + 1) % num_ifft;
                float mag2 = powf(10.0f, el[i2].mag / 20) / 2 * weight;
                me->freqdata[tgt_bin].r = mag2 * cosf(el[i2].phase);
                me->freqdata[tgt_bin].i = mag2 * sinf(el[i2].phase);
            }
        }

        // Compute inverse DFT and overlap-add the waveform
        kiss_fft_cpx me->timedata[num_ifft];
        kiss_fft(me->ifft_cfg, me->freqdata, me->timedata);
        for (int i = 0; i < num_ifft; ++i)
        {
            signal[pos + i] += me->timedata[i].i;
        }

        // Move to the next symbol
        el += me->wf.block_stride;
        pos += num_shift;
    }
}
#endif
