
#if !defined( _AUDIOPLAYBACKINTERFACE_H_ )
#define _AUDIOPLAYBACKINTERFACE_H_

/**
     @brief     Base class to derive from in order to perform buffered file playback.
*/
class AudioPlaybackInterface
{
public:
	virtual int PlayFile( string fileName, bool repeat=false, int channel = -1, int tuneSemitones = 0 ) = 0;
	virtual int PlayData( char* data, int numBytes, int bytesPerSample = 2, int sampleRate = 44100, bool repeat = false, int channel = -1, int tuneSemitones = 0) = 0;
	virtual bool StopPlayback(int playbackHandle); // Stops file or data playback using the handle returned by the
	                                               // PlayFile or PlayData methods.
private:
	virtual void MixAudio(); // Thread to handle audio file/channel mixing.  This may or may not do anything depending
	                         // on how the underlying audio engine works.
};

#endif
