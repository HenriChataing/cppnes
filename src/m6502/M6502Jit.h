
#ifndef _M6502JIT_H_INCLUDED_
#define _M6502JIT_H_INCLUDED_

#include <queue>
#include <stack>

#include "M6502Asm.h"
#include "X86Emitter.h"
#include "type.h"

namespace M6502 {

class Instruction
{
public:
    Instruction(u16 address, u8 opcode, u8 op0 = 0, u8 op1 = 0);
    ~Instruction();

    void setNext(Instruction *instr);
    void compile(X86::Emitter &emit);
    void run();

    u16 address;
    u16 branchAddress;

    u8 opcode;
    u8 operand0;
    u8 operand1;

    /**
     * This instruction is a valid entry point. The compiled native code can
     * only be executed starting on an entry point.
     */
    bool entry;

    /**  This instruction triggers an exit from the native code. */
    bool exit;

    /**  Identifies branching instructions. */
    bool branch;

    /** Flags that are required by subsequent instructions. */
    u8 requiredFlags;

    friend class InstructionCache;

private:
    Instruction *next;
    Instruction *jump;
    const u8 *nativeCode;
    u32 *nativeBranchAddress;
};

class InstructionCache
{
public:
    InstructionCache(CodeBuffer *buffer);
    ~InstructionCache();

    Instruction *fetchInstruction(u16 address);
    Instruction *cache(u16 address);

    size_t getSize() const { return _asmEmitter.getSize(); }

private:
    Instruction *cacheInstruction(u16 address);
    Instruction *cacheBlock(u16 address);

    X86::Emitter _asmEmitter;
    Instruction **_cache;
    size_t _cacheSize;
    std::queue<Instruction *> _queue;
    std::stack<Instruction *> _stack;
};

extern InstructionCache cache;

};

#endif /* _M6502JIT_H_INCLUDED_ */
