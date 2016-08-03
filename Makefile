#
# Makefile
# Adrian Perez, 2016-08-01 23:29
#

CXX = g++
CC  = g++
LD  = g++

ICECC_FLAGS := $(shell pkg-config icecc --cflags)
ICECC_LIBS  := $(shell pkg-config icecc --libs)

CPPFLAGS += -D_GLIBCXX_USE_CXX11_ABI=0
CXXFLAGS += $(ICECC_FLAGS) -std=c++14 -Wall -Os
LDLIBS   += $(ICECC_LIBS) -ldill -Os

all: icetop

icetop: icetop.o
	$(LD) $(LDFLAGS) -o $@ $^ $(LDLIBS)

clean:
	$(RM) icetop.o icetop

# vim:ft=make
#
