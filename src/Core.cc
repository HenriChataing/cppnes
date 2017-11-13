
#include "type.h"
#include "exception.h"
#include "Core.h"
#include "Memory.h"
#include "M6502State.h"
#include "M6502Eval.h"
#include "M6502Jit.h"
#include "N2C02State.h"
#include "Events.h"

#include <iostream>
#include <ctime>
#include <chrono>
#include <thread>

using namespace std;

namespace Core
{

/**
 * Emulation routine.
 */
void emulate()
{
    M6502::currentState->clear();
    M6502::currentState->reset();
    N2C02::currentState->clear();

    try {
        while (true) {
            /* Catch interrupts */
            if (M6502::currentState->nmi) {
                M6502::Eval::triggerNMI();
            } else
            if (M6502::currentState->irq) {
                M6502::Eval::triggerIRQ();
            }
            /* Try jit */
            M6502::Instruction *instr = M6502::cache.cache(M6502::currentState->regs.pc);
            if (instr != NULL)
                instr->run();
            /* Evaluate next instruction */
            u8 opcode = Memory::load(M6502::currentState->regs.pc);
            M6502::Eval::runOpcode(opcode);
            N2C02::sync();
            while (Events::isPaused() && !Events::isQuit()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            if (Events::isQuit())
                break;
        }
    } catch (const UnsupportedInstruction &exc) {
        std::cerr << "Fatal error (core): " << exc.what();
        std::cerr << ": " << (int)exc.opcode << std::endl;
        Events::quit();
    } catch (const JammingInstruction &exc) {
        std::cerr << "Fatal error (core): " << exc.what();
        std::cerr << ": " << (int)exc.opcode << std::endl;
        Events::quit();
    } catch (const char *msg) {
        std::cerr << "Fatal error (core): " << msg << std::endl;
        Events::quit();
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
