#ifndef _AUDIOINTERFACE_H_
#define _AUDIOINTERFACE_H_

#include "wx/thread.h"
#include "wx/wx.h"
#include "AudioRecordingCallback.h"
#include "Resampler.h"
#include "AudioSample.h"
#include <list>

using namespace std;

// Constants
//
#define BITS_PER_BYTE 8
#define BITS_PER_WORD 16
#define BYTES_PER_WORD 2
#define MONO 1
#define STEREO 2

/**
 @brief Interface for an audio engine.  Derive from this to create an audio
 engine compliant interface.

 For audio APIs that implement the functionality described here, functions
 will be no more than wrappers.  For lower-level APIs, such as ALSA, these
 functions give us the opportunity to implement the necessary higher-level
 functionality.
*/
class AudioBufferInterface : public wxThread
{
public:
    AudioBufferInterface();
    virtual ~AudioBufferInterface();
	virtual bool UnInit() = 0;
	virtual bool FillBuffer( int channel, unsigned char *data, int length, int sampleRate ) = 0; // Data length is in bytes, not samples.
	virtual bool FillBufferInterleaved(int channel, unsigned char* data, int length, int sampleRate, int blockSize, int everyNthBlock, int initialOffsetBytes);
	virtual bool FillBufferSilence( int channel, int length ) = 0;
	virtual bool Play() = 0;
	virtual bool Stop() = 0;
	virtual bool Stop( int channel ) = 0;
	virtual bool StartCapture( ) = 0;
	virtual bool StopCapture( ) = 0;
	virtual bool SetSampleRate( int channel, unsigned int frequency ) = 0;
	virtual bool SetRecordSampleRate( unsigned int sampleRate ) = 0;
    virtual bool IsBufferPlaying( int channel ) = 0;
	virtual unsigned int GetSampleRate( int channel ) = 0;
	virtual unsigned int GetRecordSampleRate() = 0;
	virtual void SetVolume( int channel, int volume ) = 0;
    virtual void SetMasterVolume( int volume ) = 0;
	virtual int GetVolume( int channel ) = 0;
	virtual void SetPan( int channel, int pan ) = 0;
	virtual int GetPan( int channel ) = 0;
	virtual bool DeleteCaptureBuffer() = 0;
	virtual void SetBufferLatency( int msec ) = 0;
	virtual int GetNumSamplesQueued( int channel ) = 0;

    // From DSSystem::Thread
	virtual void* Entry() = 0;
private:
	virtual bool Play( int channel ) = 0;
	virtual void MonitorBuffer() = 0;
	virtual bool ProcessCapturedData() = 0;
protected:
	std::list<std::string> _playbackSamples;
	int _numBuffers;
	bool _inited;
	bool _captureInited;
	AudioRecordingCallback* _recordingCallback;
	unsigned int _captureSampleRate;
	unsigned int _captureChunkSize;
	unsigned int _captureChunkTotal;
	unsigned int _playbackSampleRate;
	double _bufferLatency; /**< Latency, in seconds, of our buffers [total latency will be x2 - once for secondary and once for primary] */
	unsigned int _recordBufferLength;
    int _masterVolume;
    Resampler _recordResampler;
};

#endif

