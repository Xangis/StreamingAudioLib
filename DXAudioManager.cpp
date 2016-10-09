
#ifdef WIN32

#include "DXAudioManager.h"
//#include "Dxerr.h"
#define SAFE_RELEASE(p) { if(p) { (p)->Release(); (p)=NULL; } }
#define CAPTURE_CHUNK_SIZE 1400

/**
 @brief  Constructor, initializes values.
*/
DXAudioManager::DXAudioManager(int numBuffers )
{
    // Limit to 64 buffers
    if( numBuffers > 64 )
    {
        numBuffers = 64;
    }

	_pDS = NULL;
	_guid = GUID_NULL;
	_captureGuid = GUID_NULL;
	_numBuffers = numBuffers;
	_pDSCapture = NULL;
	_pDSCaptureBuffer = NULL;
	_pDSNotify = NULL;
	_dwNextCaptureOffset = 0;
	//_pDSLoader = NULL;
    _captureSampleRate = 44100;

	// TODO: Make this configurable.
	/// Record buffer = number of chunks in buffer * size in bytes of each chunk
	/// Note that at 16-bit, 8000 Hz mono, 20 PDUs = 1 second of data.
    _captureChunkSize = _captureSampleRate * _bufferLatency; // Calculated in samples.
    if( _captureChunkSize > (CAPTURE_CHUNK_SIZE / BYTES_PER_WORD ) ) // Compared in samples.
    {
        _captureChunkSize = (CAPTURE_CHUNK_SIZE / BYTES_PER_WORD ); // Calculated in samples.
    }
	_captureChunkSize &= ~1;
	_captureChunkTotal = 20;
	/// 44100, 0.05s, 2 bytes, 20 chunks = 88200 bytes.
	_recordBufferLength = _captureChunkSize * _captureChunkTotal * BYTES_PER_WORD;

	_notificationPositions = new DSBPOSITIONNOTIFY[_captureChunkTotal];

    /// Initialize COM
	HRESULT hr;
    if( FAILED( hr = CoInitialize( NULL )) )
	{
		wxMessageBox( _("Unable to initialize COM"), _("DirectSound Error"), wxOK );
	}

	/// Above normal thread priority so we can monitor the sound buffer a little better.
	if( wxThread::Create(wxTHREAD_JOINABLE) != wxTHREAD_NO_ERROR )
	{
		wxMessageBox( _("Unable to create playlist thread"), _("ERROR"), wxOK );
		return;
	}
	SetPriority(75);
	Run();
}

/**
 @brief  Destructor.
*/
DXAudioManager::~DXAudioManager()
{
	if( _inited )
	{
		UnInit();
	}
	delete[] _notificationPositions;
}

/**
 @brief  Create a list of the soundcards in the system.
*/
void DXAudioManager::Enumerate( LPDSENUMCALLBACK callback )
{
	// Enumerate sound devices so we can choose one on init.
	DirectSoundEnumerate(callback, NULL);
	// We may also want to call DirectSoundCaptureEnumerate to choose the output device.
}

