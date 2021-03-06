
#include <iostream>
#include <iomanip>

#include "M6502Jit.h"
#include "M6502State.h"
#include "M6502Eval.h"
#include "Memory.h"

#define PAGE_DIFF(addr0, addr1) ((((addr0) ^ (addr1)) & 0xff00) != 0)

using namespace M6502;

namespace M6502 {
InstructionCache cache(new CodeBuffer());
};

Instruction::Instruction(u16 address, u8 opcode, u8 op0, u8 op1)
    : address(address), opcode(opcode), operand0(op0), operand1(op1),
      entry(false), exit(false)
{
    if (Asm::instructions[opcode].type == Asm::REL) {
        u8 off = op0;
        if (off & 0x80) {
            off = ~off + 1;
            branchAddress = address + 2 - off;
        } else
            branchAddress = address + 2 + off;
        branch = true;
    } else {
        branch = false;
    }

    switch (opcode) {
        case BRK_IMP:
        case JMP_ABS:
        case JMP_IND:
        case JSR_ABS:
        case RTI_IMP:
        case RTS_IMP:
        // case BCC_REL:
        // case BCS_REL:
        // case BEQ_REL:
        // case BMI_REL:
        // case BNE_REL:
        // case BPL_REL:
        // case BVC_REL:
        // case BVS_REL:
            exit = true;
            break;
        default:
            break;
    }

    next = NULL;
    jump = NULL;
    nativeCode = NULL;
    nativeBranchAddress = NULL;
}

/**
 * TODO list
 *  - track which flags are tested in an instruction block, and pass on that
 *      information to the instruction compilers, to save restoreStatusFlags
 *      and testZeroSign calls
 *  - analyse the address of load and store absolute instructions,
 *      and implement the operation accordingly
 *  - test if branch targets do fall in the current PRG-ROM bank
 *  - compile absolute jumps
 *  - compile indirect jumps
 *  - study the possiblity of using actual PUSH instructions to implement
 *      6502 stack operations
 */

InstructionCache::InstructionCache(CodeBuffer *buffer)
:
    _asmEmitter(buffer)
{
    _cacheSize = 0x8000;
    _cache = new Instruction *[0x8000];
}

InstructionCache::~InstructionCache()
{
    for (size_t i = 0; i < _cacheSize; i++)
        if (_cache[i])
            delete _cache[i];
    delete _cache;
}

Instruction *InstructionCache::fetchInstruction(u16 address)
{
    uint offset = address - 0x8000;
    return _cache[offset];
}

Instruction *InstructionCache::cacheInstruction(u16 address)
{
    uint offset = address - 0x8000;
    if (_cache[offset] != NULL)
        return _cache[offset];
    u8 opcode = Memory::load(address);
    size_t bytes = Asm::instructions[opcode].bytes;
    /// FIXME check for bank overflow
    u8 op0 = bytes > 1 ? Memory::load(address + 1) : 0;
    u8 op1 = bytes > 2 ? Memory::load(address + 2) : 0;
    Instruction *instr = new Instruction(address, opcode, op0, op1);
    _cache[offset] = instr;
    return instr;
}

Instruction *InstructionCache::cacheBlock(u16 address)
{
    u16 pc = address;
    Instruction *instr = NULL;
    Instruction *first = NULL;
    Instruction **last = &first;
    // _stack.clear();

    // const u8 *ptr = _asmEmitter.getPtr();

    while (1) {
        instr = cacheInstruction(pc);
        *last = instr;
        last = &instr->next;

        if (instr->nativeCode || instr->exit)
            break;
        if (instr->branch)
            _queue.push(instr);

        _stack.push(instr);
        pc += Asm::instructions[instr->opcode].bytes;
    }

    /* Fast flag analysis */
    u8 requiredFlags = M6502::Asm::all; // leaving the jit, all flags must be set
    while (!_stack.empty()) {
        instr = _stack.top();
        _stack.pop();
        instr->requiredFlags = requiredFlags;
        if (instr->branch) {
            requiredFlags = M6502::Asm::all;
        } else {
            requiredFlags &= ~Asm::instructions[instr->opcode].wflags;
            requiredFlags |= Asm::instructions[instr->opcode].rflags;
        }
    }

    /* Compile instruction block. */
    for (instr = first; instr != NULL; instr = instr->next) {
        if (instr->nativeCode) {
            if (instr != first)
                _asmEmitter.JMP(instr->nativeCode);
            break;
        }
        instr->compile(_asmEmitter);
        if (instr->exit)
            break;
    }

    // if (_asmEmitter.getPtr() != ptr) {
    //     _asmEmitter.dump(ptr);
    // }

    /* Return block start */
    return first;
}

Instruction *InstructionCache::cache(u16 address)
{
    if (address < 0x8000)
        return NULL;

    // _queue.clear();
    Instruction *block = cacheBlock(address);

    while (!_queue.empty()) {
        Instruction *branch, *target;
        branch = _queue.front();
        _queue.pop();
        target = cacheBlock(branch->branchAddress);
        _asmEmitter.setJump(branch->nativeBranchAddress, target->nativeCode);
    }

    return block;
}


namespace Jit {

const X86::Reg<u8> M = X86::dl;
const X86::Reg<u8> A = X86::dh;
const X86::Reg<u8> X = X86::bl;
const X86::Reg<u8> Y = X86::bh;

static u8 requiredFlags;

};

/**
 * Increment the cycle count.
 */
static void incrementCycles(X86::Emitter &emit, u32 upd)
{
    if (upd == 1)
        emit.INC(X86::esi);
    else
        emit.ADD(X86::esi, upd);
}

/**
 * Check the cycle count.
 */
static void checkCycles(X86::Emitter &emit, u16 address)
{
    emit.CMP(X86::esi, 0);
    u32 *jmp = emit.JL();
    emit.MOV(X86::eax, address);
    emit.POPF();
    emit.RETN();
    emit.setJump(jmp);
}

/**
 * Check compatibility of 6502 status flags against x86 status flags.
 */
static bool checkStatusFlags(u32 mask, u8 flags)
{
    u8 converted = 0;
    if (mask & X86::carry) converted |= M6502::Asm::carry;
    if (mask & X86::sign) converted |= M6502::Asm::negative;
    if (mask & X86::zero) converted |= M6502::Asm::zero;
    if (mask & X86::overflow) converted |= M6502::Asm::overflow;

    return (converted & flags) != 0;
}

/**
 * Generate bytecode to update selected status flags in the saved
 * state pushed onto the stack.
 */
static void updateStatusFlags(X86::Emitter &emit, u32 mask, u8 flags)
{
    if (!checkStatusFlags(mask, flags))
        return;

    /// FIXME
    // Cannot use
    //   emit.OR(X86::esp(), X86::eax);
    // as this generates the restricted R/MOD byte 0x04, for SIB addressing,
    // which is not yet supported.
    emit.PUSHF();
    emit.POP(X86::ecx);
    emit.AND(X86::ecx, mask);
    emit.AND(X86::esp(), ~mask);
    emit.OR(X86::esp(), X86::ecx);
}

/**
 * Generate bytecode to load the status flags from the saved
 * state pushed onto the stack.
 */
static void restoreStatusFlags(X86::Emitter &emit)
{
    emit.POPF();
    emit.PUSHF();
}

