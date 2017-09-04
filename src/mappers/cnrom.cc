
#include <cstdlib>
#include <cstring>
#include <iostream>

#include "Rom.h"
#include "Mapper.h"
#include "Memory.h"

class CNROM: public Mapper
{
public:
    /* CHR-ROM memory of 0x2000 bytes, with on switchable 0x2000 byte bank. */
    BankMemory<13, 13> chrRom;

    CNROM(Rom *rom) : Mapper(rom, "CNROM") {
        Memory::prgBankSize = 0x4000;
        Memory::prgBankMask = 0x3fff;
        Memory::prgBankShift = 14;
        Memory::prgBankMax = 1;

        if (rom->chrRom == NULL)
            throw "CNROM: No CHR-ROM bank detected";

        chrRom.readOnly = true;
        chrRom.squash = Memory::chrRom;
        chrRom.source = rom->chrRom;
        chrRom.swapBank(0, 0);

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
        chrRom.swapBank(0, val);
    }

    void storeChr(u16 addr, u8 val) {
        chrRom.store(addr, val);
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
