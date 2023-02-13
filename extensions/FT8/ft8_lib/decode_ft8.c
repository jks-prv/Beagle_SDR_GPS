#include "types.h"
#include "config.h"
#include "rx/CuteSDR/datatypes.h"
#include "coroutines.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdbool.h>
#include <time.h>

#include <ft8/decode.h>
#include <ft8/encode.h>
#include <ft8/message.h>

#include <common/common.h>
#include <common/monitor.h>

#define LOG_LEVEL LOG_WARN
#include <ft8/debug_ft8.h>

int ext_send_msg_encoded(int rx_chan, bool debug, const char *dst, const char *cmd, const char *fmt, ...);

typedef struct {
    char callsign[12]; ///> Up to 11 symbols of callsign + trailing zeros (always filled)
    uint32_t hash;     ///> 8 MSBs contain the age of callsign; 22 LSBs contain hash value
} callsign_hashtable_t;

typedef struct {
    u4_t magic;
    int rx_chan;
    monitor_t mon;
    float slot_period;
    struct tm tm_slot_start;
    int num_samples;
    TYPEREAL *samples;
    bool tsync;
    int in_pos;
    int frame_pos;

    callsign_hashtable_t *callsign_hashtable;
    int callsign_hashtable_size;
} decode_ft8_t;

decode_ft8_t decode_ft8[MAX_RX_CHANS];

const int kMin_score = 10; // Minimum sync score threshold for candidates
const int kMax_candidates = 140;
const int kLDPC_iterations = 25;

const int kMax_decoded_messages = 50;

const int kFreq_osr = 2; // Frequency oversampling rate (bin subdivision)
const int kTime_osr = 2; // Time oversampling rate (symbol subdivision)

#define CALLSIGN_HASHTABLE_MAX 256

static void hashtable_init(int rx_chan)
{
    decode_ft8_t *ft8 = &decode_ft8[rx_chan];
    ft8->callsign_hashtable_size = 0;
    ft8->callsign_hashtable = (callsign_hashtable_t *) malloc(sizeof(callsign_hashtable_t) * CALLSIGN_HASHTABLE_MAX);
    memset(ft8->callsign_hashtable, 0, sizeof(callsign_hashtable_t) * CALLSIGN_HASHTABLE_MAX);
}

static void hashtable_cleanup(int rx_chan, uint8_t max_age)
{
    decode_ft8_t *ft8 = &decode_ft8[rx_chan];
    for (int idx_hash = 0; idx_hash < CALLSIGN_HASHTABLE_MAX; ++idx_hash)
    {
        if (ft8->callsign_hashtable[idx_hash].callsign[0] != '\0')
        {
            uint8_t age = (uint8_t)(ft8->callsign_hashtable[idx_hash].hash >> 24);
            if (age > max_age)
            {
                LOG(LOG_INFO, "Removing [%s] from hash table, age = %d\n", ft8->callsign_hashtable[idx_hash].callsign, age);
                // free the hash entry
                ft8->callsign_hashtable[idx_hash].callsign[0] = '\0';
                ft8->callsign_hashtable[idx_hash].hash = 0;
                ft8->callsign_hashtable_size--;
            }
            else
            {
                // increase callsign age
                ft8->callsign_hashtable[idx_hash].hash = (((uint32_t)age + 1u) << 24) | (ft8->callsign_hashtable[idx_hash].hash & 0x3FFFFFu);
            }
        }
    }
}