/**
 * Generate bytecode to set the Zero and Sign bits depending on the value
 * in the given register \p r.
 */
static void testZeroSign(X86::Emitter &emit, const X86::Reg<u8> &r, u8 flags)
{
    if ((flags & M6502::Asm::carry) == 0 &&
        (flags & M6502::Asm::negative) == 0)
        return;
    emit.TEST(r, r);
    updateStatusFlags(emit, X86::zero | X86::sign, flags);
}

/**
 * Generate the bytecode for a comparison between two byte size registers.
 * The status flags cannot be directly used as there are some variations
 * on the expected values.
 */
static void compare(
    X86::Emitter &emit,
    const X86::Reg<u8> &r0,
    const X86::Reg<u8> &r1)
{
    /*
     * Complement the carry flag (the signification is opposite for
     * 6502 (set if r0 greater than or equal to r1).
     */
    emit.CMP(r0, r1);
    emit.CMC();
    updateStatusFlags(emit, X86::zero | X86::sign | X86::carry, Jit::requiredFlags);
}

static inline bool ADC(X86::Emitter &emit, const X86::Reg<u8> &r) {
    emit.POPF();
    emit.ADC(Jit::A, r);
    emit.PUSHF();
    return false;
}

static inline bool AND(X86::Emitter &emit, const X86::Reg<u8> &r) {
    emit.AND(Jit::A, r);
    updateStatusFlags(emit, X86::zero | X86::sign, Jit::requiredFlags);
    return false;
}

static inline bool ASL(X86::Emitter &emit, const X86::Reg<u8> &r) {
    emit.SHL(r);
    updateStatusFlags(emit, X86::zero | X86::sign | X86::carry, Jit::requiredFlags);
    return true;
}

static inline bool CMP(X86::Emitter &emit, const X86::Reg<u8> &r) {
    compare(emit, Jit::A, r);
    return false;
}

static inline bool CPX(X86::Emitter &emit, const X86::Reg<u8> &r) {
    compare(emit, Jit::X, r);
    return false;
}

static inline bool CPY(X86::Emitter &emit, const X86::Reg<u8> &r) {
    compare(emit, Jit::Y, r);
    return false;
}

/** Unofficial opcode, composition of DEC and CMP. */
static inline bool DCP(X86::Emitter &emit, const X86::Reg<u8> &r) {
    emit.DEC(r);
    compare(emit, Jit::A, r);
    return true;
}

static inline bool DEC(X86::Emitter &emit, const X86::Reg<u8> &r) {
    emit.DEC(r);
    updateStatusFlags(emit, X86::zero | X86::sign, Jit::requiredFlags);
    return true;
}

static inline void DEX(X86::Emitter &emit) {
    emit.DEC(Jit::X);
    updateStatusFlags(emit, X86::zero | X86::sign, Jit::requiredFlags);
}

static inline void DEY(X86::Emitter &emit) {
    emit.DEC(Jit::Y);
    updateStatusFlags(emit, X86::zero | X86::sign, Jit::requiredFlags);
}

static inline bool EOR(X86::Emitter &emit, const X86::Reg<u8> &r) {
    emit.XOR(Jit::A, r);
    updateStatusFlags(emit, X86::zero | X86::sign, Jit::requiredFlags);
    return false;
}

static inline bool INC(X86::Emitter &emit, const X86::Reg<u8> &r) {
    emit.INC(r);
    updateStatusFlags(emit, X86::zero | X86::sign, Jit::requiredFlags);
    return true;
}

static inline void INX(X86::Emitter &emit) {
    emit.INC(Jit::X);
    updateStatusFlags(emit, X86::zero | X86::sign, Jit::requiredFlags);
}

static inline void INY(X86::Emitter &emit) {
    emit.INC(Jit::Y);
    updateStatusFlags(emit, X86::zero | X86::sign, Jit::requiredFlags);
}

/** Unofficial opcode, composition of INC and SBC. */
static inline bool ISB(X86::Emitter &emit, const X86::Reg<u8> &r) {
    emit.INC(r);
    emit.POPF();
    emit.CMC();
    emit.SBB(Jit::A, r);
    emit.CMC();
    emit.PUSHF();
    return true;
}

static inline bool LSR(X86::Emitter &emit, const X86::Reg<u8> &r) {
    emit.SHR(r);
    updateStatusFlags(emit, X86::zero | X86::sign | X86::carry, Jit::requiredFlags);
    return true;
}

static inline bool ORA(X86::Emitter &emit, const X86::Reg<u8> &r) {
    emit.OR(Jit::A, r);
    updateStatusFlags(emit, X86::zero | X86::sign, Jit::requiredFlags);
    return false;
}

/** Unofficial opcode, composition of ROL and AND. */
static inline bool RLA(X86::Emitter &emit, const X86::Reg<u8> &r) {
    restoreStatusFlags(emit);
    emit.RCL(r);
    updateStatusFlags(emit, X86::carry, Jit::requiredFlags);
    emit.AND(Jit::A, r);
    updateStatusFlags(emit, X86::zero | X86::sign, Jit::requiredFlags);
    return true;
}

static inline bool ROL(X86::Emitter &emit, const X86::Reg<u8> &r) {
    restoreStatusFlags(emit);
    emit.RCL(r);
    updateStatusFlags(emit, X86::zero | X86::sign | X86::carry, Jit::requiredFlags);
    testZeroSign(emit, r, Jit::requiredFlags);
    return true;
}

static inline bool ROR(X86::Emitter &emit, const X86::Reg<u8> &r) {
    restoreStatusFlags(emit);
    emit.RCR(r);
    updateStatusFlags(emit, X86::zero | X86::sign | X86::carry, Jit::requiredFlags);
    testZeroSign(emit, r, Jit::requiredFlags);
    return true;
}

/** Unofficial opcode, composition of ROR and ADC. */
static inline bool RRA(X86::Emitter &emit, const X86::Reg<u8> &r) {
    restoreStatusFlags(emit);
    emit.RCR(r);
    updateStatusFlags(emit, X86::zero | X86::sign | X86::carry, Jit::requiredFlags);
    testZeroSign(emit, r, Jit::requiredFlags);
    emit.POPF();
    emit.ADC(Jit::A, r);
    emit.PUSHF();
    return true;
}

static inline bool SBC(X86::Emitter &emit, const X86::Reg<u8> &r) {
    emit.POPF();
    emit.CMC();
    emit.SBB(Jit::A, r);
    emit.CMC();
    emit.PUSHF();
    return false;
}

/** Unofficial opcode, composition of ASL and ORA. */
static inline bool SLO(X86::Emitter &emit, const X86::Reg<u8> &r) {
    emit.SHL(r);
    updateStatusFlags(emit, X86::carry, Jit::requiredFlags);
    emit.OR(Jit::A, r);
    updateStatusFlags(emit, X86::zero | X86::sign, Jit::requiredFlags);
    return true;
}

/** Unofficial opcode, composition of LSR and EOR. */
static inline bool SRE(X86::Emitter &emit, const X86::Reg<u8> &r) {
    emit.SHR(r);
    updateStatusFlags(emit, X86::carry, Jit::requiredFlags);
    emit.XOR(Jit::A, r);
    updateStatusFlags(emit, X86::zero | X86::sign, Jit::requiredFlags);
    return true;
}

