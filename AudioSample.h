#ifndef _AUDIOSAMPLE_H_
#define _AUDIOSAMPLE_H_

#include <string>
using namespace std;

class AudioSample
{
	std::string _filename;
	int _playbackPosition;
	int _length;
	bool _loop;
	char* data;
	int _channel;
};

#endif