/**
 @brief  Initializes the DXAudioManager.  Creates and sets up buffers.
 @note
 Name is not used here.  It is just included to make this function look more like the ALSA version.
*/
bool DXAudioManager::Init(void * parentWindow, GUID guid, const char *)
{
	_parentWindow = (HWND)parentWindow;
    HRESULT				hr;
	LPDIRECTSOUNDBUFFER primaryBuffer = NULL;
	int count;

	_guid = guid;

	/// Create IDirectSound using the primary sound device
    if( FAILED( hr = DirectSoundCreate( &_guid, &_pDS, NULL ) ) )
    {
		MessageBox( NULL, DXGetErrorString(hr), _("DirectSoundCreate Error"), MB_OK );
        return false;
    }

    /// Set coop level to DSSCL_PRIORITY
    if( FAILED( hr = _pDS->SetCooperativeLevel( _parentWindow, DSSCL_PRIORITY ) ) )
    {
		MessageBox( NULL, DXGetErrorString(hr),_("DirectSound SetCooperativeLevel Error"), MB_OK );
        return false;
    }

	/// Setup the DSBUFFERDESC structure
	DSBUFFERDESC dsbd;
	ZeroMemory(&dsbd, sizeof(DSBUFFERDESC));
	dsbd.dwSize = sizeof(DSBUFFERDESC);
	/// CreateSoundBuffer does not like DSBCAPS_GLOBALFOCUS, DSBCAPS_CTRLVOLUME, DSBCAPS_CTRLPAN, and DSBCAPS_CTRLFREQUENCY
	/// in the primary buffer - we set those in the secondary.
	dsbd.dwFlags       = DSBCAPS_PRIMARYBUFFER;
	dsbd.dwBufferBytes = 0;
	dsbd.lpwfxFormat = 0;

    if( FAILED( hr = _pDS->CreateSoundBuffer( &dsbd, &primaryBuffer, NULL ) ) )
    {
		MessageBox( NULL, DXGetErrorString(hr), _("DirectSound CreateSoundBuffer Error"), MB_OK );
        return false;
    }

	WAVEFORMATEX wfx;
    ZeroMemory( &wfx, sizeof(WAVEFORMATEX) );
	wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nSamplesPerSec = _playbackSampleRate;
    wfx.wBitsPerSample = BITS_PER_WORD;
    wfx.nChannels = STEREO;
    wfx.nBlockAlign = STEREO * BYTES_PER_WORD;
    wfx.nAvgBytesPerSec = wfx.nBlockAlign * wfx.nSamplesPerSec;
    dsbd.lpwfxFormat = &wfx;

    if( FAILED( hr = primaryBuffer->SetFormat(&wfx) ) )
    {
		MessageBox( NULL, DXGetErrorString(hr), _("DirectSound SetFormat Error"), MB_OK );
	    return false;
    }

	/// This doesn't delete the buffer, it just frees up the interface created in CreateSoundBuffer.
	SAFE_RELEASE( primaryBuffer );

	/// Set options for secondary buffers
	//dsbd.dwFlags = DSBCAPS_CTRLPAN | DSBCAPS_CTRLVOLUME | DSBCAPS_CTRLFREQUENCY | DSBCAPS_GLOBALFOCUS;
	dsbd.dwFlags = DSBCAPS_CTRLPAN | DSBCAPS_CTRLVOLUME | DSBCAPS_GLOBALFOCUS | DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_STATIC;
	dsbd.dwBufferBytes = SECONDARY_BUFFER_SIZE;
	wfx.nSamplesPerSec = 44100;
	wfx.nChannels = MONO;
	wfx.nBlockAlign = MONO * BYTES_PER_WORD;
	wfx.nAvgBytesPerSec = wfx.nBlockAlign * wfx.nSamplesPerSec;

	/// Create a streaming secondary buffer for each channel.
	for( count = 0; count < _numBuffers; count++ )
	{
		LPDIRECTSOUNDBUFFER * buffer;
		buffer = new LPDIRECTSOUNDBUFFER;
		hr = _pDS->CreateSoundBuffer( &dsbd, buffer, NULL);
		{
			if( FAILED( hr ))
			{
				MessageBox( NULL, DXGetErrorString(hr), _("DirectSound Error"), MB_OK );
				return false;
			}
		}
		_pDSSecondaryBuffers.push_back( buffer );
		_bufferPlayCursor.push_back( new unsigned long );
		*_bufferPlayCursor[count] = 0;
		_locPointer.push_back( new int );
		*_locPointer[count] = 0;
		_soundLength.push_back( new int );
		*_soundLength[count] = 0;
		_isPlaying.push_back( new bool );
		*_isPlaying[count] = false;
		_needsData.push_back( new bool );
		*_needsData[count] = true;
        _resampler.push_back( new Resampler );
        _fillBufferMutex.push_back( new wxMutex );
		// Try a call to FillBufferSilence(SIZEOFBUFFER) for each buffer to make sure they are clear of static.
		// This will also help to initialize them empty so we can set a latency without trouble.
        FillBufferSilence( count, SECONDARY_BUFFER_SIZE );
    }


	/// For future use with loading and playing back .wav files.
//	if( FAILED( hr = CoCreateInstance(CLSID_IDirectMusicLoader, NULL, CLSCTX_INPROC, IID_IDirectMusicLoader8, (void**) &_pDSLoader)))
//	{
//		MessageBox( NULL, DXGetErrorString(hr), "DirectMusicLoader Creation Error", MB_OK );
//		return false;
//	}

	_inited = true;

	return true;
}

/**
 @brief  Releases DirectSound
*/
bool DXAudioManager::UnInit()
{
	SAFE_RELEASE( _pDS );
	
    /// Release COM
    CoUninitialize();

    if( _inited == true )
    {
        int count;
	    for( count = 0; count < _numBuffers; count++ )
	    {
		    delete _bufferPlayCursor[count];
		    delete _locPointer[count];
		    delete _soundLength[count];
		    delete _isPlaying[count];
		    delete _needsData[count];
            delete _pDSSecondaryBuffers[count];
	    }
    }

	_inited = false;

	return true;
}

/**
 @brief  Sets the master volume
*/
void DXAudioManager::SetMasterVolume( int volume )
{
  if( volume > 0 || volume < -9600 )
  {
      return;
  }

  _masterVolume = volume;
}

/**
 @brief  Sets the per-channel volume.
*/
void DXAudioManager::SetVolume(int channel, int vol)
{
	 if( !_inited || channel >= _numBuffers || channel < 0 )
	 {
        return;
	 }

	 (*_pDSSecondaryBuffers[channel])->SetVolume(vol);
}

/**
 @brief  Gets the per-channel volume.
*/
int DXAudioManager::GetVolume(int channel)
{
	if( !_inited || channel >= _numBuffers || channel < 0 )
	{
        return 0;
	}

	long volume;

	(*_pDSSecondaryBuffers[channel])->GetVolume(&volume);

	return volume;
}

/**
 @brief  Sets the per-channel pan.
*/
void DXAudioManager::SetPan(int channel, int pan)
{
	if( !_inited || channel >= _numBuffers || channel < 0 )
	{
        return;
	}

	HRESULT hr;
	if( FAILED( hr = (*_pDSSecondaryBuffers[channel])->SetPan(pan)))
	{
		MessageBox( NULL, DXGetErrorString(hr), _("SetPan Error"), MB_OK );
	}
}

/**
 @brief  Gets the per-channel pan.
*/
int DXAudioManager::GetPan(int channel)
{
	if( !_inited || channel >= _numBuffers || channel < 0 )
	{
        return 0;
	}

	long pan;

	HRESULT hr;
	if( FAILED( hr = (*_pDSSecondaryBuffers[channel])->GetPan(&pan)))
	{
		MessageBox( NULL, DXGetErrorString(hr), _("GetPan Error"), MB_OK );
	}

	return pan;
}

