
#include "types.h"
#include "config.h"
#include "str.h"
#include "rx/CuteSDR/datatypes.h"
#include "coroutines.h"
#include "FT8.h"
#include "PSKReporter.h"

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

#ifdef PR_USE_CALLSIGN_HASHTABLE
    #define CALLSIGN_HASHTABLE_MAX 1024
    #define CALLSIGN_AGE_MAX 60             // one hour
    //#define CALLSIGN_HASHTABLE_MAX 64
    //#define CALLSIGN_AGE_MAX 15
#else
    #define CALLSIGN_HASHTABLE_MAX 256
    #define CALLSIGN_AGE_MAX 3
#endif

const int kMin_score = 10; // Minimum sync score threshold for candidates
const int kMax_candidates = 140;
const int kLDPC_iterations = 25;

const int kMax_decoded_messages = 50;

const int kFreq_osr = 2; // Frequency oversampling rate (bin subdivision)
const int kTime_osr = 2; // Time oversampling rate (symbol subdivision)

typedef struct {
    // NB: callsign is NOT null terminated (to save a byte). Must use strncpy() / strncmp()
    char callsign[11]; ///> Up to 11 symbols of callsign + trailing zeros (always filled)
    u1_t uploaded;
    uint32_t hash;     ///> 10 MSBs contain the age of callsign; 22 LSBs contain hash value
    
} __attribute__((packed)) callsign_hashtable_t;

typedef struct {
    u4_t magic;
    int rx_chan;
    bool init;
    bool debug;
    
    u4_t decode_time;
    ftx_protocol_t protocol;
    char protocol_s[4];
    int have_call_and_grid;
    
    monitor_t mon;
    float slot_period;
    struct tm tm_slot_start;
    int num_samples;
    TYPEREAL *samples;
    bool tsync;
    int in_pos;
    int frame_pos;
    u4_t slot;

    callsign_hashtable_t *callsign_hashtable;
    int callsign_hashtable_size;
    
    ftx_candidate_t candidate_list[kMax_candidates];
    ftx_message_t decoded[kMax_decoded_messages];
    ftx_message_t* decoded_hashtable[kMax_decoded_messages];
} decode_ft8_t;

static decode_ft8_t decode_ft8[MAX_RX_CHANS];

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
    callsign_hashtable_t *ht = ft8->callsign_hashtable;

    for (int idx_hash = 0; idx_hash < CALLSIGN_HASHTABLE_MAX; ++idx_hash, ht++)
    {
        if (ht->callsign[0] != '\0')
        {
            uint8_t age = (uint8_t)(ht->hash >> 22);
            if (age >= max_age)
            {
                LOG(LOG_INFO, "Removing [%.11s] from hash table, age = %d, uploaded = %d\n",
                    ht->callsign, age, ht->uploaded);
                //printf("FT8 hashtable REMOVE age=%d uploaded=%d %.11s\n", age, ht->uploaded, ht->callsign);
                // free the hash entry
                ht->callsign[0] = '\0';
                ht->uploaded = 0;
                ht->hash = 0;
                ft8->callsign_hashtable_size--;
            }
            else
            {
                //printf("FT8 hashtable AGE+1 age=%d uploaded=%d %.11s\n", age, ht->uploaded, ht->callsign);
                // increase callsign age
                ht->hash = (((uint32_t)age + 1u) << 22) | (ht->hash & 0x3FFFFFu);
            }
        }
    }
}

