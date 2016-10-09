#include "AudioBufferInterface.h"

AudioBufferInterface::AudioBufferInterface()
{
  _masterVolume = 0;
  _inited = false;
  _captureInited = false;
  _bufferLatency = 0.050;
  _recordingCallback = NULL;
  _playbackSampleRate = 44100;
}

AudioBufferInterface::~AudioBufferInterface()
{
}

// Takes a buffer that is presumably filled with multi-channel data and extracts out the data we're looking for.
// This then forwards that channel data to the channel we want to write to.
bool AudioBufferInterface::FillBufferInterleaved(int channel, unsigned char* data, int length, int sampleRate, int sampleSize, int everyNthSample, int initialOffsetBytes)
{
	int newLength = length / everyNthSample;
	unsigned char* newData = new unsigned char[newLength];
	for( int i = initialOffsetBytes; i < length; i += (sampleSize * everyNthSample) )
	{
		int bytePos = i / everyNthSample;
		memcpy(&newData[bytePos], &data[i], sampleSize);
	}
	bool result = FillBuffer(channel, newData, newLength, sampleRate);
	//delete newData;
	return result;
}