/**
 @brief  Starts the playback buffer
*/
bool DXAudioManager::Play()
{
	/// No playing if we haven't even intialized our buffers yet.
	if( !_inited )
	{
		return false;
	}

	int count;
	for( count = 0; count < _numBuffers; count++ )
	{
		Play( count );
	}
	return true;
}

/**
 @brief  Starts playing an individual secondary buffer.
*/
bool DXAudioManager::Play( int channel )
{
	/// No playing if we haven't even initialized our buffers yet.
	if( !_inited || channel >= _numBuffers || channel < 0 )
	{
		return false;
	}
	
    if( *_isPlaying[channel] == true )
    {
        return true;
    }

	HRESULT hr;

    FillBufferSilence( channel, SECONDARY_BUFFER_SIZE );
	if( SUCCEEDED( hr = (*_pDSSecondaryBuffers[channel])->Play( 0, 0, DSBPLAY_LOOPING )))
	{
		*_isPlaying[channel] = true;
		return true;
	}

    return false;
}

/**
 @brief  Stop playing all of the secodary buffers
*/
bool DXAudioManager::Stop()
{
	/// Not if we haven't even initialized our buffers yet.
	if( !_inited )
    {
		return( false );
    }
	int count;
    for( count = 0; count < _numBuffers; count++ )
    {
		Stop( count );
    }

	return true;
}

/**
 @brief  Stops playing an individual secondary buffer.
*/
bool DXAudioManager::Stop(int channel)
{
	/// Not if we haven't even initialized our buffers yet.
	if( !_inited || channel >= _numBuffers || channel < 0 )
    {
		return( false );
    }

	HRESULT hr;

	if( SUCCEEDED( hr = (*_pDSSecondaryBuffers[channel])->Stop() ))
	{
		*_isPlaying[channel] = false;
		return true;
	}

	return false;
}

/**
 @brief  Starts recording and activates callbacks.
*/
bool DXAudioManager::StartCapture( )
{
	HRESULT hr;

	if( !_inited )
	{
		return false;
	}

	/// No capture if not inited.
	if( !_captureInited )
	{
		return false;
	}

	if( FAILED( hr = _pDSCaptureBuffer->Start( DSCBSTART_LOOPING ) ) )
	{
		MessageBox( NULL, DXGetErrorString(hr), _("DirectSound Capture Buffer Start Error"), MB_OK );
		return false;
	}

	return true;
}

/**
 @brief  Stops recording.
*/
bool DXAudioManager::StopCapture( )
{
	HRESULT hr;

	if( !_inited || !_captureInited )
	{
		return false;
	}
	
	// ResetEvent( _hNotificationEvent );

	if( FAILED( hr = _pDSCaptureBuffer->Stop( ) ) )
	{
		MessageBox( NULL, DXGetErrorString(hr), _("DirectSound Capture Buffer Stop Error"), MB_OK );
		return false;
	}
	return true;
}

/**
 @brief  Returns capture status.
*/
bool DXAudioManager::IsBufferCapturing( int channel )
{
	DWORD dwStatus;
	HRESULT hr;

	if( !_inited || !_captureInited || channel >= _numBuffers || channel < 0 )
	{
		return false;
	}

	if( FAILED( hr = _pDSCaptureBuffer->GetStatus( &dwStatus ) ) )
	{
		MessageBox( NULL, DXGetErrorString(hr), _("DirectSound Capture Buffer GetStatus Error"), MB_OK );
		return false;
	}

	// TODO: Check for active channel.

	if( dwStatus & DSCBSTATUS_CAPTURING )
	{
		return true;
	}

	return false;
}

