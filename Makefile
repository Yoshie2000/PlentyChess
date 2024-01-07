CXX = g++
CXXFLAGS = -std=c++17 -Wall -pedantic -Wextra -fcommon -O3

ifeq ($(OS),Windows_NT)
	CXXFLAGS := $(CXXFLAGS) -DNO_PTHREADS
else
    CXXFLAGS := $(CXXFLAGS) -pthread
endif

SOURCES = src/engine.cpp src/board.cpp src/move.cpp src/uci.cpp src/search.cpp src/thread.cpp src/evaluation.cpp src/tt.cpp src/magic.cpp src/bitboard.cpp src/history.cpp
OBJS = $(patsubst %.cpp,%.o, $(SOURCES))

PROGRAM = engine
ifdef EXE
	PROGRAM = $(EXE)
	CXXFLAGS := $(CXXFLAGS) -DNDEBUG
else
	CXXFLAGS := $(CXXFLAGS) -g -ggdb
endif

all:	engine

engine:	$(OBJS)
		$(CXX) $(CXXFLAGS) $^ -o $(PROGRAM)

clean:	
		$(RM) src/*.o *~ engine