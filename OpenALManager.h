
#if !defined(_OPENALMANAGER_H_)
#define _OPENALMANAGER_H_

#include "RingBuffer.h"
#include "al.h"
#include "alc.h"
// On Linux, this Requires that alut-dev be installed:
#include "alut.h"
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
class OpenALManager : public AudioBufferInterface
{
public:
    // Non-virtual methods
	OpenALManager(int numBuffers);
	~OpenALManager();
	void Enumerate( void (*callback)(const char *) );
	/// Get individual secondary buffer sample rate.
	unsigned int GetSampleRate(int channel);
    bool Init(void *parentWindow = NULL, int* soundCard = NULL, const char *name = NULL);
	bool CreateCaptureBuffer( AudioRecordingCallback * recordCallback = NULL, int* soundCard = NULL, const char *name = NULL);

	int MonitorCaptureBuffer();
    bool CheckALError( void );
    bool CheckALCError( ALCdevice* device );

    virtual bool UnInit();
	virtual bool Play();
	virtual bool Stop();
	virtual bool Stop( int channel );
	virtual bool StartCapture();
	virtual bool StopCapture();
	virtual bool SetSampleRate( int channel, unsigned int frequency );
	virtual bool SetRecordSampleRate( unsigned int sampleRate );
	virtual bool IsBufferPlaying( int channel );
	// Get global playback buffer sample rate.
	virtual unsigned int GetSampleRate();
	virtual unsigned int GetRecordSampleRate();
	virtual void SetVolume( int channel, int volume );
    virtual void SetMasterVolume( int volume );
	virtual int GetVolume( int channel );
	virtual void SetPan( int channel, int pan );
	virtual int GetPan( int channel );
	virtual bool DeleteCaptureBuffer();
	virtual bool FillBuffer( int channel, unsigned char *data, int length, int sampleRate );
    bool EmptyBuffer( int channel );
	virtual bool FillBufferSilence( int channel, int length );
	virtual void SetBufferLatency( int msec );
	virtual int GetNumSamplesQueued( int channel );
    virtual void* Entry();
    int GetPeak( int channel );
	int GetWriteBytesAvailable(int channel);
private:
    // Virtual private methods.
	/// Take the sound data and forward it when we have a notification.
	virtual bool ProcessCapturedData();
	/// Check whether we need to fill the buffer with silence.
	virtual void MonitorBuffer();

    // Non-virtual private methods and data
    /// This function takes our secondary buffers and mixes them into a single stream to feed to the primary buffer.
	bool ProcessSoundBuffer();
    // OpenAL source to send audio data to.
    ALuint _playbackHandle; 
    ALuint _playbackBuffers[2];
    ALCcontext* _context;
    ALCdevice* _device;
    ALCdevice* _captureDevice;
	unsigned int _playbackByteAlign;
	int _playbackFrames;
	bool _capturing;
	char * _captureBuffer;
	std::vector<SecondaryBuffer *> _secondaryBuffers;
	/// Number of bytes per sample, may be expanded to cover more formats.
    int _format;
	/// We may need to add some variables to track our buffer playing.
	//int XrunRecover( snd_pcm_t* handle, int err );
	bool MixAudio(ALuint workingBuffer);
	void CalculateChannelVolume(double* leftVolumeAdjustment, double* righVolumeAdjustment);
	int ResampleChunk( unsigned char* channelData, int channelNumber, int bytesRead, int bytesRequested);
    virtual bool Play( int channel );
	void RestartBufferIfNecessary( void );
};

#endif // _OPENALMANAGER_H_

