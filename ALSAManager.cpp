
#if !defined( WIN32 )
#include <iostream>
using namespace std;

#include "ALSAManager.h"

/// Define the size of our secondary buffer in bytes.
#define SECONDARY_BUFFER_SIZE 32768
#define CAPTURE_CHUNK_SIZE 1400
/// Used for resampling calculations and buffer info.
#define MAX_SAMPLE_RATE 44100

/**
     @brief     Constructor, sets initial values for internal data.
     Constructor, sets initial values for internal data.
     @return
     void
*/
ALSAManager::ALSAManager(int numBuffers)
{
  _playbackHandle = NULL;
  _capturing = false;
  _captureSampleRate = 44100;
  _numBuffers = numBuffers;
  /// These are the default values that we record and play at.  Other values will be resampled
  /// to match these.  It is the most widely-supported sample rate and should work on pretty much
  /// all systems.
  _playbackSampleRate = 44100;
  _playbackByteAlign = BYTES_PER_WORD;
  /// Used to keep track of the number of frames in our playback buffer.
  _playbackFrames = 0;
  /// We kind of pulled this number out of thin air - 20 chunks of data in our record buffer.
  _recordBufferLength = (int)(_captureSampleRate * _bufferLatency * BYTES_PER_WORD * 20);
  _captureBuffer = new char[_recordBufferLength];
  int count;
  /// Create volume and pan values.
  for( count = 0; count < _numBuffers; count++ )
    {
      _secondaryBuffers.push_back( new SecondaryBuffer );
      _secondaryBuffers[count]->_volume = 0;
      _secondaryBuffers[count]->_pan = 0;
      _secondaryBuffers[count]->_isPlaying = false;
      _secondaryBuffers[count]->_bytesPerSample = BYTES_PER_WORD;
      _secondaryBuffers[count]->_mutex = new DSSystem::CriticalSection;
      _secondaryBuffers[count]->_bufferData = new DSUtil::RingBuffer( SECONDARY_BUFFER_SIZE );
      _secondaryBuffers[count]->_sampleRate = 44100;
      _secondaryBuffers[count]->_chunkSize = _bufferLatency * _secondaryBuffers[count]->_sampleRate * _secondaryBuffers[count]->_bytesPerSample;
      /// Chunk size MUST be an even number of bytes.
      _secondaryBuffers[count]->_chunkSize &= ~1;
    }
	/// Higher thread priority in order to watch the sound buffers better.
	setPriorityAboveNormal();
}

/**
     @brief     Destructor, cleans up allocations and uninitializes audio.
     Destructor, cleans up allocations and uninitializes audio.
     @return
     void
*/
ALSAManager::~ALSAManager()
{
  if( _inited )
    {
      UnInit();
    }
  //cout << "~ALSAManager: Deleting channel-related Data." << endl;
  int count;
  for( count = _numBuffers - 1; count > -1; count-- )
    {
      delete _secondaryBuffers[count]->_bufferData;
      delete _secondaryBuffers[count]->_mutex;
      delete _secondaryBuffers[count];
    }
  delete[] _captureBuffer;
  //cout << "~ALSAManager: Done deleting channel-related data" << endl;
}

/**
 @brief  Enumerates sound devices in the current system.
 This first tries to call snd_names_list, a function that may not be able to get
 a list of cards depending on the system (function is quite new).  If it is unable
 to get a list, it tries brute force by iterating through the possible devices and
 tries to open them one by one.  The fallback method can be ugly due to the generation
 of "no such device" errors, but it works.
*/
void ALSAManager::Enumerate( void (*callback)(char *) )
{
  snd_devname_t *cardList, *cardIterator;
  int result = snd_names_list( "pcm", &cardList ); 
  if( result < 0 )
    {
      /// Make sure we aren't leaking memory.
      snd_names_list_free( cardList );
      cout << "Error getting snd_names_list - error " << result << " (" << snd_strerror( result ) << ")" << endl;
      /// Enumerate cards using brute force - the BAD way to do things:
      int i;
      int j;
      /// This was 8/8.  Nobody is going to have a system with 8 soundcards and 8 devices per card.  Limiting to 4/6.
      int MAX_CARDS = 4;
      int MAX_DEVS = 6;
      snd_pcm_t* handle;
      char device[1024];
      for(i=0;i<MAX_CARDS;i++)
	{
	  for(j=0;j<MAX_DEVS;j++)
	    {
	      sprintf(device,"plughw:%d,%d",i,j);
	      if(snd_pcm_open(&handle,device,SND_PCM_STREAM_PLAYBACK,0)==0)
		{
		  // printf("%s exists\n",device);
		  /// We can use the name we have because we've formatted it above.
		  (*callback)( device );
		  snd_pcm_close(handle);
		}
	    }
	}
      return;
    }

  /// If the call didn't fail we can iterate through the list and print what we have.
  for( cardIterator = cardList; cardIterator; cardIterator = cardList->next )
    {
      //cout << "Found sound device:" << endl;
      //cout << "Name: " << cardIterator->name << endl;
      //cout << "Comment: " << cardIterator->comment << endl;
      /// We can use this because it is an alias for the plughw. [I hope]
      (*callback)( cardIterator->name );
    }
  snd_names_list_free( cardList );
}

