#pragma once

#include "al.h"
#include "alc.h"
// On Linux, this Requires that freealut be installed:
#include "alut.h"
#include "wx/wx.h"

class OpenALBuffer
{
public:
	OpenALBuffer();
	~OpenALBuffer();
    static bool CheckALError( void );
    static bool CheckALCError( ALCdevice* device );
	bool InitAudio(char* deviceName = NULL);
	int GetNumBuffersProcessed();
	void RestartIfNecessary();
	ALuint UnqueueSingleBuffer();
	void PlayData(ALuint buffer, ALenum format, const ALvoid* data, ALsizei size, ALsizei frequency);
private:
    ALuint _playbackHandle; 
	ALuint _playbackBuffers[2];
    ALCcontext* _context;
    ALCdevice* _device;
	bool _inited;
};