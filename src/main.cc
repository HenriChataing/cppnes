
#include <iostream>

#include "Rom.h"
#include "Core.h"
#include "Mapper.h"
#include "M6502State.h"

int main(int argc, char *argv[])
{
    currentRom = new Rom(argv[1]);
    M6502::currentState = new M6502::State();
    Core::emulate();
    return 0;
}

#if 0

#include "X86Emitter.h"

typedef void (*proc_t)();

void test()
{
    std::cout << "Test called" << std::endl;
}

    X86::Emitter emit(0x1000);

    const void *proc = emit.getPtr();

    emit.PUSH(X86::eax);
    emit.PUSH(X86::ecx);
    emit.PUSH(X86::edx);
    emit.MOV(X86::eax, (u32)test);
    emit.CALL(X86::eax);
    emit.POP(X86::edx);
    emit.POP(X86::ecx);
    emit.POP(X86::eax);
    emit.RETN();

    std::cout << "Procedure at " << proc << std::endl;
    emit.dump();

    ((proc_t)proc)();
    std::cout << "After jumping" << std::endl;

#endif
