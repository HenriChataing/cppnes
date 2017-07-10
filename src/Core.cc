
#include "Core.h"
#include "Memory.h"
#include "M6502State.h"
#include "M6502Eval.h"
#include "M6502Jit.h"
#include "type.h"

#include <iostream>

namespace Core
{

/**
 * Emulation rountine.
 */
void emulate()
{
    M6502::InstructionCache cache;
    M6502::Instruction *instr;

    M6502::currentState->reset();

    try {
        for (int i = 0; i < 10; i++) {
            u8 opcode;
            instr = cache.cacheBlock(M6502::currentState->regs.pc);
            if (instr) {
                if (instr->branch)
                    opcode = instr->opcode;
                else {
                    instr->run();
                    opcode = Memory::load(M6502::currentState->regs.pc);
                }
            } else
                opcode = Memory::load(M6502::currentState->regs.pc);
            M6502::Eval::runOpcode(opcode);
        }
    } catch (char const * s) {
        std::cerr << "Fatal error: " << s << std::endl;
    }
}

};

// 8000  78        SEI          A:00 X:00 Y:00 P:24 SP:FD CYC:0
// 8001  D8        CLD          A:00 X:00 Y:00 P:24 SP:FD CYC:2
// 8002  A9 10     LDA #$10     A:00 X:00 Y:00 P:24 SP:FD CYC:4
// 8004  8D 00 20  STA $2000    A:10 X:00 Y:00 P:24 SP:FD CYC:6
// 8007  A2 FF     LDX #$FF     A:10 X:00 Y:00 P:24 SP:FD CYC:10
// 8009  9A        TXS          A:10 X:FF Y:00 P:A4 SP:FD CYC:12
// 800A  AD 02 20  LDA $2002    A:10 X:FF Y:00 P:A4 SP:FF CYC:14
// 800D  10 FB     BPL $800A    A:00 X:FF Y:00 P:26 SP:FF CYC:18
// 800A  AD 02 20  LDA $2002    A:00 X:FF Y:00 P:26 SP:FF CYC:21
// 800D  10 FB     BPL $800A    A:00 X:FF Y:00 P:26 SP:FF CYC:25