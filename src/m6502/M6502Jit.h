
#ifndef _M6502JIT_H_INCLUDED_
#define _M6502JIT_H_INCLUDED_

#include "M6502Asm.h"
#include "X86Emitter.h"
#include "type.h"

namespace M6502 {

class Instruction
{
    public:
        Instruction(X86::Emitter &emit, u16 address, u8 opcode);
        ~Instruction();
        void run();
        void setNext(Instruction *instr);

        u16 address;
        u8 opcode;
        Instruction *next;
        Instruction *jump;
        const void *nativeCode;
        bool branch;
};

class InstructionCache
{
    public:
        InstructionCache();
        ~InstructionCache();

        Instruction *fetchInstruction(u16 address);
        Instruction *cacheInstruction(u16 address, u8 opcode);
        Instruction *cacheBlock(u16 address);

    private:
        X86::Emitter _asmEmitter;
        Instruction **_cache;
        size_t _cacheSize;
};

};

#endif /* _M6502JIT_H_INCLUDED_ */
