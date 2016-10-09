#pragma once

#include "IWavetable.h"

#define NUM_K3_WAVEFORMS 32
#define TABLESIZE 4096

/**
* Stores wavetable data for various waveforms based on the Kawai K3.
*/
class K3Wavetable : public IWaveTable
{
public:
	void CreateWavetables();
	float _waveformTable[NUM_K3_WAVEFORMS][TABLESIZE];
    int GetNumWaveforms();
    const char* GetWaveformName(int number);
};