static inline void PUSH(X86::Emitter &emit, const X86::Reg<u8> &r) {
    emit.MOV(X86::ecx, X86::edi);
    emit.MOV(X86::ecx(), r);
    emit.DEC(X86::cl); // Will wrap on stack overflow
    emit.MOV(X86::edi, X86::ecx);
}

static inline void PULL(X86::Emitter &emit, const X86::Reg<u8> &r) {
    emit.MOV(X86::ecx, X86::edi);
    emit.INC(X86::cl); // Will wrap on stack underflow
    emit.MOV(r, X86::ecx());
    emit.MOV(X86::edi, X86::ecx);
}

static inline bool NOP(X86::Emitter &emit, const X86::Reg<u8> &r) {
    (void)emit;
    (void)r;
    return false;
}

static inline bool BIT(X86::Emitter &emit, const X86::Reg<u8> &r) {
    emit.POP(X86::ecx);
    emit.PUSH(X86::eax); // Save eax
    emit.AND(X86::ecx, 0xfffff73f); // Clear Zero,Sign,Overflow flags.
    emit.MOV(X86::al, r);
    emit.AND(X86::al, 0x80);
    emit.OR(X86::cl, X86::al); // OR bit 7 with N flag
    emit.MOV(X86::al, r);
    emit.SHL(X86::eax, 5);
    emit.AND(X86::ah, 0x8);
    emit.OR(X86::ch, X86::ah); // OR bit 6 with O flag
    emit.TEST(r, Jit::A);
    emit.PUSHF();
    emit.POP(X86::eax);
    emit.AND(X86::al, 0x40); // Keep zero flag.
    emit.OR(X86::cl, X86::al);
    emit.POP(X86::eax); // restore eax
    emit.PUSH(X86::ecx);
    return false;
}

static inline void CLC(X86::Emitter &emit) {
    emit.POPF();
    emit.CLC();
    emit.PUSHF();
}

static inline void CLD(X86::Emitter &emit) {
    u8 *p = &state->regs.p;
    emit.MOV(X86::eax, (u32)p);
    emit.AND(X86::eax(), (u8)0xf7);
}

static inline void CLI(X86::Emitter &emit) {
    u8 *p = &state->regs.p;
    emit.MOV(X86::eax, (u32)p);
    emit.AND(X86::eax(), (u8)0xfb);
}

static inline void CLV(X86::Emitter &emit) {
    emit.POP(X86::eax);
    emit.AND(X86::eax, 0xfffff7ff);
    emit.PUSH(X86::eax);
}

/** Unofficial instruction. */
static inline bool LAX(X86::Emitter &emit, const X86::Reg<u8> &r) {
    if (r != Jit::A)
        emit.MOV(Jit::A, r);
    if (r != Jit::X)
        emit.MOV(Jit::X, r);
    testZeroSign(emit, r, Jit::requiredFlags);
    return false;
}

static inline bool LDA(X86::Emitter &emit, const X86::Reg<u8> &r) {
    if (r != Jit::A)
        emit.MOV(Jit::A, r);
    testZeroSign(emit, r, Jit::requiredFlags);
    return false;
}

static inline bool LDX(X86::Emitter &emit, const X86::Reg<u8> &r) {
    if (r != Jit::X)
        emit.MOV(Jit::X, r);
    testZeroSign(emit, Jit::X, Jit::requiredFlags);
    return false;
}

static inline bool LDY(X86::Emitter &emit, const X86::Reg<u8> &r) {
    if (r != Jit::Y)
        emit.MOV(Jit::Y, r);
    testZeroSign(emit, Jit::Y, Jit::requiredFlags);
    return false;
}

static inline void PHA(X86::Emitter &emit) {
    PUSH(emit, Jit::A);
}

static inline void PHP(X86::Emitter &emit) {
    u8 *p = &state->regs.p;
    emit.MOV(X86::eax, (u32)p);
    emit.MOV(Jit::M, X86::eax());
    emit.AND(Jit::M, 0x3c); // Clear Carry, Zero, Overflow, Sign flags
    emit.OR(Jit::M, 0x30); // Set virtual flags
    emit.POP(X86::ecx);
    emit.PUSH(X86::ecx);
    emit.AND(X86::cl, 0x81);
    emit.OR(Jit::M, X86::cl);
    emit.POP(X86::ecx);
    emit.PUSH(X86::ecx);
    emit.SHR(X86::ecx, 5);
    emit.AND(X86::cl, 0x42);
    emit.OR(Jit::M, X86::cl);
    PUSH(emit, Jit::M);
}

static inline void PLA(X86::Emitter &emit) {
    PULL(emit, Jit::A);
    testZeroSign(emit, Jit::A, Jit::requiredFlags);
}

static inline void PLP(X86::Emitter &emit) {
    u8 *p = &state->regs.p;
    /* Unstack new flag values */
    PULL(emit, Jit::M);
    emit.AND(Jit::M, ~0x30); // Clear virtual flags
    /* Update Interrupt, Decimal, etc. flags in state memory. */
    emit.MOV(X86::eax, (u32)p);
    emit.MOV(X86::eax(), Jit::M);
    /* Update x86 status flags. */
    emit.POP(X86::ecx);
    emit.AND(X86::ecx, 0xfffff73e); // Clear Carry, Zero, Sign, Overflow bits
    emit.MOV(X86::al, Jit::M);
    emit.AND(X86::al, 0x81);
    emit.OR(X86::cl, X86::al);
    emit.MOV(X86::eax, 0);
    emit.MOV(X86::al, Jit::M);
    emit.SHL(X86::eax, 5);
    emit.AND(X86::eax, 0x840);
    emit.OR(X86::ecx, X86::eax);
    emit.PUSH(X86::ecx);
}

static inline void SEC(X86::Emitter &emit) {
    emit.POPF();
    emit.STC();
    emit.PUSHF();
}

static inline void SED(X86::Emitter &emit) {
    u8 *p = &state->regs.p;
    emit.MOV(X86::eax, (u32)p);
    emit.OR(X86::eax(), (u8)0x8);
}

static inline void SEI(X86::Emitter &emit) {
    u8 *p = &state->regs.p;
    emit.MOV(X86::eax, (u32)p);
    emit.OR(X86::eax(), (u8)0x4);
}

static inline void TAX(X86::Emitter &emit) {
    emit.MOV(Jit::X, Jit::A);
    testZeroSign(emit, Jit::X, Jit::requiredFlags);
}

static inline void TAY(X86::Emitter &emit) {
    emit.MOV(Jit::Y, Jit::A);
    testZeroSign(emit, Jit::Y, Jit::requiredFlags);
}

static inline void TSX(X86::Emitter &emit) {
    emit.MOV(X86::ecx, X86::edi);
    emit.MOV(Jit::X, X86::cl);
    testZeroSign(emit, Jit::X, Jit::requiredFlags);
}

static inline void TXA(X86::Emitter &emit) {
    emit.MOV(Jit::A, Jit::X);
    testZeroSign(emit, Jit::A, Jit::requiredFlags);
}

static inline void TYA(X86::Emitter &emit) {
    emit.MOV(Jit::A, Jit::Y);
    testZeroSign(emit, Jit::A, Jit::requiredFlags);
}

