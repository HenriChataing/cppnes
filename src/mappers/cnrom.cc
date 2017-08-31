
#include <cstdlib>
#include <cstring>
#include <iostream>

#include "Rom.h"
#include "Mapper.h"
#include "Memory.h"

class CNROM: public Mapper
{
public:
    CNROM(Rom *rom) : Mapper(rom, "CNROM") {
        Memory::prgBankSize = 0x4000;
        Memory::prgBankMask = 0x3fff;
        Memory::prgBankShift = 14;
        Memory::prgBankMax = 1;

        if (rom->chrRom == NULL)
            throw "CNROM: No CHR-ROM bank detected";

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

    ~CNROM() {
    }

    void storePrg(u16 addr, u8 val) {
        (void)addr;
        u8 *chrBank = &rom->chrRom[val * 0x2000];
        swapChrRomBank(0, chrBank);
        swapChrRomBank(1, chrBank + 0x1000);
    }
};

static Mapper *createCNROM(Rom *rom) {
    return new CNROM(rom);
}

/**
 * Install the NROM mapper.
 */
static void insert(void) __attribute__((constructor));
static void insert(void)
{
    mappers[3] = createCNROM;
}
