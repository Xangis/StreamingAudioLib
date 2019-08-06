
#include <iostream>
using namespace std;

#include "RtAudioManager.h"

/// Define the size of our secondary buffer in frames.
#define BUFFER_SIZE 4096
#define CAPTURE_CHUNK_SIZE 1400
/// Used for resampling calculations and buffer info.
#define MAX_SAMPLE_RATE 44100

/**
     @brief     Constructor, sets initial values for internal data.
     Constructor, sets initial values for internal data.
     @return
     void
*/
RtAudioManager::RtAudioManager(int numBuffers)
{
  _audio = NULL;
  _format = RTAUDIO_FLOAT32;
  _capturing = false;
  _numBuffers = numBuffers;
  // These are the default values that we record and play at.  Other values will be resampled
  // to match these.  It is the most widely-supported sample rate and should work on pretty much
  // all systems.
  _playbackSampleRate = MAX_SAMPLE_RATE;
  _playbackByteAlign = 2;
  /// Used to keep track of the number of frames in our playback buffer.
  _playbackFrames = 0;
  _captureSampleRate = MAX_SAMPLE_RATE;
  // A chunk of data in our record buffer.
  _recordBufferLength = (int)(_captureSampleRate * _bufferLatency * BYTES_PER_WORD);
  _captureBuffer = new char[_recordBufferLength];
  memset( _captureBuffer, 0, _recordBufferLength );
  int count;
  // Create volume and pan values.
  for( count = 0; count < _numBuffers; count++ )
    {
      _secondaryBuffers.push_back( new SecondaryBuffer );
      _secondaryBuffers[count]->_volume = 0;
      _secondaryBuffers[count]->_pan = 0;
      _secondaryBuffers[count]->_isPlaying = false;
      _secondaryBuffers[count]->_bytesPerSample = BYTES_PER_WORD;
      _secondaryBuffers[count]->_mutex = new wxMutex;
      _secondaryBuffers[count]->_bufferData = new RingBuffer( BUFFER_SIZE );
      _secondaryBuffers[count]->_sampleRate = MAX_SAMPLE_RATE;
      _secondaryBuffers[count]->_chunkSize = (int)(_bufferLatency * _secondaryBuffers[count]->_sampleRate * _secondaryBuffers[count]->_bytesPerSample);
      // Chunk size MUST be an even number of bytes.
      _secondaryBuffers[count]->_chunkSize &= ~1;
    }
	// Higher thread priority in order to watch the sound buffers better.
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
     @brief     Destructor, cleans up allocations and uninitializes audio.
     Destructor, cleans up allocations and uninitializes audio.
     @return
     void
*/
RtAudioManager::~RtAudioManager()
{
    if( _audio != NULL )
    {
        _audio->stopStream();
        if( _audio->isStreamOpen() )
        {
            _audio->closeStream();
        }
    }
    if( _inited )
    {
        UnInit();
    }
    if( _captureInited )
    {
        DeleteCaptureBuffer();
    }
    int count;
    for( count = _numBuffers - 1; count > -1; --count )
    {
      delete _secondaryBuffers[count];
    }
    delete[] _captureBuffer;

}

/**
 @brief  Enumerates sound devices in the current system.
 This first tries to call snd_names_list, a function that may not be able to get
 a list of cards depending on the system (function is quite new).  If it is unable
 to get a list, it tries brute force by iterating through the possible devices and
 tries to open them one by one.  The fallback method can be ugly due to the generation
 of "no such device" errors, but it works.
*/
void RtAudioManager::Enumerate( void (*callback)(const char *) )
{
    RtAudio::StreamParameters parameters;
    if( _audio == NULL )
    {
        _audio = new RtAudio();
    }
    int count = _audio->getDeviceCount();
    cout << count << " audio devices detected." << endl;
    parameters.deviceId = _audio->getDefaultOutputDevice();
    cout << "Default output device: " << parameters.deviceId << endl;
    RtAudio::DeviceInfo info = _audio->getDeviceInfo(parameters.deviceId);
    cout << "Device " << info.name << ", " << info.outputChannels << " output channels, " << info.inputChannels <<
        " input channels, " << info.duplexChannels << " duplex channels, " << info.nativeFormats << " native formats" << endl;
}

/**
     @brief     Initializes the RtAudioManager.
     Initializes the RtAudioManager.
     @return
     returns false if RtAudioManager could not be initialized and true if it could be.
     @note
     The ParentWindow argument should be passed in as NULL.  It is only included for
     DXAudioManager interface compatibility.
*/
bool RtAudioManager::Init(void *parentWindow, int* soundCard, const char *name)
{
    // No multiple initialization.
    if( _inited == true )
    {
        return( true );
    }

    RtAudio::StreamParameters parameters;
    if( _audio == NULL )
    {
        _audio = new RtAudio();
    }
    int count = _audio->getDeviceCount();
    cout << count << " audio devices detected." << endl;
    parameters.deviceId = _audio->getDefaultOutputDevice();
    cout << "Default output device: " << parameters.deviceId << endl;
    RtAudio::DeviceInfo info = _audio->getDeviceInfo(parameters.deviceId);
    cout << "Device " << info.name << ", " << info.outputChannels << " output channels, " << info.inputChannels <<
        " input channels, " << info.duplexChannels << " duplex channels, " << info.nativeFormats << " native formats" << endl;
    parameters.nChannels = 2;
    parameters.firstChannel = 0;
    unsigned int sampleRate = 44100;
    unsigned int bufferFrames = BUFFER_SIZE; // 256 sample frames
    try {
        _audio->openStream( &parameters, NULL, RTAUDIO_FLOAT64,
                            sampleRate, &bufferFrames, FillBuffer, (void *)_musicStream );
        _audio->startStream();
    }
    catch ( RtAudioError& e ) {
        e.printMessage();
        exit( 0 );
    }

    _inited = true;

    return( _inited );
}

/**
     @brief     Uninitializes the RtAudioManager.
     Uninitializes the RtAudioManager.
     @return
     bool value always returns true
*/
bool RtAudioManager::UnInit()
{
  _inited = false;
  return true;
}

/**
     @brief     Mixes secondary buffers into main playback buffer.
     This function mixes data from the secondary buffers into a main stereo buffer,
	 including resampling and calculations for pan and volume settings, and then
	 writes it to the soundcard.  This is the real workhorse of RtAudioManager.
     @return
     returns false if the sound has not been initialized or is not playing.
*/
bool RtAudioManager::ProcessSoundBuffer( void )
{
  static int skips = 0;
  static int underruns = 0;
  static int bufferscrewups = 0;
  bool playing = false;
  int channel = 0;      // Channel iterator.

  if( !_inited )
  {
      return false;
  }

  // We only need to monitor the buffer if one of our sound streams is playing.  If it is not,
  // then we can safely ignore it.
  for( channel = 0; channel < _numBuffers; channel++ )
  {
      _secondaryBuffers[channel]->_mutex->Lock();
      if( _secondaryBuffers[channel]->_isPlaying == true )
      {
	    playing = true;
	  }
      _secondaryBuffers[channel]->_mutex->Unlock();
  }
  // If we have nothing to monitor, we bail.
  if( playing == false )
  {
      return false;
  }

  //ALint state = 0;
  //alGetSourcei( _playbackHandle, AL_SOURCE_STATE, &state ); // Will be: AL_PLAYING, AL_PAUSED, AL_STOPPED, or AL_INITIAL
  // Check whether a buffer has been processed.  If we don't need to add data to the buffer
  // we won't bother.
  //ALint buffersprocessed = 0;
  //alGetSourcei( _playbackHandle, AL_BUFFERS_PROCESSED, &buffersprocessed );
  //ALint buffersqueued = 0;
  //alGetSourcei( _playbackHandle, AL_BUFFERS_QUEUED, &buffersqueued );

  /*if( buffersprocessed == 2 )
  {
        underruns++;
        // If we're out of buffers it's time to unfuck things up.
        //ALuint silenceBuffer;
        //alSourceUnqueueBuffers( _playbackHandle, 1, &silenceBuffer );
        //alSourceUnqueueBuffers( _playbackHandle, 1, &workingBuffer );
        // Use the first buffer to put silence in and give ourselves room to work.
        int chunkSize = (int)(_playbackSampleRate * _bufferLatency * BYTES_PER_WORD * STEREO );
        char* data = new char[chunkSize];
        memset( data, 0, chunkSize );
        //alBufferData( silenceBuffer, _format, data, chunkSize, _playbackSampleRate );
        //alSourceQueueBuffers( _playbackHandle, 1, &silenceBuffer );
        delete[] data;
        //alSourcePlay( _playbackHandle );
        // The second buffer will be used to mix data to.
  }
  // Only unqueue a buffer and get it ready to fill if one has been processed.  Otherwise we may be
  // ripping out buffers that have data that is waiting to be played.
  else if( buffersprocessed > 0 )
  {
        //alSourceUnqueueBuffers( _playbackHandle, 1, &workingBuffer);
  }
  else
  {
      skips++;
      if( buffersqueued == 0 )
      {
          bufferscrewups++;
      }
      return true;
  }*/

  return true;
  //return MixAudio(workingBuffer);
}

/**
  @brief  Copies raw data into a secondary buffer.
  @note
  The data passed in must match the format of the secondary buffer.  Matching this is the
  application's responsiblity.
*/
bool RtAudioManager::FillBuffer( int channel, unsigned char* data, int length, int sampleRate )
{
  static int overruns;
  if( channel >= _numBuffers || channel < 0 || length <= 0 )
  {
      return false;
  }

//#ifdef _DEBUG
//  FILE* fp;
//  char filename[64];
//  memset( filename, 0, 64 );
//  _snprintf( filename, 64, "fillbuffer%d.raw", channel );
//  if( (fp = fopen( filename, "ab" ) ))
//  {
//      fwrite( data, length, 1, fp );
//      fclose( fp );
//  }
//#endif

  _secondaryBuffers[channel]->_mutex->Lock();
  int result = (_secondaryBuffers[channel]->_bufferData)->Write( data, length );
  _secondaryBuffers[channel]->_mutex->Unlock();

  if( result != length )
  {
      overruns++;
      return false;
  }
	
  return true;
}

/**
  @brief  Fills an individual secondary buffer with silence.
*/
bool RtAudioManager::FillBufferSilence( int channel, int length )
{
  if( channel >= _numBuffers || channel < 0 || length == 0 )
  {
      return false;
  }

  bool err;

  unsigned char *data = new unsigned char[length];
  memset(data, 0, length );

  err = FillBuffer( channel, data, length, _secondaryBuffers[channel]->_sampleRate );

  delete[] data;

  return err;
}

bool RtAudioManager::EmptyBuffer( int channel )
{
    if( channel >= _numBuffers || channel < 0 )
    {
        return false;
    }

  _secondaryBuffers[channel]->_mutex->Lock();
  bool result = (_secondaryBuffers[channel]->_bufferData)->Empty( );
  _secondaryBuffers[channel]->_mutex->Unlock();
    
  return result;
}

/**
  @brief  Creates the capture buffer and initializes audio capture.
*/
bool RtAudioManager::CreateCaptureBuffer( AudioRecordingCallback * recordCallback, int* soundCard, const char *name )
{
  /// Have to init the RtAudioManager first.
  if( !_inited )
  {
      return false;
  }	

  /// Set our recording callback, even if the buffer is already created - this
  /// allows us to change which callback we are using.
  _recordingCallback = recordCallback;

  /// Buffer already created - don't create again.
  if( _captureInited )
  {
      return true;
  }

  //const ALCchar* captureDeviceString = alcGetString( NULL, ALC_CAPTURE_DEFAULT_DEVICE_SPECIFIER );

  //CheckALCError(_device);

  // Buffer size is in samples, NOT in bytes.
  //_captureDevice = alcCaptureOpenDevice( captureDeviceString, _captureSampleRate, AL_FORMAT_MONO16, (_recordBufferLength / 2) );

  _captureInited = true;

  return true;
}

/**
  @brief  Closes the capture buffer and sets the initialized flag to false.
*/
bool RtAudioManager::DeleteCaptureBuffer()
{
  //alcCaptureCloseDevice( _captureDevice );
  _captureInited = false;
  return true;
}

/**
  @brief  Returns the sample rate of an individual channel.
*/
unsigned int RtAudioManager::GetSampleRate(int channel)
{
  if( channel < 0 || channel >= _numBuffers )
  {
      return false;
  }

  return _secondaryBuffers[channel]->_sampleRate;
}

/**
  @brief  Returns the playback sample rate of the primary buffer.
*/
unsigned int RtAudioManager::GetSampleRate( void )
{
  return _playbackSampleRate;
}

/**
  @brief  Returns the capture sample rate.
*/
unsigned int RtAudioManager::GetRecordSampleRate()
{
  return _captureSampleRate;
}

/**
  @brief  Sets the playback sample rate for a single secondary buffer.
*/
bool RtAudioManager::SetSampleRate( int channel, unsigned int frequency )
{
  if( channel < 0 || channel >= _numBuffers )
  {
      return false;
  }
  _secondaryBuffers[channel]->_mutex->Lock();

  _secondaryBuffers[channel]->_sampleRate = frequency;

  // Changing our playback sample rate forces us to recalculate our chunk size.
  _secondaryBuffers[channel]->_chunkSize = (int)(_bufferLatency * _secondaryBuffers[channel]->_sampleRate * _secondaryBuffers[channel]->_bytesPerSample);
  // Chunk size MUST be an even number of bytes.
  _secondaryBuffers[channel]->_chunkSize &= ~1;

  _secondaryBuffers[channel]->_mutex->Unlock();
  return true;
}

/**
  @brief  Sets the sample rate for captured data.
  @note
  We do not actually change our record buffer's frequency, just the data rate at
  which outgoing data is being sent.
*/
bool RtAudioManager::SetRecordSampleRate( unsigned int frequency )
{
  _captureSampleRate = frequency;

  return true;
}

/**
  @brief  Sets the master volume level.
  @note
  Volume levels range from -9600 to 0.
*/
void RtAudioManager::SetMasterVolume( int volume )
{
  if( volume > 0 || volume < -9600 )
  {
      return;
  }

  _masterVolume = volume;
}

/**
  @brief  Sets the volume value for a single secondary buffer.
*/
void RtAudioManager::SetVolume( int channel, int volume )
{
  if( channel < 0 || channel >= _numBuffers )
  {
      return;
  }

  _secondaryBuffers[channel]->_mutex->Lock();
  _secondaryBuffers[channel]->_volume = volume;
  _secondaryBuffers[channel]->_mutex->Unlock();
}

/**
  @brief  Sets the pan value for a single secondary buffer.
*/
void RtAudioManager::SetPan( int channel, int pan )
{
  if( channel < 0 || channel >= _numBuffers )
  {
      return;
  }

  _secondaryBuffers[channel]->_mutex->Lock();
  _secondaryBuffers[channel]->_pan = pan;
  _secondaryBuffers[channel]->_mutex->Unlock();
}

/**
  @brief  Returns volume value for a single secondary buffer.
  @note
  Volume values range from -9600 to 0 and represent hundredths of a decibel
  attenuation.
*/
int RtAudioManager::GetVolume( int channel )
{
  if( channel < 0 || channel >= _numBuffers )
  {
      return 0;
  }

  _secondaryBuffers[channel]->_mutex->Lock();
  int volume = _secondaryBuffers[channel]->_volume;
  _secondaryBuffers[channel]->_mutex->Unlock();
  return volume;

}

/**
  @brief  Returns pan value for a single secondary buffer.
  @note
  Pan values range from -1000 to 1000.
*/
int RtAudioManager::GetPan( int channel )
{
  if( channel < 0 || channel >= _numBuffers )
  {
      return 0;
  }

  _secondaryBuffers[channel]->_mutex->Lock();
  int pan = _secondaryBuffers[channel]->_pan;
  _secondaryBuffers[channel]->_mutex->Unlock();

  return pan;
}

/**
     @brief     Starts the playback of all secondary buffers.
     Starts the playback of all secondary buffers.
     @return
     bool value, only false if Init() has not been called.
     @note
     Init() must be called before playback can be started.
*/
bool RtAudioManager::Play()
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
  @brief  Sets a secondary buffer as playing and starts primary buffer if needed.
*/
bool RtAudioManager::Play( int channel )
{
  if( channel >= _numBuffers || channel < 0 )
  {
      return false;
  }

  // Get the sample rate so we know how much data to fill the buffer with.
  _secondaryBuffers[channel]->_mutex->Lock();
//  int sampleRate = _secondaryBuffers[channel]->_sampleRate;
  _secondaryBuffers[channel]->_mutex->Unlock();

  // Prime the buffer so we can be sure not to start off with crap.  As soon as _isPlaying is set to true
  // this sucker could be off and running.
  EmptyBuffer( channel );
  //FillBufferSilence( channel, (int)(sampleRate * _bufferLatency) );

  // It is the responsiblity of buffer monitoring to make sure that data is actually
  // in the secondary buffer and to start copying it into the primary buffer.
  _secondaryBuffers[channel]->_mutex->Lock();
  _secondaryBuffers[channel]->_isPlaying = true;
  _secondaryBuffers[channel]->_mutex->Unlock();

  // Make sure the master buffer is playing.
  //ALint state = 0;
  //alGetSourcei( _playbackHandle, AL_SOURCE_STATE, &state ); // Will be: AL_PLAYING, AL_PAUSED, AL_STOPPED, or AL_INITIAL
  /*switch( state )
  {
  case AL_PLAYING:
      break;
  case AL_PAUSED:
      {
        // Clear any buffers that may be in the queue.
        //ALint queued;
        //alGetSourcei(_playbackHandle, AL_BUFFERS_QUEUED, &queued);
        //while(queued--)
        //{
            //ALuint buffer;

            //alSourceUnqueueBuffers(_playbackHandle, 1, &buffer);
        //}

        int chunkSize = (int)(_playbackSampleRate * _bufferLatency * BYTES_PER_WORD * STEREO );
        char* data = new char[chunkSize];
        memset( data, 0, chunkSize );
        //alBufferData( _playbackBuffers[0], _format, data, chunkSize, _playbackSampleRate );
        //alBufferData( _playbackBuffers[1], _format, data, chunkSize, _playbackSampleRate );
        //alSourceQueueBuffers( _playbackHandle, 2, _playbackBuffers );
        delete[] data;

      //alSourcePlay( _playbackHandle );
      }
      break;
  case AL_STOPPED:
      {
        // Clear any buffers that may be in the queue.
        //ALint queued;
        //alGetSourcei(_playbackHandle, AL_BUFFERS_QUEUED, &queued);
        //while(queued--)
        //{
            //ALuint buffer;

            //alSourceUnqueueBuffers(_playbackHandle, 1, &buffer);
        //}

        int chunkSize = (int)(_playbackSampleRate * _bufferLatency * BYTES_PER_WORD * STEREO );
        char* data = new char[chunkSize];
        memset( data, 0, chunkSize );
        //alBufferData( _playbackBuffers[0], _format, data, chunkSize, _playbackSampleRate );
        //alBufferData( _playbackBuffers[1], _format, data, chunkSize, _playbackSampleRate );
        //alSourceQueueBuffers( _playbackHandle, 2, _playbackBuffers );
        delete[] data;

      alSourcePlay( _playbackHandle );
      }
      break;
  case AL_INITIAL:
      {
        // Clear any buffers that may be in the queue.
        //ALint queued;
        //alGetSourcei(_playbackHandle, AL_BUFFERS_QUEUED, &queued);
        //while(queued--)
        //{
            //ALuint buffer;

            //alSourceUnqueueBuffers(_playbackHandle, 1, &buffer);
        //}

        int chunkSize = (int)(_playbackSampleRate * _bufferLatency * BYTES_PER_WORD * STEREO );
        char* data = new char[chunkSize];
        memset( data, 0, chunkSize );
        //alBufferData( _playbackBuffers[0], _format, data, chunkSize, _playbackSampleRate );
        //alBufferData( _playbackBuffers[1], _format, data, chunkSize, _playbackSampleRate );
        //alSourceQueueBuffers( _playbackHandle, 2, _playbackBuffers );
        FillBufferSilence( 0, chunkSize );
        FillBufferSilence( 1, chunkSize );
        FillBufferSilence( 2, chunkSize );
        FillBufferSilence( 3, chunkSize );
        delete[] data;

      //alSourcePlay( _playbackHandle );
      }
      break;
  default:
      break;
  }*/

  return true;
}

/**
 @brief  Stops all secondary buffers from playing and then stops the primary buffer.
*/
bool RtAudioManager::Stop( )
{
  if( !_inited )
  {
      return false;
  }

  // Stop secondary buffers.
  int count;
  for( count = 0; count < _numBuffers; count++ )
  {
      Stop( count );
  }

  // Stop primary source.
  //alSourceStop(_playbackHandle);

  // Dequeue any remaining buffers.
  //ALint queued;
  //alGetSourcei(_playbackHandle, AL_BUFFERS_QUEUED, &queued);
  //while(queued--)
  //{
    //ALuint buffer;

    //alSourceUnqueueBuffers(_playbackHandle, 1, &buffer);
  //}

  return true;
}

/**
 @brief  Stops playing of an individual secondary buffer.
*/
bool RtAudioManager::Stop( int channel )
{
  if( channel >= _numBuffers )
  {
      return false;
  }

  // It is the responsiblity of buffer monitoring to make sure that we stop
  // copying data from a secondary buffer that is no longer playing.
  _secondaryBuffers[channel]->_mutex->Lock();
  _secondaryBuffers[channel]->_isPlaying = false;
  _secondaryBuffers[channel]->_mutex->Unlock();
  EmptyBuffer( channel );

  return true;
}

/**
  @brief  Monitors the record buffer and processes data if necessary.
  This is called continuously by the run() method.
*/
int RtAudioManager::MonitorCaptureBuffer()
{
//  int err = 0;;
  int samplesAvailable = 0;

  if( !_inited || !_captureInited || !_capturing )
  {
      return false;
  }

  // Limit our capture chunk size to the largest size we can actually send out on the network.
  int captureChunkSize = (int)(_captureSampleRate * _bufferLatency); // Calculated in samples.
  if( captureChunkSize > (CAPTURE_CHUNK_SIZE / BYTES_PER_WORD ) ) // Compared in samples.
  {
      captureChunkSize = (CAPTURE_CHUNK_SIZE / BYTES_PER_WORD ); // Calculated in samples.
  }

  //alcGetIntegerv( _captureDevice, ALC_CAPTURE_SAMPLES, sizeof(int), &samplesAvailable );

  // If we don't have a whole chunk, wait until next pass.
  if( samplesAvailable < captureChunkSize )
  {
      return true;
  }

  unsigned char data[CAPTURE_CHUNK_SIZE];
  memset( data, 0, CAPTURE_CHUNK_SIZE );
  //alcCaptureSamples( _captureDevice, data, captureChunkSize );

  // OK TO HERE
  if( _recordingCallback != NULL )
  {
      int resampleTarget = captureChunkSize * _captureSampleRate / MAX_SAMPLE_RATE;

      // THIS line of code is responsible for all of the pops and clicks.
      _recordResampler.Resample( data, captureChunkSize, resampleTarget, BYTES_PER_WORD, 1 );

      _recordingCallback->ForwardRecordedData( data, (resampleTarget * BYTES_PER_WORD), _captureSampleRate);
  }

  return true;
}

/**
     @brief     Main thread function for RtAudioManager.
     This is the main thread function for RtAudioManager.  It calls functions to monitor
     the primary, secondary, and capture buffers.
     @return
     This function should not only return when the class is terminated.
     @note
     This function includes a half-millisecond microsleep call.
*/
void* RtAudioManager::Entry()
{
//  int count;
  while(!TestDestroy())
  { 
      // Grab and process any captured data.

      MonitorCaptureBuffer();

      // Call MonitorBuffer() here to make sure we fill the secondary buffers with
      // silence if nothing is being received.

      MonitorBuffer();

      // Now that we're sure we have something in our secondary buffers we can mix in
      // some data if we need to.

      ProcessSoundBuffer();

      // Sleep for 3 milliseconds.  That should be enough to get us at least 32 bytes of data at 8khz 16 bit mono.
      // At 44.1 stereo we would get about 11 times that (352+).  Adjust if necessary for slower machines and/or
      // machines running many applications.
#ifdef linux
      usleep( 3000 ); // More responsive because we have a more precise timer readily available.
#else
      Sleep( 3 );
#endif
  } 
  return NULL;
}


/**
     @brief     Starts recording audio from the sound card.
     Starts recording audio from the sound card.
     @return
     bool value indicating whether it was able to start capturing.
     @note
     Init() and CreateCaptureBuffer() must be called before capture can be started.
*/
bool RtAudioManager::StartCapture( )
{
  /// No capture without first initializing RtAudioManager and the capture buffer.
  if( !_inited || !_captureInited )
  {
      return false;
  }

  //alcCaptureStart( _captureDevice );

  _capturing = true;

  return true;
}

/**
     @brief     Stops capturing audio from the sound card.
     Stops capturing audio from the sound card.
     @return
     bool value, always returns true.
*/
bool RtAudioManager::StopCapture( )
{
  //alcCaptureStop( _captureDevice );

  _capturing = false;

  return true;
}

/**
 @brief  Checks whether an individual secondary buffer is playing.
*/
bool RtAudioManager::IsBufferPlaying( int channel )
{
  if( !_inited || channel < 0 || channel >= _numBuffers )
  {
      return false;
  }

  // Returns the status of a secondary buffer.
  _secondaryBuffers[channel]->_mutex->Lock();
  bool playing = _secondaryBuffers[channel]->_isPlaying;
  _secondaryBuffers[channel]->_mutex->Unlock();

  return playing;
}

/**
     @brief     Monitors secondary buffers and pads them with silence if necessary.
     This function is here to monitor the secondary sound buffers and add silence ot them if
     necessary in order to avoid underruns, which tend to sound like pops, clicks, and static.
     It will keep at least one "chunk" of data in a secondary buffer, a chunk being the size
     of a piece of data normally read to the primary buffer.
     @return
     No return values.
     @note
     The amount of data that MonitorBuffer tries to keep in a secondary buffer is dependent upon
     the buffer latency value.
*/
void RtAudioManager::MonitorBuffer()
{
  int count;
  int readAvail;
  int chunkSize;
  for( count = 0; count < _numBuffers; count++ )
  {
    _secondaryBuffers[count]->_mutex->Lock();
    bool state = _secondaryBuffers[count]->_isPlaying;
    _secondaryBuffers[count]->_mutex->Unlock();
    if( state == false )
    {
	    continue;
    }
    _secondaryBuffers[count]->_mutex->Lock();
    // Our buffer needs silence if we have less than one record chunk of data left in it.
    readAvail = (_secondaryBuffers[count]->_bufferData)->GetReadAvail();
    chunkSize = (int)_secondaryBuffers[count]->_chunkSize;
    _secondaryBuffers[count]->_mutex->Unlock();
    if( readAvail < chunkSize )
	{
	  //FillBufferSilence( count, chunkSize );
	}
  }

  return;
}

/**
  @brief  Sets the latency of buffers in milliseconds.
  Sets the latency for primary, secondary, and capture buffers in milliseconds.
   @note
  The playback latency will be twice the buffer latency value because it gets the
  delay once for secondary buffer monitoring and once for primary buffer monitoring.
*/
void RtAudioManager::SetBufferLatency( int msec )
{
  // Buffer length must be nonzero and less than one second.
  if( msec <= 0 || msec >= 1000 )
  {
      return;
  }
  // This would be EXTREMELY non-thread-safe if called while the app was running.
  //
  // Any dialog or entry point that allows changing this number should be grayed
  // or disabled while the app is running (such as how it is done in the Digital Radio).
  _bufferLatency = (double)msec / 1000.0;

  int channel;
  for( channel = 0; channel < _numBuffers; channel++ )
  {
      _secondaryBuffers[channel]->_mutex->Lock();
      _secondaryBuffers[channel]->_chunkSize = (int)(_bufferLatency * _secondaryBuffers[channel]->_sampleRate * _secondaryBuffers[channel]->_bytesPerSample);
      // Make it an even number.
      _secondaryBuffers[channel]->_chunkSize &= ~1;
      _secondaryBuffers[channel]->_mutex->Unlock();
  }

  return;
}

/**
 @brief  Grabs data from the capture buffer and forwards it to the appropriate function.
*/
bool RtAudioManager::ProcessCapturedData()
{
	// We received a buffer notification.  Grab a chunk of data and forward it on to the function that
	// will process it.

	return true;
}

int RtAudioManager::GetPeak( int channel )
{
    if( channel < 0 || channel >= _numBuffers )
    {
        return 0;
    }

    _secondaryBuffers[channel]->_mutex->Lock();
    int peak = _secondaryBuffers[channel]->_peak;
    _secondaryBuffers[channel]->_mutex->Unlock();
    return peak;
}

int RtAudioManager::GetNumSamplesQueued(int channel)
{
	if( channel < 0 || channel >= _numBuffers )
	{
		return 0;
	}
	int value;
	_secondaryBuffers[channel]->_mutex->Lock();
	value = _secondaryBuffers[channel]->_bufferData->GetReadAvail();
	_secondaryBuffers[channel]->_mutex->Unlock();
	return value;
}

int RtAudioManager::GetWriteBytesAvailable(int channel)
{
	if( channel < 0 || channel >= _numBuffers )
	{
		return 0;
	}
	int value;
	_secondaryBuffers[channel]->_mutex->Lock();
	value = _secondaryBuffers[channel]->_bufferData->GetWriteAvail();
	_secondaryBuffers[channel]->_mutex->Unlock();
	return value;
}

/*bool RtAudioManager::MixAudio(ALuint workingBuffer)
{
  int writePos = 0;     // Write positions of the buffer.
  int counter = 0;      // Data iterator.
  int bytesWritten = 0;
  int channel = 0;      // Channel iterator.

  // Statistical data on pauses, re-inits, underruns, overflows, etc.
  static int nodata = 0;
  static int nobytesread = 0;
  static int misreads = 0;

  // Calculate the size of buffer we need to mix a chunk of 16-bit stereo data for the length of time
  // defined by our latency and sample rate.
  int maxBufferSize = (int)(_playbackSampleRate * 2 * _bufferLatency * _playbackByteAlign);
  maxBufferSize &= ~3;
  // Buffer for a single channel's data.
  unsigned char* channelData = new unsigned char[maxBufferSize];
  memset( channelData, 0, maxBufferSize );
  // Buffer for mixed data.
  unsigned char *copyBuffer = new unsigned char[maxBufferSize];
  memset( copyBuffer, 0, maxBufferSize );
  int bytesRead;

  // Get our volume modifier per-channel.
  double* rightVolumeAdjustment = new double[_numBuffers];
  double* leftVolumeAdjustment = new double[_numBuffers];
  CalculateChannelVolume(leftVolumeAdjustment, rightVolumeAdjustment);

  // Get data from our secondary buffers and mix it all together.
  for( channel = 0; channel < _numBuffers; channel++ )
  {
      // Tracking for V/U meters.
      int maxValue = 0;

      // Don't mix it if it isn't playing.
      _secondaryBuffers[channel]->_mutex->Lock();
      bool status = _secondaryBuffers[channel]->_isPlaying;
      _secondaryBuffers[channel]->_mutex->Unlock();
      if( status == false )
      {
	    continue;
      }

      // Set set our chunk size and grab a chunk of data.  Make sure we are requesting an
      // even number of bytes.  Not doing so would hose every other chunk of data.
      _secondaryBuffers[channel]->_mutex->Lock();
      // Reset peaked data - this is a per-chunk test.
      int bytesRequested = (int)(_secondaryBuffers[channel]->_sampleRate * _bufferLatency * _secondaryBuffers[channel]->_bytesPerSample );
      bytesRequested &= ~1;

      bytesRead = (_secondaryBuffers[channel]->_bufferData)->Read( channelData, bytesRequested );
      _secondaryBuffers[channel]->_mutex->Unlock();

#ifdef _DEBUG
  // Log in debug mode only.
  FILE* fp;
  char filename[64];
  memset( filename, 0, 64 );
  _snprintf( filename, 64, "getdatafrombuffer%d.raw", channel );
  if( (fp = fopen( filename, "ab" ) ))
  {
     fwrite( channelData, bytesRead, 1, fp );
     fclose( fp );
  }
#endif


      // We can't mix a channel if it doesn't give us any data.  If we get nothing, we fake it
      // with blank data.
      if( bytesRead == 0 )
      {
          nobytesread++;
          memset( channelData, 0, maxBufferSize );
		  continue;
      }
      else if( bytesRead != bytesRequested )
      {
		  int difference = bytesRequested - bytesRead;
          misreads++;
      }

      // Add our result to the copy buffer.
      //
      // Note that with four channels being mixed down, it would be very easy to get some ugly
      // digital clipping due to volume overflow.  If it sounds bad, turn down the master volume.
      //
      // Cast these to two-byte shorts in order to allow ourselves to multiply each sample by a float without
      // breaking things.  Things will break if a short isn't two bytes.
      short* sampleBuffer = (short *)channelData;
      short* shortCopyBuffer = (short *)copyBuffer;
	  int channelOffset = channel % 2;
      for( counter = 0; counter < (bytesRead / 2); counter++ )
      {
          writePos = counter * 2 + channelOffset;
          if( abs(sampleBuffer[counter]) > maxValue )
          {
			maxValue = abs(sampleBuffer[counter]);
          }
          shortCopyBuffer[writePos] += (sampleBuffer[counter] * leftVolumeAdjustment[channel] );
      }

	  // Set the number of bytes written to the max number of bytes written for any channel.
	  if( bytesWritten < bytesRead)
	  {
		bytesWritten = bytesRead;
	  }
      _secondaryBuffers[channel]->_mutex->Lock();
      _secondaryBuffers[channel]->_peak = maxValue;
      _secondaryBuffers[channel]->_mutex->Unlock();
  } // Cycle through channels.
  delete[] leftVolumeAdjustment;
  delete[] rightVolumeAdjustment;
  delete[] channelData;

  // If we wrote no data, set our write length to the length of the blank buffer intead
  // of that of the number of bytes we've written.  This relies on the channelData being
  // initially memset to all zeroes.
  if( bytesWritten == 0 )
  {
      nodata++;
      bytesWritten = maxBufferSize;
  }

  // Put it in the buffer
  //
  // We take the number of bytes of data in our mixing buffer and divide it by the two
  // bytes per sample we are using and two channels for the number of frames.
#ifdef _DEBUG
  // Log in debug mode only.
  FILE* fp;
  if( (fp = fopen( "soundcard_data_16signed_stereo.raw", "ab" ) ))
  {
     fwrite( copyBuffer, bytesWritten, 1, fp );
     fclose( fp );
  }
#endif

  // Write the data to the buffer.  Since this is a stereo buffer we can just dump it right in.
  //alBufferData(workingBuffer, _format, copyBuffer, bytesWritten, _playbackSampleRate );
  //alSourceQueueBuffers( _playbackHandle, 1, &workingBuffer);

  // Free the buffer we were sending to snd_pcm_writei
  delete[] copyBuffer;

  RestartBufferIfNecessary();

  return true;
}*/

// After we've put data in the buffer, we may need to restart it, usually in the case
// of an underrun, etc.
void RtAudioManager::RestartBufferIfNecessary()
{
  // Statistical data on pauses, re-inits, underruns, overflows, etc.
  static int pauserestarts = 0;
  static int initrestarts = 0;
  static int stoprestarts = 0;
  //ALint state = 0;

  //alGetSourcei( _playbackHandle, AL_SOURCE_STATE, &state ); // Will be: AL_PLAYING, AL_PAUSED, AL_STOPPED, or AL_INITIAL
  // Restart the buffer if it has stopped.
  /*if( state != AL_PLAYING )
  {
      if( state == AL_PAUSED )
      {
          pauserestarts++;
      }
      else if( state == AL_STOPPED )
      {
          stoprestarts++;
      }
      else if( state == AL_INITIAL )
      {
          initrestarts++;
      }
      alSourcePlay( _playbackHandle );
  }*/
}

// Calculates the volume on each channel based on volume, pan, and master volume settings.
void RtAudioManager::CalculateChannelVolume(double * rightVolumeAdjustment, double * leftVolumeAdjustment)
{
  int channel = 0;

  // Set up our volume+pan multipliers for each channel to make this run a little quicker.
  for( channel = 0; channel < _numBuffers; channel++ )
  {
      // We are using pan to attenuate the channel that we're panning away from, but we are not increasing
      // the volume on the channel we've panned toward.  Doing so would put us in danger of digital clipping
      // unless we limit the volume adjustment values to 1.0.
      _secondaryBuffers[channel]->_mutex->Lock();
      if( _secondaryBuffers[channel]->_pan < 0 )
      {
	    leftVolumeAdjustment[channel] = ((_secondaryBuffers[channel]->_volume + 9600.0) / 9600.0) * ((_masterVolume + 9600.0) / 9600.0f);
	    rightVolumeAdjustment[channel] = ((_secondaryBuffers[channel]->_pan + 1000.0) / 1000.0) *
	      ((_secondaryBuffers[channel]->_volume + 9600.0) / 9600.0) * ((_masterVolume + 9600.0) / 9600.0);
      }
      else
      {
	    leftVolumeAdjustment[channel] = ((_secondaryBuffers[channel]->_pan * -1.0f + 1000.0 ) / 1000.0) *
	      ((_secondaryBuffers[channel]->_volume + 9600.0) / 9600.0 ) * ((_masterVolume + 9600.0) / 9600.0);
	    rightVolumeAdjustment[channel] = ((_secondaryBuffers[channel]->_volume + 9600.0) / 9600.0) * ((_masterVolume + 9600.0) / 9600.0f);
      }
      _secondaryBuffers[channel]->_mutex->Unlock();
  }
}

//// Changes data from the channel sample rate to our playback sample rate.
//int RtAudioManager::ResampleChunk(unsigned char* channelData, int channelNumber, int bytesRead, int bytesRequested)
//{
//      // Compare the number of samples received with the number requested and resample if necessary.
//      int targetSamples;
//	  // Calculate the number of samples needed for a stereo channel given our latency and sample rate.
//      if( bytesRead == bytesRequested )
//      {
//        targetSamples = (int)(_playbackSampleRate * _bufferLatency) * 2;
//      }
//      else
//      {
//        targetSamples = (int)((_playbackSampleRate * _bufferLatency) * bytesRead / bytesRequested ) * 2;
//      }
//      // Make sure it is an even number - not necessary because we can have an odd number of samples.
//      // targetSamples &= ~1;
//      // Resample our secondary buffer's data to match our primary buffer's sample rate.
//      // [channeldata must be at least as large as resultingNumSamples * bytesPerSample]
//      // We could bypass this step if our secondary buffer's sample rate matched that of
//      // the primary buffer.
//
//	#ifdef _DEBUG
//	  FILE* fp;
//	  char filename[64];
//	  memset( filename, 0, 64 );
//	  _snprintf( filename, 64, "preresample_data%d.raw", channelNumber );
//	  if( (fp = fopen( filename, "ab" ) ))
//	  {
//	      fwrite( channelData, bytesRead, 1, fp );
//	      fclose( fp );
//	  }
//	#endif
//      
//      _secondaryBuffers[channelNumber]->_mutex->Lock();
//      _secondaryBuffers[channelNumber]->_resampler.Resample( channelData, (bytesRead / _secondaryBuffers[channelNumber]->_bytesPerSample),
//		  targetSamples, _secondaryBuffers[channelNumber]->_bytesPerSample, 1 );
//      _secondaryBuffers[channelNumber]->_mutex->Unlock();
//
//	  return targetSamples;
//
//	// Log resampled data for troubleshooting purposes.
//	#ifdef _DEBUG
//	  memset( filename, 0, 64 );
//	  _snprintf( filename, 64, "resampled_data%d.raw", channelNumber );
//	  if( (fp = fopen( filename, "ab" ) ))
//	  {
//	      fwrite( channelData, (targetSamples * _secondaryBuffers[channelNumber]->_bytesPerSample), 1, fp );
//	      fclose( fp );
//	  }
//	#endif
//
//}
