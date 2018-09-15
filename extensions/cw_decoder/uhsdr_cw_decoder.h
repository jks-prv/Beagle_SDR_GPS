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

#define DEMOD_CW 0
#define TRX_MODE_TX 0

#define CW_DECODER_BLOCKSIZE_MIN		8
#define CW_DECODER_BLOCKSIZE_MAX		128
#define CW_DECODER_BLOCKSIZE_DEFAULT	88

#define CW_DECODER_THRESH_MIN			1000
#define CW_DECODER_THRESH_MAX			50000
#define CW_DECODER_THRESH_DEFAULT		32000


void CwDecode_RxProcessor(int rx_chan, int chan, int nsamps, TYPEMONO16 *samps);
//void CwDecode_RxProcessor(float32_t * const src, int16_t blockSize);
void CwDecode_Init(int chan);
void CwDecode_pboff(int rx_chan, u4_t pboff);

#endif /* AUDIO_CW_CW_DECODER_H_ */
