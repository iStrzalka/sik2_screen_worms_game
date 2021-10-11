CXX=g++
CXXFLAGS=-Wall -O2 -std=c++11
ALL = screen-worms-client screen-worms-server

all: $(ALL)

%.o: %.c %.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<

screen-worms-server: screen-worms-server.cpp server.o utility.o codec.o connection.o player.o generator.o
	$(CXX) $(CXXFLAGS) -o $@ $^ -lrt -lz

screen-worms-client: screen-worms-client.cpp client.o utility.o codec.o connection.o
	$(CXX) $(CXXFLAGS) -o $@ $^ -lrt -lz

.PHONY: clean

clean:
	rm *.o $(ALL)
