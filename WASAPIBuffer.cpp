#include "WASAPIBuffer.h"

WASAPIBuffer::WASAPIBuffer()
{
}

bool WASAPIBuffer::InitAudio(int engineLatency)
{
    //
    //  Create our shutdown and samples ready events- we want auto reset events that start in the not-signaled state.
    //
    _shutdownEvent = CreateEventEx(NULL, NULL, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);
    if (_shutdownEvent == NULL)
    {
		wxMessageBox(wxString::Format(_("Unable to create shutdown event: %d.\n"), GetLastError()));
        return false;
    }

    //
    //  Now activate an IAudioClient object on our preferred endpoint and retrieve the mix format for that endpoint.
    //
    HRESULT hr = _endpoint->Activate(__uuidof(IAudioClient), CLSCTX_INPROC_SERVER, NULL, reinterpret_cast<void **>(&_audioClient));
    if (FAILED(hr))
    {
        wxMessageBox(wxString::Format(_("Unable to activate audio client: %x.\n"), hr));
        return false;
    }

    //
    // Load the MixFormat.  This may differ depending on the shared mode used
    //
    if (!LoadFormat())
    {
        wxMessageBox(wxString::Format(_("Failed to load the mix format.\n")));
        return false;
    }

    //
    //  Remember our configured latency in case we'll need it for a stream switch later.
    //
    _engineLatencyInMS = engineLatency;

    REFERENCE_TIME bufferDuration = _engineLatencyInMS*10000*PERIODS_PER_BUFFER;
    REFERENCE_TIME periodicity = _engineLatencyInMS*10000;

    hr = _audioClient->Initialize(AUDCLNT_SHAREMODE_EXCLUSIVE, 
        AUDCLNT_STREAMFLAGS_NOPERSIST, 
        bufferDuration, 
        periodicity,
        _mixFormat, 
        NULL);
    if (FAILED(hr))
    {
        wxMessageBox(wxString::Format(_("Unable to initialize audio client: %x.\n"), hr));
        return false;
    }
	
    //
    //  Retrieve the buffer size for the audio client.
    //
    hr = _audioClient->GetBufferSize(&_bufferSize);
    if(FAILED(hr))
    {
        wxMessageBox(wxString::Format(_("Unable to get audio client buffer: %x. \n"), hr));
        return false;
    }

    hr = _audioClient->GetService(IID_PPV_ARGS(&_renderClient));
    if (FAILED(hr))
    {
        wxMessageBox(wxString::Format(_("Unable to get new render client: %x.\n"), hr));
        return false;
    }

    return true;
}

WASAPIBuffer::~WASAPIBuffer()
{
    if (_renderThread)
    {
        SetEvent(_shutdownEvent);
        WaitForSingleObject(_renderThread, INFINITE);
        CloseHandle(_renderThread);
        _renderThread = NULL;
    }

    if (_shutdownEvent)
    {
        CloseHandle(_shutdownEvent);
        _shutdownEvent = NULL;
    }

    SafeRelease(&_endpoint);
    SafeRelease(&_audioClient);
    SafeRelease(&_renderClient);

    if (_mixFormat)
    {
        CoTaskMemFree(_mixFormat);
        _mixFormat = NULL;
    }
}

//
//  Retrieve the format we'll use to rendersamples.
//
//  Start with the mix format and see if the endpoint can render that.  If not, try
//  the mix format converted to an integer form (most audio solutions don't support floating 
//  point rendering and the mix format is usually a floating point format).
//
bool WASAPIBuffer::LoadFormat()
{
    HRESULT hr = _audioClient->GetMixFormat(&_mixFormat);
    if (FAILED(hr))
    {
        printf("Unable to get mix format on audio client: %x.\n", hr);
        return false;
    }
    assert(_mixFormat != NULL);

    hr = _audioClient->IsFormatSupported(AUDCLNT_SHAREMODE_EXCLUSIVE,_mixFormat, NULL);
    if (hr == AUDCLNT_E_UNSUPPORTED_FORMAT)
    {
        printf("Device does not natively support the mix format, converting to PCM.\n");

        //
        //  If the mix format is a float format, just try to convert the format to PCM.
        //
        if (_mixFormat->wFormatTag == WAVE_FORMAT_IEEE_FLOAT)
        {
            _mixFormat->wFormatTag = WAVE_FORMAT_PCM;
            _mixFormat->wBitsPerSample = 16;
            _mixFormat->nBlockAlign = (_mixFormat->wBitsPerSample / 8) * _mixFormat->nChannels;
            _mixFormat->nAvgBytesPerSec = _mixFormat->nSamplesPerSec*_mixFormat->nBlockAlign;
        }
        else if (_mixFormat->wFormatTag == WAVE_FORMAT_EXTENSIBLE && 
            reinterpret_cast<WAVEFORMATEXTENSIBLE *>(_mixFormat)->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)
        {
            WAVEFORMATEXTENSIBLE *waveFormatExtensible = reinterpret_cast<WAVEFORMATEXTENSIBLE *>(_mixFormat);
            waveFormatExtensible->SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
            waveFormatExtensible->Format.wBitsPerSample = 16;
            waveFormatExtensible->Format.nBlockAlign = (_mixFormat->wBitsPerSample / 8) * _mixFormat->nChannels;
            waveFormatExtensible->Format.nAvgBytesPerSec = waveFormatExtensible->Format.nSamplesPerSec*waveFormatExtensible->Format.nBlockAlign;
            waveFormatExtensible->Samples.wValidBitsPerSample = 16;
        }
        else
        {
            printf("Mix format is not a floating point format.\n");
            return false;
        }

        hr = _audioClient->IsFormatSupported(AUDCLNT_SHAREMODE_EXCLUSIVE,_mixFormat,NULL);
        if (FAILED(hr))
        {
            wxMessageBox(_("Format is not supported.\n"));
            return false;
        }
    }

    _frameSize = _mixFormat->nBlockAlign;
    if (!CalculateMixFormatType())
    {
        return false;
    }
    return true;
}

//
//  Crack open the mix format and determine what kind of samples are being rendered.
//
bool WASAPIBuffer::CalculateMixFormatType()
{
    if (_mixFormat->wFormatTag == WAVE_FORMAT_PCM || 
        _mixFormat->wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
            reinterpret_cast<WAVEFORMATEXTENSIBLE *>(_mixFormat)->SubFormat == KSDATAFORMAT_SUBTYPE_PCM)
    {
        if (_mixFormat->wBitsPerSample == 16)
        {
            _renderSampleType = SampleType16BitPCM;
        }
        else
        {
            printf("Unknown PCM integer sample type\n");
            return false;
        }
    }
    else if (_mixFormat->wFormatTag == WAVE_FORMAT_IEEE_FLOAT ||
             (_mixFormat->wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
               reinterpret_cast<WAVEFORMATEXTENSIBLE *>(_mixFormat)->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT))
    {
        _renderSampleType = SampleTypeFloat;
    }
    else 
    {
        printf("unrecognized device format.\n");
        return false;
    }
    return true;
}