CC     := g++
LD     := g++

SRCDIR := src
OBJDIR := obj
BINDIR := bin
EXE    := nes

CFLAGS := -Wall -Wno-unused-function -m32 -std=c++11 -g -pg
CFLAGS += -I$(SRCDIR) -I$(SRCDIR)/m6502 -I$(SRCDIR)/x86
LFLAGS := -pg -m32
LIBS   :=

SRC    := x86/X86Emitter.cc
SRC    += m6502/M6502State.cc m6502/M6502Eval.cc m6502/M6502Asm.cc m6502/M6502Jit.cc
SRC    += mappers/nrom.cc
SRC    += Memory.cc Rom.cc Core.cc main.cc

OBJS   := $(patsubst %.cc,$(OBJDIR)/%.o, $(SRC))
DEPS   := $(patsubst %.cc,$(OBJDIR)/%.d, $(SRC))

Q = @

ifneq ($(DEBUG),)
CFLAGS += -DDEBUG=$(DEBUG)
endif


all: $(EXE)

-include $(DEPS)

$(OBJDIR)/%.o: $(SRCDIR)/%.cc
	@echo "  CC      $*.c"
	@mkdir -p $(dir $@)
	$(Q)$(CC) $(CFLAGS) -c $< -o $@
	$(Q)$(CC) $(CFLAGS) -MT "$@" -MM $< > $(OBJDIR)/$*.d

# $(Q)$(CC) $(CFLAGS) -E $< > $(OBJDIR)/$*.m

$(EXE): $(OBJS)
	@echo "  LD      $@"
	$(Q)$(LD) -o $@ $(LFLAGS) $(OBJS) $(LIBS)

clean:
	@rm -rf $(OBJDIR)/* $(EXE)

.PHONY: clean all