/**
 @brief  Fills a DirectSound secondary buffer with data.
*/
bool DXAudioManager::FillBuffer(int channel, unsigned char *data, int numBytes, int sampleRate )
{
    static int directxErrors = 0;
    static int crowdedBufferErrors = 0;
    static int bufferlostErrors = 0;
    static int bufferTerminatedErrors = 0;
    static int iterations = 0;

    unsigned char *resampleBuffer = NULL;

	if( channel >= _numBuffers || channel < 0 || numBytes <= 0 || sampleRate == 0 || data == 0 )
    {
        return false;
    }

    (*_fillBufferMutex[channel]).Lock();

    if( sampleRate != 44100 )
    {
        // Resample this into the channel's actual sample rate.
        int targetBytes = (44100 * numBytes) / sampleRate;
        targetBytes &= ~1;
        resampleBuffer = new unsigned char[ targetBytes ];
        memset( resampleBuffer, 0, targetBytes );
        memcpy( resampleBuffer, data, numBytes );
        _resampler[channel]->Resample( resampleBuffer, (numBytes / BYTES_PER_WORD), (targetBytes / BYTES_PER_WORD), BYTES_PER_WORD, 1 );
        data = resampleBuffer;
        numBytes = targetBytes;
        sampleRate = 44100;
    }

//#ifdef _DEBUG
//    if( ++iterations % 1000 == 0 )
//    {
//        char buf[1024];
//        _snprintf( buf, 1024, "directxErrors = %d, crowdedBufferErrors = %d, bufferlostErrors = %d, iterations = %d.",
//            directxErrors, crowdedBufferErrors, bufferlostErrors, iterations );
//        MessageBox( NULL, buf, "FillBuffer Report", MB_OK );
//    }
//#endif

//#ifdef _DEBUG
//    // Log in debug mode only.
//    char filename[64];
//    memset( filename, 0, 64 );
//    sprintf( filename, "fillbuffer%d.raw", channel );
//    FILE* fp;
//    if( (fp = fopen( filename, "ab" ) ))
//    {
//        fwrite( data, numBytes, 1, fp );
//        fclose( fp );
//    }
//#endif

	// Find out whether there is room in the sound buffer.
	if( (*_soundLength[channel] + numBytes) > SECONDARY_BUFFER_SIZE )
	{
        ++crowdedBufferErrors;
        if( resampleBuffer != NULL )
        {
            delete resampleBuffer;
        }
        (*_fillBufferMutex[channel]).Unlock();
		return false;
	}

	// If there is room, determine if it is because there is no data in the buffer, or because there 
	// is data there, but we are not full yet.
	//
	// In this case, we can assume that there is still data playing.  This will allow us to simply 
	// update the sound buffer by adding the data.
	if(*_soundLength[channel] > 0)
	{
        // There is data in the buffer, but it's not full yet.
	}
	else
	{
		// Check status and make sure nothing stupid has happened to our buffer.
		DWORD dwStatus;
		(*_pDSSecondaryBuffers[channel])->GetStatus( &dwStatus );
		if( dwStatus & DSBSTATUS_BUFFERLOST )
		{
            ++bufferlostErrors;
            dwStatus = dwStatus;
        }
		if( dwStatus & DSBSTATUS_LOOPING )
		{dwStatus = dwStatus;}
		if( dwStatus & DSBSTATUS_PLAYING )
		{dwStatus = dwStatus;}
		if( dwStatus & DSBSTATUS_LOCHARDWARE )
		{dwStatus = dwStatus;}
		if( dwStatus & DSBSTATUS_TERMINATED )
		{
            ++bufferTerminatedErrors;
            dwStatus = dwStatus;
        }
	}

	// Implement the global volume control here.
	if( _masterVolume < 0 )
	{
		short* dataIterator = (short*)data;
		int numSamples = numBytes / BYTES_PER_WORD;
		int count;
		float volumeAdjustment = ((_masterVolume + 9600.0f) / 9600.0f);
		for( count = 0; count < numSamples; count++ )
		{
			dataIterator[count] = (short)(dataIterator[count] * volumeAdjustment);
		}
	}
	
	LPVOID  lpvPtr1; 
	DWORD dwBytes1; 
	LPVOID  lpvPtr2; 
	DWORD dwBytes2; 
	HRESULT hr; 

	// Obtain memory address of write block. This will be in two parts if the block wraps around.
	hr = (*_pDSSecondaryBuffers[channel])->Lock(0, numBytes, &lpvPtr1, &dwBytes1, &lpvPtr2, &dwBytes2, DSBLOCK_FROMWRITECURSOR); 
	
	// If the buffer was lost, restore and retry lock. 
	if (DSERR_BUFFERLOST == hr) 
	{ 
		(*_pDSSecondaryBuffers[channel])->Restore(); 
		hr = (*_pDSSecondaryBuffers[channel])->Lock(0, numBytes, &lpvPtr1, &dwBytes1, &lpvPtr2, &dwBytes2, DSBLOCK_FROMWRITECURSOR); 
	} 
	else if( hr == DSERR_INVALIDCALL || hr== DSERR_INVALIDPARAM || hr == DSERR_PRIOLEVELNEEDED )
	{
        ++directxErrors;
		// Strange DirectX error... eject!
        if( resampleBuffer != NULL )
        {
            delete resampleBuffer;
        }
        (*_fillBufferMutex[channel]).Unlock();
		return false;
	}

	if( SUCCEEDED(hr) )
	{ 
		// Write to pointers. 	
		CopyMemory(lpvPtr1, data, dwBytes1); 
		if (NULL != lpvPtr2) 
		{ 
			CopyMemory(lpvPtr2, (data + dwBytes1), dwBytes2); 
		} 
		
		// Release the data back to DirectSound. 
		hr = (*_pDSSecondaryBuffers[channel])->Unlock(lpvPtr1, dwBytes1, lpvPtr2, dwBytes2); 
		if( SUCCEEDED(hr)) 
		{ 
			*_soundLength[channel] += numBytes;

            // Success. 
            if( resampleBuffer != NULL )
            {
                delete resampleBuffer;
            }
            (*_fillBufferMutex[channel]).Unlock();
			return true; 
		} 
	} 
	
	// Lock, Unlock, or Restore failed. 
	MessageBox( NULL, DXGetErrorString(hr), _("DirectSound Error"), MB_OK );
    if( resampleBuffer != NULL )
    {
        delete resampleBuffer;
    }
    (*_fillBufferMutex[channel]).Unlock();
	return false;
}

/**
 @brief  Fills a secondary sound buffer with silence.
*/
bool DXAudioManager::FillBufferSilence(int channel, int numBytes)
{
	if( channel >= _numBuffers || channel < 0 )
	{
		return false;
	}

	unsigned char *data = new unsigned char[numBytes];
	memset( data, 0, numBytes );

	FillBuffer( channel, data, numBytes, 44100 );

	delete[] data;

	return true; 
}

