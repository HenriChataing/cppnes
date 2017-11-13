
#include <cstring>
#include <iostream>
#include <iomanip>
#include <sys/mman.h>

#include "X86Emitter.h"

using namespace X86;

namespace X86 {

Reg<u32> eax(0);
Reg<u32> ecx(1);
Reg<u32> edx(2);
Reg<u32> ebx(3);
Reg<u32> esp(4);
Reg<u32> ebp(5);
Reg<u32> esi(6);
Reg<u32> edi(7);

Reg<u16> ax(0);
Reg<u16> cx(1);
Reg<u16> dx(2);
Reg<u16> bx(3);
Reg<u16> sp(4);
Reg<u16> bp(5);
Reg<u16> si(6);
Reg<u16> di(7);

Reg<u8> al(0);
Reg<u8> cl(1);
Reg<u8> dl(2);
Reg<u8> bl(3);
Reg<u8> ah(4);
Reg<u8> ch(5);
Reg<u8> dh(6);
Reg<u8> bh(7);

};

Emitter::Emitter(CodeBuffer *buffer)
:
    _buffer(buffer)
{
}

Emitter::~Emitter()
{
}

/**
 * @brief Generate the bytes for a conditionnal jump instruction.
 * @param ops       opcode if the jump target is close (relative offset
 *                  can fit on a signed char)
 * @param opl       opcode if the jump target is far (relative offset
 *                  can fit on a signed integer). The conditional jump
 *                  always uses an extended opcode in such cases.
 * @param loc       jump target, or NULL if the target is unknown at
 *                  this time
 * @return          pointer to the relative offset if the jump target
 *                  is not known, which can be used to later specify
 *                  the offset, or NULL otherwise
 */
u32 *Emitter::jumpCond(u8 ops, u8 opl, const u8 *loc)
{
    if (loc == NULL) {
        put(0x0f); put(opl); put((u32)0);
        return (u32 *)(_buffer->getPtr() - 4);
    }
    ptrdiff_t rel = loc - (_buffer->getPtr() + 2);
    i8 rel8 = rel;
    if ((ptrdiff_t)rel8 == rel) {
        put(ops); put(rel8);
    } else {
        rel -= 4;
        put(0x0f); put(opl); put((u32)rel);
    }
    return NULL;
}

/**
 * @brief Generate the bytes for an inconditionnal jump instruction.
 * @param ops       opcode if the jump target is close (relative offset
 *                  can fit on a signed char)
 * @param opl       opcode if the jump target is far (relative offset
 *                  can fit on a signed integer)
 * @param loc       jump target, or NULL if the target is unknown at
 *                  this time
 * @return          pointer to the relative offset if the jump target
 *                  is not known, which can be used to later specify
 *                  the offset, or NULL otherwise
 */
u32 *Emitter::jumpAbs(u8 ops, u8 opl, const u8 *loc)
{
    if (loc == NULL) {
        put(opl); put((u32)0);
        return (u32 *)(_buffer->getPtr() - 4);
    }
    ptrdiff_t rel = loc - (_buffer->getPtr() + 2);
    i8 rel8 = rel;
    if ((ptrdiff_t)rel8 == rel) {
        put(ops); put(rel8);
    } else {
        rel -= 3;
        put(opl); put((u32)rel);
    }
    return NULL;
}

u32 *Emitter::jumpAbs(u8 opl, const u8 *loc)
{
    if (loc == NULL) {
        put(opl); put((u32)0);
        return (u32 *)(_buffer->getPtr() - 4);
    }
    ptrdiff_t rel = loc - (_buffer->getPtr() + 5);
    put(opl); put((u32)rel);
    return NULL;
}

void Emitter::dump(const u8 *start) const
{
    _buffer->dump(start);
}
