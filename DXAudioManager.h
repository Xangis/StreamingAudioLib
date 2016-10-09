
#if !defined(_DXAUDIOMANAGER_H_)
#define _DXAUDIOMANAGER_H_

#ifdef WIN32

#include <dsound.h>
//#include <dmusici.h>
//#include <dmksctrl.h>
//#include <dmusicc.h>
//#include <dmusicf.h>
#include <vector>
//#include <windows.h>
//#include <mmsystem.h>
//#include <mmreg.h>
#include "AudioBufferInterface.h"
//#include "System/Thread/CriticalSection.h"
#include "Resampler.h"

/** 
     @brief DirectSound Wrapper Class 
 
     DXAudioManager: A wrapper for the DirectX Audio library for general
	 use with WIN32.  Construction argument is the number of secondary
	 buffers to create.

	 If the DXAudioManager will be used for recording, the class to receive
	 the data will need to derive from AudioRecordingCallback and add
	 code to handle the data once it has been pulled from the capture
	 buffer.  This is not necessary for playback-only use.

     @note 
     The interface for this should be kept in sync with the ALSAManager
	 class as best as possible in order to keep them interchangeable.
*/

// TODO: Make this a user-configurable option if it will improve playback quality.
// 2 seconds of 44.1 mono: 44100 * 2
#define SECONDARY_BUFFER_SIZE 32768

class DXAudioManager : public AudioBufferInterface
{
public:
    // Non-virtual Methods
	DXAudioManager(int numBuffers);
	void Enumerate( LPDSENUMCALLBACK callback );
	virtual ~DXAudioManager();
	bool Init(void *parentWindow, GUID guid = GUID_NULL, const char *name = NULL);
	bool IsBufferCapturing( int channel );
	bool CreateCaptureBuffer( AudioRecordingCallback * recordCallback = NULL, GUID guid = GUID_NULL, const char *name = NULL );

    // Virtual Methods
	virtual bool UnInit();
	virtual bool Play();
	virtual bool Play( int channel );
	virtual bool Stop();
	virtual bool Stop( int channel );
	virtual bool StartCapture( );
	virtual bool StopCapture( );
	virtual bool SetSampleRate( int channel, int frequency );
	virtual bool SetRecordSampleRate( unsigned int sampleRate );
	virtual bool IsBufferPlaying( int channel );
	virtual unsigned int GetSampleRate( int channel );
	virtual unsigned int GetRecordSampleRate();
	virtual void SetVolume( int channel, int volume );
    virtual void SetMasterVolume( int volume );
	virtual int GetVolume( int channel );
	virtual void SetPan( int channel, int pan );
	virtual int GetPan( int channel );
	virtual bool DeleteCaptureBuffer();
	virtual bool FillBuffer(int channel, unsigned char *data, int length, int sampleRate );
	virtual bool FillBufferSilence(int channel, int length );
	virtual void SetBufferLatency( int msec );
	virtual int GetNumSamplesQueued( int channel );
	virtual int run();
private:
    // Virtual private methods
	// Take the sound data and forward it when we have a notification.
    virtual bool ProcessCapturedData();
	// Check whether we need to fill the buffer with silence.
	virtual void MonitorBuffer();
    
    // Non-virtual private methods and data
    LPDIRECTSOUND						_pDS;
	std::vector<LPDIRECTSOUNDBUFFER *>	_pDSSecondaryBuffers;
	LPDIRECTSOUNDCAPTURE				_pDSCapture;
	LPDIRECTSOUNDCAPTUREBUFFER			_pDSCaptureBuffer;
	LPDIRECTSOUNDNOTIFY					_pDSNotify;
	//IDirectMusicLoader8*				_pDSLoader;
	DSBPOSITIONNOTIFY*					_notificationPositions;
	HANDLE								_hNotificationEvent;
	HWND								_parentWindow;
	GUID								_guid; 	/*< Sound card to create playback buffers for. */
	GUID								_captureGuid; /*< Sound card to create recording buffers for (may differ from playback card). */

	DWORD								_dwNextCaptureOffset;
	// Buffer position tracking variables.
    std::vector<unsigned long *> _bufferPlayCursor;
	std::vector<int *> _locPointer; /*< For tracking amount of bytes played. */
	std::vector<int *> _soundLength; /*< For tracking amount of bytes in buffer. */
	std::vector<bool *> _isPlaying;
	std::vector<bool *> _needsData; /*< Set when the buffer does not have enough data and needs to be filled with silence. */
    std::vector<wxMutex *> _fillBufferMutex; /*< Mutex for use with fillbuffer, one per channel. */
    std::vector<Resampler *> _resampler;
    Resampler _captureResampler;
};

#endif // WIN32

#endif // _DXAUDIOMANAGER_H_

