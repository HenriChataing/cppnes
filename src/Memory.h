
#ifndef _MEMORY_H_INCLUDED_
#define _MEMORY_H_INCLUDED_

#include <cstdlib>
#include <cassert>
#include <cstring>

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
extern u8 chrRom[];

/**
 * PRG ram, addresses 0x6000-0x7fff
 */
extern bool prgRamEnabled;
extern bool prgRamWriteProtected;
extern u8 *prgRam;

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

/**
 * @brief Implementation of a fixed size memory with switachable banks.
 * @param memoryOrder       order of the mapped memory size
 *                          (the memory size is 1 << \p memoryOrder)
 * @param bankOrer          order of a bank size
 *                          (the bank size is 1 << \p bankOrder)
 */
template<int memoryOrder, int bankOrder>
class BankMemory
{
public:
    BankMemory() : readOnly(true) {}
    ~BankMemory() {}

    /**
     * @brief Set the bank \p bankNr to the bank \p bankSel of the source
     *  memory.
     */
    void swapBank(int bankNr, int bankSel) {
        assert(source != NULL);
        assert(bankNr < (1 << (memoryOrder - bankOrder)));
        u8 *ptr = &source[bankSel << bankOrder];
        if (squash && banks[bankNr] != ptr) {
            memcpy(&squash[bankNr << bankOrder], ptr, 1 << bankOrder);
        }
        banks[bankNr] = ptr;
    }

    /**
     * @brief Set the bank \p bankNr to the provided bank buffer.
     */
    void swapBank(int bankNr, u8 *ptr) {
        assert(source == NULL);
        assert(bankNr < (1 << (memoryOrder - bankOrder)));
        if (squash && banks[bankNr] != ptr) {
            memcpy(&squash[bankNr << bankOrder], ptr, 1 << bankOrder);
        }
        banks[bankNr] = ptr;
    }

    void store(u16 addr, u8 val) {
        if (!readOnly) {
            const u16 mask = (1 << bankOrder) - 1;
            banks[addr >> bankOrder][addr & mask] = val;
            if (squash)
                squash[addr] = val;
            /// TODO handle mirroring.
        }
    }

    bool readOnly;
    u8 *banks[1 << (memoryOrder - bankOrder)];
    u8 *source;
    u8 *squash;
};

#endif /* _MEMORY_H_INCLUDED_ */
