#include "OpenALBuffer.h"

#define BUFFERLENGTH 3528

/**
* Constructor.  Initializes OpenAL.
*/
OpenALBuffer::OpenALBuffer()
{
	alutInit(NULL, NULL);
	CheckALError();
	_inited = false;
}

/**
* Destructor.  Frees up resources and uninitializes OpenAL.
*/
OpenALBuffer::~OpenALBuffer()
{
	if( _inited )
	{
		alDeleteSources(1, &_playbackHandle);
		alDeleteBuffers(2, _playbackBuffers);
		//alcDestroyContext(_context);
		//alcCloseDevice(_device);
	}
    alutExit();
}

void OpenALBuffer::RestartIfNecessary()
{
	int value;
	if( _inited )
	{
		alGetSourcei(_playbackHandle, AL_SOURCE_STATE, &value);
		if( value != AL_PLAYING )
		{
			alSourcePlay(_playbackHandle);
		}
	}
}

/**
 @brief  Checks for OpenAL errors.
*/
bool OpenALBuffer::CheckALCError( ALCdevice* device )
{
	int error = alcGetError(device);

	switch( error )
	{
	case ALC_NO_ERROR:
		return false;
		break;
	case ALC_INVALID_CONTEXT:
        wxMessageBox( _("ALC_INVALID_CONTEXT"), _("Error"), wxOK );
		return true;
		break;
	case ALC_INVALID_VALUE:
        wxMessageBox( _("ALC_INVALID_VALUE"), _("Error"), wxOK );
		return true;
		break;
	case ALC_INVALID_ENUM:
        wxMessageBox( _("ALC_INVALID_ENUM"), _("Error"), wxOK );
		return true;
		break;
	case ALC_INVALID_DEVICE:
        wxMessageBox( _("ALC_INVALID_DEVICE"), _("Error"), wxOK );
		return true;
		break;
	case ALC_OUT_OF_MEMORY:
		return true;
		break;
	default:
		return true;
		break;
	}
}

/**
 @brief  Checks for OpenAL errors.
*/
bool OpenALBuffer::CheckALError( void )
{
	int error = alGetError();

	switch( error )
	{
	case AL_NO_ERROR:
		return false;
		break;
	case AL_INVALID_VALUE:
		wxMessageBox(_("AL_INVALID_VALUE"));
		return true;
		break;
	case AL_INVALID_ENUM:
		wxMessageBox(_("AL_INVALID_ENUM"));
		return true;
		break;
	case AL_INVALID_NAME:
		wxMessageBox(_("AL_INVALID_NAME"));
		return true;
		break;
	case AL_OUT_OF_MEMORY:
		wxMessageBox(_("AL_OUT_OF_MEMORY"));
		return true;
		break;
	case AL_INVALID_OPERATION:
		wxMessageBox(_("AL_INVALID_OPERATION"));
		return true;
		break;
	default:
		wxMessageBox(wxString::Format(_("Unknown OpenAL Error: %d"), error));
		return true;
		break;
	}
}

/**
* Gets the number of buffers that have been processed (played) and are waiting to be reused.
*/
int OpenALBuffer::GetNumBuffersProcessed()
{
	int processed = 0;
	if( _inited )
	{
		alGetSourcei(_playbackHandle, AL_BUFFERS_PROCESSED, &processed);
		// Don't check for errors -- uses too much CPU time.
		//CheckALError();
	}
	return processed;
}

/**
* Removes a single buffer from the queue and returns its ID.
*/
ALuint OpenALBuffer::UnqueueSingleBuffer()
{
	ALuint buffer = 0;
	if( _inited )
	{
		alSourceUnqueueBuffers(_playbackHandle, 1, &buffer);
		CheckALError();
	}
	return buffer;
}

/**
* Takes data of the given format and adds it to the playback buffer.
*/
void OpenALBuffer::PlayData(ALuint buffer, ALenum format, const ALvoid* data, ALsizei size, ALsizei frequency)
{
	if( _inited )
	{
		alBufferData(buffer, format, data, size, frequency );
		CheckALError();
		alSourceQueueBuffers( _playbackHandle, 1, &buffer);
		CheckALError();
	}
}

/**
* Initializes the audio device.
*/
bool OpenALBuffer::InitAudio(char* deviceName)
{
  _device = alcOpenDevice(deviceName);
  if( _device == NULL )
  {
      wxMessageBox( _("Unable to open device.") );
      return false;
  }
  // Not needed if we call alutInit();
  //_context = alcCreateContext(_device, NULL);
  //if( _context == NULL )
  //{
  //    wxMessageBox( "Unable to create OpenAL context.");
  //    return false;
  //}
  //alcMakeContextCurrent(_context);

  CheckALCError(_device);
  alGetError();

  alGenSources( 1, &_playbackHandle );
  if( CheckALError() )
  {
        wxMessageBox( _("Unable to generate OpenAL sources.") );
		return( false );
  }
  alSourcef ( _playbackHandle, AL_PITCH, 1.0 );
  if( CheckALError() )
  {
        wxMessageBox( _("Unable to set playback pitch.") );
		return( false );
  }
  alSourcef ( _playbackHandle, AL_GAIN, 1.0 );
  if( CheckALError() )
  {
        wxMessageBox( _("Unable to set playback gain.") );
		return( false );
  }
  alSource3f( _playbackHandle, AL_POSITION, 0.0f, 0.0f, 0.0f );
  if( CheckALError() )
  {
        wxMessageBox( _("Unable to set audio source position.") );
  		return( false );
  }
  alSource3f( _playbackHandle, AL_VELOCITY, 0.0f, 0.0f, 0.0f );
  if( CheckALError() )
  {
        wxMessageBox( _("Unable to set audio source velocity.") );
		return( false );
  }
  alSourcef( _playbackHandle, AL_ROLLOFF_FACTOR,  0.0f );
  if( CheckALError() )
  {
        wxMessageBox( _("Unable to set audio source rolloff factor.") );
		return( false );
  }
  alSource3f(_playbackHandle, AL_DIRECTION, 0.0, 0.0, 0.0);
  if( CheckALError() )
  {
		wxMessageBox( _("Unable to set audio source direction.") );
		return( false );
  }

  alGenBuffers( 2, _playbackBuffers );

  short data[BUFFERLENGTH];
  memset(data, 0, (sizeof(short) * BUFFERLENGTH));
  alBufferData(_playbackBuffers[0], AL_FORMAT_MONO16, &data, BUFFERLENGTH*2, 44100);
  CheckALError();
  alBufferData(_playbackBuffers[1], AL_FORMAT_MONO16, &data, BUFFERLENGTH*2, 44100);
  CheckALError();
  alSourceQueueBuffers(_playbackHandle, 2, _playbackBuffers);
  CheckALError();
  alSourcePlay(_playbackHandle);
  CheckALError();
  _inited = true;
  return true;
}