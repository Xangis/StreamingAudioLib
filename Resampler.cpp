#include "Resampler.h"
#include "memory.h"

/**
 @brief Initializes resampling library.
*/
Resampler::Resampler()
{
  /// Initialize the resampling library (int highQuality, float lowRatio, float highRatio)
  // 0.9 for 48k -> 44.1k, 6 for 8k->48k

  //cout << "Init: calling resample_open" << endl;
  _upSampleHandle = resample_open( 1, 1.0, 6.0 );

  /// Initialize a separate handle for downsampled data to maintain accuracy.

  //cout << "Init: calling resample_open for capture" << endl;
  _downSampleHandle = resample_open( 1, 0.18, 1.0 );

  //cout << "Init: resample_open called for upsample, filter width = " << resample_get_filter_width( _upSampleHandle ) << endl;
  //cout << "Init: resample_open called for downsample, filter width = " << resample_get_filter_width( _downSampleHandle ) << endl;
  
  // We need to prime the resample handler by running it once, otherwise we get a pop glitch on startup.
  // We do this for both upsampling and downsampling.
  //
  // The data doesn't go anywhere, so we have nothing to worry about.
  unsigned char* primeBuffer = new unsigned char[1024];
  memset( primeBuffer, 0, 1024 );
  short* result = Resample( primeBuffer, 256, 512, 2, 1 );
  primeBuffer = new unsigned char[1024];
  memset( primeBuffer, 0, 1024 );
  short* result2 = Resample( primeBuffer, 512, 256, 2, 1 );
  delete[] result;
  delete[] result2;
}

Resampler::~Resampler()
{
  resample_close(_upSampleHandle);
  resample_close(_downSampleHandle);
}

/**
     @brief     Resamples audio from one bitrate to another.
     Upsamples or downsamples incoming audio data.  Copies the incoming buffer into an
     intermediate buffer, converts it into float data (because that's what the resample
     library requires), resamples it, and converts it back into integers and copies it
     back into the incoming buffer.
     @return
     This is a void function, but it overwrites the channelData passed into it.
     @note
     The original channelData passed in is deleted and destroyed.  The caller is responsible
	 for final disposal of the returned data.
*/
short* Resampler::Resample( unsigned char* channelData, int originalNumSamples, int resultingNumSamples, int bytesPerSample, int numChannels )
{
  if( numChannels == 0 || bytesPerSample == 0 || originalNumSamples == 0 || resultingNumSamples == 0 || channelData == 0)
	  return 0;
  float* fromBuffer = new float[originalNumSamples];
  memset( fromBuffer, 0, (sizeof(float) * originalNumSamples) );
  short* inBufferIterator = (short*)channelData;
  int count;
  float* fromBuffer2;
  if( numChannels == 2 )
  {
	  fromBuffer2 = new float[originalNumSamples];
	  memset( fromBuffer2, 0, (sizeof(float) * originalNumSamples) );
  }

  for( count = 0; count < originalNumSamples; ++count )
  {
	  if( numChannels == 2 )
	  {
		  fromBuffer[count] = (float)inBufferIterator[count*numChannels] / 32767.0;
		  if( fromBuffer[count] > 1.0 )
		  {
    		  fromBuffer[count] = 1.0;
		  }
		  else if( fromBuffer[count] < -1.0 )
		  {
			  fromBuffer[count] = -1.0;
		  }
		  fromBuffer2[count] = (float)inBufferIterator[count*numChannels + 1] / 32767.0;
		  if( fromBuffer2[count] > 1.0 )
		  {
    		  fromBuffer2[count] = 1.0;
		  }
		  else if( fromBuffer2[count] < -1.0 )
		  {
			  fromBuffer2[count] = -1.0;
		  }
	  }
	  else
	  {
		  fromBuffer[count] = (float)inBufferIterator[count] / 32767.0;
		  if( fromBuffer[count] > 1.0 )
		  {
    		  fromBuffer[count] = 1.0;
		  }
	  }
  }

  float* toBuffer = new float[resultingNumSamples];
  memset(toBuffer, 0, (resultingNumSamples * sizeof(float)));
  float* toBuffer2;
  if( numChannels == 2 )
  {
	  toBuffer2 = new float[resultingNumSamples];
	  memset(toBuffer2, 0, (resultingNumSamples * sizeof(float)));
  }

  int srcused = 0;
  int out = 0;
  // This tells resample_process whether this is the last group of samples it will be processing.
  // we may want to set this to true because we're sending individual chunks.
  bool lastFlag = false;
  // We have two separate sampling filters, one optimized for upsampling and one optimized for downsampling.
  if( resultingNumSamples > originalNumSamples )
  {
    out = resample_process(_upSampleHandle, ((float)resultingNumSamples / (float)originalNumSamples),
			     fromBuffer, originalNumSamples,
			     lastFlag, &srcused,
			     toBuffer, resultingNumSamples);
	if( numChannels == 2 )
	{
		out = resample_process(_upSampleHandle, ((float)resultingNumSamples / (float)originalNumSamples),
					 fromBuffer2, originalNumSamples,
					 lastFlag, &srcused,
					 toBuffer2, resultingNumSamples);
	}
  }
  else
  {
    out = resample_process(_downSampleHandle, ((float)resultingNumSamples / (float)originalNumSamples),
                             fromBuffer, originalNumSamples,
                             lastFlag, &srcused,
                             toBuffer, resultingNumSamples);
	if( numChannels == 2 )
	{
		out = resample_process(_downSampleHandle, ((float)resultingNumSamples / (float)originalNumSamples),
								 fromBuffer2, originalNumSamples,
								 lastFlag, &srcused,
								 toBuffer2, resultingNumSamples);
	}
  }

  delete[] channelData;
  inBufferIterator = new short[resultingNumSamples*numChannels];
  memset(inBufferIterator, 0, (resultingNumSamples*numChannels*sizeof(short)));
  for( count = 0; count < resultingNumSamples; ++count )
  {
	  if( numChannels == 2 )
	  {
		  inBufferIterator[count * 2] = (short)(toBuffer[count] * 32767.0);
		  inBufferIterator[count * 2 + 1] = (short)(toBuffer2[count] * 32767.0);
	  }
	  else
	  {
		inBufferIterator[count] = (short)(toBuffer[count] * 32767.0);
	  }
  }

  delete[] fromBuffer;
  if( numChannels == 2 )
	  delete[] fromBuffer2;
  delete[] toBuffer;
  if( numChannels == 2 )
	  delete[] toBuffer2;
  return inBufferIterator;
}