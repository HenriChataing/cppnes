CC     := g++
LD     := g++

SRCDIR := src
OBJDIR := obj
BINDIR := bin
EXE    := nes

CFLAGS := -Wall -Wno-unused-function -m32 -std=c++11 -g -pg
CFLAGS += -I$(SRCDIR) -I$(SRCDIR)/m6502 -I$(SRCDIR)/n2C02 -I$(SRCDIR)/x86
LFLAGS := -pg -m32
LIBS   := -lSDL2 -lpthread

CFLAGS +=
# -DPPU_MAX_FPS
# -DPPU_DEBUG

SRC    := x86/X86Emitter.cc
SRC    += m6502/M6502State.cc m6502/M6502Eval.cc m6502/M6502Asm.cc m6502/M6502Jit.cc
SRC    += n2C02/N2C02State.cc
SRC    += mappers/nrom.cc mappers/mmc1.cc mappers/cnrom.cc
SRC    += Memory.cc Events.cc Joypad.cc Rom.cc Core.cc main.cc

OBJS   := $(patsubst %.cc,$(OBJDIR)/%.o, $(SRC))
DEPS   := $(patsubst %.cc,$(OBJDIR)/%.d, $(SRC))

Q = @

ifneq ($(DEBUG),)
CFLAGS += -DDEBUG=$(DEBUG)
endif


all: $(EXE)

-include $(DEPS)

$(OBJDIR)/%.o: $(SRCDIR)/%.cc
	@echo "  CC      $*.cc"
	@mkdir -p $(dir $@)
	$(Q)$(CC) $(CFLAGS) -c $< -o $@
	$(Q)$(CC) $(CFLAGS) -MT "$@" -MM $< > $(OBJDIR)/$*.d

$(EXE): $(OBJS)
	@echo "  LD      $@"
	$(Q)$(LD) -o $@ $(LFLAGS) $(OBJS) $(LIBS)

clean:
	@rm -rf $(OBJDIR)/* $(EXE)

.PHONY: clean all