static void hashtable_add(const char* callsign, uint32_t hash, int *hash_idx)
{
    decode_ft8_t *ft8 = (decode_ft8_t *) &decode_ft8[FROM_VOID_PARAM(TaskGetUserParam())];
    uint16_t hash10 = (hash >> 12) & 0x3FFu;
    int idx_hash = (hash10 * 23) % CALLSIGN_HASHTABLE_MAX;
    callsign_hashtable_t *ht = &ft8->callsign_hashtable[idx_hash];
    int wrap_idx = -1;

    while (ht->callsign[0] != '\0')
    {
        if (((ht->hash & 0x3FFFFFu) == hash) && (0 == strncmp(ht->callsign, callsign, 11)))
        {
            #ifdef PR_USE_CALLSIGN_HASHTABLE
                // Don't reset age when using callsign hashtable as PSKReporter upload limiter.
            #else
                // reset age
                ht->hash &= 0x3FFFFFu;
            #endif
            
            if (hash_idx != NULL) *hash_idx = idx_hash;
            LOG(LOG_DEBUG, "Found a duplicate [%s]\n", callsign);
            //printf("hashtable_add DUP idx=%d hash=0x%x <%s>\n", idx_hash, hash, callsign);
            return;
        }
        else
        {
            if (wrap_idx == -1) wrap_idx = idx_hash;
            LOG(LOG_DEBUG, "Hash table clash!\n");
            // Move on to check the next entry in hash table
            idx_hash = (idx_hash + 1) % CALLSIGN_HASHTABLE_MAX;
            ht = &ft8->callsign_hashtable[idx_hash];
            if (idx_hash == wrap_idx) {
                ft8->callsign_hashtable_size--;
                //printf("hashtable_add WRAPPED idx=%d hash=0x%x <%s>\n", idx_hash, hash, callsign);
                break;
            }
        }
    }
    ft8->callsign_hashtable_size++;
    strncpy(ht->callsign, callsign, 11);    // NB: strncpy does zero-fill
    ht->hash = hash;
    if (hash_idx != NULL) *hash_idx = idx_hash;
}

static bool hashtable_lookup(ftx_callsign_hash_type_t hash_type, uint32_t hash, char* callsign)
{
    decode_ft8_t *ft8 = (decode_ft8_t *) &decode_ft8[FROM_VOID_PARAM(TaskGetUserParam())];
    uint8_t hash_shift = (hash_type == FTX_CALLSIGN_HASH_10_BITS) ? 12 : (hash_type == FTX_CALLSIGN_HASH_12_BITS ? 10 : 0);
    uint16_t hash10 = (hash >> (12 - hash_shift)) & 0x3FFu;
    int idx_hash = (hash10 * 23) % CALLSIGN_HASHTABLE_MAX;
    callsign_hashtable_t *ht = &ft8->callsign_hashtable[idx_hash];
    int wrap_idx = -1;

    while (ht->callsign[0] != '\0')
    {
        if (((ht->hash & 0x3FFFFFu) >> hash_shift) == hash)
        {
            strncpy(callsign, ht->callsign, 11);    // NB: strncpy does zero-fill
            return true;
        }
        // Move on to check the next entry in hash table until entire table has been searched.
        // Unless an empty entry is hit which means a match cannot exist
        if (wrap_idx == -1) wrap_idx = idx_hash;
        idx_hash = (idx_hash + 1) % CALLSIGN_HASHTABLE_MAX;
        ht = &ft8->callsign_hashtable[idx_hash];
        if (idx_hash == wrap_idx) {
            //printf("hashtable_lookup WRAPPED hash=0x%x\n", hash);
            break;
        }
    }
    callsign[0] = '\0';
    return false;
}

ftx_callsign_hash_interface_t hash_if = {
    .lookup_hash = hashtable_lookup,
    .save_hash = hashtable_add
};

