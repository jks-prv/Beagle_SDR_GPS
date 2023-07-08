/*
 * cw_decoder.h
 *
 *  Created on: 07.09.2017
 *      Author: danilo
 */

#ifndef AUDIO_CW_CW_DECODER_H_
#define AUDIO_CW_CW_DECODER_H_

#include "kiwi.h"
#include "datatypes.h"

#include <stdint.h>
#include <math.h>

typedef float float32_t;

#define CW_DECODER_BLOCKSIZE_MIN		8
#define CW_DECODER_BLOCKSIZE_MAX		128

// @ srate 12 kHz gives 136.36 blk/s and bin size, 7.3 ms/blk
#define CW_DECODER_BLOCKSIZE_DEFAULT	88

void CwDecode_RxProcessor(int rx_chan, int chan, int nsamps, TYPEMONO16 *samps);
void CwDecode_Init(int chan, int wpm = 0, int training_interval = 100);
void CwDecode_pboff(int rx_chan, u4_t pboff);
void CwDecode_wsc(int rx_chan, int wsc);
void CwDecode_threshold(int rx_chan, int type, int thresh);
void CwDecode_wpm(int rx_chan, int wpm, int training_interval);

#endif /* AUDIO_CW_CW_DECODER_H_ */