static inline void TXS(X86::Emitter &emit) {
    emit.MOV(X86::ecx, X86::edi);
    emit.MOV(X86::cl, Jit::X);
    emit.MOV(X86::edi, X86::ecx);
}

/**
 * Same as AND, with the difference that the status flag N is also copied
 * to the status flag C.
 */
static inline bool AAC(X86::Emitter &emit, const X86::Reg<u8> &r) {
    throw "AAC unimplemented";
    return false;
}


/**
 * This opcode ANDs the contents of the A register with an immediate value and
 * then LSRs the result.
 */
static inline bool ASR(X86::Emitter &emit, const X86::Reg<u8> &r) {
    throw "ASR unimplemented";
    return false;
}


/**
 * AND u8 with accumulator, then rotate one bit right in accu-mulator and
 * check bit 5 and 6:
 *      if both bits are 1: set C, clear V.
 *      if both bits are 0: clear C and V.
 *      if only bit 5 is 1: set V, clear C.
 *      if only bit 6 is 1: set C and V.
 */
static inline bool ARR(X86::Emitter &emit, const X86::Reg<u8> &r) {
    throw "ARR unimplemented";
    return false;
}

/**
 * AND u8 with accumulator, then transfer accumulator to X register.
 */
static inline bool ATX(X86::Emitter &emit, const X86::Reg<u8> &r) {
    throw "ATX unimplemented";
    return false;
}

/**
 * ANDs the contents of the A and X registers (leaving the contents of A
 * intact), subtracts an immediate value, and then stores the result in X.
 * ... A few points might be made about the action of subtracting an immediate
 * value.  It actually works just like the CMP instruction, except that CMP
 * does not store the result of the subtraction it performs in any register.
 * This subtract operation is not affected by the state of the Carry flag,
 * though it does affect the Carry flag.  It does not affect the Overflow
 * flag.
 */
static inline bool AXS(X86::Emitter &emit, const X86::Reg<u8> &r) {
    throw "AXS unimplemented";
    return false;
}

/**
 * Emit the code for a specific 6502 operation, regardless of the addressing
 * mode.
 * @param emit      code emitter
 * @param r         register to hold the operand and / or the result of the
 *                  operation
 * @return          true iff the register \p r was updated (i.e. the value
 *                  should be written back to memory)
 */
typedef bool (*Operation)(X86::Emitter &emit, const X86::Reg<u8> &r);

static void loadImmediate(
    X86::Emitter &emit,
    u16 pc,
    Operation cont,
    bool wb,
    const X86::Reg<u8> &r = Jit::M)
{
    emit.MOV(r, Memory::load(pc + 1));
    cont(emit, r);
}

static void loadZeroPage(
    X86::Emitter &emit,
    u16 pc,
    Operation cont,
    bool wb,
    const X86::Reg<u8> &r = Jit::M)
{
    u8 *zp = Memory::ram;
    u8 off = Memory::load(pc + 1);
    emit.MOV(X86::eax, (u32)(zp + off));
    emit.MOV(r, X86::eax());
    cont(emit, r);
    if (wb)
        emit.MOV(X86::eax(), r);
}

static void storeZeroPage(
    X86::Emitter &emit,
    u16 pc,
    const X86::Reg<u8> &r)
{
    u8 *zp = Memory::ram;
    u8 off = Memory::load(pc + 1);
    emit.MOV(X86::eax, (u32)(zp + off));
    emit.MOV(X86::eax(), r);
}

static void loadZeroPageIndexed(
    X86::Emitter &emit,
    u16 pc,
    Operation cont,
    bool wb,
    const X86::Reg<u8> &p,
    const X86::Reg<u8> &r = Jit::M)
{
    u8 *zp = Memory::ram;
    u8 off = Memory::load(pc + 1);
    emit.MOV(X86::eax, (u32)(zp + off));
    emit.ADD(X86::al, p);
    emit.MOV(r, X86::eax());
    cont(emit, r);
    if (wb)
        emit.MOV(X86::eax(), r);
}

static void storeZeroPageIndexed(
    X86::Emitter &emit,
    u16 pc,
    const X86::Reg<u8> &p,
    const X86::Reg<u8> &r)
{
    u8 *zp = Memory::ram;
    u8 off = Memory::load(pc + 1);
    emit.MOV(X86::eax, (u32)(zp + off));
    emit.ADD(X86::al, p);
    emit.MOV(X86::eax(), r);
}

static void loadAbsolute(
    X86::Emitter &emit,
    u16 pc,
    Operation cont,
    bool wb,
    const X86::Reg<u8> &r = Jit::M)
{
    u16 addr = Memory::loadw(pc + 1);
    emit.PUSH(X86::edx);
    emit.PUSH(X86::esi);
    emit.PUSH((u32)addr);
    emit.CALL((u8 *)Memory::load);
    emit.POP(X86::ecx);
    emit.POP(X86::edx);
    emit.POP(X86::edx);
    emit.MOV(Jit::M, X86::al);
    cont(emit, Jit::M);
    if (wb) {
        emit.PUSH(X86::esi);
        emit.PUSH(X86::edx);
        emit.PUSH((u32)addr);
        emit.CALL((u8 *)Memory::store);
        emit.POP(X86::ecx);
        emit.POP(X86::edx);
        emit.POP(X86::eax);
    }
}

static void storeAbsolute(
    X86::Emitter &emit,
    u16 pc,
    const X86::Reg<u8> &r)
{
    u16 addr = Memory::loadw(pc + 1);
    if (r != Jit::M)
        emit.MOV(Jit::M, r);
    emit.PUSH(X86::esi);
    emit.PUSH(X86::edx);
    emit.PUSH((u32)addr);
    emit.CALL((u8 *)Memory::store);
    emit.POP(X86::ecx);
    emit.POP(X86::edx);
    emit.POP(X86::eax);
}

/**
 * Implement the Oops cycle.
 * A first fecth is performed at the partially computed address addr + x
 * (without wrapping), and repeated if the addition causes the high u8 to
 * change.
 */
static void loadAbsoluteIndexed(
    X86::Emitter &emit,
    u16 pc,
    Operation cont,
    bool wb,
    const X86::Reg<u8> &p,
    const X86::Reg<u8> &r = Jit::M)
{
    u16 addr = Memory::loadw(pc + 1);
    emit.MOV(X86::eax, (u32)addr);
    emit.ADD(X86::al, p);
    u32 *jmp = emit.JNC();
    // Implement Oops cycle if the page changes. If the instruction is a
    // Read-Modify write, the dummy read is always compiled in.
    //
    // Note: the dummy read is important only if the address is linked
    // to the PPU registers.
    if (!wb) {
        emit.INC(X86::esi);
    }
    emit.PUSH(X86::edx);
    emit.PUSH(X86::eax);
    emit.CALL((u8 *)Memory::load0);
    emit.POP(X86::eax);
    emit.POP(X86::edx);
    emit.INC(X86::ah);
    // Back to regular load.
    emit.setJump(jmp);
    emit.PUSH(X86::edx);
    emit.PUSH(X86::eax);
    emit.CALL((u8 *)Memory::load0);
    emit.POP(X86::ecx);
    emit.POP(X86::edx);
    emit.MOV(Jit::M, X86::al);
    // Double write back, old value is written once to the new address.
    if (wb) {
        emit.PUSH(X86::edx);
        emit.PUSH(X86::ecx);
        emit.CALL((u8 *)Memory::store0);
        emit.POP(X86::ecx);
        emit.POP(X86::edx);
        emit.MOV(X86::eax, X86::ecx);
    }
    cont(emit, Jit::M);
    if (wb) {
        emit.PUSH(X86::edx);
        emit.PUSH(X86::eax);
        emit.CALL((u8 *)Memory::store0);
        emit.POP(X86::ecx);
        emit.POP(X86::edx);
    }
}

