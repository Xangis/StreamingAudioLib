#pragma once

#include "IWavetable.h"

#define NUM_ESQ1_WAVEFORMS 32
#define TABLESIZE 4096

/**
* Stores wavetable data for various waveforms based on the Kawai K3.
*/
class ESQ1Wavetable : public IWaveTable
{
public:
	void CreateWavetables();
	float _waveformTable[NUM_ESQ1_WAVEFORMS][TABLESIZE];
    int GetNumWaveforms();
    const char* GetWaveformName(int number);
};