#ifndef _RESAMPLER_H_
#define _RESAMPLER_H_

#include "libresample.h"

class Resampler
{
public:
    Resampler();
    ~Resampler();
	short* Resample( unsigned char* channelData, int originalNumSamples, int resultingNumSamples, int bytesPerSample, int numChannels );
private:
  	void* _upSampleHandle;
    void* _downSampleHandle;
};

#endif

