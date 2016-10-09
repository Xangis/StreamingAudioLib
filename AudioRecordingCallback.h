
#if !defined( _AUDIORECORDINGCALLBACK_H_ )
#define _AUDIORECORDINGCALLBACK_H_

/**
     @brief     Base class to derive from in order to receive data from capture.
     Derive from this class and override the ForwardRecordedData method if you want to be able
     to process recorded data from either ALSAManager or DXAudioManager.
     @note      This is only required if you are going to be receiving recorded data.  A playback-
     only application will not need to derive from AudioRecordingCallback
*/
class AudioRecordingCallback
{
public:
        virtual void ForwardRecordedData( unsigned char* data, int length, int sampleRate ) = 0;
};

#endif
