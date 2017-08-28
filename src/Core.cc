
#include "type.h"
#include "Core.h"
#include "Memory.h"
#include "M6502State.h"
#include "M6502Eval.h"
#include "M6502Jit.h"
#include "N2C02State.h"
#include "Events.h"

#include <iostream>
#include <ctime>

using namespace std;

namespace Core
{

/**
 * Emulation rountine.
 */
void emulate()
{
    M6502::currentState->clear();
    M6502::currentState->reset();

    try {
        while (true) {
            uint cycles = M6502::currentState->cycles;
            /* Catch interrupts */
            if (M6502::currentState->nmi) {
                M6502::Eval::triggerNMI();
            }
            /* Evaluate next instruction */
            u8 opcode = Memory::load(M6502::currentState->regs.pc);
            M6502::Eval::runOpcode(opcode);
            cycles = M6502::currentState->cycles - cycles;
            while (cycles > 0) {
                N2C02::dot();
                N2C02::dot();
                N2C02::dot();
                cycles--;
            }
            if (Events::isQuit())
                break;
        }
    } catch (const std::exception &exc) {
        std::cerr << "Fatal error (core): " << exc.what() << std::endl;
        Events::quit();
    }
}

#if 0

/**
 * Emulation rountine.
 */
void emulate()
{
    M6502::InstructionCache cache;
    clock_t begin, end;
    double elapsed;

    begin = clock();

    for (int round = 0; round < 10000; round++) {
        M6502::currentState->clear();
        M6502::currentState->reset();
        /// FIXME remove this line !
        M6502::currentState->regs.pc = 0xc000;

        try {
            while (M6502::currentState->cycles < 6000) {
                u8 opcode = Memory::load(M6502::currentState->regs.pc);
                M6502::Eval::runOpcode(opcode);
            }
        } catch (char const * s) {
            std::cerr << "Fatal error: " << s << std::endl;
        }
    }

    end = clock();
    elapsed = double(end - begin) * 1000.f / CLOCKS_PER_SEC;

    cerr << "Interpreter : " << elapsed << " ms" << endl;

    begin = clock();

    for (int round = 0; round < 10000; round++) {
        M6502::currentState->clear();
        M6502::currentState->reset();
        /// FIXME remove this line !
        M6502::currentState->regs.pc = 0xc000;

        try {
            while (M6502::currentState->cycles < 6000) {
                M6502::Instruction *instr;
                u8 opcode;

                instr = cache.fetchInstruction(M6502::currentState->regs.pc);
                if (instr) {
                    instr->run();
                } else {
                    instr = cache.cacheBlock(M6502::currentState->regs.pc);
                    if (instr)
                        instr->run();
                }
                opcode = Memory::load(M6502::currentState->regs.pc);
                M6502::Eval::runOpcode(opcode);
            }
        } catch (char const * s) {
            std::cerr << "Fatal error: " << s << std::endl;
        }
    }

    end = clock();
    elapsed = double(end - begin) * 1000.f / CLOCKS_PER_SEC;

    cerr << "Dynamic recompiler : " << elapsed << " ms" << endl;
    cerr << "Cache size: " << hex << cache.getSize() << endl;
}

#endif

};
