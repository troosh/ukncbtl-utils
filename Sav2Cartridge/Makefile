CXX = g++
CXXFLAGS = -std=c++11 -O3 -Wall

SOURCES = LZSS.cpp Sav2Cart.cpp

OBJECTS = LZSS.o Sav2Cart.o

all: sav2cart sav2cart_lz4

sav2cart: $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o sav2cart $(OBJECTS)

sav2cart_lz4:
	$(CXX) -std=c++11 -Wall -O0 -g -o sav2cart_lz4 Sav2CartLZ4.cpp


.PHONY: clean

clean:
	rm -f $(OBJECTS)