static void decode(int rx_chan, const monitor_t* mon, int _freqHz)
{
    decode_ft8_t *ft8 = &decode_ft8[rx_chan];

    const ftx_waterfall_t* wf = &mon->wf;
    // Find top candidates by Costas sync score and localize them in time and frequency
    int num_candidates = ftx_find_candidates(wf, kMax_candidates, ft8->candidate_list, kMin_score);

    // Hash table for decoded messages (to check for duplicates)
    int num_decoded = 0, num_spots = 0;

    // Initialize hash table pointers
    for (int i = 0; i < kMax_decoded_messages; ++i)
    {
        ft8->decoded_hashtable[i] = NULL;
    }

    // Go over candidates and attempt to decode messages
    bool need_header = true;
    int limiter = 0;

    for (int idx = 0; idx < num_candidates; ++idx)
    {
        NextTask("candidate");
        const ftx_candidate_t* cand = &ft8->candidate_list[idx];

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
            if (ft8->decoded_hashtable[idx_hash] == NULL)
            {
                LOG(LOG_DEBUG, "Found an empty slot\n");
                found_empty_slot = true;
            }
            else if ((ft8->decoded_hashtable[idx_hash]->hash == message.hash) && (0 == memcmp(ft8->decoded_hashtable[idx_hash]->payload, message.payload, sizeof(message.payload))))
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
            memcpy(&ft8->decoded[idx_hash], &message, sizeof(message));
            ft8->decoded_hashtable[idx_hash] = &ft8->decoded[idx_hash];
            ++num_decoded;

            char text[FTX_MAX_MESSAGE_LENGTH];
            int hash_idx = -1;
            bool need_free = false, uploaded = false;
            char *f[4];
            int n = 0, km = 0;
            ftx_message_rc_t unpack_status = ftx_message_decode(&message, &hash_if, text, &hash_idx);
            if (unpack_status != FTX_MESSAGE_RC_OK && unpack_status != FTX_MESSAGE_RC_PSKR_OK && unpack_status != FTX_MESSAGE_RC_ERROR_TYPE)
            {
                snprintf(text, sizeof(text), "Error [%d] while unpacking!", (int)unpack_status);
            }

            float snr = cand->score * 0.5f + ft8_conf.SNR_adj;
            bool pskr_ok = false;
            int age = 0;
            if (unpack_status == FTX_MESSAGE_RC_PSKR_OK) {
                if (hash_idx < 0 || hash_idx >= CALLSIGN_HASHTABLE_MAX) {
                    printf("FT8 hash_idx=%d ??? <%s>\n", hash_idx, text);
                } else {
                    #ifdef PR_USE_CALLSIGN_HASHTABLE
                        callsign_hashtable_t *ht = &ft8->callsign_hashtable[hash_idx];
                        
                        // n=3: "call_to call_de grid4"  "{<...> DE QRZ CQ CQ_nnn CQ_ABCD} call_de grid4"
                        // n=4: "CQ DX call_de grid4"
                        // n=4: "call_to call_de R grid4"
                        const char *call = NULL, *grid = NULL;
                        need_free = true;
                        for (n = 0; n < 4; n++) f[n] = NULL;
                        n = sscanf(text, "%ms %ms %ms %ms", &f[0], &f[1], &f[2], &f[3]);
                        //printf("FT8 n=%d <%s> | <%s> <%s> <%s> <%s>\n", n, text, f[0], f[1], f[2], f[3]);
                        //ext_send_msg_encoded(rx_chan, false, "EXT", "debug",
                        //    "FT8 n=%d <%s> | <%s> <%s> <%s> <%s>\n", n, text, f[0], f[1], f[2], f[3]);
                        if (n == 3 || n == 4) {
                            pskr_ok = true;
                            grid = f[n-1];
                            if (strcmp(f[n-2], "R") == 0) {
                                call = f[n-3];
                                n = 0;
                            } else {
                                call = f[n-2];
                            }
                        }

                        if (strncmp(ht->callsign, call, 11) != 0) {
                            printf("FT8 ######## UNEXPECTED CALLSIGN IN HASHTABLE hash_idx=%d want <%.11s> got <%.11s>\n", hash_idx, call, ht->callsign);
                        } else {
                            if (ht->uploaded == 0) {
                                if (pskr_ok && limiter <= PR_LIMITER) {
                                    #ifdef PR_TESTING
                                        limiter++;
                                    #endif
                        
                                    if (strlen(call) >= 3 && strlen(grid) == 4) {
                                        // silently ignore incorrect encoding of RR73 (i.e. sent as grid code instead of MAXGRID4+3)
                                        if (strcmp(grid, "RR73") != 0) {
                                            u4_t passband_freq = (u4_t) roundf(freq_hz);
                                            s1_t snr_i = (s1_t) roundf(snr);
                                            #ifdef PR_TESTING
                                            #else
                                                // unless PR_TESTING don't spot if in test mode
                                                if (!ft8_conf.test)
                                            #endif
                                                    {
                                                        km = PSKReporter_spot(rx_chan, call, passband_freq, snr_i, ft8->protocol,
                                                            grid, ft8->decode_time, ft8->slot);
                                                    }
                                            ht->uploaded = 1;
                                            uploaded = true;
                                            num_spots++;
                                        }
                                    } else {
                                        printf("FT8 bad call or grid <%s> <%s> <%s>\n", text, call, grid);
                                    }
                                }
                            } else {
                                km = PSKReporter_distance(grid);
                                age = ht->hash >> 22;
                                //printf("FT8 already uploaded age=%d/%d <%s>\n", age, CALLSIGN_AGE_MAX, text);
                            }
                        }
                    #endif
                }
            }

            if (need_header) {
                ext_send_msg_encoded(rx_chan, false, "EXT", "chars",
                //   22:40:30 +11.0 +2.80 1931 nnnnn  nn i.n  CQ DH1NAS JO50
                    "     UTC   SNR    dT Freq    km age msg  freq: %.2f  mode: %s\n",
                    _freqHz/1e3, ft8->protocol_s);
                need_header = false;
            }
            
            time_sec += ft8_conf.dT_adj;
            
            LOG(LOG_INFO, "%02d%02d%02d %+05.1f %+4.2f %4.0f %5d  %s\n",
                ft8->tm_slot_start.tm_hour, ft8->tm_slot_start.tm_min, ft8->tm_slot_start.tm_sec,
                snr, time_sec, freq_hz, km, text);
            
            char *ks = NULL;
            ks = kstr_asprintf(ks, "%02d:%02d:%02d %+05.1f %+4.2f %4.0f",
                ft8->tm_slot_start.tm_hour, ft8->tm_slot_start.tm_min, ft8->tm_slot_start.tm_sec,
                snr, time_sec, freq_hz);
            ks = kstr_asprintf(ks, (km > 0)? " %5d" : "      ", km);
            ks = kstr_asprintf(ks, (age != 0)? "  %02d" : "    ", age);

            uint8_t i3 = FTX_MSG_I3(&message);
            if (i3)
                ks = kstr_asprintf(ks, " %d.0 ", i3);
            else
                ks = kstr_asprintf(ks, " 0.%d*", FTX_MSG_N3(&message));
                
            if (pskr_ok) {
                char *call_to, *call_to_s = NULL, *call_de, *call_de_s, *grid_de, *grid_de_s;
                if (n == 3)
                    call_to = f[0], call_de = f[1], grid_de = f[2];
                else
                if (n == 4)
                    call_to = NULL, call_de = f[2], grid_de = f[3];
                else
                    call_to = f[0], call_de = f[1], grid_de = f[3];

                // silently ignore incorrect encoding of RR73 (i.e. sent as grid code instead of MAXGRID4+3)
                bool rr73 = (strcmp(grid_de, "RR73") == 0);
                
                // call_to
                if (call_to &&
                    strcmp(call_to, "&lt;...&gt;") != 0 &&
                    strcmp(call_to, "CQ") != 0 &&
                    strncmp(call_to, "CQ ", 3) != 0 &&
                    strcmp(call_to, "DE") != 0 &&
                    strcmp(call_to, "QRZ") != 0)
                    asprintf(&call_to_s, "<a style=\"color:blue\" href=\"https://www.qrz.com/lookup/%s\" target=\"_blank\">%s</a>", call_to, call_to);
                else {
                    if (call_to) call_to_s = strdup(call_to);
                }
                
                // call_de
                asprintf(&call_de_s, "<a style=\"color:blue\" href=\"https://www.qrz.com/lookup/%s\" target=\"_blank\">%s</a>", call_de, call_de);
                asprintf(&grid_de_s, rr73? "(RR73)" : "<a style=\"color:blue\" href=\"http://www.levinecentral.com/ham/grid_square.php?"
                    "Grid=%s\" target=\"_blank\">%s</a>", grid_de, grid_de);

                if (n == 3) {
                    // call_to call_de grid4
                    ext_send_msg_encoded(rx_chan, false, "EXT", "chars",
                        "%s %s%s %s%s %s" NONL, kstr_sp(ks), ft8->debug? "3> ":"", call_to_s, uploaded? GREEN : "", call_de_s, grid_de_s);
                } else
                if (n == 4) {
                    // CQ DX call_de grid4
                    ext_send_msg_encoded(rx_chan, false, "EXT", "chars",
                        "%s %s%s %s %s%s %s" NONL, kstr_sp(ks), ft8->debug? "4> ":"", f[0], f[1], uploaded? GREEN : "", call_de_s, grid_de_s);
                } else {
                    // call_to call_de R grid4
                    ext_send_msg_encoded(rx_chan, false, "EXT", "chars",
                        "%s %s%s %s%s %s %s" NONL, kstr_sp(ks), ft8->debug? "9> ":"", call_to_s, uploaded? GREEN : "", call_de_s, f[2], grid_de_s);
                }
                
                free(call_to_s); free(call_de_s); free(grid_de_s);
            } else {
                ext_send_msg_encoded(rx_chan, false, "EXT", "chars",
                    "%s %s%s\n", kstr_sp(ks), ft8->debug? "0> ":"", text);
            }

            kstr_free(ks);
            if (need_free) for (n = 0; n < 4; n++) free(f[n]);     // NB: free(NULL) is okay
        }
    }
    LOG(LOG_INFO, "Decoded %d messages, callsign hashtable size %d\n", num_decoded, ft8->callsign_hashtable_size);
    if (num_decoded > 0) {
        char *ks = NULL;
        ks = kstr_asprintf(ks, "%02d:%02d:%02d %s decoded %d, ",
            ft8->tm_slot_start.tm_hour, ft8->tm_slot_start.tm_min, ft8->tm_slot_start.tm_sec,
            ft8->protocol_s, num_decoded);
        if (num_spots != 0) {
            ks = kstr_asprintf(ks, "new spots %d, ", num_spots);
        }
        
        static u4_t last_num_uploads[MAX_RX_CHANS];
        u4_t num_uploads = PSKReporter_num_uploads(rx_chan);
        if (num_uploads > last_num_uploads[rx_chan]) {
            u4_t diff = num_uploads - last_num_uploads[rx_chan];
            //printf("FT8 SPOTS %+d num_uploads=%d last_num_uploads=%d\n", diff, num_uploads, last_num_uploads[rx_chan]);
            last_num_uploads[rx_chan] = num_uploads;
            if (diff != 0) {
                ks = kstr_asprintf(ks, YELLOW "uploaded %d spot%s to pskreporter.info" NORM ", ",
                    diff, (diff == 1)? "" : "s");
            }
        } else
        if (num_uploads < last_num_uploads[rx_chan]) {
            //printf("FT8 RESET num_uploads=%d last_num_uploads=%d\n", num_uploads, last_num_uploads[rx_chan]);
            last_num_uploads[rx_chan] = num_uploads;
        }
        
        ks = kstr_asprintf(ks, "hashtable %d%%",
            ft8->callsign_hashtable_size * 100 / CALLSIGN_HASHTABLE_MAX);
        ext_send_msg_encoded(rx_chan, false, "EXT", "chars", "%s\n", kstr_sp(ks));
        kstr_free(ks);
    }

    if (ft8->tm_slot_start.tm_sec == 0) {
        hashtable_cleanup(rx_chan, CALLSIGN_AGE_MAX);
    }
}

