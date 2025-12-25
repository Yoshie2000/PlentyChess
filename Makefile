CXX = clang++
CC  = $(CXX)
CFLAGS = -w -pthread -O3
CXXFLAGS = -std=c++17 -Wall -Wextra -pedantic -Wshadow -fcommon -pthread -O3
LDFLAGS = 
CXXFLAGS_EXTRA = 

SOURCES = src/engine.cpp src/board.cpp src/move.cpp src/uci.cpp src/search.cpp src/thread.cpp src/evaluation.cpp src/tt.cpp src/magic.cpp src/bitboard.cpp src/history.cpp src/nnue.cpp src/time.cpp src/spsa.cpp src/zobrist.cpp src/datagen.cpp src/threat-inputs.cpp src/debug.cpp src/fathom/src/tbprobe.c
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

ifeq ($(arch),)
# Autodetect architecture if none is provided

# First detect x86 / arm
	ifneq ($(OS), Windows_NT)
		ARCH_CMD := $(shell uname -m)
		ifeq ($(ARCH_CMD), aarch64)
			arch := arm64
$(info Autodetected architecture: arm64)
		else ifeq ($(ARCH_CMD), arm64)
			arch := arm64
$(info Autodetected architecture: arm64)
		else ifneq ($(ARCH_CMD), x86_64)
$(error Architecture not supported: $(ARCH_CMD))
		endif
	endif
endif
# If not arm, determine optimal x86 compile
ifeq ($(arch),)
# Detect CPU flags
	ifeq ($(OS), Windows_NT)
		HAS_SSSE3 := $(shell .\detect_flags.bat $(CXX) __SSSE3)
		HAS_FMA := $(shell .\detect_flags.bat $(CXX) __FMA)
		HAS_AVX2 := $(shell .\detect_flags.bat $(CXX) __AVX2)
		HAS_BMI2 := $(shell .\detect_flags.bat $(CXX) __BMI2)
		HAS_AVX512 := $(shell .\detect_flags.bat $(CXX) __AVX512)
		IS_ZEN1  := $(shell .\detect_flags.bat $(CXX) __znver1)
		IS_ZEN2  := $(shell .\detect_flags.bat $(CXX) __znver2)
		IS_ZEN5  := $(shell .\detect_flags.bat $(CXX) __znver5)
	else
		HAS_SSSE3 := $(shell echo | $(CXX) -march=native -dM -E - | grep -c "__SSSE3")
		HAS_FMA := $(shell echo | $(CXX) -march=native -dM -E - | grep -c "__FMA")
		HAS_AVX2 := $(shell echo | $(CXX) -march=native -dM -E - | grep -c "__AVX2")
		HAS_BMI2 := $(shell echo | $(CXX) -march=native -dM -E - | grep -c "__BMI2")
		HAS_AVX512 := $(shell echo | $(CXX) -march=native -dM -E - | grep -c "__AVX512")
		IS_ZEN1  := $(shell echo | $(CXX) -march=native -dM -E - | grep -c "__znver1")
		IS_ZEN2  := $(shell echo | $(CXX) -march=native -dM -E - | grep -c "__znver2")
		IS_ZEN5  := $(shell echo | $(CXX) -march=native -dM -E - | grep -c "__znver5")
	endif

# Select best build
	ifneq ($(IS_ZEN5),0)
		arch := avx512vnni
	else ifneq ($(HAS_AVX512),0)
		arch := avx512
	else ifneq ($(HAS_AVX2),0)
		arch := avx2

# On Zen1/2 systems, which have slow PEXT instructions, we don't want to build with BMI2
		ifeq ($(HAS_BMI2),1)
			ifeq ($(IS_ZEN1),0)
				ifeq ($(IS_ZEN2),0)
					arch := bmi2
				endif
			endif
		endif

	else ifneq ($(HAS_FMA),0)
		arch := fma
	else ifneq ($(HAS_SSSE3),0)
		arch := ssse3
	else
		arch := generic
	endif
$(info Autodetected architecture: $(arch))
endif

# CPU Flags
ifeq ($(arch), android)
	CXXFLAGS := $(CXXFLAGS) -DARCH_ARM -march=armv8-a+simd -static
	LDFLAGS := $(LDFLAGS) -lstdc++
	CXX := aarch64-linux-android29-clang++
	CC := aarch64-linux-android29-clang
else ifeq ($(arch), arm64)
	CXXFLAGS := $(CXXFLAGS) -DARCH_ARM -march=armv8-a+simd
else ifeq ($(arch), avx512vnni)
	CXXFLAGS := $(CXXFLAGS) -DARCH_X86 -DUSE_BMI2 -march=cascadelake -mbmi2
	CFLAGS := $(CFLAGS) -march=cascadelake -mbmi2