/**
     @brief     Initializes the ALSAManager.
     Initializes the ALSAManager.
     @return
     returns false if ALSAManager could not be initialized and true if it could be.
     @note
     The ParentWindow argument should be passed in as NULL.  It is only included for
     DXAudioManager interface compatibility.
*/
bool ALSAManager::Init(void *parentWindow, int* soundCard, const char *name)
{
  int i;
  int err;
  short buf[128];
  snd_pcm_hw_params_t *hw_params;

  if( name == NULL )
  {
      cout << "Init: Sound device name is NULL, opening default device" << endl;
      /// Try to open the default device
      err = snd_pcm_open( &_playbackHandle, "plughw:0,0", SND_PCM_STREAM_PLAYBACK, 0 );
  }
  else
  {
      cout << "Init: Sound device name is not null, opening: " << name << endl;
      /// Open the device we were told to open.
      err = snd_pcm_open (&_playbackHandle, name, SND_PCM_STREAM_PLAYBACK, 0);
  }	

  //cout << "Init: checking error for snd_pcm_open" << endl;
  if( err < 0 )
  {
      cout << "Init: cannot open audio device " << name << " (" << snd_strerror (err) << ")" << endl;
      return false;
  }
		   
  //cout << "Init: snd_pcm_hw_params_malloc" << endl;
  if ((err = snd_pcm_hw_params_malloc (&hw_params)) < 0)
  {
      cout << "Init: cannot allocate hardware parameter structure (" << snd_strerror (err) << ")" << endl;
      return false;
  }
				 
  //cout << "snd_pcm_hw_params_any" << endl;
  if ((err = snd_pcm_hw_params_any (_playbackHandle, hw_params)) < 0)
  {
      cout << "Init: cannot initialize hardware parameter structure (" << snd_strerror (err) << ")" << endl;
      return false;
  }

  /// Enable resampling.
  unsigned int resample = 1;
  //cout << "Init: snd_pcm_hw_params_set_rate_resample" << endl;
  err = snd_pcm_hw_params_set_rate_resample(_playbackHandle, hw_params, resample);
  if (err < 0)
  {
      cout << "Init: Resampling setup failed for playback: " << snd_strerror(err) << endl;
      return err;
  }
	
  //cout << "Init: snd_pcm_hw_params_set_access" << endl;
  if ((err = snd_pcm_hw_params_set_access (_playbackHandle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0)
  {
      cout << "Init: cannot set access type (" << snd_strerror (err) << ")" << endl;
      return false;
  }
	
  if ((err = snd_pcm_hw_params_set_format (_playbackHandle, hw_params, SND_PCM_FORMAT_S16_LE)) < 0)
  {
      cout << "Init: cannot set sample format (" << snd_strerror (err) << ")" << endl;
      return false;
  }

  //cout << "snd_pcm_hw_params_set_channels" << endl;
  /// Should this be numBuffers or just two for stereo?
  //if ((err = snd_pcm_hw_params_set_channels (_playbackHandle, hw_params, _numBuffers)) < 0)
  if ((err = snd_pcm_hw_params_set_channels (_playbackHandle, hw_params, 2)) < 0)
  {
      cout << "Init: cannot set channel count (" << snd_strerror (err) << ")" << endl;
      return false;
  }
	
  //cout << "Init: snd_pcm_hw_params_set_rate_near" << endl;
  unsigned int actualRate = _captureSampleRate;
  if ((err = snd_pcm_hw_params_set_rate_near (_playbackHandle, hw_params, &actualRate, 0)) < 0)
  {
      cout << "Init: cannot set sample rate " << _captureSampleRate << ". (" << snd_strerror (err) << ")" << endl;
      return false;
  }
  if( actualRate < _captureSampleRate )
  {
      cout << "Init: sample rate does not match requested rate. (" << _captureSampleRate << " requested, " << actualRate
           << " acquired)" << endl;
  }

  /// Setting the period and buffer size may be completely unnecessary.  Since it doesn't actually work, we'll bypass
  /// it for now.

  //cout << "snd_pcm_hw_params_set_period_size" << endl;
  //if(( err = snd_pcm_hw_params_set_period_size( _captureHandle, hw_params, _recordChunkSize, 0 )) < 0 )
  //{
  //        fprintf( stderr, "cannot set period size (%s)\n", snd_strerror(err));
  //        return false;
  //}


  /// Playback buffer same size as capture buffer.
  //snd_pcm_uframes_t numFrames = _recordChunkSize * _recordChunkTotal;
  //if(( err = snd_pcm_hw_params_set_buffer_size( _playbackHandle, hw_params, numFrames )) < 0 )
  //{
  //        fprintf( stderr, "cannot set buffer size (%s)\n", snd_strerror(err));
  //        return false;
  //}
	
  //cout << "Init: snd_pcm_hw_params" << endl;
  if ((err = snd_pcm_hw_params (_playbackHandle, hw_params)) < 0)
  {
      cout << "Init: cannot set parameters (" << snd_strerror (err) << ")" << endl;
      return false;
  }

  snd_pcm_uframes_t bufferSize;
  snd_pcm_hw_params_get_buffer_size( hw_params, &bufferSize );
  cout << "Init: Buffer size = " << bufferSize << " frames." << endl;
  _playbackFrames = bufferSize;
  cout << "Init: Significant bits for linear samples = " << snd_pcm_hw_params_get_sbits(hw_params) << endl;

  //cout << "Init: snd_pcm_hw_params_free" << endl;
  snd_pcm_hw_params_free (hw_params);
	
  //cout << "Init: snd_pcm_prepare" << endl;
  if ((err = snd_pcm_prepare (_playbackHandle)) < 0)
  {
      cout << "Init: cannot prepare audio interface for use (" << snd_strerror (err) << ")" << endl;
      return false;
  }

  //cout << "Init: Checking parameters of buffer" << endl;
  /// snd_pcm_get_params is apparently too new to be supported in this version of ALSA.  Hopefully a newer
  /// version will be released shortly.  1.0.11.rc3 is where it was added.  We have 1.0.10.
  //snd_pcm_uframes_t bufferSize, periodSize;
  //snd_pcm_get_params( _playbackHandle, &bufferSize, &periodSize );
  //cout << "Init: Buffer size " << bufferSize << " bytes.  Period size " << periodSize << " bytes." << endl;

  _inited = true;

  return true;
}

/**
     @brief     Uninitializes the ALSAManager.
     Uninitializes the ALSAManager.
     @return
     bool value always returns true
*/
bool ALSAManager::UnInit()
{
  snd_pcm_close (_playbackHandle);
  _inited = false;
  return true;
}

/**
     @brief     Mixes secondary buffers into main playback buffer.
     This function mixes data from the secondary buffers into a main stereo buffer,
	 including resampling and calculations for pan and volume settings, and then
	 writes it to the soundcard.  This is the real workhorse of ALSAManager.
     @return
     returns false if the sound has not been initialized or is not playing.
*/
bool ALSAManager::ProcessSoundBuffer( void )
{
  /// Error checking variable.
  int err = 0;
  /// Write positions of the buffer.
  int writePos = 0;
  bool playing = false;
  /// Samples avaialble.
  int pcmreturn = 0;
  /// Channel iterator.
  int channel = 0;
  /// Data iterator.
  int counter = 0;

  /// Check to see whether we actually have anything to do.
  if( !_inited )
  {
      //cout << "ProcessSoundBuffer: Returning because _inited = false" << endl;
      return false;
  }

  /// We only need to monitor the buffer if one of our sound streams is playing.  If it is not,
  /// then we can safely ignore it.
  for( channel = 0; channel < _numBuffers; channel++ )
  {
      _secondaryBuffers[channel]->_mutex->lock();
      if( _secondaryBuffers[channel]->_isPlaying == true )
	{
	  playing = true;
	}
      _secondaryBuffers[channel]->_mutex->unlock();
  }
  /// If we have nothing to monitor, we bail.
  if( playing == false )
  {
      return false;
  }

  /// OK, so we have a buffer that is initialized and playing past this point.  It's time to check
  /// whether it has enough stuff in it and fill it if it doesn't.

  /// Check status of buffer.  Check for underruns and make sure it is running.
  //cout << "ProcessSoundBuffer: Checking state of _playbackHandle: " << endl;
  switch( snd_pcm_state( _playbackHandle ) )
    {
    case SND_PCM_STATE_OPEN:
      cout << "ProcessSoundBuffer: snd_pcm_state_open" << endl;
      break;
    case SND_PCM_STATE_SETUP:
      cout << "ProcessSoundBuffer: snd_pcm_state_setup - calling snd_pcm_prepare()" << endl;
      snd_pcm_prepare( _playbackHandle );
      //cout << "snd_pcm_prepare() called." << endl;
      break;
    case SND_PCM_STATE_PREPARED:
      {
	/// Fill our primary buffer with a bit of silence so give us room to fill it.
	/// Sample rate (44100) * buffer latency (0.05) * 2 (channels) * 2 (bytes per sample)
	int bytesRequired = (int)(_playbackSampleRate * _bufferLatency * STEREO * _playbackByteAlign);
        /// The amount of bytes being put into the playback buffer has to be an even multiple of 4.
        bytesRequired &= ~3;
	//cout << "ProcessSoundBuffer: Filling primary buffer with " << _bufferLatency << " seconds [" << bytesRequired << "bytes] of silence: " << endl;
	unsigned char *silence = new unsigned char[bytesRequired];
	snd_pcm_writei( _playbackHandle, silence, ( bytesRequired / 4 ));
	delete[] silence;
	//cout << "ProcessSoundBuffer: Primary buffer filled with silence.  calling snd_pcm_start" << endl;
	snd_pcm_start( _playbackHandle );
	cout << "ProcessSoundBuffer: snd_pcm_start called." << endl;
	return true;
	break;
      }
    case SND_PCM_STATE_RUNNING:
      /// This is where we want to be.
      //cout << "ProcessSoundBuffer: snd_pcm_state_running" << endl;
      break;
    case SND_PCM_STATE_XRUN:
      {
	cout << "ProcessSoundBuffer: snd_pcm_state_xrun - attempting to recover by calling snd_pcm_prepare." << endl;
	snd_pcm_prepare( _playbackHandle );
	int bytesRequired = (int)(_playbackSampleRate * _bufferLatency * STEREO * _playbackByteAlign);
        /// The amount of bytes being written to the playback buffer must be an even multiple of 4.
        bytesRequired &= ~3;
	//cout << "ProcessSoundBuffer: Filling primary buffer with " << _bufferLatency << " seconds [" << bytesRequired << "bytes] of silence: " << endl;
	unsigned char *silence = new unsigned char[bytesRequired];
	snd_pcm_writei( _playbackHandle, silence, ( bytesRequired / 4 ) );
	delete[] silence;
	//cout << "ProcessSoundBuffer: Primary buffer filled with silence.  calling snd_pcm_start" << endl;
	snd_pcm_start( _playbackHandle );
	/// Sure, we had a glitch, but it's still running, right?
	return true;
	break;
      }
    case SND_PCM_STATE_DRAINING:
      cout << "snd_pcm_state_draining" << endl;
      break;
    case SND_PCM_STATE_PAUSED:
      cout << "snd_pcm_state_paused" << endl;
      break;
    case SND_PCM_STATE_SUSPENDED:
      cout << "snd_pcm_state_suspended" << endl;
      break;
    case SND_PCM_STATE_DISCONNECTED:
      cout << "snd_pcm_state_disconnected" << endl;
      break;
    default:
      cout << "_playbackHandle is in an unknown state." << endl;
    }

  //cout << "ProcessSoundBuffer: snd_pcm_avail update ";
  pcmreturn = snd_pcm_avail_update( _playbackHandle );
  if( pcmreturn < 0 )
  {
      if( pcmreturn == -EPIPE )
      {
        cout << "ProcessSoundBuffer: A buffer underrun (xrun) occurred" << endl;
      }
      else
      {
        cout << "ProcessSoundBuffer: Unknown ALSA avail update return value (" << pcmreturn << ")." << endl;
      }
  }
  else
  {
      //cout <<  "ProcessSoundBuffer: returned " << pcmreturn << " samples in playback buffer" << endl;
      /// Note that _bufferLatency is used for both capture and playback buffers.  We may want
      /// separate variables in the future.
      if( ( _playbackFrames - pcmreturn ) >= (int)(_playbackSampleRate * _bufferLatency * _playbackByteAlign) )
      {
	  /// Sound buffer has enough data for now, nothing else to do./
	  //cout << "ProcessSoundBuffer: No need to write data.  Returning." << endl;
	  return true;
      }
  }

  ///---------------------- STAGE TWO: LET THE REAL WORK BEGIN ---------------------------//

  /// Right now we require that our secondary buffers match format with our primary buffer.
  ///
  /// At some point we're going to have to be able to support mixing different bit and sample
  /// rates into our primary buffer.
  //cout << "ProcessSoundBuffer: Creating variables for mixing" << endl;
  /// Set using the maximum sample rate we can use so we can mix higher sample rates down into
  /// lower sample rates, i.e. 48000 Hz into 22050 Hz.  This is for mono data.
  int maxBufferSize = (int)(MAX_SAMPLE_RATE * _bufferLatency * _playbackByteAlign);
  /// Full Length of copy buffer to be mixed into main playback buffer: samples x time [2205]
  /// x bytes per sample [2] x number of channels [2].  This is for stereo data.
  int fullLength = (int)(_playbackSampleRate * _bufferLatency * STEREO * _playbackByteAlign);
  /// The full length of our buffer MUST be divisible by 4 (2 bytes per sample x 2 channels )
  fullLength &= ~3;
  /// copyBuffer:  temporary back-buffer for mixing that will be copied into audio stream.
  unsigned char *copyBuffer = new unsigned char[fullLength];
  memset( copyBuffer, 0, fullLength );
  float* rightVolumeAdjustment = new float[_numBuffers];
  float* leftVolumeAdjustment = new float[_numBuffers];
  int bytesRead;
  /// We will be using this to grab data that may be of a smaller or larger sample rate than the
  /// playback buffer, but after we resample this buffer will be exactly half the size of "fullLength"
  /// [a mono channel for mixing into stereo].
  ///
  /// This is used both for grabbing the data from the secondary buffer and also passing to the
  /// Resample() function to be filled with interpolated data, thus the data size will vary based
  /// on the sample rates that are being converted to and from but will never be larger than MAX_SAMPLE_RATE.
  unsigned char* channelData = new unsigned char[maxBufferSize];
  memset( channelData, 0, maxBufferSize );

  //cout << "ProcessSoundBuffer: Setting up multipliers for volume and pan" << endl;
  /// Set up our multipliers for each channel to make this run a little quicker.
  for( channel = 0; channel < _numBuffers; channel++ )
  {
      /// We are using pan to attenuate the channel that we're panning away from, but we are not increasing
      /// the volume on the channel we've panned toward.  Doing so would put us in danger of digital clipping
      /// unless we limit the volume adjustment values to 1.0.
      _secondaryBuffers[channel]->_mutex->lock();
      if( _secondaryBuffers[channel]->_pan < 0 )
      {
	  leftVolumeAdjustment[channel] = ((_secondaryBuffers[channel]->_volume + 9600.0f) / 9600.0f) * ((_masterVolume + 9600.0f) / 9600.0f);
	  rightVolumeAdjustment[channel] = ((_secondaryBuffers[channel]->_pan + 1000.0f) / 1000.0f) *
	    ((_secondaryBuffers[channel]->_volume + 9600.0f) / 9600.0f) * ((_masterVolume + 9600.0f) / 9600.0f);
          //cout << "ProcessSoundBuffer: pan > 0: " << _secondaryBuffers[channel]->_pan << ", rightVolumeAdjustment = " << rightVolumeAdjustment[channel] << endl;
      }
      else
      {
	  leftVolumeAdjustment[channel] = ((_secondaryBuffers[channel]->_pan * -1.0f + 1000.0f ) / 1000.0f) *
	    ((_secondaryBuffers[channel]->_volume + 9600.0f) / 9600.0f ) * ((_masterVolume + 9600.0f) / 9600.0f);
          //cout << "ProcessSoundBuffer: pan > 0: " << _secondaryBuffers[channel]->_pan << ", leftVolumeAdjustment = " << leftVolumeAdjustment[channel] << endl;
	  rightVolumeAdjustment[channel] = ((_secondaryBuffers[channel]->_volume + 9600.0f) / 9600.0f) * ((_masterVolume + 9600.0f) / 9600.0f);
      }
      _secondaryBuffers[channel]->_mutex->unlock();
  }

  /// Get data from our secondary buffers and mix it all together.
  //cout << "ProcessSoundBuffer: Getting data from secondary buffers and mixing it" << endl;
  for( channel = 0; channel < _numBuffers; channel++ )
  {
      /// Don't mix it if it isn't playing.
      //cout << "ProcessSoundBuffer: Checking whether buffer " << channel << " is playing" << endl;
      _secondaryBuffers[channel]->_mutex->lock();
      bool status = _secondaryBuffers[channel]->_isPlaying;
      _secondaryBuffers[channel]->_mutex->unlock();
      if( status == false )
      {
	  //cout << "ProcessSoundBuffer: Buffer is not playing.  Skipping it." << endl;
	  continue;
      }
      //cout << "ProcessSoundBuffer: Buffer is playing - reading data from ring buffer for channel " << channel << endl;
      _secondaryBuffers[channel]->_mutex->lock();
      int bytesRequested = (int)(_secondaryBuffers[channel]->_sampleRate * _bufferLatency * _secondaryBuffers[channel]->_bytesPerSample );
      /// Make sure we are requesting an even number of bytes.  Not doing so would hose every other packet.
      bytesRequested &= ~1;
      int bytesPerSample = _secondaryBuffers[channel]->_bytesPerSample;
      bytesRead = (_secondaryBuffers[channel]->_bufferData)->Read( channelData, bytesRequested );
      _secondaryBuffers[channel]->_mutex->unlock();
      FILE* fp;
      //if( (fp = fopen( "readbuffer.raw", "wb" ) ))
      //{
      //  fwrite( channelData, bytesRead, 1, fp );
      //  fclose( fp );
      //}
      //cout << "ProcessSoundBuffer: Read " << bytesRead << " bytes." << endl;

      /// Resample our secondary buffer's data to match our primary buffer's rate if necessary.  Note that normally we will
      /// be resampling from lower to higher rates, but we may go the other way, i.e. from 48Khz to 44.1KHz.
      //if( _secondaryBuffers[channel]->_sampleRate != _playbackSampleRate )
      //cout << "ProcessSoundBuffer: Logging " << bytesRead << " bytes of data read from ring buffer to readbuffer_pre_resample.raw" << endl;

      //if( (fp = fopen( "readbuffer_pre_resample.raw", "ab" ) ))
      //{
      //  fwrite( channelData, bytesRead, 1, fp );
      //  fclose( fp );
      //}

      int targetSamples;
      if( bytesRead == bytesRequested )
      {
         targetSamples = (int)(_playbackSampleRate * _bufferLatency);
      }
      else
      {
         targetSamples = (int)((_playbackSampleRate * _bufferLatency) * bytesRead / bytesRequested );
      }
      /// Make sure it is an even number.
      targetSamples &= ~1;

      /// Resample our secondary buffer's data to match our primary buffer's sample rate.
      _secondaryBuffers[channel]->_resampler.Resample( channelData, (bytesRead / bytesPerSample), targetSamples, bytesPerSample );
      /// [channeldata must be at least as large as resultingNumSamples * bytesPerSample]
	
      /// Add our result to the copy buffer.
      ///
      /// Note that with four channels being mixed down, it would be very easy to get some ugly
      /// digital clipping/aliasing/etc.  If it sounds bad, turn down the volume on what you're putting
      /// into the buffers in the first place.
      ///
      /// Adding a master volume control would make it easier to control any volume overloads.
      ///
      /// For this to not nuke the stack, our _recordChunkSize MUST be an even number.
      //cout << "ProcessSoundBuffer: mixing channel " << channel << endl;
      int count;
      /// Cast these to two-byte shorts in order to allow ourselves to multiply each sample by a float without
      /// breaking things.  Things will break if a short isn't two bytes.
      short* shortChannelData = (short *)channelData;
      short* shortCopyBuffer = (short *)copyBuffer;
      for( count = 0; count < targetSamples; count++ )
      {
          writePos = count * 2;
	  shortCopyBuffer[writePos  ] += (short)(shortChannelData[count] * leftVolumeAdjustment[channel]);
          shortCopyBuffer[writePos+1] += (short)(shortChannelData[count] * rightVolumeAdjustment[channel]);
      }
      //cout << "ProcessSoundBuffer: finished mixing channel " << channel << " with readPos " << count << " and writePos " << writePos << endl;
  } /// Cycle through channels.
  //cout << "deleting leftVolumeAdjustment" << endl;
  delete[] leftVolumeAdjustment;
  //cout << "deleting rightVolumeAdjustment" << endl;
  delete[] rightVolumeAdjustment;
  //cout << "ProcessSoundBuffer: deleting channelData." << endl;
  /// Deleting this makes everything explode if we've violated our buffer.
  delete[] channelData;

  /// Put it in the buffer
  //cout << "ProcessSoundBuffer: Entering while loop for write to soundcard" << endl;
  while( 1 )
  {
      //cout << "ProcessSoundBuffer: calling snd_pcm_writei, length: " << fullLength << ", frames: " << (fullLength / 4) << endl;
      //cout << "ProcessSoundBuffer: _playbackByteAlign = " << _playbackByteAlign << endl;
      /// We take the number of bytes of data in our mixing buffer and divide it by the two
      /// bytes per sample we are using and two channels for the number of frames.
//#ifdef _DEBUG
//      // Log in debug mode only.
//      FILE* fp;
//      if( (fp = fopen( "snd_pcm_writei_16signed_stereo.raw", "ab" ) ))
//      {
//         fwrite( copyBuffer, fullLength, 1, fp );
//         fclose( fp );
//      }
//#endif
      err = snd_pcm_writei( _playbackHandle, copyBuffer, (fullLength / 4) );
      //cout << "ProcessSoundBuffer: snd_pcm_writei has been called, checking err value" << endl;
      if( err )
	{
	  if( err == -EAGAIN )
	    {
	      continue;
	    }
	  if( err == -EBADFD )
	    {
	      cout << "ProcessSoundBuffer: -EBADFD on snd_pcm_writei." << endl;
	      delete[] copyBuffer;
	      return false;
	    }
	  if( err < 0 )
	    {
	      cout << "ProcessSoundBuffer: Calling XrunRecover" << endl;
	      if( XrunRecover( _playbackHandle, err ) < 0 )
		{
		  cout << "ProcessSoundBuffer: write to audio interface failed. (" << snd_strerror(err) << ")\n" << endl;
		  delete[] copyBuffer;
		  return false;
		}
	    }
	  else
	    {
	      // TODO: Try to write the rest of the bytes to the buffer.
	      //cout << "ProcessSoundBuffer: returned value " << err << endl;
	      break;
	    }
	}
  }

  /// Free the buffer we were sending to snd_pcm_writei
  //cout << "ProcessSoundBuffer: deleting copyBuffer" << endl;
  delete[] copyBuffer;

  //cout << "ProcessSoundBuffer:  End of function, returning true" << endl;
  return true;
}

/**
  @brief  Copies raw data into a secondary buffer.
  @note
  The data passed in must match the format of the secondary buffer.  Matching this is the
  application's responsiblity.
*/
bool ALSAManager::FillBuffer( int channel, unsigned char* data, int length, int sampleRate )
{
  if( channel >= _numBuffers || channel < 0 || length <= 0 )
  {
      return false;
  }

  //FILE* fp;
  //if( (fp = fopen( "fillbuffer.raw", "ab" ) ))
  //{
  //    fwrite( data, length, 1, fp );
  //    fclose( fp );
  //}

  _secondaryBuffers[channel]->_mutex->lock();
  int result = (_secondaryBuffers[channel]->_bufferData)->Write( data, length );
  _secondaryBuffers[channel]->_mutex->unlock();

  /*if( ( fp = fopen( "fillbuffer_postwrite.raw", "ab" ) ))
  {
     fwrite( data, length, 1, fp );
     fclose( fp );
  }*/

  //cout << "FillBuffer: Wrote " << result << " bytes to ring buffer out of an attempted " << length << endl;

  if( result != length )
  {
      return false;
  }
	
  return true;
}

/**
  @brief  Fills an individual secondary buffer with silence.
*/
bool ALSAManager::FillBufferSilence( int channel, int length )
{
  if( channel >= _numBuffers || channel < 0 || length == 0 )
  {
      return false;
  }

  bool err;
  //cout << "FillBufferSilence called" << endl;
  unsigned char *data = new unsigned char[length];
  memset(data, 0, length );

  //cout << "FillBufferSilence: Calling FillBuffer with " << length << " bytes of newly created blank data" << endl;
  err = FillBuffer( channel, data, length, _secondaryBuffers[channel]->_sampleRate );

  // cout << "FillBufferSilence: Deleting data, call to FillBuffer returned " << err << endl;
  delete[] data;

  //cout << "FillBufferSilence: Returning" << endl;
  return err;
}

/**
  @brief  Creates the capture buffer and initializes audio capture.
*/
bool ALSAManager::CreateCaptureBuffer( AudioRecordingCallback * recordCallback, int* soundCard, const char *name )
{
  int i;
  int err;
  short buf[128];
  snd_pcm_hw_params_t *hw_params;

  /// Have to init the ALSAManager first.
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

  //cout << "CreateCaptureBuffer: snd_pcm_open" << endl;
  if( name == NULL )
  {
      /// Try to open the default device.
      err = snd_pcm_open (&_captureHandle, "plughw:0,0", SND_PCM_STREAM_CAPTURE, 0);
      //cout << "CreateCaptureBuffer: snd_pcm_open" << endl;
  }
  else
  {
      err = snd_pcm_open (&_captureHandle, name, SND_PCM_STREAM_CAPTURE, 0);
  }

  if( err < 0 )
  {
      cout << "CreateCaptureBuffer: Cannot open audio device " << name << " (" << snd_strerror( err ) << ")" << endl;
      return false;
  }
		
  //cout << "CreateCaptureBuffer: snd_pcm_hw_params_malloc" << endl;
  if ((err = snd_pcm_hw_params_malloc (&hw_params)) < 0) 
  {
      cout << "CreateCaptureBuffer: Cannot allocate hardware parameter structure (" << snd_strerror (err) << ")" << endl;
      return false;
  }
				
  //cout << "CreateCaptureBuffer: snd_pcm_hw_params_any" << endl;
  if ((err = snd_pcm_hw_params_any (_captureHandle, hw_params)) < 0) 
  {
      cout << "CreateCaptureBuffer: Cannot initialize hardware parameter structure (" << snd_strerror (err) << ")" << endl;
      return false;
  }

  /// Enable resampling.
  unsigned int resample = 1;
  //cout << "CreateCaptureBuffer: snd_pcm_hw_params_set_rate_resample" << endl;
  err = snd_pcm_hw_params_set_rate_resample(_captureHandle, hw_params, resample);
  if (err < 0)
  {
    cout << "CreateCaptureBuffer: Resampling setup failed for playback: " << snd_strerror(err) << endl;
    return err;
  }

  //cout << "CreateCaptureBuffer: snd_pcm_hw_params_set_access" << endl;
  if ((err = snd_pcm_hw_params_set_access (_captureHandle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) 
  {
      cout << "CreateCaptureBuffer: Cannot set access type (" << snd_strerror (err) << ")" << endl;
      return false;
  }

  //cout << "CreateCaptureBuffer: snd_pcm_hw_params_set_format" << endl;
  if ((err = snd_pcm_hw_params_set_format (_captureHandle, hw_params, SND_PCM_FORMAT_S16)) < 0) 
  //if ((err = snd_pcm_hw_params_set_format (_playbackHandle, hw_params, SND_PCM_FORMAT_U16)) < 0)
  //if ((err = snd_pcm_hw_params_set_format (_playbackHandle, hw_params, SND_PCM_FORMAT_S16_LE)) < 0)
  {
      cout << "CreateCaptureBuffer: Cannot set sample format (" << snd_strerror (err) << ")" << endl;
      return false;
  }

  /// Single channel: Mono record from microphone.
  //cout << "CreateCaptureBuffer: snd_pcm_hw_params_set_channels" << endl;
  if ((err = snd_pcm_hw_params_set_channels (_captureHandle, hw_params, 1)) < 0) 
  {
      cout << "CreateCaptureBuffer: Cannot set channel count to 1. (" << snd_strerror (err) << ")" << endl;
      return false;
  }

  //cout << "CreateCaptureBuffer: snd_pcm_hw_params_set_rate_near" << endl;
  unsigned int actualRate = _captureSampleRate;
  if ((err = snd_pcm_hw_params_set_rate_near (_captureHandle, hw_params, &actualRate, 0)) < 0)
  {
      cout << "CreateCaptureBuffer: Cannot set sample rate to " << _captureSampleRate << ". (" << snd_strerror (err) << ")" << endl;
      return false;
  }
  if( actualRate < _captureSampleRate )
  {
      cout << "CreateCaptureBuffer: Sample rate does not match requested rate. (" << _captureSampleRate << " requested, "
           << actualRate << " acquired)" << endl;
  }

  /// Setting the period and buffer size may be completely unnecessary.  Since it doesn't actually work, we'll bypass
  /// it for now.

  //cout << "snd_pcm_hw_params_set_period_size" << endl;
  //if(( err = snd_pcm_hw_params_set_period_size( _captureHandle, hw_params, _recordChunkSize, 0 )) < 0 )
  //{
  //        fprintf( stderr, "cannot set period size (%s)\n", snd_strerror(err));
  //        return false;
  //}

  //cout << "snd_pcm_hw_params_set_buffer_size" << endl;
  //snd_pcm_uframes_t numFrames = _recordChunkSize * _recordChunkTotal;
  //if(( err = snd_pcm_hw_params_set_buffer_size( _captureHandle, hw_params, numFrames )) < 0 )
  //{
  //        fprintf( stderr, "cannot set buffer size (%s)\n", snd_strerror(err));
  //        return false;
  //}

  //cout << "CreateCaptureBuffer: snd_pcm_hw_params" << endl;
  if ((err = snd_pcm_hw_params (_captureHandle, hw_params)) < 0) 
  {
      cout << "CreateCaptureBuffer: Cannot set parameters (" << snd_strerror(err) << ")" << endl;
      return false;
  }

  //cout << "CreateCaptureBuffer: snd_pcm_hw_params_free" << endl;
  snd_pcm_hw_params_free (hw_params);

  _captureInited = true;

  return true;
}

/**
  @brief  Closes the capture buffer and sets the initialized flag to false.
*/
bool ALSAManager::DeleteCaptureBuffer()
{
  snd_pcm_close( _captureHandle );
  _captureInited = false;
  return true;
}

/**
  @brief  Returns the sample rate of an individual channel.
*/
unsigned int ALSAManager::GetSampleRate(int channel)
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
unsigned int ALSAManager::GetSampleRate( void )
{
  return _playbackSampleRate;
}

/**
  @brief  Returns the capture sample rate.
*/
unsigned int ALSAManager::GetRecordSampleRate()
{
  return _captureSampleRate;
}

/**
  @brief  Sets the playback sample rate for a single secondary buffer.
*/
bool ALSAManager::SetSampleRate( int channel, int frequency )
{
  if( channel < 0 || channel >= _numBuffers )
  {
      return false;
  }
  _secondaryBuffers[channel]->_mutex->lock();

  _secondaryBuffers[channel]->_sampleRate = frequency;

  /// Changing our playback sample rate forces us to recalculate our chunk size.
  _secondaryBuffers[channel]->_chunkSize = _bufferLatency * _secondaryBuffers[channel]->_sampleRate * _secondaryBuffers[channel]->_bytesPerSample;
  /// Chunk size MUST be an even number of bytes.
  _secondaryBuffers[channel]->_chunkSize &= ~1;

  _secondaryBuffers[channel]->_mutex->unlock();
  return true;
}

/**
  @brief  Sets the sample rate for captured data.
  @note
  We do not actually change our record buffer's frequency, just the data rate at
  which outgoing data is being sent.
*/
bool ALSAManager::SetRecordSampleRate( unsigned int frequency )
{
  _captureSampleRate = frequency;

  return true;
}

/**
  @brief  Sets the master volume level.
  @note
  Volume levels range from -9600 to 0.
*/
void ALSAManager::SetMasterVolume( int volume )
{
  if( volume > 0 || volume < -9600 )
  {
      return;
  }

  //cout << "Setting master volume to " << volume << "." << endl;
  _masterVolume = volume;
}

/**
  @brief  Sets the volume value for a single secondary buffer.
*/
void ALSAManager::SetVolume( int channel, int volume )
{
  if( channel < 0 || channel >= _numBuffers )
  {
      return;
  }

  //cout << "Setting volume for channel " << channel << " to " << volume << endl;
  _secondaryBuffers[channel]->_mutex->lock();
  _secondaryBuffers[channel]->_volume = volume;
  _secondaryBuffers[channel]->_mutex->unlock();
}

/**
  @brief  Sets the pan value for a single secondary buffer.
*/
void ALSAManager::SetPan( int channel, int pan )
{
  if( channel < 0 || channel >= _numBuffers )
  {
      return;
  }

  //cout << "Setting pan for channel " << channel << " to " << pan << endl;
  _secondaryBuffers[channel]->_mutex->lock();
  _secondaryBuffers[channel]->_pan = pan;
  _secondaryBuffers[channel]->_mutex->unlock();
}

/**
  @brief  Returns volume value for a single secondary buffer.
  @note
  Volume values range from -9600 to 0 and represent hundredths of a decibel
  attenuation.
*/
int ALSAManager::GetVolume( int channel )
{
  if( channel < 0 || channel >= _numBuffers )
  {
      return 0;
  }

  _secondaryBuffers[channel]->_mutex->lock();
  int volume = _secondaryBuffers[channel]->_volume;
  _secondaryBuffers[channel]->_mutex->unlock();
  return volume;

}

/**
  @brief  Returns pan value for a single secondary buffer.
  @note
  Pan values range from -1000 to 1000.
*/
int ALSAManager::GetPan( int channel )
{
  if( channel < 0 || channel >= _numBuffers )
  {
      return 0;
  }

  _secondaryBuffers[channel]->_mutex->lock();
  int pan = _secondaryBuffers[channel]->_pan;
  _secondaryBuffers[channel]->_mutex->unlock();

  return pan;
}

/**
  @brief  Sets a secondary buffer as playing and starts primary buffer if needed.
*/
bool ALSAManager::Play( int channel )
{
  if( channel >= _numBuffers )
  {
      return false;
  }

  _secondaryBuffers[channel]->_mutex->lock();
  _secondaryBuffers[channel]->_isPlaying = true;
  _secondaryBuffers[channel]->_mutex->unlock();

  //cout << "ALSAManager::Play - Checking state of _playbackHandle:  ";
  switch( snd_pcm_state( _playbackHandle ) )
  {
    case SND_PCM_STATE_OPEN:
      cout << "Play: snd_pcm_state_open" << endl;
      break;
    case SND_PCM_STATE_SETUP:
      cout << "Play: snd_pcm_state_setup" << endl;
      break;
    case SND_PCM_STATE_PREPARED:
      cout << "Play: snd_pcm_state_prepared - calling snd_pcm_start()" << endl;
      snd_pcm_start( _playbackHandle );
      //cout << "Play: snd_pcm_start called." << endl;
      break;
    case SND_PCM_STATE_RUNNING:
      //cout << "snd_pcm_state_running" << endl;
      break;
    case SND_PCM_STATE_XRUN:
      cout << "Play: snd_pcm_state_xrun - attempting to recover by calling snd_pcm_prepare()." << endl;
      snd_pcm_prepare( _playbackHandle );
      //cout << "Play: snd_pcm_prepare called." << endl;
      //return false;
      break;
    case SND_PCM_STATE_DRAINING:
      cout << "Play: snd_pcm_state_draining" << endl;
      break;
    case SND_PCM_STATE_PAUSED:
      cout << "Play: snd_pcm_state_paused" << endl;
      break;
    case SND_PCM_STATE_SUSPENDED:
      cout << "Play: snd_pcm_state_suspended" << endl;
      break;
    case SND_PCM_STATE_DISCONNECTED:
      cout << "Play: snd_pcm_state_disconnected" << endl;
      break;
    default:
      cout << "Play: _playbackHandle is in an unknown state." << endl;
  }

  return true;
}

/**
 @brief  Stops all secondary buffers from playing and then stops the primary buffer.
*/
bool ALSAManager::Stop( )
{
  int count;
  for( count = 0; count < _numBuffers; count++ )
  {
      Stop( count );
  }

  if( _inited )
  {
      cout << "Stop:  Calling snd_pcm_drop on playback buffer" << endl;
      snd_pcm_drop( _playbackHandle );	
  }

  return true;
}

/**
 @brief  Stops playing of an individual secondary buffer.
*/
bool ALSAManager::Stop( int channel )
{
  if( channel >= _numBuffers )
  {
      return false;
  }

  _secondaryBuffers[channel]->_mutex->lock();
  _secondaryBuffers[channel]->_isPlaying = false;
  _secondaryBuffers[channel]->_mutex->unlock();

  return true;
}

/**
  @brief  Monitors the record buffer and processes data if necessary.
  This is called continuously by the run() method.
*/
int ALSAManager::MonitorCaptureBuffer()
{
  snd_pcm_sframes_t pcmreturn;
  int err;

  if( !_inited || !_captureInited || !_capturing )
  {
      return false;
  }

  //cout << "MonitorCaptureBuffer: Checking state of _captureHandle" << endl;
  switch( snd_pcm_state( _captureHandle ) )
  {
    case SND_PCM_STATE_OPEN:
      cout << "MonitorCaptureBuffer: snd_pcm_state_open" << endl;
      break;
    case SND_PCM_STATE_SETUP:
      cout << "MonitorCaptureBuffer: snd_pcm_state_setup" << endl;
      break;
    case SND_PCM_STATE_PREPARED:
      cout << "MonitorCaptureBuffer: snd_pcm_state_prepared - calling snd_pcm_start()" << endl;
      snd_pcm_start( _captureHandle );
      cout << "MonitorCaptureBuffer: snd_pcm_start called." << endl;
      break;
    case SND_PCM_STATE_RUNNING:
      //cout << "MonitorCaptureBuffer: snd_pcm_state_running" << endl;
      break;
    case SND_PCM_STATE_XRUN:
      cout << "MonitorCaptureBuffer: snd_pcm_state_xrun - attempting to recover by calling snd_pcm_prepare." << endl;
      snd_pcm_prepare( _captureHandle );
      cout << "MonitorCaptureBuffer: snd_pcm_prepare called - returning for next go around." << endl;
      return false;
      break;
    case SND_PCM_STATE_DRAINING:
      cout << "MonitorCaptureBuffer: snd_pcm_state_draining" << endl;
      break;
    case SND_PCM_STATE_PAUSED:
      cout << "MonitorCaptureBuffer: snd_pcm_state_paused" << endl;
      break;
    case SND_PCM_STATE_SUSPENDED:
      cout << "MonitorCaptureBuffer: snd_pcm_state_suspended" << endl;
      break;
    case SND_PCM_STATE_DISCONNECTED:
      cout << "MonitorCaptureBuffer: snd_pcm_state_disconnected" << endl;
      break;
    default:
      cout << "MonitorCaptureBuffer: _captureHandle is in an unknown state." << endl;
  }

  /// Limit our capture chunk size to the largest size we can actually send out on the network.
  int captureChunkSize = _captureSampleRate * _bufferLatency; // Calculated in samples.
  if( captureChunkSize > (CAPTURE_CHUNK_SIZE / BYTES_PER_WORD ) ) // Compared in samples.
  {
      captureChunkSize = (CAPTURE_CHUNK_SIZE / BYTES_PER_WORD ); // Calculated in samples.
  }
  /// snd_pcm_avail_update returns the number of samples available, NOT the number of bytes available.
  /// This is why we multiply the number of samples times our record bytes to get the number of
  /// bytes we can read.
  if(( pcmreturn = snd_pcm_avail_update( _captureHandle )) < 0 )
  {
      if( pcmreturn == -EPIPE )
      {
	  cout << "MonitorCaptureBuffer: A buffer underrun (xrun) occurred." << endl;
	  return false;
      }
      else
      {
	  cout << "MonitorCaptureBuffer: Unknown ALSA avail update return value (" << pcmreturn << ")." << endl;
	  return false;
      }
  }
  else
  {
      //cout << "MonitorCaptureBuffer: snd_pcm_avail_update returned " << pcmreturn << " samples available of a requested " << captureChunkSize << endl;
      if( pcmreturn < captureChunkSize ) // Compared in samples.
	{
	  //cout << "MonitorCaptureBuffer: returning - we will wait for more data" << endl;
	  return false;
	}
  }

  if( _recordingCallback != NULL )
  {
      //cout << "MonitorCaptureBuffer: Grabbing data to pass to recording callback.  Grabbing " << captureChunkSize << " samples of data at " 
      //     << BYTES_PER_WORD << " bytes per sample." << endl;
      unsigned char data[CAPTURE_CHUNK_SIZE];
      memset( data, 0, CAPTURE_CHUNK_SIZE );

      /// snd_pcm_readi takes size in frames, not bytes.
      if(( err = snd_pcm_readi( _captureHandle, data, captureChunkSize )) < 0 )
      {
	  cout << "MonitorCaptureBuffer: Read failed (" << snd_strerror( err ) << ")" << endl;
	  return false;
      }
      else
      {
	  //cout << "MonitorCaptureBuffer: snd_pcm_readi returned value of " << err << " samples. " << endl;
      }

      int resampleTarget = captureChunkSize * _captureSampleRate / 44100;
      cout << "MonitorCaptureBuffer: Sending data to resample with " << (captureChunkSize * BYTES_PER_WORD)
           << " bytes of data to be turned into " << (resampleTarget * BYTES_PER_WORD ) << " bytes of data." << endl;
      /*FILE *fp;
      if( (fp = fopen( "record_data_pre_resample.raw", "ab" ) ))
      {
                        fwrite( data, (captureChunkSize * BYTES_PER_WORD), 1, fp );
                        fclose( fp );
      }*/

      _recordResampler.Resample( data, captureChunkSize, resampleTarget, BYTES_PER_WORD );

      //cout << "MonitorCaptureBuffer: Sending data to callback - ForwardRecordedData with " << (resampleTarget * BYTES_PER_WORD) << " bytes of data." << endl;
      /*if( (fp = fopen( "forwarded_record_data.raw", "ab" ) ))
      {
                        fwrite( data, (resampleTarget * BYTES_PER_WORD), 1, fp );
                        fclose( fp );
      }*/
      _recordingCallback->ForwardRecordedData( data, (resampleTarget * BYTES_PER_WORD), _captureSampleRate);
  }

  //cout << "MonitorCaptureBuffer: Returning true from MonitorCaptureBuffer." << endl;
  return true;
}

/**
     @brief     Main thread function for ALSAManager.
     This is the main thread function for ALSAManager.  It calls functions to monitor
     the primary, secondary, and capture buffers.
     @return
     This function should not only return when the class is terminated.
     @note
     This function includes a half-millisecond microsleep call.
*/
int ALSAManager::run()
{
  int count;
  while(handleMessages())
    { 
      // _current_time = timeGetTime();
      /// TODO: Re-enable this code with appropriate sound functions.
      /// Right now we turn off the light after 1 second.
      //for( int count = 0; count < _numChannels; count++ )
      //{
      //	if( *_receiving[count] && _current_time > (*_last_rx[count] + 1000) )
      //	{
      //		SetRX( count, false );
      //	}
      //}

      /// Grab and process any captured data.

      MonitorCaptureBuffer();

      /// Call MonitorBuffer() here to make sure we fill the secondary buffers with
      /// silence if nothing is being received.

      MonitorBuffer();

      /// Now that we're sure we have something in our secondary buffers we can mix in
      /// some data if we need to.

      ProcessSoundBuffer();

      /// Sleep for 0.5 milliseconds.  That should be enough to get us at least 32 bytes of data at 8khz 16 bit mono.
      /// At 44.1 stereo we would get about 11 times that (352+).  Adjust if necessary for slower machines and/or
      /// machines running many application.
      usleep( 500 );
    } 
  return 1;
}


/**
     @brief     Starts recording audio from the sound card.
     Starts recording audio from the sound card.
     @return
     bool value indicating whether it was able to start capturing.
     @note
     Init() and CreateCaptureBuffer() must be called before capture can be started.
*/
bool ALSAManager::StartCapture( )
{
  /// No capture without first initializing ALSAManager and the capture buffer.
  if( !_inited || !_captureInited )
  {
      return false;
  }

  snd_pcm_sw_params_t *sw_params;
  int err;

  if(( err = snd_pcm_sw_params_malloc( &sw_params )) < 0 )
  {
      cout << "Cannot allocate software parameter structure (" << snd_strerror( err ) << ")." << endl;
      return false;
  }

  if(( err = snd_pcm_sw_params_current( _captureHandle, sw_params )) < 0 )
  {
      cout << "Cannot initializa software parameter structure (" << snd_strerror( err ) << ")." << endl;
      return false;
  }

  if(( err = snd_pcm_sw_params_set_avail_min( _captureHandle, sw_params, 4096 )) < 0 )
  {
      cout << "Cannot set minimum available count (" << snd_strerror( err ) << ")." << endl;
      return false;
  }

  if(( err = snd_pcm_sw_params_set_start_threshold( _captureHandle, sw_params, 0U)) < 0 )
  {
      cout << "Cannot set start mode (" << snd_strerror( err ) << ")." << endl;
      return false;
  }

  if(( err = snd_pcm_sw_params( _captureHandle, sw_params )) < 0 )
  {
      cout << "Cannot set software parameters (" << snd_strerror( err ) << ")." << endl;
      return false;
  }

  snd_pcm_sw_params_free( sw_params );

  if ((err = snd_pcm_prepare (_captureHandle)) < 0) 
  {
      cout << "Cannot prepare audio capture interface for use (" << snd_strerror(err) << ")." << endl;
      return false;
  }

  /// We will monitor the capture buffer manually with MonitorCaptureBuffer.  That is also where we set our
  /// capture buffer to start.  Doing so before we're actually transmitting will result in unnecessary
  /// buffer overruns (xrun).
  // cout << "starting capture buffer (_captureHandle)" << endl;
  // if(( err = snd_pcm_start( _captureHandle )) < 0 )
  // {
  //	fprintf(stderr, "Cannot start capture interface (%s)\n", snd_strerror( err ));
  //	return false;
  // }

  _capturing = true;

  return true;
}

/**
     @brief     Stops capturing audio from the sound card.
     Stops capturing audio from the sound card.
     @return
     bool value, always returns true.
*/
bool ALSAManager::StopCapture( )
{
  cout << "StopCapture: calling snd_pcm_drop on capture handle." << endl;
  snd_pcm_drop(_captureHandle);
  _capturing = false;
  return true;
}

/**
 @brief  Checks whether an individual secondary buffer is playing.
*/
bool ALSAManager::IsBufferPlaying( int channel )
{
  if( !_inited || channel < 0 || channel >= _numBuffers )
    {
      return false;
    }
  _secondaryBuffers[channel]->_mutex->lock();
  bool playing = _secondaryBuffers[channel]->_isPlaying;
  _secondaryBuffers[channel]->_mutex->unlock();

  return playing;
}

/**
     @brief     Starts the playback of all secondary buffers.
     Starts the playback of all secondary buffers.
     @return
     bool value, only false if Init() has not been called.
     @note
     Init() must be called before playback can be started.
*/
bool ALSAManager::Play()
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
void ALSAManager::MonitorBuffer()
{
  int count;
  int readAvail;
  int chunkSize;
  for( count = 0; count < _numBuffers; count++ )
    {
      _secondaryBuffers[count]->_mutex->lock();
      bool state = _secondaryBuffers[count]->_isPlaying;
      _secondaryBuffers[count]->_mutex->unlock();
      if( state == false )
	continue;
      _secondaryBuffers[count]->_mutex->lock();
      /// Our buffer needs silence if we have less than one record chunk of data left in it.
      readAvail = (_secondaryBuffers[count]->_bufferData)->GetReadAvail();
      chunkSize = (int)_secondaryBuffers[count]->_chunkSize;
      _secondaryBuffers[count]->_mutex->unlock();
      //cout << "MonitorBuffer:  Secondary buffer " << count <<  " has " << readAvail << " bytes available to be read." << endl;
      if( readAvail < chunkSize )
	{
	  //cout << "MonitorBuffer: Filling buffer " << count << " with silence." << endl;
	  FillBufferSilence( count, chunkSize );
	}
    }

  return;
}

/**
     @brief     Attempts to recover from buffer underruns.
     Attempts to recover from buffer underruns (XRUN) and suspends.
     @return
     Integer value representing the error returned by a call to snd_pcm_prepare or
     snd_pcm_resume.
*/
int ALSAManager::XrunRecover(snd_pcm_t *handle, int err)
{
  if (err == -EPIPE)
    {
      /// Underrun 
      err = snd_pcm_prepare(handle);
      if (err < 0)
	{
	  printf("Can't recover from underrun, snd_pcm_prepare failed: %s\n", snd_strerror(err));
	}
      return 0;
    }
  else if (err == -ESTRPIPE)
    {
      while ((err = snd_pcm_resume(handle)) == -EAGAIN)
	{
	  /// Wait until the suspend flag is released.
	  sleep(1);
	}
      if (err < 0)
	{
	  err = snd_pcm_prepare(handle);
	  if (err < 0)
	    {
	      printf("Can't recover from suspend, snd_pcm_prepare failed: %s\n", snd_strerror(err));
	    }
	}
      return 0;
    }
  return err;
}

/**
  @brief  Sets the latency of buffers in milliseconds.
  Sets the latency for primary, secondary, and capture buffers in milliseconds.
   @note
  The playback latency will be twice the buffer latency value because it gets the
  delay once for secondary buffer monitoring and once for primary buffer monitoring.
*/
void ALSAManager::SetBufferLatency( int msec )
{
  /// Buffer length must be nonzero and less than one second.
  if( msec <= 0 || msec >= 1000 )
  {
      return;
  }
  /// This would be EXTREMELY non-thread-safe if called while the app was running.
  _bufferLatency = (float)msec / 1000.0f;

  int channel;
  for( channel = 0; channel < _numBuffers; channel++ )
  {
      _secondaryBuffers[channel]->_mutex->lock();
      _secondaryBuffers[channel]->_chunkSize = _bufferLatency * _secondaryBuffers[channel]->_sampleRate * _secondaryBuffers[channel]->_bytesPerSample;
      /// Make it an even number.
      _secondaryBuffers[channel]->_chunkSize &= ~1;
      _secondaryBuffers[channel]->_mutex->unlock();
  }

  return;
}

int ALSAManager::GetPeak( int channel )
{
    if( channel < 0 || channel >= _numBuffers )
    {
        return 0;
    }

    _secondaryBuffers[channel]->_mutex->lock();
    int peak = _secondaryBuffers[channel]->_peak;
    _secondaryBuffers[channel]->_mutex->unlock();
    return peak;
}

#endif // !WIN32

