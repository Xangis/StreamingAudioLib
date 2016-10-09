#pragma once

#include "wx/wx.h"
#include <MMDeviceAPI.h>
#include <AudioClient.h>

#define PERIODS_PER_BUFFER 4

class WASAPIBuffer
{
public:
	WASAPIBuffer();
	~WASAPIBuffer();
	bool InitAudio(int engineLatency);

    enum RenderSampleType
    {
        SampleTypeFloat,
        SampleType16BitPCM,
    };
private:
	bool LoadFormat();
	bool CalculateMixFormatType();
    IMMDevice * _endpoint;
    IAudioClient *_audioClient;
    IAudioRenderClient *_renderClient;
    WAVEFORMATEX *_mixFormat;
	LONG _engineLatencyInMS;
	HANDLE _shutdownEvent;
	UINT32 _bufferSize;
	HANDLE _renderThread;
	UINT32 _frameSize;
	RenderSampleType _renderSampleType;
};

template <class T> void SafeRelease(T **ppT)
{
    if (*ppT)
    {
        (*ppT)->Release();
        *ppT = NULL;
    }
}