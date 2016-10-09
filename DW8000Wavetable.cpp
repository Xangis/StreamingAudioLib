#include "DW8000Wavetable.h"
#include <math.h>

/**
* Generates wavetables for each of the waveform types defined in the header.
*/
void DW8000Wavetable::CreateWavetables()
{
    // TODO: Populate waveforms.
}

/**
* Gets the number of waveforms in the wave table.  Valid waveforms are from 0 to NUM_DW8000_WAVEFORMS-1.
*/
int DW8000Wavetable::GetNumWaveforms()
{
    return NUM_DW8000_WAVEFORMS;
}

/**
* Gets the name of a waveform based on its number.  Valid waveforms are from 0 to NUM_DW8000_WAVEFORMS-1.
*/
const char* DW8000Wavetable::GetWaveformName(int number)
{
    switch( number )
    {
    case 0:
        return "DW8000 Waveform 1";
    case 1:
        return "DW8000 Waveform 2";
    case 2:
        return "DW8000 Waveform 3";
    case 3:
        return "DW8000 Waveform 4";
    case 4:
        return "DW8000 Waveform 5";
    case 5:
        return "DW8000 Waveform 6";
    case 6:
        return "DW8000 Waveform 7";
    case 7:
        return "DW8000 Waveform 8";
    case 8:
        return "DW8000 Waveform 9";
    case 9:
        return "DW8000 Waveform 10";
    case 10:
        return "DW8000 Waveform 11";
    case 11:
        return "DW8000 Waveform 12";
    case 12:
        return "DW8000 Waveform 13";
    case 13:
        return "DW8000 Waveform 14";
    case 14:
        return "DW8000 Waveform 15";
    case 15:
        return "DW8000 Waveform 16";
	default:
        return "Invalid - Index Out of Range";
    }
}