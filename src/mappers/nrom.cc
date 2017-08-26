
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

            if (rom->chrRom == NULL) {
                rom->header.crom = 1;
                rom->chrRom = new u8[0x2000];
                if (rom->chrRom == NULL)
                    throw "NROM: Cannot allocate CHR-RAM";
                _chrRam = true;
            } else
                _chrRam = false;

            if (rom->header.prom >= 2) {
                Memory::prgBank[0] = rom->prgRom;
                Memory::prgBank[1] = rom->prgRom + 0x4000;
            } else {
                Memory::prgBank[0] = rom->prgRom;
                Memory::prgBank[1] = rom->prgRom;
            }
            memcpy(Memory::chrRom, rom->chrRom, 0x2000);
        }

        ~NROM() {
        }

        void storePrg(u16 addr, u8 val) {
            (void)addr; (void)val;
        }

        void storeChr(u16 addr, u8 val) {
            if (_chrRam)
                rom->chrRom[addr] = val;
        }

    private:
        bool _chrRam;
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

