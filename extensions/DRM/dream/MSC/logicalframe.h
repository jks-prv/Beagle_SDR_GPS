#ifndef LOGICALFRAME_H
#define LOGICALFRAME_H

/*
 * 3.1
 * logical frame: data contained in one stream during 400 ms or 100 ms
 * 5.2 The channel coding of DRM is performed on logical frames of constant bit rate for any given combination of parameters.
 * 6.2.2
 * The MSC contains between one and four streams. Each stream is divided into logical frames. Audio streams comprise compressed
 * audio and optionally they can carry text messages. Data streams may be composed of data packets, carrying information
 * for up to four "sub-streams". An audio service comprises one audio stream and optionally one to four data streams or
 * data sub-streams. A data service comprises one data stream or data sub-stream.
 *
 * Each logical frame generally consists of two parts, each with its own protection level. The lengths of the two parts
 * are independently assigned. Unequal error protection for a stream is provided by setting different protection levels to the two parts.
 *
 * For robustness modes A, B, C and D, the logical frames are each 400 ms long. If the stream carries audio, the logical frame
 * carries the data for one audio super frame.
 *
 * For robustness mode E, the logical frames are each 100 ms long. If the stream carries audio, the logical frame carries the data for either
 * the first or the second part of one audio super frame containing the audio information for 200 ms duration. Since, in general, the stream
 * may be assigned two protection levels, the logical frames carry precisely half of the bytes from each protection level.
 * The logical frames from all the streams are mapped together to form multiplex frames of the same duration, which are passed to the
 * channel coder. In some cases, the first stream may be carried in logical frames mapped to hierarchical frames.
 *
 * 6.5.1
 * The text message (when present) shall occupy the last four bytes of the lower protected part of each logical frame
 * carrying an audio stream. The message is divided into a number of segments and UTF-8 character coding is used.
 * The beginning of each segment of the message is indicated by setting all four bytes to the value 0xFF.

 */

class LogicalFrame
{
public:
    LogicalFrame();
};

#endif // LOGICALFRAME_H
