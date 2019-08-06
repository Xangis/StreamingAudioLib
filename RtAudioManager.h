
#if !defined(_RTAUDIOMANAGER_H_)
#define _RTAUDIOMANAGER_H_

#include "RingBuffer.h"
#include "RtAudio.h"
// On Linux, this Requires that alut-dev be installed:
#include <vector>

#include "AudioRecordingCallback.h"
#include "SecondaryBuffer.h"
#include "AudioBufferInterface.h"

class RtAudioManager : public AudioBufferInterface
{
public:
    // Non-virtual methods
	RtAudioManager(int numBuffers);
	~RtAudioManager();
	void Enumerate( void (*callback)(const char *) );
	/// Get individual secondary buffer sample rate.
	unsigned int GetSampleRate(int channel);
    bool Init(void *parentWindow = NULL, int* soundCard = NULL, const char *name = NULL);
	bool CreateCaptureBuffer( AudioRecordingCallback * recordCallback = NULL, int* soundCard = NULL, const char *name = NULL);

	int MonitorCaptureBuffer();

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
    // RtAudio source to send audio data to.
    RtAudio* _audio;
	unsigned int _playbackByteAlign;
	int _playbackFrames;
	bool _capturing;
	char * _captureBuffer;
	std::vector<SecondaryBuffer *> _secondaryBuffers;
	/// Number of bytes per sample, may be expanded to cover more formats.
    int _format;
	/// We may need to add some variables to track our buffer playing.
	//int XrunRecover( snd_pcm_t* handle, int err );
	//bool MixAudio(ALuint workingBuffer);
	void CalculateChannelVolume(double* leftVolumeAdjustment, double* righVolumeAdjustment);
	int ResampleChunk( unsigned char* channelData, int channelNumber, int bytesRead, int bytesRequested);
    virtual bool Play( int channel );
	void RestartBufferIfNecessary( void );
};

#endif // _RTAUDIOMANAGER_H_

