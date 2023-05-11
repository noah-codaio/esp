# Copyright (c) 2011-2023 Columbia University, System Level Design Group
# SPDX-License-Identifier: Apache-2.0
include ../../../common/common.mk

ifeq ("$(STRATUS_PATH)", "")
$(error please define STRATUS_PATH required for FlexChannels and FixedPoint library headers)
endif

ifeq ("$(SYSTEMC)", "")
$(error please define SYSTEMC to execute a standalone simulation)
endif

ifeq ("$(DMA_WIDTH)", "")
$(error please define the desired DMA_WIDTH for simulation)
endif

INCDIR ?=
INCDIR += -I../src
INCDIR += -I../tb
INCDIR += -I$(SYSTEMC)/include
INCDIR += -I$(STRATUS_PATH)/share/stratus/include
INCDIR += -I$(ESP_ROOT)/accelerators/stratus_hls/common/inc
INCDIR += -I$(ESP_ROOT)/tools/esp-noxim/bin/libs/yaml-cpp/include
INCDIR += -I$(ESP_ROOT)/tools/esp-noxim/src

CXXFLAGS ?=
CXXFLAGS += -O3
CXXFLAGS += $(INCDIR)
CXXFLAGS += -DDMA_WIDTH=$(DMA_WIDTH)
CXXFLAGS += -DCLOCK_PERIOD=10000
CXXFLAGS += -DSC_INCLUDE_DYNAMIC_PROCESSES

LDFLAGS :=
LDFLAGS += -L$(SYSTEMC)/lib-linux64
LDFLAGS += -lsystemc
LDFLAGS += -L$(ESP_ROOT)/tools/esp-noxim/bin/libs/yaml-cpp/lib
LDFLAGS += -lyaml-cpp


TARGET = $(ACCELERATOR)

VPATH ?=
VPATH += ../src
VPATH += ../tb
VPATH += $(ESP_ROOT)/tools/esp-noxim/src 
VPATH += $(ESP_ROOT)/tools/esp-noxim/src/routingAlgorithms
VPATH += $(ESP_ROOT)/tools/esp-noxim/src/selectionStrategies

SRCS :=
SRCS += $(foreach s, $(wildcard ../src/*.cpp) $(wildcard ../tb/*.cpp) $(filter-out $(ESP_ROOT)/tools/esp-noxim/src/Main.cpp, $(wildcard $(ESP_ROOT)/tools/esp-noxim/src/*.cpp)) $(wildcard $(ESP_ROOT)/tools/esp-noxim/src/routingAlgorithms/*.cpp) $(wildcard $(ESP_ROOT)/tools/esp-noxim/src/selectionStrategies/*.cpp), $(shell basename $(s)))

RUN_ARGS += -config ~/esp/tools/esp-noxim/config/esp_demo_config.yaml 
RUN_ARGS += -power ~/esp/tools/esp-noxim/bin/power.yaml 
RUN_ARGS += -traffic table ~/esp/tools/esp-noxim/config/exp_demo_ttable.txt 

OBJS := $(SRCS:.cpp=.o)

HDRS := $(wildcard ../src/*.hpp) $(wildcard ../tb/*.hpp)


all: $(TARGET)

.SUFFIXES: .cpp .hpp .o

$(OBJS): $(HDRS)

$(TARGET): $(OBJS)
	$(CXX) -o $@ $^ ${LDFLAGS}
	@echo $(SRCS)

.cpp.o:
	$(CXX) $(CXXFLAGS) ${INCDIR} -c $< -o $@

run: $(TARGET)
	LD_LIBRARY_PATH=$(LD_LIBRARY_PATH):$(SYSTEMC)/lib-linux64 ./$< $(RUN_ARGS)

clean:
	$(QUIET_CLEAN)rm -f *.o $(TARGET)

.PHONY: all clean run