static void hashtable_add(const char* callsign, uint32_t hash)
{
    decode_ft8_t *ft8 = (decode_ft8_t *) FROM_VOID_PARAM(TaskGetUserParam());
    uint16_t hash10 = (hash >> 12) & 0x3FFu;
    int idx_hash = (hash10 * 23) % CALLSIGN_HASHTABLE_MAX;
    while (ft8->callsign_hashtable[idx_hash].callsign[0] != '\0')
    {
        if (((ft8->callsign_hashtable[idx_hash].hash & 0x3FFFFFu) == hash) && (0 == strcmp(ft8->callsign_hashtable[idx_hash].callsign, callsign)))
        {
            // reset age
            ft8->callsign_hashtable[idx_hash].hash &= 0x3FFFFFu;
            LOG(LOG_DEBUG, "Found a duplicate [%s]\n", callsign);
            return;
        }
        else
        {
            LOG(LOG_DEBUG, "Hash table clash!\n");
            // Move on to check the next entry in hash table
            idx_hash = (idx_hash + 1) % CALLSIGN_HASHTABLE_MAX;
        }
    }
    ft8->callsign_hashtable_size++;
    strncpy(ft8->callsign_hashtable[idx_hash].callsign, callsign, 11);
    ft8->callsign_hashtable[idx_hash].callsign[11] = '\0';
    ft8->callsign_hashtable[idx_hash].hash = hash;
}

static bool hashtable_lookup(ftx_callsign_hash_type_t hash_type, uint32_t hash, char* callsign)
{
    decode_ft8_t *ft8 = (decode_ft8_t *) FROM_VOID_PARAM(TaskGetUserParam());
    uint8_t hash_shift = (hash_type == FTX_CALLSIGN_HASH_10_BITS) ? 12 : (hash_type == FTX_CALLSIGN_HASH_12_BITS ? 10 : 0);
    uint16_t hash10 = (hash >> (12 - hash_shift)) & 0x3FFu;
    int idx_hash = (hash10 * 23) % CALLSIGN_HASHTABLE_MAX;
    while (ft8->callsign_hashtable[idx_hash].callsign[0] != '\0')
    {
        if (((ft8->callsign_hashtable[idx_hash].hash & 0x3FFFFFu) >> hash_shift) == hash)
        {
            strcpy(callsign, ft8->callsign_hashtable[idx_hash].callsign);
            return true;
        }
        // Move on to check the next entry in hash table
        idx_hash = (idx_hash + 1) % CALLSIGN_HASHTABLE_MAX;
    }
    callsign[0] = '\0';
    return false;
}

ftx_callsign_hash_interface_t hash_if = {
    .lookup_hash = hashtable_lookup,
    .save_hash = hashtable_add
};

