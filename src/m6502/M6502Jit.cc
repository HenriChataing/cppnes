
#include <iostream>
#include <iomanip>
#include <queue>

#include "M6502Jit.h"
#include "M6502State.h"
#include "M6502Eval.h"
#include "Memory.h"

#define PAGE_DIFF(addr0, addr1) ((((addr0) ^ (addr1)) & 0xff00) != 0)

using namespace M6502;

InstructionCache::InstructionCache()
    : _asmEmitter(0x100000)
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

Instruction *InstructionCache::cacheInstruction(u16 address, u8 opcode)
{
    uint offset = address - 0x8000;
    if (_cache[offset] == NULL)
        _cache[offset] = new Instruction(_asmEmitter, address, opcode);
    return _cache[offset];
}

Instruction *InstructionCache::cacheBlock(u16 address)
{
    if (address < 0x8000)
        return NULL;

    std::queue<Instruction *> queue;
    Instruction *prev = NULL, *instr;
    u16 pc = address;

    while (true)
    {
        instr = fetchInstruction(pc);
        if (instr != NULL) {
            /*
             * Using setNext because the compiled instruction is probably
             * not contiguous.
             */
            if (prev != NULL)
                prev->setNext(instr);
            break;
        }

        u8 opcode = Memory::load(pc);
        instr = cacheInstruction(pc, opcode);
        if (prev != NULL)
            prev->next = instr;
        if (instr->branch)
            queue.push(instr);
        if (instr->exit) {
            /* Get the next uncompiled branch. */
            while (!queue.empty()) {
                instr = queue.front();
                Instruction *branch = fetchInstruction(instr->branchAddress);
                if (branch != NULL) {
                    _asmEmitter.setJump(
                        instr->nativeBranchAddress, branch->nativeCode);
                } else
                    break;
                queue.pop();
            }
            if (queue.empty())
                break;
            queue.pop();
            _asmEmitter.setJump(
                instr->nativeBranchAddress, _asmEmitter.getPtr());
            pc = instr->branchAddress;
            prev = NULL;
            continue;
        }
        prev = instr;
        pc += Asm::instructions[opcode].bytes;
    }

    return fetchInstruction(address);
}


namespace Jit {

const X86::Reg<u8> M = X86::dl;
const X86::Reg<u8> A = X86::dh;
const X86::Reg<u8> X = X86::bl;
const X86::Reg<u8> Y = X86::bh;

};

/**
 * Increment the cycle count.
 */
static void incrementCycles(X86::Emitter &emit, u32 upd)
{
    emit.MOV(X86::eax, (u32)&currentState->cycles);
    emit.PUSHF();
    emit.ADD(X86::eax(), upd);
    emit.POPF();
}

/**
 * Generate bytecode to restore selected status flags from the a saved
 * state pushed onto the stack (i.e. PUSHF)
 */
static void restoreStatusFlags(X86::Emitter &emit, u32 mask)
{
    /// FIXME
    // Cannot use
    //   emit.OR(X86::esp(), X86::eax);
    // as this generates the restricted R/MOD byte 0x04, for SIB addressing,
    // which is not yet supported.
    emit.POP(X86::eax);
    emit.PUSHF();
    emit.POP(X86::ecx);
    emit.AND(X86::eax, mask);
    emit.AND(X86::ecx, ~mask);
    emit.OR(X86::ecx, X86::eax);
    emit.PUSH(X86::ecx);
    emit.POPF();
}

/**
 * Generate bytecode to set the Zero and Sign bits depending on the value
 * in the given register \p r.
 */
static void testZeroSign(X86::Emitter &emit, const X86::Reg<u8> &r)
{
    emit.PUSHF();
    emit.TEST(r, r);
    restoreStatusFlags(emit, 0x801);
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
    emit.PUSHF();
    emit.CMP(r0, r1);
    emit.POP(X86::eax);
    emit.PUSHF();
    emit.POP(X86::ecx);
    /* Discard the overflow flag. */
    emit.AND(X86::eax, 0x800);
    emit.AND(X86::ecx, 0xfffff7ff);
    emit.OR(X86::ecx, X86::eax);
    /*
     * Complement the carry flag (the signification is opposite for
     * 6502 (set if r0 greater than or equal to r1).
     */
    emit.PUSH(X86::ecx);
    emit.POPF();
    emit.CMC();
}

static inline bool ADC(X86::Emitter &emit, const X86::Reg<u8> &r) {
    emit.ADC(Jit::A, r);
    return false;
}

static inline bool AND(X86::Emitter &emit, const X86::Reg<u8> &r) {
    emit.PUSHF();
    emit.AND(Jit::A, r);
    restoreStatusFlags(emit, 0x801);
    return false;
}

static inline bool ASL(X86::Emitter &emit, const X86::Reg<u8> &r) {
    /* Modified by restoreStatusFlags, possibly needed for write-back. */
    emit.PUSH(X86::eax);
    emit.PUSHF();
    emit.SHL(r);
    restoreStatusFlags(emit, 0x800);
    emit.POP(X86::eax);
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
    throw "DCP unimplemented";
    return true;
}