/**
 @brief  Creates the capture buffer.
 @note
 Refer to:
 http://msdn.microsoft.com/archive/default.asp?url=/archive/en-us/directx9_c_Summer_04/directx/htm/creatingacapturebuffer.asp

 recordCallback is optional, but required if you actually want to record data.
 guid is optional.  Not passing it will give you the default sound device.
*/
bool DXAudioManager::CreateCaptureBuffer( AudioRecordingCallback * recordCallback, GUID guid, const char * )
{
	// Bail if we haven't inited our DirectX yet.
	if( !_inited )
	{
		return false;
	}

	// Don't double-init.
	if( _captureInited )
	{
		return true;
	}

	HRESULT hr;

	_captureGuid = guid;
	_recordingCallback = recordCallback;

    if( FAILED( hr = DirectSoundCaptureCreate8( &_captureGuid, &_pDSCapture, NULL ) ) )
	{
		MessageBox( NULL, DXGetErrorString(hr), _("DirectSound DirectSoundCaptureCreate Error"), MB_OK );
		return false;
	}

	WAVEFORMATEX wfx;
	ZeroMemory( &wfx, sizeof( WAVEFORMATEX ));
	wfx.wFormatTag = WAVE_FORMAT_PCM;
	wfx.nChannels = MONO;
	wfx.nSamplesPerSec = 44100;
	wfx.nBlockAlign = BYTES_PER_WORD;
	wfx.wBitsPerSample = BITS_PER_WORD;
	wfx.cbSize = 0;
	wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
	
	DSCBUFFERDESC dscbd;
	ZeroMemory( &dscbd, sizeof( DSCBUFFERDESC ));
	dscbd.dwSize = sizeof( DSCBUFFERDESC );
	dscbd.dwBufferBytes = _recordBufferLength;
	dscbd.lpwfxFormat = &wfx;
	dscbd.dwFlags = 0;
	dscbd.dwFXCount = 0;
	dscbd.dwReserved = 0;
	dscbd.lpDSCFXDesc = 0;

	if (FAILED(hr = _pDSCapture->CreateCaptureBuffer(&dscbd, &_pDSCaptureBuffer, NULL)))
	{
		MessageBox( NULL, DXGetErrorString(hr), _("DirectSound CreateCaptureBuffer Error"), MB_OK );
		return false; 
	}

    if( FAILED( hr = _pDSCaptureBuffer->QueryInterface( IID_IDirectSoundNotify, (VOID**)&_pDSNotify ) ) )
	{
		MessageBox( NULL, DXGetErrorString(hr), _("DirectSound Notify Error"), MB_OK );
        return false;
	}

	_hNotificationEvent = CreateEvent( NULL, false, false, NULL );

    // Setup the notification positions - one notification per PDU worth of data in the buffer.
	// This should only ever need to be done once.
	unsigned int i;
    for( i = 0; i < _captureChunkTotal; i++ )
    {
        _notificationPositions[i].dwOffset = ((_captureChunkSize * BYTES_PER_WORD) * i) + (_captureChunkSize * BYTES_PER_WORD) - 1;
        _notificationPositions[i].hEventNotify = _hNotificationEvent;             
    }

    if( FAILED( hr = _pDSNotify->SetNotificationPositions( _captureChunkTotal, &_notificationPositions[0] ) ) )
	{
		MessageBox( NULL, DXGetErrorString(hr), _("DirectSound SetNotificationPositions Error"), MB_OK );
		return false;
	}

	_captureInited = true;

	return true;
}

/**
 @brief  Does nothing.
*/
bool DXAudioManager::DeleteCaptureBuffer( )
{
	return true;
}

