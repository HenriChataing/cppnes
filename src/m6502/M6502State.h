#ifndef _CPU_H_INCLUDED_
#define _CPU_H_INCLUDED_

#include <cstddef>

#include "type.h"

namespace M6502
{

struct Registers
{
    u8 a;
    u8 x;
    u8 y;
    u8 p;
    u8 sp;
    u16 pc;
};

static_assert(offsetof(Registers, a) == 0, "Unexpected Registers.a offset");
static_assert(offsetof(Registers, x) == 1, "Unexpected Registers.x offset");
static_assert(offsetof(Registers, y) == 2, "Unexpected Registers.y offset");
static_assert(offsetof(Registers, p) == 3, "Unexpected Registers.p offset");
static_assert(offsetof(Registers, sp) == 4, "Unexpected Registers.sp offset");
static_assert(offsetof(Registers, pc) == 6, "Unexpected Registers.pc offset");

class State
{
public:
    State();
    ~State();

    void clear();
    void reset();

    /** CPU registers. */
    Registers regs;

    /** Stack pointer (equal to Memory::ram + 0x100 + SP) */
    u8 *stack;

    /** Cycle count. */
    ulong cycles;

    /** Set to true if an NMI is pending. */
    bool nmi;

    /** Set to true if an IRQ is pending. */
    bool irq;
};

extern State *currentState;

};

#endif /* _CPU_H_INCLUDED_ */
