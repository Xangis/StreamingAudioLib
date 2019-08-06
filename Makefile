# Vorbital makefile.  Requires wxWidgets.
# by default, wx-config from the PATH is used
WX_CONFIG := wx-config

# Main executable file
PROGRAM = libstreamingaudio.a

# Directory containing library portions of code.
LIBDIR = /usr/lib/i386-linux-gnu/
INCLUDEDIR = ../../lib/rtaudio-5.1.0/
INCLUDEDIR2 = /usr/include/AL

# Object files
OBJECTS = resamplesubs.o filterkit.o resample.o OpenALManager.o AudioInterface.o Resampler.o RingBuffer.o StaticRingBuffer.o RtAudioManager.o

CXX = $(shell $(WX_CONFIG) --cxx -ggdb)

.SUFFIXES:	.o .cpp

.cpp.o :
	$(CXX) -ggdb -c -I$(INCLUDEDIR) -I$(INCLUDEDIR2) `$(WX_CONFIG) --cxxflags` -o $@ $<

all:    $(PROGRAM)

$(PROGRAM):	$(OBJECTS)
	$(CXX) -o $(PROGRAM) -static -I$(INCLUDEDIR) $(OBJECTS) -L$(LIBDIR) -lvorbisfile -lvorbis -lalut -lopenal -lrtaudio `$(WX_CONFIG) --libs`

clean: 
	rm -f *.o $(PROGRAM)
