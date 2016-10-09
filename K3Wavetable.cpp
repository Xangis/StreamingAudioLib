#include "K3Wavetable.h"
#include <math.h>

/**
* Generates wavetables for each of the waveform types defined in the header.
*/
void K3Wavetable::CreateWavetables()
{
    // TODO: Populate waveforms.
}

/**
* Gets the number of waveforms in the wave table.  Valid waveforms are from 0 to NUM_K3_WAVEFORMS-1.
*/
int K3Wavetable::GetNumWaveforms()
{
    return NUM_K3_WAVEFORMS;
}

/**
* Gets the name of a waveform based on its number.  Valid waveforms are from 0 to NUM_K3_WAVEFORMS-1.
*/
const char* K3Wavetable::GetWaveformName(int number)
{
    switch( number )
    {
    case 0:
        return "K3 Waveform 1";
    case 1:
        return "K3 Waveform 2";
    case 2:
        return "K3 Waveform 3";
    case 3:
        return "K3 Waveform 4";
    case 4:
        return "K3 Waveform 5";
    case 5:
        return "K3 Waveform 6";
    case 6:
        return "K3 Waveform 7";
    case 7:
        return "K3 Waveform 8";
    case 8:
        return "K3 Waveform 9";
    case 9:
        return "K3 Waveform 10";
    case 10:
        return "K3 Waveform 11";
    case 11:
        return "K3 Waveform 12";
    case 12:
        return "K3 Waveform 13";
    case 13:
        return "K3 Waveform 14";
    case 14:
        return "K3 Waveform 15";
    case 15:
        return "K3 Waveform 16";
    case 16:
        return "K3 Waveform 17";
    case 17:
        return "K3 Waveform 18";
    case 18:
        return "K3 Waveform 19";
    case 19:
        return "K3 Waveform 20";
    case 20:
        return "K3 Waveform 21";
    case 21:
        return "K3 Waveform 22";
    case 22:
        return "K3 Waveform 23";
    case 23:
        return "K3 Waveform 24";
    case 24:
        return "K3 Waveform 25";
    case 25:
        return "K3 Waveform 26";
	case 26:
        return "K3 Waveform 27";
    case 27:
        return "K3 Waveform 28";
    case 28:
        return "K3 Waveform 29";
    case 29:
        return "K3 Waveform 30";
    case 30:
        return "K3 Waveform 31";
    case 31:
        return "K3 Waveform 32";
	default:
        return "Invalid - Index Out of Range";
    }
}