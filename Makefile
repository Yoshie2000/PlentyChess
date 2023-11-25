CXX = g++
CXXFLAGS = -std=c++17 -Wall -pedantic -Wextra -fcommon -pthread -O3

SOURCES = src/engine.cpp src/board.cpp src/move.cpp src/uci.cpp
OBJS = $(patsubst %.cpp,%.o, $(SOURCES))

all:	engine

engine:	$(OBJS)
		$(CXX) $(CXXFLAGS) $^ -o $@

clean:	
		$(RM) src/*.o *~ engine