
#ifndef STATICRING_BUFFER_H
#define STATICRING_BUFFER_H

#include <memory.h>

/**
	@brief     Ring buffer class.

	Ring buffer class.  Specifically for use with ALSA sound mixing as a
	secondary buffer, but may have other uses.  Maintains internal read and
	write pointers for filling and pulling data.

	This is not thread-safe.  If you want to use this in a multithreaded
	environment you will have to create your own mutex and lock it before
	each read or write.

	This buffer will only allow you to write a total number of bytes equal
	to the size of the buffer.  If a write larger than the buffer is requested
	it will write as many as it has room for and then return the number of bytes
	written.  In order to make more room for data you must read the data from
	the buffer.

	It will also only allow you to read a total number of bytes equal to the number
	of bytes that has been written.  If a read larger than that is requested, it
	will only return the number of bytes that it could actually read.
*/
class StaticRingBuffer {
public:
	StaticRingBuffer( int sizeBytes );
	~StaticRingBuffer();
	int Read( unsigned char* dataPtr, int numBytes );
	int Write( unsigned char *dataPtr, int numBytes );
    bool Empty( void );
	int GetSize( );
	int GetWriteAvail( );
	int GetReadAvail( );
private:
	unsigned char * _data;
	int _size;
	int _readPtr;
	int _writePtr;
};

#endif

