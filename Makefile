CXX        := g++
LD         := g++

SRCDIR     := src
OBJDIR     := obj
BINDIR     := bin
EXE        := nes

CXXFLAGS   := -Wall -Wno-unused-function -m32 -masm=intel -std=c++11 -g -pg
CXXFLAGS   += -I$(SRCDIR) -I$(SRCDIR)/m6502 -I$(SRCDIR)/n2C02 -I$(SRCDIR)/x86
LDFLAGS    := -pg -m32
LIBS       := -lSDL2 -lpthread

CXXFLAGS   += -DNDEBUG -DPPU_MAX_FPS -O1

# -DPPU_MAX_FPS
# -DPPU_DEBUG
# -DCPU_BACKTRACE

SRC        := x86/X86Emitter.cc
SRC        += m6502/M6502State.cc m6502/M6502Eval.cc m6502/M6502Asm.cc m6502/M6502Jit.cc
SRC        += jit/entry.S
SRC        += n2C02/N2C02State.cc
# SRC    += rp2A03/RP2A03State.cc
SRC        += mappers/nrom.cc mappers/mmc1.cc mappers/cnrom.cc mappers/mmc3.cc
SRC        += Memory.cc CodeBuffer.cc Events.cc Joypad.cc Rom.cc Core.cc main.cc

OBJS       := $(patsubst %.S, $(OBJDIR)/%.o, $(patsubst %.cc,$(OBJDIR)/%.o, $(SRC)))
DEPS       := $(patsubst %.cc,$(OBJDIR)/%.d, $(SRC))

Q = @

ifneq ($(DEBUG),)
CXXFLAGS   += -DDEBUG=$(DEBUG)
endif


all: $(EXE)

-include $(DEPS)

$(OBJDIR)/%.o: $(SRCDIR)/%.cc
	@echo "  CXX      $*.cc"
	@mkdir -p $(dir $@)
	$(Q)$(CXX) $(CXXFLAGS) -c $< -o $@
	$(Q)$(CXX) $(CXXFLAGS) -MT "$@" -MM $< > $(OBJDIR)/$*.d

$(OBJDIR)/%.o: $(SRCDIR)/%.S
	@echo "  CXX      $*.S"
	@mkdir -p $(dir $@)
	$(Q)$(CXX) $(CXXFLAGS) -c $< -o $@
	$(Q)$(CXX) $(CXXFLAGS) -MT "$@" -MM $< > $(OBJDIR)/$*.d

$(EXE): $(OBJS)
	@echo "  LD       $@"
	$(Q)$(LD) -o $@ $(LDFLAGS) $(OBJS) $(LIBS)

clean:
	@rm -rf $(OBJDIR)/* $(EXE)

.PHONY: clean all