else ifeq ($(arch), avx512)
	CXXFLAGS := $(CXXFLAGS) -DARCH_X86 -DUSE_BMI2 -march=skylake-avx512 -mbmi2
	CFLAGS := $(CFLAGS) -march=skylake-avx512 -mbmi2
else ifeq ($(arch), bmi2)
	CXXFLAGS := $(CXXFLAGS) -DARCH_X86 -DUSE_BMI2 -march=haswell -mbmi2
	CFLAGS := $(CFLAGS) -march=haswell -mbmi2
else ifeq ($(arch), avx2)
	CXXFLAGS := $(CXXFLAGS) -DARCH_X86 -march=haswell
	CFLAGS := $(CFLAGS) -march=haswell
else ifeq ($(arch), fma)
	CXXFLAGS := $(CXXFLAGS) -DARCH_X86 -mssse3 -mfma -finline-functions -fno-exceptions -pipe -fno-rtti -fomit-frame-pointer -fsee
	CFLAGS := $(CFLAGS) -mssse3 -mfma -finline-functions -fno-exceptions -pipe -fno-rtti -fomit-frame-pointer -fsee
else ifeq ($(arch), ssse3)
	CXXFLAGS := $(CXXFLAGS) -DARCH_X86 -mssse3 -finline-functions -fno-exceptions -pipe -fno-rtti -fomit-frame-pointer -fsee
	CFLAGS := $(CFLAGS) -mssse3 -finline-functions -fno-exceptions -pipe -fno-rtti -fomit-frame-pointer -fsee
else ifeq ($(arch), generic)
	CXXFLAGS := $(CXXFLAGS) -DARCH_X86 -finline-functions -fno-exceptions -pipe -fno-rtti -fomit-frame-pointer -fsee
	CFLAGS := $(CFLAGS) -finline-functions -fno-exceptions -pipe -fno-rtti -fomit-frame-pointer -fsee
else
$(error Architecture not supported: $(arch))
endif

ifdef PROCESS_NET
	CXXFLAGS := $(CXXFLAGS) -DPROCESS_NET
	PROCESS_NET := true
else
	PROCESS_NET := false
endif

# Windows only flags
ifeq ($(OS), Windows_NT)
	CXXFLAGS := $(CXXFLAGS) -static
	LDFLAGS := $(LDFLAGS) -lstdc++ -fuse-ld=lld
	
	CLANG_IS_MSVC := $(findstring msvc,$(shell $(CXX) --version))
	ifeq ($(CLANG_IS_MSVC),msvc)
		CFLAGS := $(CFLAGS) -flto=auto
        	CXXFLAGS := $(CXXFLAGS) -flto=auto
	endif
else
	CFLAGS := $(CFLAGS) -flto=auto
	CXXFLAGS := $(CXXFLAGS) -flto=auto

	UNAME_S := $(shell uname -s)
# Use LLD on Linux with clang if major versions match
	ifneq ($(UNAME_S), Darwin)
# Get major versions
    CLANG_MAJOR := $(shell $(CXX) -dumpversion | cut -d. -f1)
    LLD_VERSION_STR := $(shell ld.lld --version 2>/dev/null)
    
    ifneq ($(LLD_VERSION_STR),)
        LLD_MAJOR := $(shell ld.lld --version | grep -oE '[0-9]+' | head -n1)
        
# Only use LLD if it exists and matches Clang's major version
        ifeq ($(CLANG_MAJOR),$(LLD_MAJOR))
            ifeq ($(shell $(CXX) -fuse-ld=lld -Wl,--version >/dev/null 2>&1 && echo yes),yes)
                COMPILER := $(shell $(CXX) --version | head -n 1)
                ifeq (,$(findstring g++,$(COMPILER)))
                    LDFLAGS := $(LDFLAGS) -fuse-ld=lld
                endif
            endif
        endif
    endif
else
    CFLAGS := $(filter-out -mpopcnt,$(CFLAGS))
endif
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
		$(CXX) $(CXXFLAGS) $(CXXFLAGS_EXTRA) $(LDFLAGS) $(filter-out $(EVALFILE) process-net,$^) -o $(PROGRAM)
		./$(PROGRAM) bench
		$(RM) src/*.o *~ engine
		$(PGO_MERGE)
		$(MAKE) CXXFLAGS_EXTRA="$(PGO_USE)" _nopgo
		$(RM) -rf $(PGO_FILES)

_nopgo:	$(OBJS)
		$(CXX) $(CXXFLAGS) $(CXXFLAGS_EXTRA) $(LDFLAGS) $(filter-out $(EVALFILE) process-net,$^) -o $(PROGRAM)

clean:	
		$(RM) src/*.o src/fathom/src/*.o *~ engine processed.bin