/** 
 @brief  Sets the sample rate for an individual channel.
*/
bool DXAudioManager::SetSampleRate( int channel, int sampleRate )
{
	if( channel >= _numBuffers || channel < 0 || sampleRate > 48000 || sampleRate < 0 )
	{
		return false;
	}

    // IGNORED: Playback sample rate is fixed at 44100 and we resample in the FillBuffer call.

    //if( *_secondaryBufferSampleRate[channel] == sampleRate )
    //{
    //    return true;
    //}

    //*_secondaryBufferSampleRate[channel] = sampleRate;

    return true;

 //   if( sampleRate == GetSampleRate(channel))
 //   {
 //       return true;
 //   }

 //   bool playing = IsBufferPlaying( channel );
 //   int volume = GetVolume( channel );
 //   int pan = GetPan( channel );

 //   // Stop the channel from playing.
 //   Stop( channel );

 //   _secondaryBufferLock[channel].lock();
 //   HRESULT hr;
	////if( SUCCEEDED(hr = (*_pDSSecondaryBuffers[channel])->SetFrequency( sampleRate )))
	////{
	////	return true;
	////}

 //   // Set the old buffer aside so we can delete it later.
 //   LPDIRECTSOUNDBUFFER tempbuffer = *_pDSSecondaryBuffers[channel];

 //   // Setup the WAVEFORMATEX structure.
	//WAVEFORMATEX wfx;
 //   ZeroMemory( &wfx, sizeof(WAVEFORMATEX) );
 //   wfx.cbSize = 0;
	//wfx.wFormatTag = WAVE_FORMAT_PCM;
	//wfx.nSamplesPerSec = sampleRate;
 //   wfx.wBitsPerSample = BITS_PER_WORD;
	//wfx.nChannels = MONO;
	//wfx.nBlockAlign = MONO * BYTES_PER_WORD;
 //   wfx.nAvgBytesPerSec = wfx.nBlockAlign * wfx.nSamplesPerSec;

 //   // Setup the DSBUFFERDESC structure.
 //   DSBUFFERDESC dsbd;
	//ZeroMemory(&dsbd, sizeof(DSBUFFERDESC));
	//dsbd.dwSize = sizeof(DSBUFFERDESC);
	//dsbd.dwFlags = DSBCAPS_CTRLPAN | DSBCAPS_CTRLVOLUME | DSBCAPS_GLOBALFOCUS;
	//dsbd.dwBufferBytes = SECONDARY_BUFFER_SIZE;
 //   dsbd.lpwfxFormat = &wfx;

	//// Create the new secondary buffer.
	//_pDSSecondaryBuffers[channel] = new LPDIRECTSOUNDBUFFER;
	//hr = _pDS->CreateSoundBuffer( &dsbd, _pDSSecondaryBuffers[channel], NULL);
	//{
	//	if( FAILED( hr ))
	//	{
	//		MessageBox( NULL, DXGetErrorString(hr), "DirectSound error changing sample rate.", MB_OK );
	//		return( false );
	//	}
 //   }
 //   *_locPointer[channel] = 0;
 //   *_bufferPlayCursor[channel] = 0;
 //   *_soundLength[channel] = 0;
 //   _secondaryBufferLock[channel].unlock();

 //   //MessageBox( NULL, "Sample Rate Changed OK", "OK", MB_OK );

 //   // Delete the old buffer.
 //   //delete &tempbuffer;

 //   SetVolume( channel, volume );
 //   SetPan( channel, pan );
 //   if( playing )
 //   {
 //       //FillBufferSilence( channel, SECONDARY_BUFFER_SIZE );
 //       Play( channel );
 //   }

	//return( true );
}

/**
 @brief  Sets the sample rate for recording.  Resampling does the rest.

 We are always sampling at 44100, but the _captureSampleRate determines
 whether we need to resample the data and/or what we need to resample
 it to before forwarding it to the AudioRecordingCallback.
*/
bool DXAudioManager::SetRecordSampleRate( unsigned int sampleRate )
{
	/// Successful if nothing is changing.
	if( sampleRate == _captureSampleRate )
	{
		return true;
	}

	_captureSampleRate = sampleRate;

    _captureChunkSize = _captureSampleRate * _bufferLatency; // Calculated in samples.
    if( _captureChunkSize > (CAPTURE_CHUNK_SIZE / BYTES_PER_WORD ) ) // Compared in samples.
    {
        _captureChunkSize = (CAPTURE_CHUNK_SIZE / BYTES_PER_WORD ); // Calculated in samples.
    }
	_captureChunkSize &= ~1;

    return true;
}

/**
 @brief  Returns the playback sample rate for an individual channel.
*/
unsigned int DXAudioManager::GetSampleRate( int channel )
{
	if( channel >= _numBuffers || channel < 0 )
	{
		return 0;
	}

    // IGNORED: Fixed sample rate.

    return 44100;

	//unsigned long freq = 0;
	//HRESULT hr;
	//if( SUCCEEDED(hr = (*_pDSSecondaryBuffers[channel])->GetFrequency(&freq)))
	//{
	//	return freq;
	//}

	//return 0;
}

/**
 @brief  Returns the capture sample rate.
*/
unsigned int DXAudioManager::GetRecordSampleRate()
{
	return _captureSampleRate;
}

/**
 @brief  Monitors the playback buffer and fills it with silence if necessary.
*/
void DXAudioManager::MonitorBuffer()
{
    static int soundLengthUnderrunErrors = 0;
    unsigned long unused;

	if(_inited)
	{
    	int channel;
		for( channel = 0; channel < _numBuffers; channel++ )
		{
			if (IsBufferPlaying(channel))
			{
                // The write cursor is the point in the buffer ahead of which it is safe
                // to write data to the buffer. Data should not be written to the part of
                // the buffer after the play cursor and before the write cursor.
                (*_pDSSecondaryBuffers[channel])->GetCurrentPosition( _bufferPlayCursor[channel], &unused );
                // Something is in the buffer.  It may be plenty and may be less than one "packet" of data.
				if(*_soundLength[channel] > 0)
				{
					int numBytesPlayed;
					// If the play cursor is less than the start location, then the buffer wrapped
					// adjust accordingly...else it is just the play cursor - _start_location
                    //
                    // This depends upon our cycling through this loop more frequently than once per
                    // buffer cycle, otherwise our bytes played count will be wrong.
                    if( *_bufferPlayCursor[channel] < *_locPointer[channel] )
					{
						numBytesPlayed = (SECONDARY_BUFFER_SIZE - *_locPointer[channel]) + *_bufferPlayCursor[channel];
					}
					else
					{
						numBytesPlayed = *_bufferPlayCursor[channel] - *_locPointer[channel];
					}
                    // We set the empty flag if we have run out of data to play back.
                    //
                    if( *_soundLength[channel] < (int)(GetSampleRate(channel) * _bufferLatency * BYTES_PER_WORD) )
                    {
					    *_needsData[channel] = true;
                    }
					*_soundLength[channel] -= numBytesPlayed;
                    // Set the last location to the play cursor
                    *_locPointer[channel] = *_bufferPlayCursor[channel];
				}
				// If we are at less than 0 length, reset.  Might want to clear buffer as well, just to make sure.
                else if(*_soundLength[channel] < 0)
                {
                    ++soundLengthUnderrunErrors;
                    // It would be good to throw an error of some sort here.
					*_soundLength[channel] = 0;
                    *_needsData[channel] = true;
                }
                // Nothing at all in the buffer - we need to fill it.
				else
				{
                    *_needsData[channel] = true;
				}

				if(*_needsData[channel])
				{
                    // Fill buffer with silence:  Latency x sample rate x bytes per sample.
					FillBufferSilence( channel, (int)(GetSampleRate(channel) * _bufferLatency * BYTES_PER_WORD) );
					*_needsData[channel] = false;
                }
			}
		}
	}
}

