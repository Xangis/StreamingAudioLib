#pragma once

#include "IWavetable.h"

#define NUM_DW8000_WAVEFORMS 16
#define TABLESIZE 4096

/**
* Stores wavetable data for various waveforms based on the Kawai K3.
*/
class DW8000Wavetable : public IWaveTable
{
public:
	void CreateWavetables();
	float _waveformTable[NUM_DW8000_WAVEFORMS][TABLESIZE];
    int GetNumWaveforms();
    const char* GetWaveformName(int number);
};