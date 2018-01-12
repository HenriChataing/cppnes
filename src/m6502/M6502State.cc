
#include <iostream>
#include <cstddef>

#include "Memory.h"
#include "M6502State.h"

using namespace M6502;

namespace M6502 {
    State *state = NULL;
};

State::State()
{
    clear();
}

State::~State()
{
}

void State::clear()
{
    regs.pc = 0;
    regs.sp = 0xfd;
    regs.a = 0;
    regs.x = 0;
    regs.y = 0;
    regs.p = 0x24;
    cycles = 0;
    nmi = false;
    irq = false;
}

void State::reset()
{
    regs.pc = Memory::loadw(Memory::RST_ADDR);
}