static void storeAbsoluteIndexed(
    X86::Emitter &emit,
    u16 pc,
    const X86::Reg<u8> &p,
    const X86::Reg<u8> &r)
{
    u16 addr = Memory::loadw(pc + 1);
    if (r != Jit::M)
        emit.MOV(Jit::M, r);
    emit.MOV(X86::eax, (u32)addr);
    emit.MOV(X86::ecx, 0);
    emit.MOV(X86::cl, p);
    emit.ADD(X86::eax, X86::ecx);
    emit.PUSH(X86::edx);
    emit.PUSH(X86::eax);
    emit.CALL((u8 *)Memory::store0);
    emit.POP(X86::eax);
    emit.POP(X86::edx);
}

static void loadIndexedIndirect(
    X86::Emitter &emit,
    u16 pc,
    Operation cont,
    bool wb,
    const X86::Reg<u8> &r = Jit::M)
{
    u8 *zp = Memory::ram;
    u8 off = Memory::load(pc + 1);
    emit.MOV(X86::eax, (u32)(zp + off));
    emit.MOV(X86::ecx, 0);
    emit.ADD(X86::al, Jit::X);
    emit.MOV(X86::cl, X86::eax());
    emit.INC(X86::al);
    emit.MOV(X86::ch, X86::eax());
    emit.PUSH(X86::edx);
    emit.PUSH(X86::ecx);
    emit.CALL((u8 *)Memory::load0);
    emit.POP(X86::ecx);
    emit.POP(X86::edx);
    emit.MOV(Jit::M, X86::al);
    // Double write back, old value is written once to the new address.
    if (wb) {
        emit.PUSH(X86::edx);
        emit.PUSH(X86::ecx);
        emit.CALL((u8 *)Memory::store0);
        emit.POP(X86::ecx);
        emit.POP(X86::edx);
        emit.MOV(X86::eax, X86::ecx);
    }
    cont(emit, Jit::M);
    if (wb) {
        emit.PUSH(X86::edx);
        emit.PUSH(X86::eax);
        emit.CALL((u8 *)Memory::store0);
        emit.POP(X86::ecx);
        emit.POP(X86::edx);
    }
}

static void storeIndexedIndirect(
    X86::Emitter &emit,
    u16 pc,
    const X86::Reg<u8> &r)
{
    u8 *zp = Memory::ram;
    u8 off = Memory::load(pc + 1);
    if (r != Jit::M)
        emit.MOV(Jit::M, r);
    emit.MOV(X86::eax, (u32)(zp + off));
    emit.MOV(X86::ecx, 0);
    emit.ADD(X86::al, Jit::X);
    emit.MOV(X86::cl, X86::eax());
    emit.INC(X86::al);
    emit.MOV(X86::ch, X86::eax());
    emit.PUSH(X86::edx);
    emit.PUSH(X86::ecx);
    emit.CALL((u8 *)Memory::store0);
    emit.POP(X86::ecx);
    emit.POP(X86::edx);
}

/**
 * Implement the Oops cycle.
 * A first fecth is performed at the partially computed address
 * (without wrapping), and repeated if the addition causes the high u8 to
 * change.
 */
static void loadIndirectIndexed(
    X86::Emitter &emit,
    u16 pc,
    Operation cont,
    bool wb,
    const X86::Reg<u8> &r = Jit::M)
{
    u8 *zp = Memory::ram;
    u8 off = Memory::load(pc + 1);
    emit.MOV(X86::eax, (u32)(zp + off));
    emit.MOV(X86::ecx, 0);
    emit.MOV(X86::cl, X86::eax());
    emit.INC(X86::al);
    emit.MOV(X86::ch, X86::eax());
    emit.ADD(X86::cl, Jit::Y);
    u32 *jmp = emit.JNC();
    // Implement Oops cycle if the page changes.
    // Note: the dummy read is important only if the address is linked
    // to the PPU registers.
    if (!wb) {
        emit.INC(X86::esi);
    }
    emit.PUSH(X86::edx);
    emit.PUSH(X86::ecx);
    emit.CALL((u8 *)Memory::load0);
    emit.POP(X86::ecx);
    emit.POP(X86::edx);
    emit.INC(X86::ch);
    // Back to regular load.
    emit.setJump(jmp);
    emit.PUSH(X86::edx);
    emit.PUSH(X86::ecx);
    emit.CALL((u8 *)Memory::load0);
    emit.POP(X86::ecx);
    emit.POP(X86::edx);
    emit.MOV(Jit::M, X86::al);
    // Double write back, old value is written once to the new address.
    if (wb) {
        emit.PUSH(X86::edx);
        emit.PUSH(X86::ecx);
        emit.CALL((u8 *)Memory::store0);
        emit.POP(X86::ecx);
        emit.POP(X86::edx);
        emit.MOV(X86::eax, X86::ecx); // Save write back address to eax
    }
    cont(emit, Jit::M);
    if (wb) {
        emit.PUSH(X86::edx);
        emit.PUSH(X86::eax);
        emit.CALL((u8 *)Memory::store0);
        emit.POP(X86::ecx);
        emit.POP(X86::edx);
    }
}

/**
 * Implement the Oops cycle.
 * A first fecth is performed at the partially computed address addr + y
 * (without wrapping).
 */
static void storeIndirectIndexed(
    X86::Emitter &emit,
    u16 pc,
    const X86::Reg<u8> &r)
{
    u8 *zp = Memory::ram;
    u8 off = Memory::load(pc + 1);
    emit.MOV(X86::eax, (u32)(zp + off));
    emit.MOV(X86::ecx, 0);
    emit.MOV(X86::cl, X86::eax());
    emit.INC(X86::al);
    emit.MOV(X86::ch, X86::eax());
    emit.ADD(X86::cl, Jit::Y);
    // TODO perform load at intermediate address (without wrapping)
    u32 *jmp = emit.JNC();
    emit.INC(X86::ch);
    emit.setJump(jmp);
    if (r != Jit::M)
        emit.MOV(Jit::M, r);
    emit.PUSH(X86::edx);
    emit.PUSH(X86::ecx);
    emit.CALL((u8 *)Memory::store0);
    emit.POP(X86::ecx);
    emit.POP(X86::edx);
}

/**
 * Generate the code for an instruction fetching a value from memory.
 */
#define CASE_LD_MEM(op, mode, fun, ...)                                        \
    case op: {                                                                 \
        load##mode(emit, address, fun, false, ##__VA_ARGS__);                  \
        break;                                                                 \
    }

/**
 * Generate the code for an instruction with Read-Modify-Write behaviour.
 */
#define CASE_UP_MEM(op, mode, fun, ...)                                        \
    case op: {                                                                 \
        load##mode(emit, address, fun, true, ##__VA_ARGS__);                   \
        break;                                                                 \
    }

/**
 * Generate the code for an instruction writing a value to memory.
 * The source register is always specified.
 */
