CXX = g++
CXXFLAGS = -std=c++17 -Wall -pedantic -Wextra -fcommon -pthread -O3 -g -ggdb

SOURCES = src/engine.cpp src/board.cpp src/move.cpp src/uci.cpp src/search.cpp src/thread.cpp src/evaluation.cpp
OBJS = $(patsubst %.cpp,%.o, $(SOURCES))

all:	engine

engine:	$(OBJS)
		$(CXX) $(CXXFLAGS) $^ -o $@

clean:	
		$(RM) src/*.o *~ engine