CXX = g++
# Use relative paths for portability
H3PPCI_INC_DIR = ./include
H3PPCI_LIB_DIR = ./lib

# CUDA path discovery
# Priority: 1. Environment variable CUDA_PATH, 2. /usr/local/cuda, 3. Results from 'which nvcc'
CUDA_PATH ?= $(firstword $(wildcard /usr/local/cuda) $(shell which nvcc >/dev/null 2>&1 && readlink -f $$(which nvcc) | sed 's|/bin/nvcc||'))

# Robust include path detection by searching for the actual NVTX header
POSSIBLE_HEADERS = $(CUDA_PATH)/targets/x86_64-linux/include/nvtx3/nvToolsExtCounters.h \
                   $(CUDA_PATH)/include/nvtx3/nvToolsExtCounters.h \
                   /usr/include/nvtx3/nvToolsExtCounters.h \
                   /usr/local/cuda/include/nvtx3/nvToolsExtCounters.h

FOUND_HEADER := $(firstword $(foreach h,$(POSSIBLE_HEADERS),$(wildcard $(h))))
CUDA_INC_DIR := $(patsubst %/nvtx3/nvToolsExtCounters.h,%,$(FOUND_HEADER))

# Fallback if detection fails
ifeq ($(CUDA_INC_DIR),)
    CUDA_INC_DIR := /usr/local/cuda/include
endif

CXXFLAGS = -I$(H3PPCI_INC_DIR) -I$(CUDA_INC_DIR) -Wall
# Set rpath to $ORIGIN/lib so the dynamic linker finds libh3ppci.so in the local lib folder
LDFLAGS = -L$(H3PPCI_LIB_DIR) -lh3ppci -Wl,-rpath,'$$ORIGIN/lib' -ldl

TARGET = sw_nsys_plugin
SRCS = src/sw_nsys_plugin.cpp

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(TARGET)
