
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
                instr->run(1000);
            /* Evaluate next instruction */
            u8 opcode = Memory::load(M6502::currentState->regs.pc);
            M6502::Eval::runOpcode(opcode);
            N2C02::sync();
            while (Events::isPaused() && !Events::isQuit()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            if (Events::isQuit()) {
                M6502::backtrace();
                break;
            }
        }
    } catch (const UnsupportedInstruction &exc) {
        std::cerr << "Fatal error (core): " << exc.what();
        std::cerr << ": " << (int)exc.opcode << std::endl;
        M6502::backtrace();
        Events::quit();
    } catch (const JammingInstruction &exc) {
        std::cerr << "Fatal error (core): " << exc.what();
        std::cerr << ": " << (int)exc.opcode << std::endl;
        M6502::backtrace();
        Events::quit();
    } catch (const char *msg) {
        std::cerr << "Fatal error (core): " << msg << std::endl;
        Events::quit();
    } catch (const std::exception &exc) {
        std::cerr << "Fatal error (core): " << exc.what() << std::endl;
        Events::quit();
    }
}

};