#define CASE_ST_MEM(op, mode, r, ...)                                          \
    case op: {                                                                 \
        store##mode(emit, address, r, ##__VA_ARGS__);                          \
        break;                                                                 \
    }

/**
 * Generate the code for an instruction updating a value in memory.
 * The code implements the double write back behaviour, whereby
 * Read-Modify-Write instructions cause the original value to be written back
 * to memory one cycle before the modified value. This has a significant impact
 * on writes to state registers.
 */

/** Generate the code for an instruction updating the value of a register. */
#define CASE_UP_REG(op, fun, reg)                                              \
    case op: {                                                                 \
        fun(emit, reg);                                                        \
        break;                                                                 \
    }

/**
 * Generate the code for an instruction with no operand.
 * The CPU normally performs a dummy fetch of the next instruction byte, but
 * as the pc points to the ROM (the precompiler applies only to RO code),
 * this load provokes no side effect and can be safely ignored.
 */
#define CASE____EXP(op, e)                                                     \
    case op: {                                                                 \
        e;                                                                     \
        break;                                                                 \
    }

#define CASE____NOP()               CASE____EXP(NOP_IMP, )
#define CASE____NOP_UO(op)          CASE____EXP(op, )
#define CASE____IMP(op, fun)        CASE____EXP(op##_IMP, fun(emit))
#define CASE____IMP_UO(op, fun)     CASE____EXP(op, fun(emit))

#define CASE_LD_IMM(op, ...)        CASE_LD_MEM(op##_IMM, Immediate, __VA_ARGS__)
#define CASE_LD_ZPG(op, ...)        CASE_LD_MEM(op##_ZPG, ZeroPage, __VA_ARGS__)
#define CASE_LD_ZPX(op, fun, ...)   CASE_LD_MEM(op##_ZPX, ZeroPageIndexed, fun, Jit::X, ##__VA_ARGS__)
#define CASE_LD_ZPY(op, fun, ...)   CASE_LD_MEM(op##_ZPY, ZeroPageIndexed, fun, Jit::Y, ##__VA_ARGS__)
#define CASE_LD_ABS(op, ...)        CASE_LD_MEM(op##_ABS, Absolute, __VA_ARGS__)
#define CASE_LD_ABX(op, fun, ...)   CASE_LD_MEM(op##_ABX, AbsoluteIndexed, fun, Jit::X, ##__VA_ARGS__)
#define CASE_LD_ABY(op, fun, ...)   CASE_LD_MEM(op##_ABY, AbsoluteIndexed, fun, Jit::Y, ##__VA_ARGS__)
#define CASE_LD_INX(op, ...)        CASE_LD_MEM(op##_INX, IndexedIndirect, __VA_ARGS__)
#define CASE_LD_INY(op, ...)        CASE_LD_MEM(op##_INY, IndirectIndexed, __VA_ARGS__)

#define CASE_LD_IMM_UO(op, ...)     CASE_LD_MEM(op, Immediate, __VA_ARGS__)
#define CASE_LD_ZPG_UO(op, ...)     CASE_LD_MEM(op, ZeroPage, __VA_ARGS__)
#define CASE_LD_ZPX_UO(op, fun, ...) CASE_LD_MEM(op, ZeroPageIndexed, fun, Jit::X, ##__VA_ARGS__)
#define CASE_LD_ABS_UO(op, ...)     CASE_LD_MEM(op, Absolute, __VA_ARGS__)
#define CASE_LD_ABX_UO(op, fun, ...) CASE_LD_MEM(op, AbsoluteIndexed, fun, Jit::X, ##__VA_ARGS__)

#define CASE_UP_ACC(op, fun)        CASE_UP_REG(op##_ACC, fun, Jit::A)
#define CASE_UP_ZPG(op, fun)        CASE_UP_MEM(op##_ZPG, ZeroPage, fun)
#define CASE_UP_ZPX(op, fun)        CASE_UP_MEM(op##_ZPX, ZeroPageIndexed, fun, Jit::X)
#define CASE_UP_ABS(op, fun)        CASE_UP_MEM(op##_ABS, Absolute, fun)
#define CASE_UP_ABX(op, fun)        CASE_UP_MEM(op##_ABX, AbsoluteIndexed, fun, Jit::X)
#define CASE_UP_ABY(op, fun)        CASE_UP_MEM(op##_ABY, AbsoluteIndexed, fun, Jit::Y)
#define CASE_UP_INX(op, fun)        CASE_UP_MEM(op##_INX, IndexedIndirect, fun)
#define CASE_UP_INY(op, fun)        CASE_UP_MEM(op##_INY, IndirectIndexed, fun)

#define CASE_ST_ZPG(op, reg)        CASE_ST_MEM(op##_ZPG, ZeroPage, reg)
#define CASE_ST_ZPX(op, reg)        CASE_ST_MEM(op##_ZPX, ZeroPageIndexed, Jit::X, reg)
#define CASE_ST_ZPY(op, reg)        CASE_ST_MEM(op##_ZPY, ZeroPageIndexed, Jit::Y, reg)
#define CASE_ST_ABS(op, reg)        CASE_ST_MEM(op##_ABS, Absolute, reg)
#define CASE_ST_ABX(op, reg)        CASE_ST_MEM(op##_ABX, AbsoluteIndexed, Jit::X, reg)
#define CASE_ST_ABY(op, reg)        CASE_ST_MEM(op##_ABY, AbsoluteIndexed, Jit::Y, reg)
#define CASE_ST_INX(op, reg)        CASE_ST_MEM(op##_INX, IndexedIndirect, reg)
#define CASE_ST_INY(op, reg)        CASE_ST_MEM(op##_INY, IndirectIndexed, reg)

/** Create a banch instruction with the given condition. */
#define CASE_BR_REL(op, oppcond)                                               \
    case op##_REL: {                                                           \
        checkCycles(emit, address);                                            \
        emit.POPF();                                                           \
        u32 *__next = oppcond();                                               \
        emit.PUSHF();                                                          \
        emit.ADD(X86::esi, (u32)(3 + PAGE_DIFF(address + 2, branchAddress)));  \
        nativeBranchAddress = emit.JMP();                                      \
        emit.setJump(__next);                                                  \
        emit.PUSHF();                                                          \
        emit.ADD(X86::esi, (u32)2);                                            \
        break;                                                                 \
    }

