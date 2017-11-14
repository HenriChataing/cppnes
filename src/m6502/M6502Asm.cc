
#include "M6502Asm.h"

namespace M6502 {
namespace Asm {

using namespace M6502::Asm;

static const u8 v           = overflow;
static const u8 c           = carry;
static const u8 z           = zero;
static const u8 n           = negative;
static const u8 cnz         = carry | negative | zero;
static const u8 vnz         = overflow | negative | zero;
static const u8 cvnz        = carry | overflow | negative | zero;
static const u8 nz          = negative | zero;

const struct metadata instructions[256] =
{
    { 2, 7, IMP, "BRK", rflags: all, wflags: 0 },
    { 2, 6, INX, "ORA", rflags: 0, wflags: nz },
    { 1, 0, IMP, "???", rflags: 0, wflags: 0,      unofficial: 1, jam: 1 },
    { 2, 8, INX, "SLO", rflags: 0, wflags: cnz,    unofficial : 1 },
    { 2, 3, ZPG, "NOP", rflags: 0, wflags: 0,      unofficial : 1 },
    { 2, 3, ZPG, "ORA", rflags: 0, wflags: nz },
    { 2, 5, ZPG, "ASL", rflags: 0, wflags: cnz },
    { 2, 5, ZPG, "SLO", rflags: 0, wflags: cnz, unofficial : 1 },
    { 1, 3, IMP, "PHP", rflags: all, wflags: 0 },
    { 2, 2, IMM, "ORA", rflags: 0, wflags: nz },
    { 1, 2, ACC, "ASL", rflags: 0, wflags: cnz },
    { 2, 2, IMM, "AAC", rflags: 0, wflags: cnz, unofficial : 1 },
    { 3, 4, ABS, "NOP", rflags: 0, wflags: 0, unofficial : 1 },
    { 3, 4, ABS, "ORA", rflags: 0, wflags: nz },
    { 3, 6, ABS, "ASL", rflags: 0, wflags: cnz },
    { 3, 6, ABS, "SLO", rflags: 0, wflags: cnz, unofficial : 1 },
    { 2, 2, REL, "BPL", rflags: n, wflags: 0 },
    { 2, 5, INY, "ORA", rflags: 0, wflags: nz },
    { 1, 0, IMP, "???", rflags: 0, wflags: 0, unofficial: 1, jam: 1 },
    { 2, 8, INY, "SLO", rflags: 0, wflags: cnz, unofficial : 1 },
    { 2, 4, ZPX, "NOP", rflags: 0, wflags: 0, unofficial : 1 },
    { 2, 4, ZPX, "ORA", rflags: 0, wflags: nz },
    { 2, 6, ZPX, "ASL", rflags: 0, wflags: cnz },
    { 2, 6, ZPX, "SLO", rflags: 0, wflags: cnz, unofficial : 1 },
    { 1, 2, IMP, "CLC", rflags: 0, wflags: 0 },
    { 3, 4, ABY, "ORA", rflags: 0, wflags: nz },
    { 1, 2, IMP, "NOP", rflags: 0, wflags: 0, unofficial : 1 },
    { 3, 7, ABY, "SLO", rflags: 0, wflags: cnz, unofficial : 1 },
    { 3, 4, ABX, "NOP", rflags: 0, wflags: 0, unofficial : 1 },
    { 3, 4, ABX, "ORA", rflags: 0, wflags: nz },
    { 3, 7, ABX, "ASL", rflags: 0, wflags: cnz },
    { 3, 7, ABX, "SLO", rflags: 0, wflags: cnz, unofficial : 1 },
    { 3, 6, ABS, "JSR", rflags: all, wflags: 0 },
    { 2, 6, INX, "AND", rflags: 0, wflags: nz },
    { 1, 0, IMP, "???", rflags: 0, wflags: 0, unofficial: 1, jam: 1 },
    { 2, 8, INX, "RLA", rflags: c, wflags: cnz, unofficial : 1 },
    { 2, 3, ZPG, "BIT", rflags: 0, wflags: vnz },
    { 2, 3, ZPG, "AND", rflags: 0, wflags: nz },
    { 2, 5, ZPG, "ROL", rflags: c, wflags: cnz },
    { 2, 5, ZPG, "RLA", rflags: c, wflags: cnz, unofficial : 1 },
    { 1, 4, IMP, "PLP", rflags: 0, wflags: all },
    { 2, 2, IMM, "AND", rflags: 0, wflags: nz },
    { 1, 2, ACC, "ROL", rflags: c, wflags: cnz },
    { 2, 2, IMM, "AAC", rflags: 0, wflags: cnz, unofficial : 1 },
    { 3, 4, ABS, "BIT", rflags: 0, wflags: vnz },
    { 3, 4, ABS, "AND", rflags: 0, wflags: nz },
    { 3, 6, ABS, "ROL", rflags: c, wflags: cnz },
    { 3, 6, ABS, "RLA", rflags: c, wflags: cnz, unofficial : 1 },
    { 2, 2, REL, "BMI", rflags: n, wflags: 0 },
    { 2, 5, INY, "AND", rflags: 0, wflags: nz },
    { 1, 0, IMP, "???", rflags: 0, wflags: 0, unofficial: 1, jam: 1 },
    { 2, 8, INY, "RLA", rflags: c, wflags: cnz, unofficial : 1 },
    { 2, 4, ZPX, "NOP", rflags: 0, wflags: 0, unofficial : 1 },
    { 2, 4, ZPX, "AND", rflags: 0, wflags: nz },
    { 2, 6, ZPX, "ROL", rflags: c, wflags: cnz },
    { 2, 6, ZPX, "RLA", rflags: c, wflags: cnz, unofficial : 1 },
    { 1, 2, IMP, "SEC", rflags: 0, wflags: c },
    { 3, 4, ABY, "AND", rflags: 0, wflags: nz },
    { 1, 2, IMP, "NOP", rflags: 0, wflags: 0, unofficial : 1 },
    { 3, 7, ABY, "RLA", rflags: c, wflags: cnz, unofficial : 1 },
    { 3, 4, ABX, "NOP", rflags: 0, wflags: 0, unofficial : 1 },
    { 3, 4, ABX, "AND", rflags: 0, wflags: nz },
    { 3, 7, ABX, "ROL", rflags: c, wflags: cnz },
    { 3, 7, ABX, "RLA", rflags: c, wflags: cnz, unofficial : 1 },
    { 1, 6, IMP, "RTI", rflags: all, wflags: 0 },
    { 2, 6, INX, "EOR", rflags: 0, wflags: nz },
    { 1, 0, IMP, "???", rflags: 0, wflags: 0, unofficial: 1, jam: 1 },
    { 2, 8, INX, "SRE", rflags: 0, wflags: cnz, unofficial : 1 },
    { 2, 3, ZPG, "NOP", rflags: 0, wflags: 0, unofficial : 1 },
    { 2, 3, ZPG, "EOR", rflags: 0, wflags: nz },
    { 2, 5, ZPG, "LSR", rflags: 0, wflags: cnz },
    { 2, 5, ZPG, "SRE", rflags: 0, wflags: cnz, unofficial : 1 },
    { 1, 3, IMP, "PHA", rflags: 0, wflags: 0 },
    { 2, 2, IMM, "EOR", rflags: 0, wflags: nz },
    { 1, 2, ACC, "LSR", rflags: 0, wflags: cnz },
    { 2, 2, IMM, "ASR", rflags: 0, wflags: cnz, unofficial : 1 },
    { 3, 3, ABS, "JMP", rflags: all, wflags: 0 },
    { 3, 4, ABS, "EOR", rflags: 0, wflags: nz },
    { 3, 6, ABS, "LSR", rflags: 0, wflags: cnz },
    { 3, 6, ABS, "SRE", rflags: 0, wflags: cnz, unofficial : 1 },
    { 2, 2, REL, "BVC", rflags: v, wflags: 0 },
    { 2, 5, INY, "EOR", rflags: 0, wflags: nz },
    { 1, 0, IMP, "???", rflags: 0, wflags: 0, unofficial: 1, jam: 1 },
    { 2, 8, INY, "SRE", rflags: 0, wflags: cnz, unofficial : 1 },
    { 2, 4, ZPX, "NOP", rflags: 0, wflags: 0, unofficial : 1 },
    { 2, 4, ZPX, "EOR", rflags: 0, wflags: nz },
    { 2, 6, ZPX, "LSR", rflags: 0, wflags: cnz },
    { 2, 6, ZPX, "SRE", rflags: 0, wflags: cnz, unofficial : 1 },
    { 1, 2, IMP, "CLI", rflags: 0, wflags: 0 },
    { 3, 4, ABY, "EOR", rflags: 0, wflags: nz },
    { 1, 2, IMP, "NOP", rflags: 0, wflags: 0, unofficial : 1 },
    { 3, 7, ABY, "SRE", rflags: 0, wflags: cnz, unofficial : 1 },
    { 3, 4, ABX, "NOP", rflags: 0, wflags: 0, unofficial : 1 },
    { 3, 4, ABX, "EOR", rflags: 0, wflags: nz },
    { 3, 7, ABX, "LSR", rflags: 0, wflags: cnz },
    { 3, 7, ABX, "SRE", rflags: 0, wflags: cnz, unofficial : 1 },
    { 1, 6, IMP, "RTS", rflags: all, wflags: 0 },
    { 2, 6, INX, "ADC", rflags: c, wflags: cvnz },
    { 1, 0, IMP, "???", rflags: 0, wflags: 0, unofficial: 1, jam: 1 },
    { 2, 8, INX, "RRA", rflags: c, wflags: cvnz, unofficial : 1 },
    { 2, 3, ZPG, "NOP", rflags: 0, wflags: 0, unofficial : 1 },
    { 2, 3, ZPG, "ADC", rflags: c, wflags: cvnz },
    { 2, 5, ZPG, "ROR", rflags: c, wflags: cnz },
    { 2, 5, ZPG, "RRA", rflags: c, wflags: cvnz, unofficial : 1 },
    { 1, 4, IMP, "PLA", rflags: 0, wflags: nz },
    { 2, 2, IMM, "ADC", rflags: c, wflags: cvnz },
    { 1, 2, ACC, "ROR", rflags: c, wflags: cnz },
    { 2, 2, IMM, "ARR", rflags: c, wflags: cvnz, unofficial : 1 },
    { 3, 5, IND, "JMP", rflags: all, wflags: 0 },
    { 3, 4, ABS, "ADC", rflags: c, wflags: cvnz },
    { 3, 6, ABS, "ROR", rflags: c, wflags: cnz },
    { 3, 6, ABS, "RRA", rflags: c, wflags: cvnz, unofficial : 1 },
    { 2, 2, REL, "BVS", rflags: v, wflags: 0 },
    { 2, 5, INY, "ADC", rflags: c, wflags: cvnz },
    { 1, 0, IMP, "???", rflags: 0, wflags: 0, unofficial: 1, jam: 1 },
    { 2, 8, INY, "RRA", rflags: c, wflags: cvnz, unofficial : 1 },
    { 2, 4, ZPX, "NOP", rflags: 0, wflags: 0, unofficial : 1 },
    { 2, 4, ZPX, "ADC", rflags: c, wflags: cvnz },
    { 2, 6, ZPX, "ROR", rflags: c, wflags: cnz },
    { 2, 6, ZPX, "RRA", rflags: c, wflags: cvnz, unofficial : 1 },
    { 1, 2, IMP, "SEI", rflags: 0, wflags: 0 },
    { 3, 4, ABY, "ADC", rflags: c, wflags: cvnz },
    { 1, 2, IMP, "NOP", rflags: 0, wflags: 0, unofficial : 1 },
    { 3, 7, ABY, "RRA", rflags: c, wflags: cvnz, unofficial : 1 },
    { 3, 4, ABX, "NOP", rflags: 0, wflags: 0, unofficial : 1 },
    { 3, 4, ABX, "ADC", rflags: c, wflags: cvnz },
    { 3, 7, ABX, "ROR", rflags: c, wflags: cnz },
    { 3, 7, ABX, "RRA", rflags: c, wflags: cvnz, unofficial : 1 },
    { 2, 2, IMM, "NOP", rflags: 0, wflags: 0, unofficial : 1 },
    { 2, 6, INX, "STA", rflags: 0, wflags: 0 },
    { 2, 2, IMM, "NOP", rflags: 0, wflags: 0, unofficial : 1 },
    { 2, 6, INX, "SAX", rflags: 0, wflags: 0, unofficial : 1 },
    { 2, 3, ZPG, "STY", rflags: 0, wflags: 0 },
    { 2, 3, ZPG, "STA", rflags: 0, wflags: 0 },
    { 2, 3, ZPG, "STX", rflags: 0, wflags: 0 },
    { 2, 3, ZPG, "SAX", rflags: 0, wflags: 0, unofficial : 1 },
    { 1, 2, IMP, "DEY", rflags: 0, wflags: nz },
    { 2, 2, IMM, "NOP", rflags: 0, wflags: 0, unofficial : 1 },
    { 1, 2, IMP, "TXA", rflags: 0, wflags: nz },
    { 2, 2, IMM, "ANE", rflags: 0, wflags: 0, unofficial : 1 }, // not implemented
    { 3, 4, ABS, "STY", rflags: 0, wflags: 0 },
    { 3, 4, ABS, "STA", rflags: 0, wflags: 0 },
    { 3, 4, ABS, "STX", rflags: 0, wflags: 0 },
    { 3, 4, ABS, "SAX", rflags: 0, wflags: 0, unofficial : 1 },
    { 2, 2, REL, "BCC", rflags: c, wflags: 0 },
    { 2, 6, INY, "STA", rflags: 0, wflags: 0 },
    { 1, 0, IMP, "???", rflags: 0, wflags: 0, unofficial: 1, jam: 1 },
    { 2, 5, INY, "SHA", rflags: 0, wflags: 0, unofficial : 1 }, // not implemented
    { 2, 4, ZPX, "STY", rflags: 0, wflags: 0 },
    { 2, 4, ZPX, "STA", rflags: 0, wflags: 0 },
    { 2, 4, ZPY, "STX", rflags: 0, wflags: 0 },
    { 2, 4, ZPY, "SAX", rflags: 0, wflags: 0, unofficial : 1 },
    { 1, 2, IMP, "TYA", rflags: 0, wflags: nz },
    { 3, 5, ABY, "STA", rflags: 0, wflags: 0 },
    { 1, 2, IMP, "TXS", rflags: 0, wflags: 0 },
    { 3, 4, ABY, "SHS", rflags: 0, wflags: 0, unofficial : 1 }, // not implemented
    { 3, 4, ABX, "SHY", rflags: 0, wflags: 0, unofficial : 1 }, // not implemented
    { 3, 5, ABX, "STA", rflags: 0, wflags: 0 },
    { 3, 4, ABY, "SHX", rflags: 0, wflags: 0, unofficial : 1 }, // not implemented
    { 3, 4, ABY, "SHA", rflags: 0, wflags: 0, unofficial : 1 }, // not implemented
    { 2, 2, IMM, "LDY", rflags: 0, wflags: nz },
    { 2, 6, INX, "LDA", rflags: 0, wflags: nz },
    { 2, 2, IMM, "LDX", rflags: 0, wflags: nz },
    { 2, 6, INX, "LAX", rflags: 0, wflags: nz, unofficial : 1 },
    { 2, 3, ZPG, "LDY", rflags: 0, wflags: nz },
    { 2, 3, ZPG, "LDA", rflags: 0, wflags: nz },
    { 2, 3, ZPG, "LDX", rflags: 0, wflags: nz },
    { 2, 3, ZPG, "LAX", rflags: 0, wflags: nz, unofficial : 1 },
    { 1, 2, IMP, "TAY", rflags: 0, wflags: nz },
    { 2, 2, IMM, "LDA", rflags: 0, wflags: nz },
    { 1, 2, IMP, "TAX", rflags: 0, wflags: nz },
    { 2, 2, IMM, "ATX", rflags: 0, wflags: nz, unofficial : 1 },
    { 3, 4, ABS, "LDY", rflags: 0, wflags: nz },
    { 3, 4, ABS, "LDA", rflags: 0, wflags: nz },
    { 3, 4, ABS, "LDX", rflags: 0, wflags: nz },
    { 3, 4, ABS, "LAX", rflags: 0, wflags: nz, unofficial : 1 },
    { 2, 2, REL, "BCS", rflags: c, wflags: 0 },
    { 2, 5, INY, "LDA", rflags: 0, wflags: nz },
    { 1, 0, IMP, "???", rflags: 0, wflags: 0, unofficial: 1, jam: 1 },
    { 2, 5, INY, "LAX", rflags: 0, wflags: nz, unofficial : 1 },
    { 2, 4, ZPX, "LDY", rflags: 0, wflags: nz },
    { 2, 4, ZPX, "LDA", rflags: 0, wflags: nz },
    { 2, 4, ZPY, "LDX", rflags: 0, wflags: nz },
    { 2, 4, ZPY, "LAX", rflags: 0, wflags: nz, unofficial : 1 },
    { 1, 2, IMP, "CLV", rflags: 0, wflags: v },
    { 3, 4, ABY, "LDA", rflags: 0, wflags: nz },
    { 1, 2, IMP, "TSX", rflags: 0, wflags: nz },
    { 3, 4, ABY, "LAS", rflags: 0, wflags: 0, unofficial : 1 }, // not implemented
    { 3, 4, ABX, "LDY", rflags: 0, wflags: nz },
    { 3, 4, ABX, "LDA", rflags: 0, wflags: nz },
    { 3, 4, ABY, "LDX", rflags: 0, wflags: nz },
    { 3, 4, ABY, "LAX", rflags: 0, wflags: nz, unofficial : 1 },
    { 2, 2, IMM, "CPY", rflags: 0, wflags: cnz },
    { 2, 6, INX, "CMP", rflags: 0, wflags: cnz },
    { 2, 2, IMM, "NOP", rflags: 0, wflags: 0, unofficial : 1 },
    { 2, 8, INX, "DCP", rflags: 0, wflags: cnz, unofficial : 1 },
    { 2, 3, ZPG, "CPY", rflags: 0, wflags: cnz },
    { 2, 3, ZPG, "CMP", rflags: 0, wflags: cnz },
    { 2, 5, ZPG, "DEC", rflags: 0, wflags: nz },
    { 2, 5, ZPG, "DCP", rflags: 0, wflags: cnz, unofficial : 1 },
    { 1, 2, IMP, "INY", rflags: 0, wflags: nz },
    { 2, 2, IMM, "CMP", rflags: 0, wflags: cnz },
    { 1, 2, IMP, "DEX", rflags: 0, wflags: nz },
    { 2, 2, IMM, "AXS", rflags: 0, wflags: cnz, unofficial : 1 },
    { 3, 4, ABS, "CPY", rflags: 0, wflags: cnz },
    { 3, 4, ABS, "CMP", rflags: 0, wflags: cnz },
    { 3, 6, ABS, "DEC", rflags: 0, wflags: nz },
    { 3, 6, ABS, "DCP", rflags: 0, wflags: cnz, unofficial : 1 },
    { 2, 2, REL, "BNE", rflags: 0, wflags: z },
    { 2, 5, INY, "CMP", rflags: 0, wflags: cnz },
    { 1, 0, IMP, "???", rflags: 0, wflags: 0, unofficial: 1, jam: 1 },
    { 2, 8, INY, "DCP", rflags: 0, wflags: cnz, unofficial : 1 },
    { 2, 4, ZPX, "NOP", rflags: 0, wflags: 0, unofficial : 1 },
    { 2, 4, ZPX, "CMP", rflags: 0, wflags: cnz },
    { 2, 6, ZPX, "DEC", rflags: 0, wflags: nz },
    { 2, 6, ZPX, "DCP", rflags: 0, wflags: cnz, unofficial : 1 },
    { 1, 2, IMP, "CLD", rflags: 0, wflags: 0 },
    { 3, 4, ABY, "CMP", rflags: 0, wflags: cnz },
    { 1, 2, IMP, "NOP", rflags: 0, wflags: 0, unofficial : 1 },
    { 3, 7, ABY, "DCP", rflags: 0, wflags: cnz, unofficial : 1 },
    { 3, 4, ABX, "NOP", rflags: 0, wflags: 0, unofficial : 1 },
    { 3, 4, ABX, "CMP", rflags: 0, wflags: cnz },
    { 3, 7, ABX, "DEC", rflags: 0, wflags: nz },
    { 3, 7, ABX, "DCP", rflags: 0, wflags: cnz, unofficial : 1 },
    { 2, 2, IMM, "CPX", rflags: 0, wflags: cnz },
    { 2, 6, INX, "SBC", rflags: c, wflags: cvnz },
    { 2, 2, IMM, "NOP", rflags: 0, wflags: 0, unofficial : 1 },
    { 2, 8, INX, "ISB", rflags: c, wflags: cvnz, unofficial : 1 },
    { 2, 3, ZPG, "CPX", rflags: 0, wflags: cnz },
    { 2, 3, ZPG, "SBC", rflags: c, wflags: cvnz },
    { 2, 5, ZPG, "INC", rflags: 0, wflags: nz },
    { 2, 5, ZPG, "ISB", rflags: c, wflags: cvnz, unofficial : 1 },
    { 1, 2, IMP, "INX", rflags: 0, wflags: nz },
    { 2, 2, IMM, "SBC", rflags: c, wflags: cvnz },
    { 1, 2, IMP, "NOP", rflags: 0, wflags: 0 },
    { 2, 2, IMM, "SBC", rflags: c, wflags: cvnz, unofficial : 1 },
    { 3, 4, ABS, "CPX", rflags: 0, wflags: cnz },
    { 3, 4, ABS, "SBC", rflags: c, wflags: cvnz },
    { 3, 6, ABS, "INC", rflags: 0, wflags: nz },
    { 3, 6, ABS, "ISB", rflags: c, wflags: cvnz, unofficial : 1 },
    { 2, 2, REL, "BEQ", rflags: z, wflags: 0 },
    { 2, 5, INY, "SBC", rflags: c, wflags: cvnz },
    { 1, 0, IMP, "???", rflags: 0, wflags: 0, unofficial: 1, jam: 1 },
    { 2, 8, INY, "ISB", rflags: c, wflags: cvnz, unofficial : 1 },
    { 2, 4, ZPX, "NOP", rflags: 0, wflags: 0, unofficial : 1 },
    { 2, 4, ZPX, "SBC", rflags: c, wflags: cvnz },
    { 2, 6, ZPX, "INC", rflags: 0, wflags: nz },
    { 2, 6, ZPX, "ISB", rflags: c, wflags: cvnz, unofficial : 1 },
    { 1, 2, IMP, "SED", rflags: 0, wflags: 0 },
    { 3, 4, ABY, "SBC", rflags: c, wflags: cvnz },
    { 1, 2, IMP, "NOP", rflags: 0, wflags: 0, unofficial : 1 },
    { 3, 7, ABY, "ISB", rflags: c, wflags: cvnz, unofficial : 1 },
    { 3, 4, ABX, "NOP", rflags: 0, wflags: 0, unofficial : 1 },
    { 3, 4, ABX, "SBC", rflags: c, wflags: cvnz },
    { 3, 7, ABX, "INC", rflags: 0, wflags: nz },
    { 3, 7, ABX, "ISB", rflags: c, wflags: cvnz, unofficial : 1 },
 };

 };
 };
