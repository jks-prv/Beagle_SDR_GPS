#pragma once

typedef struct ImaState {
   int index;    		// Index into step size table
   int previousValue;	// Most recent sample value
   int pos_clamp, neg_clamp;
} ima_adpcm_state_t;

void encode_ima_adpcm_i16_e8(short* input, unsigned char* output, int input_length, ima_adpcm_state_t *state);
void encode_ima_adpcm_u8_e8(unsigned char* input, unsigned char* output, int input_length, ima_adpcm_state_t *state);

void decode_ima_adpcm_e8_i16(unsigned char* input, short* output, int input_length, ima_adpcm_state_t *state);
void decode_ima_adpcm_e8_u8(unsigned char* input, unsigned char* output, int input_length, ima_adpcm_state_t *state);