static inline bool DEC(X86::Emitter &emit, const X86::Reg<u8> &r) {
    /* Modified by restoreStatusFlags, possibly needed for write-back. */
    emit.PUSH(X86::eax);
    emit.PUSHF();
    emit.DEC(r);
    restoreStatusFlags(emit, 0x800);
    emit.POP(X86::eax);
    return true;
}

static inline void DEX(X86::Emitter &emit) {
    emit.PUSHF();
    emit.DEC(Jit::X);
    restoreStatusFlags(emit, 0x800);
}

static inline void DEY(X86::Emitter &emit) {
    emit.PUSHF();
    emit.DEC(Jit::Y);
    restoreStatusFlags(emit, 0x800);
}

static inline bool EOR(X86::Emitter &emit, const X86::Reg<u8> &r) {
    emit.PUSHF();
    emit.XOR(Jit::A, r);
    restoreStatusFlags(emit, 0x801);
    return false;
}

static inline bool INC(X86::Emitter &emit, const X86::Reg<u8> &r) {
    /* Modified by restoreStatusFlags, possibly needed for write-back. */
    emit.PUSH(X86::eax);
    emit.PUSHF();
    emit.INC(r);
    restoreStatusFlags(emit, 0x800);
    emit.POP(X86::eax);
    return true;
}

static inline void INX(X86::Emitter &emit) {
    emit.PUSHF();
    emit.INC(Jit::X);
    restoreStatusFlags(emit, 0x800);
}

static inline void INY(X86::Emitter &emit) {
    emit.PUSHF();
    emit.INC(Jit::Y);
    restoreStatusFlags(emit, 0x800);
}

/** Unofficial opcode, composition of INC and SBC. */
static inline bool ISB(X86::Emitter &emit, const X86::Reg<u8> &r) {
    throw "ISB unimplemented";
    return true;
}

static inline bool LSR(X86::Emitter &emit, const X86::Reg<u8> &r) {
    /* Modified by restoreStatusFlags, possibly needed for write-back. */
    emit.PUSH(X86::eax);
    emit.PUSHF();
    emit.SHR(r);
    restoreStatusFlags(emit, 0x800);
    emit.POP(X86::eax);
    return true;
}

static inline bool ORA(X86::Emitter &emit, const X86::Reg<u8> &r) {
    emit.PUSHF();
    emit.OR(Jit::A, r);
    restoreStatusFlags(emit, 0x801);
    return false;
}

/** Unofficial opcode, composition of ROL and AND. */
static inline bool RLA(X86::Emitter &emit, const X86::Reg<u8> &r) {
    throw "RLA unimplemented";
    return true;
}

static inline bool ROL(X86::Emitter &emit, const X86::Reg<u8> &r) {
    /* Modified by restoreStatusFlags, possibly needed for write-back. */
    emit.PUSH(X86::eax);
    emit.PUSHF();
    emit.RCL(r);
    restoreStatusFlags(emit, 0x800);
    testZeroSign(emit, r);
    emit.POP(X86::eax);
    return true;
}

static inline bool ROR(X86::Emitter &emit, const X86::Reg<u8> &r) {
    /* Modified by restoreStatusFlags, possibly needed for write-back. */
    emit.PUSH(X86::eax);
    emit.PUSHF();
    emit.RCR(r);
    restoreStatusFlags(emit, 0x800);
    testZeroSign(emit, r);
    emit.POP(X86::eax);
    return true;
}

static inline bool RRA(X86::Emitter &emit, const X86::Reg<u8> &r) {
    throw "RRA unimplemented";
    return true;
}

static inline bool SBC(X86::Emitter &emit, const X86::Reg<u8> &r) {
    emit.CMC();
    emit.SBB(Jit::A, r);
    emit.CMC();
    return false;
}

static inline bool SLO(X86::Emitter &emit, const X86::Reg<u8> &r) {
    throw "SLO unimplemented";
    return true;
}

static inline bool SRE(X86::Emitter &emit, const X86::Reg<u8> &r) {
    throw "SRE unimplemented";
    return true;
}

static inline void PUSH(X86::Emitter &emit, const X86::Reg<u8> &r) {
    emit.MOV(X86::eax, (u32)&currentState->stack);
    emit.MOV(X86::ecx, X86::eax());
    emit.MOV(X86::ecx(), r);
    emit.PUSHF();
    emit.DEC(X86::cl); // Will wrap on stack overflow
    emit.POPF();
    emit.MOV(X86::eax(), X86::ecx);
}

static inline void PULL(X86::Emitter &emit, const X86::Reg<u8> &r) {
    emit.MOV(X86::eax, (u32)&currentState->stack);
    emit.MOV(X86::ecx, X86::eax());
    emit.PUSHF();
    emit.INC(X86::cl); // Will wrap on stack underflow
    emit.POPF();
    emit.MOV(r, X86::ecx());
    emit.MOV(X86::eax(), X86::ecx);
}

static inline bool NOP(X86::Emitter &emit, const X86::Reg<u8> &r) {
    (void)emit;
    (void)r;
    return false;
}

static inline bool BIT(X86::Emitter &emit, const X86::Reg<u8> &r) {
    emit.PUSHF();
    emit.POP(X86::ecx);
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
    emit.PUSH(X86::ecx);
    emit.POPF();
    return false;
}

static inline void CLC(X86::Emitter &emit) {
    emit.CLC();
}

