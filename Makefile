CXX = g++
CXXFLAGS = -std=c++17 -Wall -pedantic -Wextra -fcommon -fext-numeric-literals -pthread -O3 -funroll-all-loops
CXXFLAGS_EXTRA = 

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
	HAS_BMI2 := $(shell echo | $(CXX) -march=native -dM -E - | grep -c "__BMI2__")
	IS_ZEN1  := $(shell echo | $(CXX) -march=native -dM -E - | grep -c "__znver1")
	IS_ZEN2  := $(shell echo | $(CXX) -march=native -dM -E - | grep -c "__znver2")
# On Zen1/2 systems, which have slow PEXT instructions, we don't want to build with BMI2
	ifeq ($(HAS_BMI2),1)
		ifneq ($(IS_ZEN1),1)
			ifneq ($(IS_ZEN2),1)
				CXXFLAGS := $(CXXFLAGS) -DUSE_BMI2
			endif
		endif
	endif
endif

ifdef BMI2
	CXXFLAGS := $(CXXFLAGS) -DUSE_BMI2 -mbmi2
endif

# Windows only flags
ifeq ($(OS), Windows_NT)
	CXXFLAGS := $(CXXFLAGS) -lstdc++ -static -Wl,--no-as-needed,--stack,16777216
endif

%.o:	%.cpp
		$(CXX) $(CXXFLAGS) $(CXXFLAGS_EXTRA) -c $< -o $@

all:	pgo

pgo:	CXXFLAGS_EXTRA := -fprofile-generate="pgo"
pgo:	$(OBJS)
		$(CXX) $(CXXFLAGS) $(CXXFLAGS_EXTRA) $^ -o $(PROGRAM)
		./$(PROGRAM) bench
		$(MAKE) clean
		$(MAKE) CXXFLAGS_EXTRA="-fprofile-use="pgo"" nopgo
		rm -rf pgo

nopgo:	$(OBJS)
		$(CXX) $(CXXFLAGS) $(CXXFLAGS_EXTRA) $^ -o $(PROGRAM)

clean:	
		$(RM) src/*.o *~ engine