/**
 @brief  Returns playback status of a secondary buffer channel.
*/
bool DXAudioManager::IsBufferPlaying( int channel )
{
	if( !_inited || channel >= _numBuffers || channel < 0 )
	{
		return false;
	}

	return *_isPlaying[channel];
}

/** 
 @brief  Catches recording callbacks and reacts accordingly.
*/
int DXAudioManager::run()
{
	static int numNotificationsReceived = 0;
	/// wait on status messages from buffers

	/// We need to make sure we're keeping track of whether we're recording or not, and whether to
	/// start or stop the capture buffer.

	while(!TestDestroy())
	{
		/// MonitorBuffer needs to run near-continuously in order to keep decrementing the "samples in buffer but
		/// not played" variable _soundLength[x].
		MonitorBuffer();
		DWORD dwResult = WaitForSingleObject( _hNotificationEvent, 0 );
		switch( dwResult )
		{
			case WAIT_OBJECT_0 + 0:
				/// g_hNotificationEvents[0] is signaled
				
				/// This means that we hit one of the notifications associated    
				/// with capture and should record anything that we have.  These
				/// events come only when the buffer is running, which is controlled
				/// with the PTT button on the dialog.
				if( !( ProcessCapturedData() ) )
				{
					//g_ContinueRunning = false;
				}
				++numNotificationsReceived;
				break;

			default:
				break;
		}
		Sleep( 3 );
	}
	return( 0 );
}

