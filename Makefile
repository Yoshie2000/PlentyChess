CC  = clang
CFLAGS = -mpopcnt -Wall -pedantic -Wextra -fcommon -pthread -O3 -flto=auto -fuse-ld=lld
CXX = clang++
CXXFLAGS = -std=c++17 -Wall -pedantic -Wextra -fcommon -pthread -O3 -flto=auto -fuse-ld=lld
CXXFLAGS_EXTRA = 

SOURCES = src/engine.cpp src/board.cpp src/move.cpp src/uci.cpp src/search.cpp src/thread.cpp src/evaluation.cpp src/tt.cpp src/magic.cpp src/bitboard.cpp src/history.cpp src/nnue.cpp src/time.cpp src/spsa.cpp src/zobrist.cpp src/datagen.cpp src/fathom/src/tbprobe.c
OBJS = $(patsubst %.cpp,%.o, $(patsubst %.c,%.o, $(SOURCES)))

# Compiler detection for PGO
COMPILER_VERSION := $(shell $(CXX) --version)
ifneq (, $(filter profile-build _pgo,$(MAKECMDGOALS)))
	ifneq (, $(findstring clang,$(COMPILER_VERSION)))
		PGO_GENERATE := -fprofile-instr-generate -DPROFILE_GENERATE
		PGO_USE := -fprofile-instr-use=default.profdata
		PGO_MERGE := llvm-profdata merge -output=default.profdata default.profraw
		PGO_FILES := default.profraw default.profdata

		ifneq ($(OS), Windows_NT)
		    ifneq (,$(shell xcrun -f llvm-profdata 2>/dev/null))
		        PGO_MERGE := xcrun $(PGO_MERGE)
		    else
			    ifeq (,$(shell which llvm-profdata))
$(warning llvm-profdata not found, disabling profile-build)
				    PGO_SKIP := true
				endif
			endif
		else
			ifeq (,$(shell where llvm-profdata))
$(warning llvm-profdata not found, disabling profile-build)
				PGO_SKIP := true
			endif
		endif
	else
		PGO_GENERATE := -fprofile-generate
		PGO_USE := -fprofile-use
		PGO_MERGE := 
		PGO_FILES := pgo src/*.gcda *.gcda
	endif
endif

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
ifeq ($(arch), arm64)
	CXXFLAGS := $(CXXFLAGS) -DARCH_ARM
else ifeq ($(arch), avx512vnni)
	CXXFLAGS := $(CXXFLAGS) -DARCH_X86 -march=cascadelake
	CFLAGS := $(CFLAGS) -march=cascadelake
else ifeq ($(arch), avx512)
	CXXFLAGS := $(CXXFLAGS) -DARCH_X86 -march=skylake-avx512
	CFLAGS := $(CFLAGS) -march=skylake-avx512
else ifeq ($(arch), avx2)
	CXXFLAGS := $(CXXFLAGS) -DARCH_X86 -march=haswell
	CFLAGS := $(CFLAGS) -march=haswell
else ifeq ($(arch), fma)
	CXXFLAGS := $(CXXFLAGS) -DARCH_X86 -mssse3 -mfma
	CFLAGS := $(CFLAGS) -mssse3 -mfma
else ifeq ($(arch), ssse3)
	CXXFLAGS := $(CXXFLAGS) -DARCH_X86 -mssse3
	CFLAGS := $(CFLAGS) -mssse3
else ifeq ($(arch), generic)
	CXXFLAGS := $(CXXFLAGS) -DARCH_X86
else ifneq ($(origin arch), undefined)
$(error Architecture not supported: $(arch))
else
	ifneq ($(OS), Windows_NT)
		ARCH_CMD := $(shell uname -m)
		ifeq ($(ARCH_CMD), x86_64)
			CXXFLAGS := $(CXXFLAGS) -DARCH_X86
		else ifeq ($(ARCH_CMD), aarch64)
			CXXFLAGS := $(CXXFLAGS) -DARCH_ARM
		else ifeq ($(ARCH_CMD), arm64)
			CXXFLAGS := $(CXXFLAGS) -DARCH_ARM
		else
$(error Architecture not supported: $(ARCH_CMD))
		endif
	else
		CXXFLAGS := $(CXXFLAGS) -DARCH_X86
	endif

	CXXFLAGS := $(CXXFLAGS) -march=native
	CFLAGS := $(CFLAGS) -march=native
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
	CFLAGS := $(CFLAGS) -mbmi2
endif

ifdef PROCESS_NET
	CXXFLAGS := $(CXXFLAGS) -DPROCESS_NET
	PROCESS_NET := true
else
	PROCESS_NET := false
endif

# Windows only flags
ifeq ($(OS), Windows_NT)
	CXXFLAGS := $(CXXFLAGS) -lstdc++ -static
endif

# Network flags
ifndef EVALFILE
	NET_ID := $(shell cat network.txt)
	EVALFILE := $(NET_ID).bin
	EVALFILE_NOT_DEFINED = true
endif

CXXFLAGS := $(CXXFLAGS) -DEVALFILE=\"processed.bin\"

# Targets

.PHONY:	all
.DEFAULT_GOAL := all

process-net:
ifndef SKIP_PROCESS_NET
	$(info Using network $(EVALFILE))
ifdef EVALFILE_NOT_DEFINED
ifeq ($(wildcard $(EVALFILE)),)
	$(info Downloading network $(NET_ID).bin)
	curl -sOL https://github.com/Yoshie2000/PlentyNetworks/releases/download/$(NET_ID)/$(EVALFILE)
endif
endif
	$(info Processing network)
	$(MAKE) -C tools clean
	$(MAKE) -C tools arch=$(arch)
	./tools/process_net $(PROCESS_NET) $(EVALFILE) ./processed.bin
endif

all:
		$(MAKE) process-net
		$(MAKE) _nopgo SKIP_PROCESS_NET=true

profile-build:
ifneq ($(PGO_SKIP), true)
		$(MAKE) process-net
		$(MAKE) _pgo SKIP_PROCESS_NET=true
else
		$(MAKE) all
endif

%.o:	%.cpp
		$(CXX) $(CXXFLAGS) $(CXXFLAGS_EXTRA) -c $< -o $@

_pgo:	CXXFLAGS_EXTRA := $(PGO_GENERATE)
_pgo:	$(OBJS)
		$(CXX) $(CXXFLAGS) $(CXXFLAGS_EXTRA) $(filter-out $(EVALFILE) process-net,$^) -o $(PROGRAM)
		./$(PROGRAM) bench
		$(RM) src/*.o *~ engine
		$(PGO_MERGE)
		$(MAKE) CXXFLAGS_EXTRA="$(PGO_USE)" _nopgo
		$(RM) -rf $(PGO_FILES)

_nopgo:	$(OBJS)
		$(CXX) $(CXXFLAGS) $(CXXFLAGS_EXTRA) $(filter-out $(EVALFILE) process-net,$^) -o $(PROGRAM)

clean:	
		$(RM) src/*.o src/fathom/src/*.o *~ engine processed.bin
