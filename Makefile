PROG=telegraph-controller
CPPSRC=$(wildcard *.cpp)
CXXFLAGS = -std=gnu++14 -Wall -g -pthread 
LDFLAGS = -lpigpiod_if2 -lrt -lev -lhiredis

all: $(PROG)

$(PROG): $(CPPSRC)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $<