static void decode(int rx_chan, const monitor_t* mon)
{
    decode_ft8_t *ft8 = &decode_ft8[rx_chan];

    const ftx_waterfall_t* wf = &mon->wf;
    // Find top candidates by Costas sync score and localize them in time and frequency
    ftx_candidate_t candidate_list[kMax_candidates];
    int num_candidates = ftx_find_candidates(wf, kMax_candidates, candidate_list, kMin_score);

    // Hash table for decoded messages (to check for duplicates)
    int num_decoded = 0;
    ftx_message_t decoded[kMax_decoded_messages];
    ftx_message_t* decoded_hashtable[kMax_decoded_messages];

    // Initialize hash table pointers
    for (int i = 0; i < kMax_decoded_messages; ++i)
    {
        decoded_hashtable[i] = NULL;
    }

    // Go over candidates and attempt to decode messages
    for (int idx = 0; idx < num_candidates; ++idx)
    {
        NextTask("candidate");
        const ftx_candidate_t* cand = &candidate_list[idx];

        float freq_hz = (mon->min_bin + cand->freq_offset + (float)cand->freq_sub / wf->freq_osr) / mon->symbol_period;
        float time_sec = (cand->time_offset + (float)cand->time_sub / wf->time_osr) * mon->symbol_period;

#ifdef WATERFALL_USE_PHASE
        // int resynth_len = 12000 * 16;
        // float resynth_signal[resynth_len];
        // for (int pos = 0; pos < resynth_len; ++pos)
        // {
        //     resynth_signal[pos] = 0;
        // }
        // monitor_resynth(mon, cand, resynth_signal);
        // char resynth_path[80];
        // sprintf(resynth_path, "resynth_%04f_%02.1f.wav", freq_hz, time_sec);
        // save_wav(resynth_signal, resynth_len, 12000, resynth_path);
#endif

        ftx_message_t message;
        ftx_decode_status_t status;
        if (!ftx_decode_candidate(wf, cand, kLDPC_iterations, &message, &status))
        {
            if (status.ldpc_errors > 0)
            {
                LOG(LOG_DEBUG, "LDPC decode: %d errors\n", status.ldpc_errors);
            }
            else if (status.crc_calculated != status.crc_extracted)
            {
                LOG(LOG_DEBUG, "CRC mismatch!\n");
            }
            continue;
        }

        LOG(LOG_DEBUG, "Checking hash table for %4.1fs / %4.1fHz [%d]...\n", time_sec, freq_hz, cand->score);
        int idx_hash = message.hash % kMax_decoded_messages;
        bool found_empty_slot = false;
        bool found_duplicate = false;
        do
        {
            if (decoded_hashtable[idx_hash] == NULL)
            {
                LOG(LOG_DEBUG, "Found an empty slot\n");
                found_empty_slot = true;
            }
            else if ((decoded_hashtable[idx_hash]->hash == message.hash) && (0 == memcmp(decoded_hashtable[idx_hash]->payload, message.payload, sizeof(message.payload))))
            {
                LOG(LOG_DEBUG, "Found a duplicate!\n");
                found_duplicate = true;
            }
            else
            {
                LOG(LOG_DEBUG, "Hash table clash!\n");
                // Move on to check the next entry in hash table
                idx_hash = (idx_hash + 1) % kMax_decoded_messages;
            }
        } while (!found_empty_slot && !found_duplicate);

        if (found_empty_slot)
        {
            // Fill the empty hashtable slot
            memcpy(&decoded[idx_hash], &message, sizeof(message));
            decoded_hashtable[idx_hash] = &decoded[idx_hash];
            ++num_decoded;

            char text[FTX_MAX_MESSAGE_LENGTH];
            ftx_message_rc_t unpack_status = ftx_message_decode(&message, &hash_if, text);
            if (unpack_status != FTX_MESSAGE_RC_OK)
            {
                snprintf(text, sizeof(text), "Error [%d] while unpacking!", (int)unpack_status);
            }

            // Fake WSJT-X-like output for now
            float snr = cand->score * 0.5f; // TODO: compute better approximation of SNR
            LOG(LOG_INFO, "%02d%02d%02d %+05.1f %+4.2f %4.0f ~  %s\n",
                ft8->tm_slot_start.tm_hour, ft8->tm_slot_start.tm_min, ft8->tm_slot_start.tm_sec,
                snr, time_sec, freq_hz, text);
            ext_send_msg_encoded(rx_chan, false, "EXT", "chars",
                "%02d%02d%02d %+05.1f %+4.2f %4.0f ~  %s\n",
                ft8->tm_slot_start.tm_hour, ft8->tm_slot_start.tm_min, ft8->tm_slot_start.tm_sec,
                snr, time_sec, freq_hz, text);
        }
    }
    LOG(LOG_INFO, "Decoded %d messages, callsign hashtable size %d\n", num_decoded, ft8->callsign_hashtable_size);
    if (num_decoded > 0)
        ext_send_msg_encoded(rx_chan, false, "EXT", "chars",
            "%02d%02d%02d decoded %d, hashtable %d%%\n",
            ft8->tm_slot_start.tm_hour, ft8->tm_slot_start.tm_min, ft8->tm_slot_start.tm_sec,
            num_decoded, ft8->callsign_hashtable_size * 100 / CALLSIGN_HASHTABLE_MAX);
    hashtable_cleanup(rx_chan, 10);
}

void decode_ft8_setup(int rx_chan)
{
    decode_ft8_t *ft8 = &decode_ft8[rx_chan];
    TaskSetUserParam(TO_VOID_PARAM(ft8));
}

