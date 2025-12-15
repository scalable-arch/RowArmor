SHELL := /bin/sh

.PHONY: all clean
.SUFFIXES: .cc .o

default: all

ifeq ($(TAG),dbg)
  DBG = -Wall
  OPT = -ggdb -g -O0
else
  DBG =
  OPT = -O3 -g -Wno-nonnull
endif

CXXFLAGS = -Wno-unknown-pragmas $(DBG) $(OPT) -std=c++11
CXX = g++ -DTARGET_IA32E
CC  = gcc -DTARGET_IA32E

PINFLAGS = 

SRCS = PTSCache.cc \
  PTSComponent.cc \
  PTSCore.cc \
  PTSO3Core.cc \
	PTSHash.cc \
	PTSDirectory.cc \
	PTSRBoL.cc \
	PTSMemoryController.cc \
	PTSTLB.cc \
	PTSXbar.cc \
  McSim.cc \
	PTS.cc

OBJS = $(patsubst %.cc,obj_$(TAG)/%.o,$(SRCS))

all: obj_$(TAG)/mcsim

obj_$(TAG)/mcsim : $(OBJS) main.cc
	$(CXX) $(CXXFLAGS) -o obj_$(TAG)/mcsim $(OBJS) main.cc

obj_$(TAG)/%.o : %.cc
	$(CXX) -c $(CXXFLAGS) $(PIN_CXXFLAGS) $(INCS) -o $@ $<

clean:
	@echo "Cleaning..."
	@rm -f *.o pin.log mcsim 
