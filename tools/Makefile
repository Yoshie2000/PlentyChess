CXX = g++
CXXFLAGS = -std=c++17 -Wall -pedantic -Wextra -fcommon -pthread -O3 -funroll-all-loops -Wno-ignored-optimization-argument -g -ggdb

SOURCES = process_net.cpp

PROGRAM = process_net

# CPU Flags
ifeq ($(arch), arm64)
	CXXFLAGS := $(CXXFLAGS) -DARCH_ARM
else ifeq ($(arch), avx512vnni)
	CXXFLAGS := $(CXXFLAGS) -DARCH_X86 -march=cascadelake
else ifeq ($(arch), avx512)
	CXXFLAGS := $(CXXFLAGS) -DARCH_X86 -march=skylake-avx512
else ifeq ($(arch), avx2)
	CXXFLAGS := $(CXXFLAGS) -DARCH_X86 -march=haswell
else ifeq ($(arch), fma)
	CXXFLAGS := $(CXXFLAGS) -DARCH_X86 -mssse3 -mfma
else ifeq ($(arch), ssse3)
	CXXFLAGS := $(CXXFLAGS) -DARCH_X86 -mssse3
else ifeq ($(arch), generic)
	CXXFLAGS := $(CXXFLAGS) -DARCH_X86
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
endif

all:	$(SOURCES)
		$(CXX) $(CXXFLAGS) $(CXXFLAGS_EXTRA) $^ -o $(PROGRAM)

clean:	
		$(RM) *~ $(PROGRAM)