static inline void CLD(X86::Emitter &emit) {
    u8 *p = &currentState->regs.p;
    emit.MOV(X86::eax, (u32)p);
    emit.PUSHF();
    emit.AND(X86::eax(), (u8)0xf7);
    emit.POPF();
}

static inline void CLI(X86::Emitter &emit) {
    u8 *p = &currentState->regs.p;
    emit.MOV(X86::eax, (u32)p);
    emit.PUSHF();
    emit.AND(X86::eax(), (u8)0xfb);
    emit.POPF();
}

static inline void CLV(X86::Emitter &emit) {
    emit.PUSHF();
    emit.POP(X86::eax);
    emit.AND(X86::eax, 0xfffff7ff);
    emit.PUSH(X86::eax);
    emit.POPF();
}

/** Unofficial instruction. */
static inline bool LAX(X86::Emitter &emit, const X86::Reg<u8> &r) {
    emit.MOV(Jit::A, r);
    emit.MOV(Jit::X, r);
    testZeroSign(emit, r);
    return false;
}

static inline bool LDA(X86::Emitter &emit, const X86::Reg<u8> &r) {
    if (r != Jit::A)
        emit.MOV(Jit::A, r);
    testZeroSign(emit, r);
    return false;
}

static inline bool LDX(X86::Emitter &emit, const X86::Reg<u8> &r) {
    if (r != Jit::X)
        emit.MOV(Jit::X, r);
    testZeroSign(emit, Jit::X);
    return false;
}

static inline bool LDY(X86::Emitter &emit, const X86::Reg<u8> &r) {
    if (r != Jit::Y)
        emit.MOV(Jit::Y, r);
    testZeroSign(emit, Jit::Y);
    return false;
}

static inline void PHA(X86::Emitter &emit) {
    PUSH(emit, Jit::A);
}

static inline void PHP(X86::Emitter &emit) {
    u8 *p = &currentState->regs.p;
    emit.PUSHF();
    emit.MOV(X86::eax, (u32)p);
    emit.MOV(Jit::M, X86::eax());
    emit.AND(Jit::M, 0x3c); // Clear Carry, Zero, Overflow, Sign flags
    emit.POP(X86::ecx);
    emit.PUSH(X86::ecx);
    emit.AND(X86::cl, 0x81);
    emit.OR(Jit::M, X86::cl);
    emit.POP(X86::ecx);
    emit.PUSH(X86::ecx);
    emit.SHR(X86::ecx, 5);
    emit.AND(X86::cl, 0x42);
    emit.OR(Jit::M, X86::cl);
    emit.MOV(X86::eax, (u32)&currentState->stack);
    emit.MOV(X86::ecx, X86::eax());
    emit.MOV(X86::ecx(), Jit::M);
    emit.DEC(X86::cl); // Will wrap on stack overflow
    emit.MOV(X86::eax(), X86::ecx);
    emit.POPF();
}

static inline void PLA(X86::Emitter &emit) {
    PULL(emit, Jit::A);
    testZeroSign(emit, Jit::A);
}

static inline void PLP(X86::Emitter &emit) {
    u8 *p = &currentState->regs.p;
    emit.PUSHF();
    /* Unstack new flag values */
    emit.MOV(X86::eax, (u32)&currentState->stack);
    emit.MOV(X86::ecx, X86::eax());
    emit.INC(X86::cl); // Will wrap on stack underflow
    emit.MOV(Jit::M, X86::ecx());
    emit.MOV(X86::eax(), X86::ecx);
    /* Update Interrupt, Decimal, etc. flags in currentState memory. */
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
    emit.POPF();
}

static inline void SEC(X86::Emitter &emit) {
    emit.STC();
}

static inline void SED(X86::Emitter &emit) {
    u8 *p = &currentState->regs.p;
    emit.MOV(X86::eax, (u32)p);
    emit.PUSHF();
    emit.OR(X86::eax(), (u8)0x8);
    emit.POPF();
}

static inline void SEI(X86::Emitter &emit) {
    u8 *p = &currentState->regs.p;
    emit.MOV(X86::eax, (u32)p);
    emit.PUSHF();
    emit.OR(X86::eax(), (u8)0x4);
    emit.POPF();
}

static inline void TAX(X86::Emitter &emit) {
    emit.MOV(Jit::X, Jit::A);
}

static inline void TAY(X86::Emitter &emit) {
    emit.MOV(Jit::Y, Jit::A);
}

static inline void TSX(X86::Emitter &emit) {
    emit.MOV(X86::eax, (u32)&currentState->stack);
    emit.MOV(X86::ecx, X86::eax());
    emit.MOV(Jit::X, X86::cl);
    testZeroSign(emit, Jit::X);
}

static inline void TXA(X86::Emitter &emit) {
    emit.MOV(Jit::A, Jit::X);
    testZeroSign(emit, Jit::A);
}

static inline void TXS(X86::Emitter &emit) {
    emit.MOV(X86::eax, (u32)&currentState->stack);
    emit.MOV(X86::ecx, X86::eax());
    emit.MOV(X86::cl, Jit::X);
    emit.MOV(X86::eax(), X86::ecx);
}

