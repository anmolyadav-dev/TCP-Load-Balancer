CXX = g++
CXXFLAGS = -std=c++20 -O2 -Wall -pthread -I.

lb: load_balancer.cpp
	$(CXX) $(CXXFLAGS) -o lb load_balancer.cpp

clean:
	rm -f lb

.PHONY: clean
