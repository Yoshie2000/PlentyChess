CXX = g++
CXXFLAGS = -std=c++17 -Wall -pedantic -Wextra -fcommon -fext-numeric-literals -pthread -O3 -funroll-all-loops

SOURCES = src/engine.cpp src/board.cpp src/move.cpp src/uci.cpp src/search.cpp src/thread.cpp src/evaluation.cpp src/tt.cpp src/magic.cpp src/bitboard.cpp src/history.cpp src/nnue.cpp src/time.cpp src/spsa.cpp
OBJS = $(patsubst %.cpp,%.o, $(SOURCES))

# Debug vs. Production flags
PROGRAM = engine
ifdef EXE
	PROGRAM = $(EXE)
	CXXFLAGS := $(CXXFLAGS) -DNDEBUG
else
	CXXFLAGS := $(CXXFLAGS) -g -ggdb
endif

# CPU Flags
ifeq ($(arch), avx512)
	CXXFLAGS := $(CXXFLAGS) -march=skylake-avx512
else ifeq ($(arch), avx2)
	CXXFLAGS := $(CXXFLAGS) -march=haswell
else ifeq ($(arch), generic)
	CXXFLAGS := $(CXXFLAGS)
else
	CXXFLAGS := $(CXXFLAGS) -march=native
endif

all:	engine

engine:	$(OBJS)
		$(CXX) $(CXXFLAGS) $^ -o $(PROGRAM)

clean:	
		$(RM) src/*.o *~ engine