void decode_ft8_setup(int rx_chan, int debug)
{
    decode_ft8_t *ft8 = &decode_ft8[rx_chan];
    TaskSetUserParam(TO_VOID_PARAM(rx_chan));
    ft8->debug = debug;
    int have_call_and_grid = PSKReporter_setup(rx_chan);
    if (have_call_and_grid != 0) ft8->have_call_and_grid = have_call_and_grid;
}

void decode_ft8_samples(int rx_chan, TYPEMONO16 *samps, int nsamps, int _freqHz, u1_t *start_test)
{
    decode_ft8_t *ft8 = &decode_ft8[rx_chan];
    if (!ft8->init) return;

    if (!ft8->tsync) {
        const float time_shift = 0.8;
        struct timespec spec;
        clock_gettime(CLOCK_REALTIME, &spec);
        double time_sec = (double) spec.tv_sec + (spec.tv_nsec / 1e9);
        double time_within_slot = fmod(time_sec - time_shift, ft8->slot_period);
        if (time_within_slot > ft8->slot_period / 4) {
            *start_test = 0;
            return;     // wait for beginning of slot
        }

        time_t time_slot_start = (time_t) (time_sec - time_within_slot);
        gmtime_r(&time_slot_start, &ft8->tm_slot_start);
        LOG(LOG_INFO, "FT8 Time within slot %02d:%02d:%02d %.3f s\n", ft8->tm_slot_start.tm_hour,
            ft8->tm_slot_start.tm_min, ft8->tm_slot_start.tm_sec, time_within_slot);
        ft8->in_pos = ft8->frame_pos = 0;
        *start_test = 1;
        ft8->decode_time = spec.tv_sec;
        ft8->slot++;
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
    
    *start_test = 0;
    LOG(LOG_DEBUG, "FT8 Waterfall accumulated %d symbols\n", ft8->mon.wf.num_blocks);
    LOG(LOG_INFO, "FT8 Max magnitude: %.1f dB\n", ft8->mon.max_mag);

    // Decode accumulated data (containing slightly less than a full time slot)
    NextTask("decode");
    decode(rx_chan, &ft8->mon, _freqHz);

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
    ft8->protocol = protocol;
    float slot_period = ((protocol == FTX_PROTOCOL_FT8) ? FT8_SLOT_TIME : FT4_SLOT_TIME);
    sprintf(ft8->protocol_s, "FT%d", (protocol == FTX_PROTOCOL_FT8)? 8:4);
    ft8->slot_period = slot_period;
    int sample_rate = snd_rate;
    int num_samples = slot_period * sample_rate;
    ft8->samples = (TYPEREAL *) malloc(num_samples * sizeof(TYPEREAL));

    // active period
    num_samples = (slot_period - 0.4f) * sample_rate;
    ft8->num_samples = num_samples;

    // Compute FFT over the whole signal and store it
    monitor_config_t mon_cfg = {
        .f_min = FT8_PASSBAND_LO,
        .f_max = FT8_PASSBAND_HI,
        .sample_rate = sample_rate,
        .time_osr = kTime_osr,
        .freq_osr = kFreq_osr,
        .protocol = protocol
    };

    hashtable_init(rx_chan);
    monitor_init(&ft8->mon, &mon_cfg);
    LOG(LOG_DEBUG, "FT8 Waterfall allocated %d symbols\n", ft8->mon.wf.max_blocks);
    ft8->tsync = false;
    PSKReporter_reset(rx_chan);
    ft8->init = true;
}

void decode_ft8_free(int rx_chan)
{
    decode_ft8_t *ft8 = &decode_ft8[rx_chan];
    ft8->init = false;
    free(ft8->samples);
    ft8->samples = NULL;
    free(ft8->callsign_hashtable);
    ft8->callsign_hashtable = NULL;
    monitor_free(&ft8->mon);
}

void decode_ft8_protocol(int rx_chan, u64_t _freqHz, int proto)
{
    decode_ft8_t *ft8 = &decode_ft8[rx_chan];
    decode_ft8_free(rx_chan);
    decode_ft8_init(rx_chan, proto);
    ext_send_msg_encoded(rx_chan, false, "EXT", "chars",
        "-------------------------------------------------------  new freq %.2f mode %s\n",
        (double) _freqHz / 1e3, ft8->protocol_s);
}