/**
 @brief  Grabs data from the capture buffer and forwards it to the appropriate function.
*/
bool DXAudioManager::ProcessCapturedData()
{
	/// We received a buffer notification.  Grab a chunk of data and forward it on to the function that
	/// will process it.

    HRESULT hr;
    VOID*   pbCaptureData    = NULL;
    DWORD   dwCaptureLength;
    VOID*   pbCaptureData2   = NULL;
    DWORD   dwCaptureLength2;
    DWORD   dwReadPos;
    DWORD   dwCapturePos;
    long lLockSize;

	/// Statistics variables - for keeping an eye on things and detecting problems.
	/// None of these are important for the operation of this function.
	static int sampleDifference = 0;
	static int callbackErrors = 0;
	static int boundaryErrors = 0;
	static int initErrors = 0;
	static int lockErrors = 0;
	static int unprocessedData = 0;
	static int overprocessedData = 0;
	static int offsetWraps = 0;
	static int lockWraps = 0;
	static int totalBytesAttempted = 0;
	static int totalBytesCaptured = 0;
	/// End stat variables.
	
    if( !_inited || !_captureInited )
	{
		++initErrors;
        return false;
	}
	
    if( FAILED( hr = _pDSCaptureBuffer->GetCurrentPosition( &dwCapturePos, &dwReadPos ) ) )
	{
		MessageBox( NULL, DXGetErrorString(hr), _("Capture Buffer GetCurrentPosition Error"), MB_OK );
        return false;
	}

	sampleDifference = dwCapturePos - dwReadPos;

    lLockSize = dwReadPos - _dwNextCaptureOffset;
    if( lLockSize < 0 )
	{
        lLockSize += _recordBufferLength;
		++lockWraps;
	}
	
    /// Block align lock size so that we are always right on a boundary.  May cause clicks and/or pops.
	if( (lLockSize % (_captureChunkSize * BYTES_PER_WORD) != 0 ))
	{
		lLockSize -= (lLockSize % (_captureChunkSize * BYTES_PER_WORD));
		++boundaryErrors;
	}
	
	/// This should never be 0 since we're here for a reason - we received a callback.
    if( lLockSize == 0 )
	{
		++lockErrors;
        return false;
	}
	
    /// Lock the capture buffer down
    if( FAILED( hr = _pDSCaptureBuffer->Lock( _dwNextCaptureOffset, lLockSize, 
		&pbCaptureData, &dwCaptureLength, &pbCaptureData2, &dwCaptureLength2, 0L ) ) )
	{
		MessageBox( NULL, DXGetErrorString(hr), _("Capture Buffer Lock Error"), MB_OK );
		return false;
	}

	unsigned int length = dwCaptureLength;
	totalBytesAttempted += dwCaptureLength;
	
	/// While there is captured data..get the data and copy it into 
	/// the signal PDU.  It is 16bit PCM as this format.  Allow the 
	/// SetSPDU method to handle the sending to all channels that 
	/// are set to broadcast and their conversion types.
	while(length >= (_captureChunkSize * BYTES_PER_WORD))
	{
		if(length >= (_captureChunkSize * BYTES_PER_WORD) && _recordingCallback != NULL)
		{
            int resampleTarget = _captureChunkSize * _captureSampleRate / 44100;
            if( _captureSampleRate != 44100 )
            {
                _captureResampler.Resample( (unsigned char *)pbCaptureData, _captureChunkSize, resampleTarget, BYTES_PER_WORD, 1 );
            }
            _recordingCallback->ForwardRecordedData((unsigned char *)pbCaptureData, (resampleTarget * BYTES_PER_WORD), _captureSampleRate );
			totalBytesCaptured += (_captureChunkSize * BYTES_PER_WORD);
		}
		else if( _recordingCallback != NULL )
		{	
            int resampleTarget = (length / BYTES_PER_WORD) * _captureSampleRate / 44100;
            if( _captureSampleRate != 44100 )
            {
                _captureResampler.Resample( (unsigned char *)pbCaptureData, (length / BYTES_PER_WORD), resampleTarget, BYTES_PER_WORD, 1 );
            }
			_recordingCallback->ForwardRecordedData((unsigned char *)pbCaptureData, (resampleTarget * BYTES_PER_WORD), _captureSampleRate );
			totalBytesCaptured += length;
		}
		else
		{
			++callbackErrors;
		}
		
		length -= (_captureChunkSize * BYTES_PER_WORD);
	}
	
	if( length > 0 )
	{
		unprocessedData += length;
	}
	else
	{
		overprocessedData -= length;
	}

    /// Unlock the capture buffer
    _pDSCaptureBuffer->Unlock( pbCaptureData, dwCaptureLength, NULL, 0 );
	
    /// Move the capture offset along
    _dwNextCaptureOffset += dwCaptureLength; 
	if( _dwNextCaptureOffset >= _recordBufferLength )
	{
		_dwNextCaptureOffset %= _recordBufferLength; 
		++offsetWraps;
	}
	
    if( pbCaptureData2 != NULL )
    {
		unsigned int length = dwCaptureLength2;
		totalBytesAttempted += dwCaptureLength2;
		
		while(length >= (_captureChunkSize * BYTES_PER_WORD))
		{
			if(length >= (_captureChunkSize * BYTES_PER_WORD) && _recordingCallback != NULL)
			{
                int resampleTarget = _captureChunkSize * _captureSampleRate / 44100;
                if( _captureSampleRate != 44100 )
                {
                    _captureResampler.Resample( (unsigned char *)pbCaptureData2, _captureChunkSize, resampleTarget, BYTES_PER_WORD, 1 );
                }
				_recordingCallback->ForwardRecordedData((unsigned char *)pbCaptureData2, (resampleTarget * BYTES_PER_WORD), _captureSampleRate );
				totalBytesCaptured += (_captureChunkSize * BYTES_PER_WORD);
			}
			else if( _recordingCallback != NULL )
			{	
                int resampleTarget = (length / BYTES_PER_WORD) * _captureSampleRate / 44100;
                if( _captureSampleRate != 44100 )
                {
                    _captureResampler.Resample( (unsigned char *)pbCaptureData2, (length / BYTES_PER_WORD), resampleTarget, BYTES_PER_WORD, 1 );
                }
				_recordingCallback->ForwardRecordedData((unsigned char *)pbCaptureData2, (resampleTarget * BYTES_PER_WORD), _captureSampleRate );
				totalBytesCaptured += length;
			}
			else
			{
				callbackErrors++;
			}
			
			length -= (_captureChunkSize * BYTES_PER_WORD);
		}

		if( length > 0 )
		{
			unprocessedData += length;
		}
		else
		{
			overprocessedData -= length;
		}

		/// Unlock the capture buffer
		_pDSCaptureBuffer->Unlock( pbCaptureData2, dwCaptureLength2, NULL, 0 );
		
		/// Move the capture offset along
		_dwNextCaptureOffset += dwCaptureLength2; 
		if( _dwNextCaptureOffset >= _recordBufferLength )
		{
			_dwNextCaptureOffset %= _recordBufferLength;  // Circular buffer
			++offsetWraps;
		}
	}

	return true;
}

/**
 @brief  Sets the latency of record and playback buffers.
*/
void DXAudioManager::SetBufferLatency( int msec )
{
	/// Buffer length must be nonzero and less than one second.
	if( msec <= 0 || msec >= 1000 )
	{
		return;
	}
	_bufferLatency = (float)msec / 1000.0f;

    int _captureChunkSize = _captureSampleRate * _bufferLatency; // Calculated in samples.
    if( _captureChunkSize > (CAPTURE_CHUNK_SIZE / BYTES_PER_WORD ) ) // Compared in samples.
    {
        _captureChunkSize = (CAPTURE_CHUNK_SIZE / BYTES_PER_WORD ); // Calculated in samples.
    }
	_captureChunkSize &= ~1;

	return;
}

int DXAudioManager::GetNumSamplesQueued(int channel)
{
	//if( channel < 0 || channel >= _numBuffers )
	//{
	//	return 0;
	//}
	//int value;
	//_secondaryBuffers[channel]->_mutex->Lock();
	//value = _secondaryBuffers[channel]->_bufferData->GetReadAvail();
	//_secondaryBuffers[channel]->_mutex->Unlock();
	//return value;
	return 0;
}
#endif // WIN32
