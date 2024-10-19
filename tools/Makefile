CXX = g++
CXXFLAGS = -std=c++17 -Wall -pedantic -Wextra -fcommon -pthread -O3 -funroll-all-loops -g -ggdb

SOURCES = process_net.cpp
OBJS = $(patsubst %.cpp,%.o, $(SOURCES))

PROGRAM = process_net

%.o:	%.cpp
		$(CXX) $(CXXFLAGS) $(CXXFLAGS_EXTRA) -c $< -o $@

all:	$(OBJS)
		$(CXX) $(CXXFLAGS) $(CXXFLAGS_EXTRA) $^ -o $(PROGRAM)

clean:	
		$(RM) *.o *~ $(PROGRAM)