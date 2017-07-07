
#include "Core.h"
#include "Memory.h"
#include "M6502State.h"
#include "M6502Eval.h"
#include "type.h"

#include <iostream>

namespace Core
{

/**
 * Emulation rountine.
 */
void emulate()
{
    M6502::currentState->reset();
    for (int i = 0; i < 10; i++) {
        u8 opcode = Memory::load(M6502::currentState->regs.pc);
        M6502::Eval::runOpcode(opcode);
    }
}

};
