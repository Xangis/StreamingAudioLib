#include "ESQ1Wavetable.h"
#include <math.h>

/**
* Generates wavetables for each of the waveform types defined in the header.
*/
void ESQ1Wavetable::CreateWavetables()
{
    // TODO: Populate waveforms.
}

/**
* Gets the number of waveforms in the wave table.  Valid waveforms are from 0 to NUM_ESQ1_WAVEFORMS-1.
*/
int ESQ1Wavetable::GetNumWaveforms()
{
    return NUM_ESQ1_WAVEFORMS;
}

/**
* Gets the name of a waveform based on its number.  Valid waveforms are from 0 to NUM_ESQ1_WAVEFORMS-1.
*/
const char* ESQ1Wavetable::GetWaveformName(int number)
{
    switch( number )
    {
    case 0:
        return "ESQ1 Waveform 1";
    case 1:
        return "ESQ1 Waveform 2";
    case 2:
        return "ESQ1 Waveform 3";
    case 3:
        return "ESQ1 Waveform 4";
    case 4:
        return "ESQ1 Waveform 5";
    case 5:
        return "ESQ1 Waveform 6";
    case 6:
        return "ESQ1 Waveform 7";
    case 7:
        return "ESQ1 Waveform 8";
    case 8:
        return "ESQ1 Waveform 9";
    case 9:
        return "ESQ1 Waveform 10";
    case 10:
        return "ESQ1 Waveform 11";
    case 11:
        return "ESQ1 Waveform 12";
    case 12:
        return "ESQ1 Waveform 13";
    case 13:
        return "ESQ1 Waveform 14";
    case 14:
        return "ESQ1 Waveform 15";
    case 15:
        return "ESQ1 Waveform 16";
    case 16:
        return "ESQ1 Waveform 17";
    case 17:
        return "ESQ1 Waveform 18";
    case 18:
        return "ESQ1 Waveform 19";
    case 19:
        return "ESQ1 Waveform 20";
    case 20:
        return "ESQ1 Waveform 21";
    case 21:
        return "ESQ1 Waveform 22";
    case 22:
        return "ESQ1 Waveform 23";
    case 23:
        return "ESQ1 Waveform 24";
    case 24:
        return "ESQ1 Waveform 25";
    case 25:
        return "ESQ1 Waveform 26";
	case 26:
        return "ESQ1 Waveform 27";
    case 27:
        return "ESQ1 Waveform 28";
    case 28:
        return "ESQ1 Waveform 29";
    case 29:
        return "ESQ1 Waveform 30";
    case 30:
        return "ESQ1 Waveform 31";
    case 31:
        return "ESQ1 Waveform 32";
	default:
        return "Invalid - Index Out of Range";
    }
}