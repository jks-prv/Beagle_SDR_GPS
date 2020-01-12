#include "audiosuperframe.h"

/*
 * 5.1 The encoded audio is composed into audio super frames of constant length.
 * 5.1.1 xHE-AAC
 * One audio super frame is always placed in one DRM logical frame in robustness
 * modes A, B, C and D and in two logical frames in robustness mode E (see clause 6).
 * In this way no additional synchronization is needed for the audio coding.
 * Retrieval of frame boundaries is also taken care of within the audio super frame.
 *
 * 5.1.2 AAC
 * Transform length: the transform length is 960 to ensure that one audio frame
 * corresponds to 80 ms or 40 ms (robustness modes A, B, C and D) or to 40 ms or
 * 20 ms (robustness mode E) in time. This is required to allow the combination
 * of an integer number of audio frames to build an audio super frame of 400 ms (robustness
 * modes A, B, C and D) or 200 ms (robustness mode E) duration.
 *
 * Audio super framing: 5 or 10 audio frames are composed into one audio super frame.
 * For robustness modes A, B, C and D, the respective sampling rates are 12 kHz
 * and 24 kHz producing an audio super frame of 400 ms duration; for robustness
 * mode E, the respective sampling rates are 24 kHz and 48 kHz producing an audio super
 * frame of 200 ms duration. The audio frames in the audio super frames are encoded
 * together such that each audio super frame is of constant length, i.e. that bit
 * exchange between audio frames is only possible within an audio super frame.
 * One audio super frame is always placed in one logical frame in robustness
 * modes A, B, C and D and in two logical frames in robustness mode E (see clause 6).
 * In this way no additional synchronization is needed for the audio coding.
 * Retrieval of frame boundaries and provisions for UEP are also taken care of within the audio super frame.
 * UEP: better graceful degradation and better operation at higher BERs is achieved
 * by applying UEP to the AAC bit stream. Unequal error protection is realized via
 * the multiplex/coding units. For robustness mode E, the length of the higher protected part of
 * an audio super frame shall be a multiple of 2 bytes.
 *
 * 5.2
 *
 *    audio_super_frame(audio_info) {
 *        switch (audio_info.audio_coding) {
 *        case xHE-AAC:
 *            xhe_aac_super_frame(audio_info);
 *            break;
 *        case AAC:
 *            aac_super_frame(audio_info);
 *            break;
 *        }
 *    }
 */

AudioSuperFrame::AudioSuperFrame():audioFrame()
{

}

AudioSuperFrame::~AudioSuperFrame()
{
}

