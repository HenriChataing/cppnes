
#ifndef _MEMORY_H_INCLUDED_
#define _MEMORY_H_INCLUDED_

#include <cstdlib>

#include "type.h"

namespace Memory
{

/**
 * Internal CPU ram, of size 2048.
 */
extern u8 ram[];

/**
 * PRG rom banks, addresses 0x8000-0xffff
 */
extern size_t prgBankSize;
extern uint prgBankShift;
extern u16 prgBankMax;
extern u16 prgBankMask;
extern u8 *prgBank[];

/**
 * CHR rom, addresses 0x0000-0x1fff of the PPU address space
 */
extern u8 *chrRom;

/**
 * PRG ram, addresses 0x6000-0x7fff
 */
extern bool prgRamEnabled;
extern bool prgRamWriteProtected;
extern u8 prgRam[];

/**
 * Configure the PRG-ROM banks.
 */
void configPrg(size_t bankSize);

/**
 * @brief Load a byte from an address in the CPU memory address space.
 * @param addr          absolute memory address
 * @return              the value read from the address \p addr
 */
u8 load(u16 addr);

/**
 * @brief Load a word for the addresses \p addr and \p addr+1.
 * The word is encoded in big-endian.
 * @param addr          absolute address
 * @return              the word read from the addresses \p addr
 *                      and \p addr+1
 */
inline u16 loadw(u16 addr) {
    return WORD(load(addr + 1), load(addr));
}

/**
 * @brief Load a word from the specified absolute address in the
 * zero page. If \p addr is 0xff the second word byte is read from
 * the address 0x00.
 * @param addr          absolute address
 * @return              the wordd stored at addresses \p  addr
 *                      and \p addr+1
 */
inline u16 loadzw(u8 addr) {
    return WORD(ram[(addr + 1) & 0xff], ram[addr]);
}

/**
 * @brief Store a byte at an address in the CPU memory address space.
 * @param addr          absolute memory address
 * @param val           written value
 */
void store(u16 addr, u8 val);

enum {
    OAMDMA_ADDR = 0x4014,
    JOYPAD1_ADDR = 0x4016,
    NMI_ADDR = 0xfffa,
    RST_ADDR = 0xfffc,
    IRQ_ADDR = 0xfffe,
};

};

#endif /* _MEMORY_H_INCLUDED_ */
