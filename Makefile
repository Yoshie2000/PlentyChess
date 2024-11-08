CXX = g++
CXXFLAGS = -std=c++17 -Wall -pedantic -Wextra -fcommon -pthread -O3 -funroll-all-loops
CXXFLAGS_EXTRA = 

SOURCES = src/engine.cpp src/board.cpp src/move.cpp src/uci.cpp src/search.cpp src/thread.cpp src/evaluation.cpp src/tt.cpp src/magic.cpp src/bitboard.cpp src/history.cpp src/nnue.cpp src/time.cpp src/spsa.cpp src/zobrist.cpp src/datagen.cpp
OBJS = $(patsubst %.cpp,%.o, $(SOURCES))

# Debug vs. Production flags
PROGRAM = engine
ifdef EXE
	PROGRAM = $(EXE)
	CXXFLAGS := $(CXXFLAGS) -DNDEBUG
else
	CXXFLAGS := $(CXXFLAGS) -g -ggdb
endif

ifdef INCLUDE_DEBUG_SYMBOLS
	CXXFLAGS := $(CXXFLAGS) -g -ggdb
endif

# CPU Flags
ifeq ($(arch), avx512vnni)
	CXXFLAGS := $(CXXFLAGS) -march=cascadelake
else ifeq ($(arch), avx512)
	CXXFLAGS := $(CXXFLAGS) -march=skylake-avx512
else ifeq ($(arch), avx2)
	CXXFLAGS := $(CXXFLAGS) -march=haswell
else ifeq ($(arch), fma)
	CXXFLAGS := $(CXXFLAGS) -mssse3 -mfma
else ifeq ($(arch), ssse3)
	CXXFLAGS := $(CXXFLAGS) -mssse3
else ifeq ($(arch), generic)
	CXXFLAGS := $(CXXFLAGS)
else
	CXXFLAGS := $(CXXFLAGS) -march=native
	ifeq ($(OS), Windows_NT)
		HAS_BMI2 := $(shell .\detect_flags.bat $(CXX) __BMI2__)
		IS_ZEN1  := $(shell .\detect_flags.bat $(CXX) __znver1)
		IS_ZEN2  := $(shell .\detect_flags.bat $(CXX) __znver2)
	else
		HAS_BMI2 := $(shell echo | $(CXX) -march=native -dM -E - | grep -c "__BMI2__")
		IS_ZEN1  := $(shell echo | $(CXX) -march=native -dM -E - | grep -c "__znver1")
		IS_ZEN2  := $(shell echo | $(CXX) -march=native -dM -E - | grep -c "__znver2")
	endif
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

ifdef PROCESS_NET
	CXXFLAGS := $(CXXFLAGS) -DPROCESS_NET
endif

# Windows only flags
ifeq ($(OS), Windows_NT)
	CXXFLAGS := $(CXXFLAGS) -lstdc++ -static -Wl,--no-as-needed
endif

# Network flags
ifndef EVALFILE
	NET_ID := $(file < network.txt)
	EVALFILE := $(NET_ID).bin
	EVALFILE_NOT_DEFINED = true
endif

CXXFLAGS := $(CXXFLAGS) -DEVALFILE=\"$(EVALFILE)\"

# Targets

.PHONY:	all
.DEFAULT_GOAL := all

ifdef EVALFILE_NOT_DEFINED
$(EVALFILE):
	$(info Downloading network $(NET_ID))
	curl -sOL https://github.com/Yoshie2000/PlentyNetworks/releases/download/$(NET_ID)/$(EVALFILE)
endif

all:	nopgo

%.o:	%.cpp
		$(CXX) $(CXXFLAGS) $(CXXFLAGS_EXTRA) -c $< -o $@

pgo:	CXXFLAGS_EXTRA := -fprofile-generate="pgo"
pgo:	$(EVALFILE) $(OBJS)
		$(CXX) $(CXXFLAGS) $(CXXFLAGS_EXTRA) $(filter-out $(EVALFILE),$^) -o $(PROGRAM)
		./$(PROGRAM) bench
		$(MAKE) clean
		$(MAKE) CXXFLAGS_EXTRA="-fprofile-use="pgo"" nopgo
		$(RM) -rf pgo

nopgo:	$(EVALFILE) $(OBJS)
		$(CXX) $(CXXFLAGS) $(CXXFLAGS_EXTRA) $(filter-out $(EVALFILE),$^) -o $(PROGRAM)

clean:	
		$(RM) src/*.o *~ engine