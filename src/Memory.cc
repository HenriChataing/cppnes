
#include <iostream>

#include "Memory.h"
#include "Mapper.h"

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
 * PRG ram, addresses 0x6000-0x7fff
 */
bool prgRamEnabled;
bool prgRamWriteProtected;
u8 prgRam[0x2000];

/**
 * @brief Load a byte from an address in the CPU memory address space.
 * @param addr          absolute memory address
 * @return              the value read from the address \p addr
 */
u8 load(u16 addr)
{
    // std::cerr << std::hex << "LOAD " << (int)addr << std::endl;

    if (addr >= 0x8000)
        return prgBank[(addr >> prgBankShift) & prgBankMax][addr & prgBankMask];
    else
    if (addr < 0x2000)
        return ram[addr & 0x7ff];
    else
    if (addr < 0x4000)
        // return readPPURegister(addr);
        return 0x0;
    else
    if (addr == JOYPAD1_ADDR)
        // return readJoypadRegister();
        return 0x0;
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

/**
 * @brief Store a byte at an address in the CPU memory address space.
 * @param addr          absolute memory address
 * @param val           written value
 */
void store(u16 addr, u8 val)
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
        // writePPURegister(addr, val);
        return;
    else
    if (addr == JOYPAD1_ADDR)
        // writeJoypadRegister(val);
        return;
    else
    if (addr == OAMDMA_ADDR)
        // writeOAMDMARegister(val);
        return;
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

};
