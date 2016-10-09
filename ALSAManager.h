#if !defined(_ALSAMANAGER_H_)
#define _ALSAMANAGER_H_

#if !defined( WIN32 )

#include "System/Thread/Thread.h"
#include "System/Thread/CriticalSection.h"
#include "Util/RingBuffer/RingBuffer.h"
#include <alsa/asoundlib.h>
#include <vector>

#include "AudioRecordingCallback.h"
#include "SecondaryBuffer.h"
#include "AudioBufferInterface.h"

/** 
     @brief     A wrapper for the ALSA audio library that implements DirectSound-like functionality.
     This is a wrapper for the ALSA audio library that implements DirectSound-like functionality,
     including the mixing and resampling of any number of secondary buffers into a primary playback
     buffer.  The number of secondary buffers is passed to the constructor of ALSAManager.
     @note      [class note]
*/
class ALSAManager : public AudioBufferInterface
{
public:
    // Non-virtual methods
	ALSAManager(int numBuffers);
	~ALSAManager();
	void Enumerate( void (*callback)(char *) );
	bool Init(void *parentWindow = NULL, int* soundCard = NULL, const char *name = NULL);
	/// Get individual secondary buffer sample rate.
	unsigned int GetSampleRate(int channel);
	bool CreateCaptureBuffer( AudioRecordingCallback * recordCallback = NULL, int* soundCard = NULL, const char *name = NULL);
	int MonitorCaptureBuffer();

    virtual bool UnInit();
	virtual bool Play();
	virtual bool Play( int channel );
	virtual bool Stop();
	virtual bool Stop( int channel );
	virtual bool StartCapture();
	virtual bool StopCapture();
	virtual bool SetSampleRate( int channel, int frequency );
	virtual bool SetRecordSampleRate( unsigned int sampleRate );
	virtual bool IsBufferPlaying( int channel );
	/// Get global playback buffer sample rate.
	virtual unsigned int GetSampleRate();
	virtual unsigned int GetRecordSampleRate();
	virtual void SetVolume( int channel, int volume );
    virtual void SetMasterVolume( int volume );
	virtual int GetVolume( int channel );
	virtual void SetPan( int channel, int pan );
	virtual int GetPan( int channel );
	virtual bool DeleteCaptureBuffer();
	virtual bool FillBuffer( int channel, unsigned char *data, int length, int sampleRate );
	virtual bool FillBufferSilence( int channel, int length );
	virtual void SetBufferLatency( int msec );
    virtual int run();
        int GetPeak( int channel );
private:
    // Virtual private methods.
	/// Take the sound data and forward it when we have a notification.
	virtual bool ProcessCapturedData();
	/// Check whether we need to fill the buffer with silence.
	virtual void MonitorBuffer();

    // Non-virtual private methods and data
    /// This function takes our secondary buffers and mixes them into a single stream to feed to the primary buffer.
	bool ProcessSoundBuffer();
    snd_pcm_t * _playbackHandle;
	snd_pcm_t * _captureHandle;
	unsigned int _playbackByteAlign;
	int _playbackFrames;
	bool _capturing;
	char * _captureBuffer;
	std::vector<SecondaryBuffer *> _secondaryBuffers;
	/// We may need to add some variables to track our buffer playing.
	int XrunRecover( snd_pcm_t* handle, int err );
};

#endif // !WIN32

#endif // _ALSAMANAGER_H_

