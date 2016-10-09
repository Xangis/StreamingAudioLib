#if !defined(_SECONDARYBUFFER_H_)
#define _SECONDARYBUFFER_H_
#include "Resampler.h"
#include "RingBuffer.h"
#include "wx/thread.h"
//#include "System/Thread/CriticalSection.h"

/**
     @brief     A struct that represents a single mono channel for a secondary audio buffer.
     This struct contains the data necessary to keep track of a single mono channel of audio
     data.  It includes a ring buffer to hold the data and information such as volume, pan,
     and sample rate settings.  This is intended to be an equivalent to a DirectSound
     secondary buffer.
     @note      The secondary buffer is not itself thread-safe.  Users are required to lock
     the included mutex whenever necessary.
*/
class SecondaryBuffer
{
public:
	SecondaryBuffer() {};
	~SecondaryBuffer() {};
    unsigned int _sampleRate;
    int _volume;
    int _pan;
    unsigned int _bytesPerSample;  // Should default to 2.
    /// This is [buffer latency] x [samplerate] x [bytes per sample] and is the chunk size used for buffer writes and reads.
    /// typically this will be 800 for 8KHz and 4410 for 44.1KHz
    unsigned int _chunkSize;
    bool _isPlaying;
    RingBuffer* _bufferData;
    wxMutex* _mutex;
    int _peak;
    Resampler _resampler; /**< Allows sample rate conversion */
};

#endif
