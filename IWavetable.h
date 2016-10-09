#ifndef _IWAVETABLE_H_
#define _IWAVETABLE_H_

/**
* Interface for a wavetable.
*/
class IWaveTable
{
public:
	void CreateWavetables();
    int GetNumWaveforms();
    const char* GetWaveformName(int number);
};

#endif