static inline void TYA(X86::Emitter &emit) {
    emit.MOV(Jit::A, Jit::Y);
    testZeroSign(emit, Jit::A);
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
    const X86::Reg<u8> &r)
{
    emit.MOV(Jit::M, Memory::load(pc + 1));
    cont(emit, Jit::M);
}

// static void getImmediate(X86::Emitter &emit, u16 pc) {
//     emit.MOV(Jit::M, Memory::load(pc + 1));
// }

static void loadZeroPage(
    X86::Emitter &emit,
    u16 pc,
    Operation cont,
    const X86::Reg<u8> &r)
{
    u8 *zp = Memory::ram;
    u8 off = Memory::load(pc + 1);
    emit.MOV(X86::eax, (u32)(zp + off));
    emit.MOV(Jit::M, X86::eax());
    bool wb = cont(emit, Jit::M);
    if (wb)
        emit.MOV(X86::eax(), Jit::M);
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

static void loadZeroPageX(
    X86::Emitter &emit,
    u16 pc,
    Operation cont,
    const X86::Reg<u8> &r)
{
    u8 *zp = Memory::ram;
    u8 off = Memory::load(pc + 1);
    emit.MOV(X86::eax, (u32)(zp + off));
    emit.PUSHF();
    emit.ADD(X86::al, Jit::X);
    emit.POPF();
    emit.MOV(Jit::M, X86::eax());
    bool wb = cont(emit, Jit::M);
    if (wb)
        emit.MOV(X86::eax(), Jit::M);
}

static void storeZeroPageX(
    X86::Emitter &emit,
    u16 pc,
    const X86::Reg<u8> &r)
{
    u8 *zp = Memory::ram;
    u8 off = Memory::load(pc + 1);
    emit.MOV(X86::eax, (u32)(zp + off));
    emit.PUSHF();
    emit.ADD(X86::al, Jit::X);
    emit.POPF();
    emit.MOV(X86::eax(), r);
}

static void loadZeroPageY(
    X86::Emitter &emit,
    u16 pc,
    Operation cont,
    const X86::Reg<u8> &r)
{
    u8 *zp = Memory::ram;
    u8 off = Memory::load(pc + 1);
    emit.MOV(X86::eax, (u32)(zp + off));
    emit.PUSHF();
    emit.ADD(X86::al, Jit::Y);
    emit.POPF();
    emit.MOV(Jit::M, X86::eax());
    bool wb = cont(emit, Jit::M);
    if (wb)
        emit.MOV(X86::eax(), Jit::M);
}

static void storeZeroPageY(
    X86::Emitter &emit,
    u16 pc,
    const X86::Reg<u8> &r)
{
    u8 *zp = Memory::ram;
    u8 off = Memory::load(pc + 1);
    emit.MOV(X86::eax, (u32)(zp + off));
    emit.PUSHF();
    emit.ADD(X86::al, Jit::Y);
    emit.POPF();
    emit.MOV(X86::eax(), r);
}

static void loadAbsolute(
    X86::Emitter &emit,
    u16 pc,
    Operation cont,
    const X86::Reg<u8> &r)
{
    u16 addr = Memory::loadw(pc + 1);
    emit.PUSHF();
    emit.PUSH(X86::edx);
    emit.PUSH((u32)addr);
    emit.CALL((u8 *)Memory::load);
    emit.POP(X86::ecx);
    emit.POP(X86::edx);
    emit.POPF();
    emit.MOV(Jit::M, X86::al);
    bool wb = cont(emit, Jit::M);
    if (wb) {
        emit.PUSHF();
        emit.PUSH(X86::edx);
        emit.PUSH((u32)addr);
        emit.CALL((u8 *)Memory::store);
        emit.POP(X86::ecx);
        emit.POP(X86::edx);
        emit.POPF();
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
    emit.PUSHF();
    emit.PUSH(X86::edx);
    emit.PUSH((u32)addr);
    emit.CALL((u8 *)Memory::store);
    emit.POP(X86::ecx);
    emit.POP(X86::edx);
    emit.POPF();
}

/**
 * Implement the Oops cycle.
 * A first fecth is performed at the partially computed address addr + x
 * (without wrapping), and repeated if the addition causes the high u8 to
 * change.
 */
static void loadAbsoluteX(
    X86::Emitter &emit,
    u16 pc,
    Operation cont,
    const X86::Reg<u8> &r)
{
    u16 addr = Memory::loadw(pc + 1);
    emit.MOV(X86::eax, (u32)addr);
    emit.PUSHF();
    emit.ADD(X86::al, Jit::X);
    emit.PUSH(X86::edx);
    emit.PUSH(X86::eax);
    emit.CALL((u8 *)Memory::load);
    emit.POP(X86::ecx);
    emit.POP(X86::edx);
    emit.POPF();
    emit.MOV(Jit::M, X86::al);
    /// TODO check carry bit, if 1 add cycle and repeat load
    bool wb = cont(emit, Jit::M);
    if (wb) {
        emit.PUSHF();
        emit.PUSH(X86::edx);
        emit.PUSH(X86::ecx);
        emit.CALL((u8 *)Memory::store);
        emit.POP(X86::ecx);
        emit.POP(X86::edx);
        emit.POPF();
    }

    // u16 addrX = addr + X;
    // if ((addr & 0xff00) != (addrX & 0xff00)) {
    //     (void)Memory::load((addr & 0xff00) | (addrX & 0x00ff));
    //     currentState->cycles++;
    // }
}

static void storeAbsoluteX(
    X86::Emitter &emit,
    u16 pc,
    const X86::Reg<u8> &r)
{
    u16 addr = Memory::loadw(pc + 1);
    if (r != Jit::M)
        emit.MOV(Jit::M, r);
    emit.MOV(X86::eax, (u32)addr);
    emit.MOV(X86::ecx, 0);
    emit.MOV(X86::cl, Jit::X);
    emit.PUSHF();
    emit.ADD(X86::eax, X86::ecx);
    emit.PUSH(X86::edx);
    emit.PUSH(X86::eax);
    emit.CALL((u8 *)Memory::store);
    emit.POP(X86::eax);
    emit.POP(X86::edx);
    emit.POPF();
}

/**
 * Implement the Oops cycle.
 * A first fecth is performed at the partially computed address addr + x
 * (without wrapping), and repeated if the addition causes the high u8 to
 * change.
 */
static void loadAbsoluteY(
    X86::Emitter &emit,
    u16 pc,
    Operation cont,
    const X86::Reg<u8> &r)
{
    u16 addr = Memory::loadw(pc + 1);
    emit.MOV(X86::eax, (u32)addr);
    emit.PUSHF();
    emit.ADD(X86::al, Jit::Y);
    emit.PUSH(X86::edx);
    emit.PUSH(X86::eax);
    emit.CALL((u8 *)Memory::load);
    emit.POP(X86::ecx);
    emit.POP(X86::edx);
    emit.POPF();
    emit.MOV(Jit::M, X86::al);
    /// TODO check carry bit, if 1 add cycle and repeat load
    bool wb = cont(emit, Jit::M);
    if (wb) {
        emit.PUSHF();
        emit.PUSH(X86::edx);
        emit.PUSH(X86::ecx);
        emit.CALL((u8 *)Memory::store);
        emit.POP(X86::ecx);
        emit.POP(X86::edx);
        emit.POPF();
    }

    // u16 addrY = addr + Y;
    // if ((addr & 0xff00) != (addrY & 0xff00)) {
    //     (void)Memory::load((addr & 0xff00) | (addrY & 0x00ff));
    //     currentState->cycles++;
    // }
}

static void storeAbsoluteY(
    X86::Emitter &emit,
    u16 pc,
    const X86::Reg<u8> &r)
{
    u16 addr = Memory::loadw(pc + 1);
    if (r != Jit::M)
        emit.MOV(Jit::M, r);
    emit.MOV(X86::eax, (u32)addr);
    emit.MOV(X86::ecx, 0);
    emit.MOV(X86::cl, Jit::Y);
    emit.PUSHF();
    emit.ADD(X86::eax, X86::ecx);
    emit.PUSH(X86::edx);
    emit.PUSH(X86::eax);
    emit.CALL((u8 *)Memory::store);
    emit.POP(X86::eax);
    emit.POP(X86::edx);
    emit.POPF();
}

static void loadIndexedIndirect(
    X86::Emitter &emit,
    u16 pc,
    Operation cont,
    const X86::Reg<u8> &r)
{
    u8 *zp = Memory::ram;
    u8 off = Memory::load(pc + 1);
    emit.MOV(X86::eax, (u32)(zp + off));
    emit.MOV(X86::ecx, 0);
    emit.PUSHF();
    emit.ADD(X86::al, Jit::X);
    emit.MOV(X86::cl, X86::eax());
    emit.INC(X86::al);
    emit.MOV(X86::ch, X86::eax());
    emit.PUSH(X86::edx);
    emit.PUSH(X86::ecx);
    emit.CALL((u8 *)Memory::load);
    emit.POP(X86::ecx);
    emit.POP(X86::edx);
    emit.POPF();
    emit.MOV(Jit::M, X86::al);
    /// TODO popf
    bool wb = cont(emit, Jit::M);
    if (wb) {
        emit.PUSHF();
        emit.PUSH(X86::edx);
        emit.PUSH(X86::ecx);
        emit.CALL((u8 *)Memory::store);
        emit.POP(X86::ecx);
        emit.POP(X86::edx);
        emit.POPF();
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
    emit.PUSHF();
    emit.ADD(X86::al, Jit::X);
    emit.MOV(X86::cl, X86::eax());
    emit.INC(X86::al);
    emit.MOV(X86::ch, X86::eax());
    emit.PUSH(X86::edx);
    emit.PUSH(X86::ecx);
    emit.CALL((u8 *)Memory::store);
    emit.POP(X86::ecx);
    emit.POP(X86::edx);
    emit.POPF();
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
    const X86::Reg<u8> &r)
{
    u8 *zp = Memory::ram;
    u8 off = Memory::load(pc + 1);
    emit.MOV(X86::eax, (u32)(zp + off));
    emit.MOV(X86::ecx, 0);
    emit.PUSHF();
    emit.MOV(X86::cl, X86::eax());
    emit.INC(X86::al);
    emit.MOV(X86::ch, X86::eax());
    emit.ADD(X86::cl, Jit::Y);
    /// TODO check carry bit, if 1 add cycle and repeat load
    emit.PUSH(X86::edx);
    emit.PUSH(X86::ecx);
    emit.CALL((u8 *)Memory::load);
    emit.POP(X86::ecx);
    emit.POP(X86::edx);
    emit.POPF();
    emit.MOV(Jit::M, X86::al);
    bool wb = cont(emit, Jit::M);
    if (wb) {
        emit.PUSHF();
        emit.PUSH(X86::edx);
        emit.PUSH(X86::ecx);
        emit.CALL((u8 *)Memory::store);
        emit.POP(X86::ecx);
        emit.POP(X86::edx);
        emit.POPF();
    }

    // u16 addr = Memory::load(PC + 1), addrY;
    // addr = Memory::loadzw(addr);
    // addrY = addr + Y;
    // if ((addr & 0xff00) != (addrY & 0xff00)) {
    //     (void)Memory::load((addr & 0xff00) | (addrY & 0x00ff));
    //     currentState->cycles++;
    // }
    // return Memory::load(addrY);
}

/**
 * Implement the Oops cycle.
 * A first fecth is performed at the partially computed address addr + x
 * (without wrapping).
 */
static void storeIndirectIndexed(
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
    emit.PUSHF();
    emit.MOV(X86::cl, X86::eax());
    emit.INC(X86::al);
    emit.MOV(X86::ch, X86::eax());
    emit.ADD(X86::cl, Jit::Y);
    /// TODO check carry bit, if 1 add cycle and repeat load
    emit.PUSH(X86::edx);
    emit.PUSH(X86::ecx);
    emit.CALL((u8 *)Memory::store);
    emit.POP(X86::ecx);
    emit.POP(X86::edx);
    emit.POPF();

    // u16 addr = Memory::load(PC + 1), addrY;
    // addr = Memory::loadzw(addr);
    // addrY = addr + Y;
    // (void)Memory::load((addr & 0xff00) | (addrY & 0x00ff));
    // return addrY;
}

#if 0
static u16 getIndirect(void) {
    u16 lo = Memory::load(PC + 1);
    u16 hi = Memory::load(PC + 2);
    /*
     * Incorrect fetch if the address falls on a page boundary : JMP ($xxff).
     * In this case, the LSB is fetched from $xxff and the MSB from $xx00.
     * http://obelisk.me.uk/6502/reference.html#JMP
     */
#ifdef CPU_6502
    if (lo == 0xff) {
        hi = (hi << 8) & 0xff00;
        lo = Memory::load(hi | lo);
        hi = Memory::load(hi);
        return WORD(hi, lo);
    } else
#endif
        return Memory::loadw(WORD(hi, lo));
}
#endif

#define CASE_LD_MEM(op, load, ...)                                             \
    CASE_LD_MEM_GEN(op, load, __VA_ARGS__, Jit::M)

/**
 * Generate the code for an instruction fetching a value from memory.
 */
#define CASE_LD_MEM_GEN(op, load, fun, r, ...)                                 \
    case op: {                                                                 \
        load(emit, pc, fun, r);                                                \
        break;                                                                 \
    }

/**
 * Generate the code for an instruction writing a value to memory.
 * The source register is always specified.
 */
#define CASE_ST_MEM(op, r, store)                                              \
    case op: {                                                                 \
        store(emit, pc, r);                                                    \
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
#define CASE_NN_EXP(op, e)                                                     \
    case op: {                                                                 \
        e;                                                                     \
        break;                                                                 \
    }

#define CASE_NN_NOP() CASE_NN_EXP(NOP_IMP, )
#define CASE_NN_NOP_UO(op) CASE_NN_EXP(op, )
#define CASE_NN(op, fun) CASE_NN_EXP(op##_IMP, fun(emit))
#define CASE_NN_UO(op, fun) CASE_NN_EXP(op, fun(emit))

#define CASE_LD_IMM(op, ...)  CASE_LD_MEM(op##_IMM, loadImmediate, __VA_ARGS__)
#define CASE_LD_ZPG(op, ...)  CASE_LD_MEM(op##_ZPG, loadZeroPage, __VA_ARGS__)
#define CASE_LD_ZPX(op, ...)  CASE_LD_MEM(op##_ZPX, loadZeroPageX, __VA_ARGS__)
#define CASE_LD_ZPY(op, ...)  CASE_LD_MEM(op##_ZPY, loadZeroPageY, __VA_ARGS__)
#define CASE_LD_ABS(op, ...)  CASE_LD_MEM(op##_ABS, loadAbsolute, __VA_ARGS__)
#define CASE_LD_ABX(op, ...)  CASE_LD_MEM(op##_ABX, loadAbsoluteX, __VA_ARGS__)
#define CASE_LD_ABY(op, ...)  CASE_LD_MEM(op##_ABY, loadAbsoluteY, __VA_ARGS__)
#define CASE_LD_IND(op, ...)  CASE_LD_MEM(op##_IND, loadIndirect, __VA_ARGS__)
#define CASE_LD_INX(op, ...)  CASE_LD_MEM(op##_INX, loadIndexedIndirect, __VA_ARGS__)
#define CASE_LD_INY(op, ...)  CASE_LD_MEM(op##_INY, loadIndirectIndexed, __VA_ARGS__)

#define CASE_LD_IMM_UO(op, ...)  CASE_LD_MEM(op, loadImmediate, __VA_ARGS__)
#define CASE_LD_ZPG_UO(op, ...)  CASE_LD_MEM(op, loadZeroPage, __VA_ARGS__)
#define CASE_LD_ZPX_UO(op, ...)  CASE_LD_MEM(op, loadZeroPageX, __VA_ARGS__)
#define CASE_LD_ABS_UO(op, ...)  CASE_LD_MEM(op, loadAbsolute, __VA_ARGS__)
#define CASE_LD_ABX_UO(op, ...)  CASE_LD_MEM(op, loadAbsoluteX, __VA_ARGS__)

#define CASE_ST_ZPG(op, reg)  CASE_ST_MEM(op##_ZPG, reg, storeZeroPage)
#define CASE_ST_ZPX(op, reg)  CASE_ST_MEM(op##_ZPX, reg, storeZeroPageX)
#define CASE_ST_ZPY(op, reg)  CASE_ST_MEM(op##_ZPY, reg, storeZeroPageY)
#define CASE_ST_ABS(op, reg)  CASE_ST_MEM(op##_ABS, reg, storeAbsolute)
#define CASE_ST_ABX(op, reg)  CASE_ST_MEM(op##_ABX, reg, storeAbsoluteX)
#define CASE_ST_ABY(op, reg)  CASE_ST_MEM(op##_ABY, reg, storeAbsoluteY)
#define CASE_ST_INX(op, reg)  CASE_ST_MEM(op##_INX, reg, storeIndexedIndirect)
#define CASE_ST_INY(op, reg)  CASE_ST_MEM(op##_INY, reg, storeIndirectIndexed)

#define CASE_UP_ACC(op, fun)  CASE_UP_REG(op##_ACC, fun, Jit::A)
#define CASE_UP_ZPG(op, fun)  CASE_LD_MEM(op##_ZPG, loadZeroPage, fun)
#define CASE_UP_ZPX(op, fun)  CASE_LD_MEM(op##_ZPX, loadZeroPageX, fun)
#define CASE_UP_ABS(op, fun)  CASE_LD_MEM(op##_ABS, loadAbsolute, fun)
#define CASE_UP_ABX(op, fun)  CASE_LD_MEM(op##_ABX, loadAbsoluteX, fun)
#define CASE_UP_ABY(op, fun)  CASE_LD_MEM(op##_ABY, loadAbsoluteY, fun)
#define CASE_UP_INX(op, fun)  CASE_LD_MEM(op##_INX, loadIndexedIndirect, fun)
#define CASE_UP_INY(op, fun)  CASE_LD_MEM(op##_INY, loadIndirectIndexed, fun)

/** Create a banch instruction with the given condition. */
#define CASE_BR(op, oppcond)                                                   \
    case op##_REL: {                                                           \
        u8 __off = Memory::load(pc + 1);                                       \
        pc += Asm::instructions[op##_REL].bytes;                               \
        if (__off & 0x80) {                                                    \
            __off = ~__off + 1;                                                \
            branchAddress = pc - __off;                                        \
        } else                                                                 \
            branchAddress = pc + __off;                                        \
        u32 *__next = oppcond();                                               \
        emit.MOV(X86::eax, (u32)&currentState->cycles);                        \
        emit.PUSHF();                                                          \
        emit.ADD(X86::eax(), (u32)(3 + PAGE_DIFF(pc, branchAddress)));         \
        emit.POPF();                                                           \
        nativeBranchAddress = emit.JMP();                                      \
        emit.setJump(__next, emit.getPtr());                                   \
        emit.MOV(X86::eax, (u32)&currentState->cycles);                        \
        emit.PUSHF();                                                          \
        emit.ADD(X86::eax(), (u32)2);                                          \
        emit.POPF();                                                           \
        branch = true;                                                         \
        break;                                                                 \
    }

Instruction::Instruction(X86::Emitter &emit, u16 pc, u8 opcode)
    : address(pc), opcode(opcode), next(NULL), jump(NULL)
{
    nativeCode = emit.getPtr();
    branch = false;
    exit = false;

    // if (pc == 0xc731 || pc == 0xc72e) {
    //     exit = true;
    //     emit.MOV(X86::eax, pc);
    //     emit.PUSHF();
    //     emit.POP(X86::ecx);
    //     emit.RETN();
    //     return;
    // }

    /* Check jamming instructions. */
    if (Asm::instructions[opcode].jam) {
        // error("jamming instruction, stopping execution\n");
        // error("  PC $%04x\n", cpu.regs.pc);
        // error("  opcode %02x\n", opcode);
        throw "Jamming instruction";
    }

    /* Interpret instruction. */
    switch (opcode)
    {
        case BRK_IMP:
        case JMP_ABS:
        case JMP_IND:
        case JSR_ABS:
        case RTI_IMP:
        case RTS_IMP:
            exit = true;
            emit.MOV(X86::eax, pc);
            emit.PUSHF();
            emit.POP(X86::ecx);
            emit.RETN();
            break;

        CASE_BR(BCC, emit.JC);
        CASE_BR(BCS, emit.JNC);
        CASE_BR(BEQ, emit.JNZ);
        CASE_BR(BMI, emit.JNS);
        CASE_BR(BNE, emit.JZ);
        CASE_BR(BPL, emit.JS);
        CASE_BR(BVC, emit.JO);
        CASE_BR(BVS, emit.JNO);

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

        CASE_NN(CLC, CLC);
        CASE_NN(CLD, CLD);
        CASE_NN(CLI, CLI);
        CASE_NN(CLV, CLV);

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

        CASE_NN(DEX, DEX);
        CASE_NN(DEY, DEY);

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

        CASE_NN(INX, INX);
        CASE_NN(INY, INY);

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

        CASE_NN_NOP();

        CASE_LD_IMM(ORA, ORA);
        CASE_LD_ZPG(ORA, ORA);
        CASE_LD_ZPX(ORA, ORA);
        CASE_LD_ABS(ORA, ORA);
        CASE_LD_ABX(ORA, ORA);
        CASE_LD_ABY(ORA, ORA);
        CASE_LD_INX(ORA, ORA);
        CASE_LD_INY(ORA, ORA);

        CASE_NN(PHA, PHA);
        CASE_NN(PHP, PHP);
        CASE_NN(PLA, PLA);
        CASE_NN(PLP, PLP);

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

        CASE_NN(SEC, SEC);
        CASE_NN(SED, SED);
        CASE_NN(SEI, SEI);

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

        CASE_NN(TAX, TAX);
        CASE_NN(TAY, TAY);
        CASE_NN(TSX, TSX);
        CASE_NN(TXA, TXA);
        CASE_NN(TXS, TXS);
        CASE_NN(TYA, TYA);

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
        CASE_NN_NOP_UO(0x1a);
        CASE_NN_NOP_UO(0x3a);
        CASE_NN_NOP_UO(0x5a);
        CASE_NN_NOP_UO(0x7a);
        CASE_NN_NOP_UO(0xda);
        CASE_NN_NOP_UO(0xfa);

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
            // error("unsupported instruction, stopping execution\n");
            // error("  PC $%04x\n", cpu.regs.pc);
            // error("  opcode %02x\n", opcode);
            throw "Unsupported instruction";
    }

    if (!exit && !branch)
        incrementCycles(emit, Asm::instructions[opcode].cycles);
}

Instruction::~Instruction()
{
}

void Instruction::setNext(Instruction *instr)
{
    if (instr == NULL)
        return;
    /// TODO add x86 instruction to jump to selected instruction
    next = instr;
}

/**
 * Jump to the compiled instruction and continue the emulation in native code.
 * The registers are loaded into matching native registers, and the code jumps
 * to the compiled instruction.
 * The last instruction is supposed to place the value of PC onto the stack to
 * inform the caller of the state reached.
 */
void Instruction::run()
{
    Registers *regs = &currentState->regs;
    trace(opcode);

    currentState->stack = Memory::ram + 0x100 + regs->sp;
    asm (
        /* Save pointer to regs (scratched by compiled code) */
        "push %%ecx\n"
        /* Load status flags */
        "pushf\n"               // Load x86 status flags into %edx
        "pop %%edx\n"
        "and $0xf73e, %%dx\n"   // Clear Carry, Zero, Sign, Overflow bits
        "mov 3(%%ecx), %%bl\n"  // Load 6502 status flags into %bl
        "and $0x81, %%bl\n"     // Keep bits Carry and Sign
        "or %%bl, %%dl\n"       // Write them to %edx
        "mov $0, %%bx\n"
        "mov 3(%%ecx), %%bl\n"  // Get 6502 status flags
        "and $0x42, %%bx\n"     // Keep bits Zero and Overflow
        "shl $5, %%bx\n"        // Left shift by 5 to place them correctly
        "or %%bx, %%dx\n"       // Write them to %edx
        "push %%edx\n"          // Override the original flags with
        "popf\n"                // the constructed value
        /* Load A,X,Y registers */
        "mov (%%ecx), %%dh\n"
        "mov 1(%%ecx), %%bl\n"
        "mov 2(%%ecx), %%bh\n"
        /* Jump to native code */
        "call *%1\n"
        /* Store A,X,Y registers */
        "pop %%ecx\n"
        "mov %%dh, (%%ecx)\n"
        "mov %%bl, 1(%%ecx)\n"
        "mov %%bh, 2(%%ecx)\n"
        "mov %%ax, 6(%%ecx)\n"
        /* Store status flags */
        "pushf\n"               // Load x86 status flags into %edx
        "pop %%edx\n"
        "push %%edx\n"          // Keep a copy of the original x86 flags
        "mov 3(%%ecx), %%bl\n"  // Get 6502 status flags
        "and $0x3c, %%bl\n"     // Clear Carry, Zero, Sign, Overflow bits
        "and $0x81, %%dl\n"
        "or %%dl, %%bl\n"
        "pop %%edx\n"
        "shr $5, %%dx\n"
        "and $0x42, %%dl\n"
        "or %%dl, %%bl\n"
        "mov %%bl, 3(%%ecx)\n"
        :
        : "c" (regs), "r" (nativeCode)
        : "%edx", "%ebx");
    regs->sp = currentState->stack - Memory::ram - 0x100;
}
