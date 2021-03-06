
#include <iostream>

#include "Memory.h"
#include "Mapper.h"
#include "N2C02State.h"
#include "M6502State.h"
#include "Joypad.h"

using namespace Memory;

namespace Memory {

/**
 * Internal CPU ram, of size 2048. Force the alignment to be 0x100, this has
 * consequences on the x86 binary code re-compilation.
 */
__attribute__((aligned(0x100)))
u8 ram[0x800];

/**
 * PRG rom banks, addresses 0x8000-0xffff
 */
size_t prgBankSize;
uint prgBankShift;
u16 prgBankMax;
u16 prgBankMask;
u8 *prgBank[4];

/**
 * CHR rom, addresses 0x0000-0x1fff of the PPU address space
 */
u8 chrRom[0x2000];

/**
 * PRG ram, addresses 0x6000-0x7fff
 */
bool prgRamEnabled = false;
bool prgRamWriteProtected = false;
u8 *prgRam;

/**
 * Initiate a DMA tranfer with the 2C02 PPU memory.
 */
static void writeOAMDMARegister(u8 val, long quantum);

/**
 * @brief Load a byte from an address in the CPU memory address space.
 * @param addr          absolute memory address
 * @return              the value read from the address \p addr
 */
static inline __attribute__((always_inline))
u8 _load(u16 addr, long quantum)
{
    // std::cerr << std::hex << "LOAD " << (int)addr << std::endl;

    if (addr >= 0x8000)
        return prgBank[(addr >> prgBankShift) & prgBankMax][addr & prgBankMask];
    else
    if (addr < 0x2000)
        return ram[addr & 0x7ff];
    else
    if (addr < 0x4000)
        return N2C02::state.readRegister(addr, quantum);
    else
    if (addr == JOYPAD1_ADDR)
        return Joypad::currentJoypad->readRegister();
    else
    if (addr == OAMDMA_ADDR)
        return 0x0;
    else
    if (addr < 0x4020)
        // return readAPURegister(addr);
        return 0x0;
    else
    if (addr < 0x6000)
        return 0x0; /* open bus */
    else
    if (prgRamEnabled)
        /* Read from cartridge space */
        // return rom.prg.load(addr);
        return prgRam[addr & 0x1fff];
    else
        return 0x0;
}

u8 load(u16 addr, long quantum)
{
    return _load(addr, quantum);
}

u8 load0(u16 addr)
{
    return _load(addr, 0);
}

/**
 * @brief Store a byte at an address in the CPU memory address space.
 * @param addr          absolute memory address
 * @param val           written value
 */
static inline __attribute__((always_inline))
void _store(u16 addr, u8 val, long quantum)
{
    // std::cerr << std::hex << "STORE ";
    // std::cerr << (int)addr << " " << (int)val << std::endl;

    if (addr < 0x2000)
        ram[addr & 0x7ff] = val;
    else
    if (addr >= 0x8000)
        currentMapper->storePrg(addr, val);
    else
    if (addr < 0x4000)
        N2C02::state.writeRegister(addr, val, quantum);
    else
    if (addr == JOYPAD1_ADDR)
        Joypad::currentJoypad->writeRegister(val);
    else
    if (addr == OAMDMA_ADDR)
        writeOAMDMARegister(val, quantum);
    else
    if (addr < 0x4020)
        // writeAPURegister(addr, val);
        return;
    else
    if (addr < 0x6000)
        return; /* open bus */
    else
    if (prgRamEnabled && !prgRamWriteProtected)
        prgRam[addr & 0x1fff] = val;
}

void store(u16 addr, u8 val, long quantum)
{
    _store(addr, val, quantum);
}

void store0(u16 addr, u8 val)
{
    _store(addr, val, 0);
}

static inline void writeOAMDMARegister(u8 val, long quantum)
{
    u16 dmaoffset, dmapage = (u16)val << 8;
    N2C02::sync(quantum);
    for (dmaoffset = 0; dmaoffset < 0x100; dmaoffset++) {
        val = load(dmapage + dmaoffset);
        N2C02::state.dmaTransfer(val);
    }
    M6502::state->cycles += 514; /* Technically 513 OR 514. */
}

};
