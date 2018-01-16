
#ifndef _M6502EVAL_H_INCLUDED_
#define _M6502EVAL_H_INCLUDED_

#include "type.h"

namespace M6502 {

void trace(u8 opcode);
void backtrace();

namespace Eval {

void triggerNMI();
void triggerIRQ();
void step();

};

};

#endif /* _M6502EVAL_H_INCLUDED_ */
