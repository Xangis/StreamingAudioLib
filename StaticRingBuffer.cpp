
// A static ring buffer is a buffer meant to be created once with a specific size, filled once, and then
// played many times in a loop.  It is specifically useful for looping sounds (ambient noises, looped drum
// tracks, etc.)

#include "StaticRingBuffer.h"

StaticRingBuffer::StaticRingBuffer( int sizeBytes )
{
	_data = new unsigned char[sizeBytes];
	memset( _data, 0, sizeBytes );
	_size = sizeBytes;
	_readPtr = 0;
	_writePtr = 0;
}

StaticRingBuffer::~StaticRingBuffer( )
{
	delete[] _data;
}

// Set all data to 0 and flag buffer as empty.
bool StaticRingBuffer::Empty( void )
{
    memset( _data, 0, _size );
    _readPtr = 0;
    _writePtr = 0;
    return true;
}

int StaticRingBuffer::Read( unsigned char *dataPtr, int numBytes )
{
	// If there's nothing to read or no data available, then we can't read anything.
	if( numBytes <= 0 )
	{
		return 0;
	}

	if( numBytes > _size )
	{
		numBytes = _size;
	}

	// Simultaneously keep track of how many bytes we've read and our position in the outgoing buffer
	if(numBytes > (_size - _readPtr))
	{ 
		// We have to wrap our buffer to provide a proper read.
		int len = _size - _readPtr;
		memcpy(dataPtr, (_data + _readPtr), len);
		memcpy(dataPtr + len, _data, numBytes - len);
	}
	else 
	{
		memcpy(dataPtr, _data+_readPtr, numBytes);
	}

	_readPtr = (_readPtr + numBytes) % _size;

	return numBytes;
}

// Write to the ring buffer.  Do not overwrite data that has not yet
// been read.
int StaticRingBuffer::Write( unsigned char *dataPtr, int numBytes )
{
	// If there's nothing to write or no room available, we can't write anything.
	if( dataPtr == 0 || numBytes <= 0 )
	{
		return 0;
	}

	// Cap our write at the number of bytes available to be written.
	if( numBytes > (_size - _writePtr) )
	{
		numBytes = (_size - _writePtr);
	}

	// Simultaneously keep track of how many bytes we've written and our position in the incoming buffer
	//if(numBytes > _size - _writePtr)
	//{
	//	// We would have to wrap our buffer to provide a proper write.  The
	//	// StaticRingBuffer reads as a ring, but writes linear.
	//	int len = _size-_writePtr;
	//	memcpy(_data+_writePtr,dataPtr,len);
	//	memcpy(_data, dataPtr+len, numBytes-len);
	//}
	//else
	//{
	memcpy(_data+_writePtr, dataPtr, numBytes);
	//}

	_writePtr = (_writePtr + numBytes);

	return numBytes;
}

int StaticRingBuffer::GetSize( void )
{
	return _size;
}

int StaticRingBuffer::GetWriteAvail( void )
{
	return _size - _writePtr;
}

int StaticRingBuffer::GetReadAvail( void )
{
	return _writePtr;
}