void Instruction::compile(X86::Emitter &emit)
{
    nativeCode = emit.getPtr();
    Jit::requiredFlags = requiredFlags;

    /* Exit instruction. */
    if (exit || Asm::instructions[opcode].jam) {
        emit.MOV(X86::eax, address);
        emit.POPF();
        emit.RETN();
        return;
    }

    /* Interpret instruction. */
    switch (opcode)
    {
        CASE_BR_REL(BCC, emit.JC);
        CASE_BR_REL(BCS, emit.JNC);
        CASE_BR_REL(BEQ, emit.JNZ);
        CASE_BR_REL(BMI, emit.JNS);
        CASE_BR_REL(BNE, emit.JZ);
        CASE_BR_REL(BPL, emit.JS);
        CASE_BR_REL(BVC, emit.JO);
        CASE_BR_REL(BVS, emit.JNO);

        CASE_LD_IMM(ADC, ADC);
        CASE_LD_ZPG(ADC, ADC);
        CASE_LD_ZPX(ADC, ADC);
        CASE_LD_ABS(ADC, ADC);
        CASE_LD_ABX(ADC, ADC);
        CASE_LD_ABY(ADC, ADC);
        CASE_LD_INX(ADC, ADC);
        CASE_LD_INY(ADC, ADC);

        CASE_LD_IMM(AND, AND);
        CASE_LD_ZPG(AND, AND);
        CASE_LD_ZPX(AND, AND);
        CASE_LD_ABS(AND, AND);
        CASE_LD_ABX(AND, AND);
        CASE_LD_ABY(AND, AND);
        CASE_LD_INX(AND, AND);
        CASE_LD_INY(AND, AND);

        CASE_UP_ACC(ASL, ASL);
        CASE_UP_ZPG(ASL, ASL);
        CASE_UP_ZPX(ASL, ASL);
        CASE_UP_ABS(ASL, ASL);
        CASE_UP_ABX(ASL, ASL);

        CASE_LD_ZPG(BIT, BIT);
        CASE_LD_ABS(BIT, BIT);

        CASE____IMP(CLC, CLC);
        CASE____IMP(CLD, CLD);
        CASE____IMP(CLI, CLI);
        CASE____IMP(CLV, CLV);

        CASE_LD_IMM(CMP, CMP);
        CASE_LD_ZPG(CMP, CMP);
        CASE_LD_ZPX(CMP, CMP);
        CASE_LD_ABS(CMP, CMP);
        CASE_LD_ABX(CMP, CMP);
        CASE_LD_ABY(CMP, CMP);
        CASE_LD_INX(CMP, CMP);
        CASE_LD_INY(CMP, CMP);

        CASE_LD_IMM(CPX, CPX);
        CASE_LD_ZPG(CPX, CPX);
        CASE_LD_ABS(CPX, CPX);

        CASE_LD_IMM(CPY, CPY);
        CASE_LD_ZPG(CPY, CPY);
        CASE_LD_ABS(CPY, CPY);

        CASE_UP_ZPG(DEC, DEC);
        CASE_UP_ZPX(DEC, DEC);
        CASE_UP_ABS(DEC, DEC);
        CASE_UP_ABX(DEC, DEC);

        CASE____IMP(DEX, DEX);
        CASE____IMP(DEY, DEY);

        CASE_LD_IMM(EOR, EOR);
        CASE_LD_ZPG(EOR, EOR);
        CASE_LD_ZPX(EOR, EOR);
        CASE_LD_ABS(EOR, EOR);
        CASE_LD_ABX(EOR, EOR);
        CASE_LD_ABY(EOR, EOR);
        CASE_LD_INX(EOR, EOR);
        CASE_LD_INY(EOR, EOR);

        CASE_UP_ZPG(INC, INC);
        CASE_UP_ZPX(INC, INC);
        CASE_UP_ABS(INC, INC);
        CASE_UP_ABX(INC, INC);

        CASE____IMP(INX, INX);
        CASE____IMP(INY, INY);

        CASE_LD_IMM(LDA, LDA, Jit::A);
        CASE_LD_ZPG(LDA, LDA, Jit::A);
        CASE_LD_ZPX(LDA, LDA, Jit::A);
        CASE_LD_ABS(LDA, LDA, Jit::A);
        CASE_LD_ABX(LDA, LDA, Jit::A);
        CASE_LD_ABY(LDA, LDA, Jit::A);
        CASE_LD_INX(LDA, LDA, Jit::A);
        CASE_LD_INY(LDA, LDA, Jit::A);

        CASE_LD_IMM(LDX, LDX, Jit::X);
        CASE_LD_ZPG(LDX, LDX, Jit::X);
        CASE_LD_ZPY(LDX, LDX, Jit::X);
        CASE_LD_ABS(LDX, LDX, Jit::X);
        CASE_LD_ABY(LDX, LDX, Jit::X);

        CASE_LD_IMM(LDY, LDY, Jit::Y);
        CASE_LD_ZPG(LDY, LDY, Jit::Y);
        CASE_LD_ZPX(LDY, LDY, Jit::Y);
        CASE_LD_ABS(LDY, LDY, Jit::Y);
        CASE_LD_ABX(LDY, LDY, Jit::Y);

        CASE_UP_ACC(LSR, LSR);
        CASE_UP_ZPG(LSR, LSR);
        CASE_UP_ZPX(LSR, LSR);
        CASE_UP_ABS(LSR, LSR);
        CASE_UP_ABX(LSR, LSR);

        CASE____NOP();

        CASE_LD_IMM(ORA, ORA);
        CASE_LD_ZPG(ORA, ORA);
        CASE_LD_ZPX(ORA, ORA);
        CASE_LD_ABS(ORA, ORA);
        CASE_LD_ABX(ORA, ORA);
        CASE_LD_ABY(ORA, ORA);
        CASE_LD_INX(ORA, ORA);
        CASE_LD_INY(ORA, ORA);

        CASE____IMP(PHA, PHA);
        CASE____IMP(PHP, PHP);
        CASE____IMP(PLA, PLA);
        CASE____IMP(PLP, PLP);

        CASE_UP_ACC(ROL, ROL);
        CASE_UP_ZPG(ROL, ROL);
        CASE_UP_ZPX(ROL, ROL);
        CASE_UP_ABS(ROL, ROL);
        CASE_UP_ABX(ROL, ROL);

        CASE_UP_ACC(ROR, ROR);
        CASE_UP_ZPG(ROR, ROR);
        CASE_UP_ZPX(ROR, ROR);
        CASE_UP_ABS(ROR, ROR);
        CASE_UP_ABX(ROR, ROR);

        CASE_LD_IMM(SBC, SBC);
        CASE_LD_ZPG(SBC, SBC);
        CASE_LD_ZPX(SBC, SBC);
        CASE_LD_ABS(SBC, SBC);
        CASE_LD_ABX(SBC, SBC);
        CASE_LD_ABY(SBC, SBC);
        CASE_LD_INX(SBC, SBC);
        CASE_LD_INY(SBC, SBC);
        CASE_LD_IMM_UO(0xeb, SBC);

        CASE____IMP(SEC, SEC);
        CASE____IMP(SED, SED);
        CASE____IMP(SEI, SEI);

        CASE_ST_ZPG(STA, Jit::A);
        CASE_ST_ZPX(STA, Jit::A);
        CASE_ST_ABS(STA, Jit::A);
        CASE_ST_ABX(STA, Jit::A);
        CASE_ST_ABY(STA, Jit::A);
        CASE_ST_INX(STA, Jit::A);
        CASE_ST_INY(STA, Jit::A);

        CASE_ST_ZPG(STX, Jit::X);
        CASE_ST_ZPY(STX, Jit::X);
        CASE_ST_ABS(STX, Jit::X);

        CASE_ST_ZPG(STY, Jit::Y);
        CASE_ST_ZPX(STY, Jit::Y);
        CASE_ST_ABS(STY, Jit::Y);

        CASE____IMP(TAX, TAX);
        CASE____IMP(TAY, TAY);
        CASE____IMP(TSX, TSX);
        CASE____IMP(TXA, TXA);
        CASE____IMP(TXS, TXS);
        CASE____IMP(TYA, TYA);

        /** Unofficial NOP instructions with IMM addressing. */
        CASE_LD_IMM_UO(0x80, NOP);
        CASE_LD_IMM_UO(0x82, NOP);
        CASE_LD_IMM_UO(0x89, NOP);
        CASE_LD_IMM_UO(0xc2, NOP);
        CASE_LD_IMM_UO(0xe2, NOP);

        /** Unofficial NOP instructions with ZPG addressing. */
        CASE_LD_ZPG_UO(0x04, NOP);
        CASE_LD_ZPG_UO(0x44, NOP);
        CASE_LD_ZPG_UO(0x64, NOP);

        /** Unofficial NOP instructions with ZPX addressing. */
        CASE_LD_ZPX_UO(0x14, NOP);
        CASE_LD_ZPX_UO(0x34, NOP);
        CASE_LD_ZPX_UO(0x54, NOP);
        CASE_LD_ZPX_UO(0x74, NOP);
        CASE_LD_ZPX_UO(0xd4, NOP);
        CASE_LD_ZPX_UO(0xf4, NOP);

        /** Unofficial NOP instructions with IMP addressing. */
        CASE____NOP_UO(0x1a);
        CASE____NOP_UO(0x3a);
        CASE____NOP_UO(0x5a);
        CASE____NOP_UO(0x7a);
        CASE____NOP_UO(0xda);
        CASE____NOP_UO(0xfa);

        /** Unofficial NOP instructions with ABS addressing. */
        CASE_LD_ABS_UO(0x0c, NOP);

        /** Unofficial NOP instructions with ABX addressing. */
        CASE_LD_ABX_UO(0x1c, NOP);
        CASE_LD_ABX_UO(0x3c, NOP);
        CASE_LD_ABX_UO(0x5c, NOP);
        CASE_LD_ABX_UO(0x7c, NOP);
        CASE_LD_ABX_UO(0xdc, NOP);
        CASE_LD_ABX_UO(0xfc, NOP);

        /** Unofficial LAX instruction. */
        CASE_LD_ZPG(LAX, LAX);
        CASE_LD_ZPY(LAX, LAX);
        CASE_LD_ABS(LAX, LAX);
        CASE_LD_ABY(LAX, LAX);
        CASE_LD_INX(LAX, LAX);
        CASE_LD_INY(LAX, LAX);

        /** Unofficial SAX instruction. */
        CASE_ST_ZPG(SAX, ({
            emit.MOV(Jit::M, Jit::A);
            emit.AND(Jit::M, Jit::X);
            Jit::M;
        }));
        CASE_ST_ZPY(SAX, ({
            emit.MOV(Jit::M, Jit::A);
            emit.AND(Jit::M, Jit::X);
            Jit::M;
        }));
        CASE_ST_ABS(SAX, ({
            emit.MOV(Jit::M, Jit::A);
            emit.AND(Jit::M, Jit::X);
            Jit::M;
        }));
        CASE_ST_INX(SAX, ({
            emit.MOV(Jit::M, Jit::A);
            emit.AND(Jit::M, Jit::X);
            Jit::M;
        }));

        /** Unofficial DCP instruction. */
        CASE_UP_ZPG(DCP, DCP);
        CASE_UP_ZPX(DCP, DCP);
        CASE_UP_ABS(DCP, DCP);
        CASE_UP_ABX(DCP, DCP);
        CASE_UP_ABY(DCP, DCP);
        CASE_UP_INX(DCP, DCP);
        CASE_UP_INY(DCP, DCP);

        /** Unofficial ISB instruction. */
        CASE_UP_ZPG(ISB, ISB);
        CASE_UP_ZPX(ISB, ISB);
        CASE_UP_ABS(ISB, ISB);
        CASE_UP_ABX(ISB, ISB);
        CASE_UP_ABY(ISB, ISB);
        CASE_UP_INX(ISB, ISB);
        CASE_UP_INY(ISB, ISB);

        /** Unofficial SLO instruction. */
        CASE_UP_ZPG(SLO, SLO);
        CASE_UP_ZPX(SLO, SLO);
        CASE_UP_ABS(SLO, SLO);
        CASE_UP_ABX(SLO, SLO);
        CASE_UP_ABY(SLO, SLO);
        CASE_UP_INX(SLO, SLO);
        CASE_UP_INY(SLO, SLO);

        /** Unofficial RLA instruction. */
        CASE_UP_ZPG(RLA, RLA);
        CASE_UP_ZPX(RLA, RLA);
        CASE_UP_ABS(RLA, RLA);
        CASE_UP_ABX(RLA, RLA);
        CASE_UP_ABY(RLA, RLA);
        CASE_UP_INX(RLA, RLA);
        CASE_UP_INY(RLA, RLA);

        /** Unofficial SRE instruction. */
        CASE_UP_ZPG(SRE, SRE);
        CASE_UP_ZPX(SRE, SRE);
        CASE_UP_ABS(SRE, SRE);
        CASE_UP_ABX(SRE, SRE);
        CASE_UP_ABY(SRE, SRE);
        CASE_UP_INX(SRE, SRE);
        CASE_UP_INY(SRE, SRE);

        /** Unofficial RRA instruction. */
        CASE_UP_ZPG(RRA, RRA);
        CASE_UP_ZPX(RRA, RRA);
        CASE_UP_ABS(RRA, RRA);
        CASE_UP_ABX(RRA, RRA);
        CASE_UP_ABY(RRA, RRA);
        CASE_UP_INX(RRA, RRA);
        CASE_UP_INY(RRA, RRA);

        /* Unofficial instructions with immediate addressing mode. */
        CASE_LD_IMM(AAC0, AAC);
        CASE_LD_IMM(AAC1, AAC);
        CASE_LD_IMM(ASR, ASR);
        CASE_LD_IMM(ARR, ARR);
        CASE_LD_IMM(ATX, ATX);
        CASE_LD_IMM(AXS, AXS);

        default:
            /* Unsupported instruction, must be handled by the interpreter. */
            emit.MOV(X86::eax, address);
            emit.POPF();
            emit.RETN();
            break;
    }

    if (!exit && !branch)
        incrementCycles(emit, Asm::instructions[opcode].cycles);
}

Instruction::~Instruction()
{
}

extern "C" {
/**
 * Assembly entry point.
 *
 * The quantum is negative and incremented for each instruction. It is tested
 * for zero at each branch instructions, meaning the jit can emulate more
 * cpu cycles than indicated by the \p quantum.
 *
 * @param code      Pointer to recompiled native code
 * @param regs      Pointer to the structure containing the register values
 * @param stack     Pointer to the start of the 6502 stack buffer
 * @param quantum   Expected number of cycles to emulate
 * @return          Incremented value of the quantum
 */
extern long asmEntry(
    const void *nativeCode,
    Registers *regs,
    u8 *stack,
    long quantum);
};

/**
 * @brief Execute a dynamically recompiled code, starting from the current
 *      instruction.
 * @param quantum   Number of cpu cycles to emulate
 */
void Instruction::run(long quantum)
{
    if (exit || quantum <= 0)
        return;

    // trace(opcode);
    Registers *regs = &state->regs;
    u8 *stack = Memory::ram + 0x100;
    state->cycles += quantum;
    long r = asmEntry(nativeCode, regs, stack, -quantum);
    state->cycles += r;
}