void decode_ft8_samples(int rx_chan, TYPEMONO16 *samps, int nsamps, bool *start_test)
{
    decode_ft8_t *ft8 = &decode_ft8[rx_chan];

    if (!ft8->tsync) {
        const float time_shift = 0.8;
        struct timespec spec;
        clock_gettime(CLOCK_REALTIME, &spec);
        double time_sec = (double) spec.tv_sec + (spec.tv_nsec / 1e9);
        double time_within_slot = fmod(time_sec - time_shift, ft8->slot_period);
        if (time_within_slot > ft8->slot_period / 4) {
            *start_test = false;
            return;     // wait for beginning of slot
        }

        time_t time_slot_start = (time_t) (time_sec - time_within_slot);
        gmtime_r(&time_slot_start, &ft8->tm_slot_start);
        LOG(LOG_INFO, "FT8 Time within slot %02d:%02d:%02d %.3f s\n", ft8->tm_slot_start.tm_hour,
            ft8->tm_slot_start.tm_min, ft8->tm_slot_start.tm_sec, time_within_slot);
        ft8->in_pos = ft8->frame_pos = 0;
        *start_test = true;
        ft8->tsync = true;
    }
    
    for (int i = 0; i < nsamps /*&& ft8->in_pos < ft8->num_samples*/; i++) {
        // NB: must normalize to +/- 1.0 or there won't be any decodes
        ft8->samples[ft8->in_pos] = ((TYPEREAL) samps[i]) / 32768.0f;
        ft8->in_pos++;
    }
    
    int block_size = ft8->mon.block_size;
    if (ft8->in_pos < ft8->frame_pos + block_size)
        return;      // not yet a full block

    while (ft8->in_pos >= ft8->frame_pos + block_size && ft8->frame_pos < ft8->num_samples) {
        monitor_process(&ft8->mon, ft8->samples + ft8->frame_pos);
        ft8->frame_pos += block_size;
    }
    if (ft8->frame_pos < ft8->num_samples)
        return;
    
    *start_test = false;
    LOG(LOG_DEBUG, "FT8 Waterfall accumulated %d symbols\n", ft8->mon.wf.num_blocks);
    LOG(LOG_INFO, "FT8 Max magnitude: %.1f dB\n", ft8->mon.max_mag);

    // Decode accumulated data (containing slightly less than a full time slot)
    NextTask("decode");
    decode(rx_chan, &ft8->mon);

    // Reset internal variables for the next time slot
    monitor_reset(&ft8->mon);
    ft8->tsync = false;
}

void decode_ft8_init(int rx_chan, int proto)
{
    decode_ft8_t *ft8 = &decode_ft8[rx_chan];
    memset(ft8, 0, sizeof(decode_ft8_t));
    ft8->magic = 0xbeefcafe;
    ft8->rx_chan = rx_chan;

    ftx_protocol_t protocol = proto? FTX_PROTOCOL_FT4 : FTX_PROTOCOL_FT8;
    float slot_period = ((protocol == FTX_PROTOCOL_FT8) ? FT8_SLOT_TIME : FT4_SLOT_TIME);
    ft8->slot_period = slot_period;
    int sample_rate = snd_rate;
    int num_samples = slot_period * sample_rate;
    ft8->samples = (TYPEREAL *) malloc(num_samples * sizeof(TYPEREAL));

    // active period
    num_samples = (slot_period - 0.4f) * sample_rate;
    ft8->num_samples = num_samples;

    // Compute FFT over the whole signal and store it
    monitor_config_t mon_cfg = {
        .f_min = 200,
        .f_max = 3000,
        .sample_rate = sample_rate,
        .time_osr = kTime_osr,
        .freq_osr = kFreq_osr,
        .protocol = protocol
    };

    hashtable_init(rx_chan);
    monitor_init(&ft8->mon, &mon_cfg);
    LOG(LOG_DEBUG, "FT8 Waterfall allocated %d symbols\n", ft8->mon.wf.max_blocks);
    ft8->tsync = false;
}

void decode_ft8_close(int rx_chan)
{
    decode_ft8_t *ft8 = &decode_ft8[rx_chan];
    free(ft8->samples);
    free(ft8->callsign_hashtable);
    monitor_free(&ft8->mon);
}
