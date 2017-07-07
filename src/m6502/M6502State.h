#ifndef _CPU_H_INCLUDED_
#define _CPU_H_INCLUDED_

#include "type.h"

namespace M6502
{

struct Registers
{
    u8 a;
    u8 x;
    u8 y;
    u8 p;
    u16 pc;
    u8 sp;
};

class State
{
    public:
        State();
        ~State();

        void reset();

        /** CPU registers. */
        Registers regs;

        /** Cycle count. */
        ulong cycles;

        /** Set to true if an NMI is pending. */
        bool nmi;

        /** Set to true if an IRQ is pending. */
        bool irq;
};

extern class State *currentState;

};

#endif /* _CPU_H_INCLUDED_ */
