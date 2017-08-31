
#include <cstdlib>
#include <cstring>
#include <iostream>

#include "Rom.h"
#include "Mapper.h"
#include "Memory.h"

class NROM: public Mapper
{
public:
    NROM(Rom *rom) : Mapper(rom, "NROM") {
        Memory::prgBankSize = 0x4000;
        Memory::prgBankMask = 0x3fff;
        Memory::prgBankShift = 14;
        Memory::prgBankMax = 1;
        Memory::prgRamEnabled = false;

        if (rom->chrRom == NULL) {
            rom->chrRom = new u8[0x2000];
            if (rom->chrRom == NULL)
                throw "MMC1: Cannot allocate CHR-RAM";
            _chrRam = true;
        } else
            _chrRam = false;

        swapChrRomBank(0, rom->chrRom);
        swapChrRomBank(1, rom->chrRom + 0x1000);

        if (rom->header.prom >= 2) {
            Memory::prgBank[0] = rom->prgRom;
            Memory::prgBank[1] = rom->prgRom + 0x4000;
        } else {
            Memory::prgBank[0] = rom->prgRom;
            Memory::prgBank[1] = rom->prgRom;
        }
    }

    ~NROM() {
    }

    void storePrg(u16 addr, u8 val) {
        (void)addr; (void)val;
    }
};

static Mapper *createNROM(Rom *rom) {
    return new NROM(rom);
}

/**
 * Install the NROM mapper.
 */
static void insert(void) __attribute__((constructor));
static void insert(void)
{
    mappers[0] = createNROM;
}

