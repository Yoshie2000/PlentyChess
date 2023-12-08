CXX = g++
CXXFLAGS = -std=c++17 -Wall -pedantic -Wextra -fcommon -pthread -O3

SOURCES = src/engine.cpp src/board.cpp src/move.cpp src/uci.cpp src/search.cpp src/thread.cpp src/evaluation.cpp src/tt.cpp src/magic.cpp
OBJS = $(patsubst %.cpp,%.o, $(SOURCES))

PROGRAM = engine
ifdef EXE
	PROGRAM = $(EXE)
else
	CXXFLAGS := $(CXXFLAGS) -g -ggdb
endif

all:	engine

engine:	$(OBJS)
		$(CXX) $(CXXFLAGS) $^ -o $(PROGRAM)

clean:	
		$(RM) src/*.o *~ engine