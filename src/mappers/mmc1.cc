
#include <cstdlib>
#include <cstring>
#include <iostream>

#include "Rom.h"
#include "Mapper.h"
#include "Memory.h"
#include "N2C02State.h"

class MMC1: public Mapper
{
public:
    /* CHR-ROM memory of 0x2000 bytes, with 0x1000 byte banks. */
    BankMemory<13, 12> chrRom;

    MMC1(Rom *rom) : Mapper(rom, "MMC1") {
        /* Define ROM geometry. */
        Memory::prgBankSize = 0x4000;
        Memory::prgBankMask = 0x3fff;
        Memory::prgBankShift = 14;
        Memory::prgBankMax = 1;

        /* Test presence of PRG-RAM */
        if (rom->prgRam == NULL)
            throw "MMC1: Missing PRG-RAM";

        Memory::prgRam = rom->prgRam;
        Memory::prgRamEnabled = true;
        Memory::prgRamWriteProtected = true;

        /* Allocate CHR-RAM */
        if (rom->chrRom == NULL) {
            rom->chrRom = new u8[0x2000];
            if (rom->chrRom == NULL)
                throw "MMC1: Cannot allocate CHR-RAM";
            chrRom.readOnly = false;
        } else
            chrRom.readOnly = true;

        /* Initial banks. */
        Memory::prgBank[0] = rom->prgRom;
        Memory::prgBank[1] = rom->prgRom;
        chrRom.squash = Memory::chrRom;
        chrRom.source = rom->chrRom;
        chrRom.swapBank(0, 0);
        chrRom.swapBank(1, 1);
        writeControlRegister(0x0c);
    }

    ~MMC1() {
    }

    void storePrg(u16 addr, u8 val) {
        /* Reset the load register. */
        if (val & 0x80) {
            _loadRegisterSize = 0;
            writeControlRegister(_controlRegister | 0x0c);
            return;
        }
        /* Feed data into the load register. */
        _loadRegister >>= 1;
        _loadRegister |= (val << 4);
        _loadRegisterSize++;

        if (_loadRegisterSize < 5)
            return;

        if (addr < 0xa000)
            writeControlRegister(_loadRegister);
        else
        if (addr < 0xc000)
            writeChrBank0Register(_loadRegister);
        else
        if (addr < 0xe000)
            writeChrBank1Register(_loadRegister);
        else
            writePrgBankRegister(_loadRegister);

        _loadRegisterSize = 0;
        _loadRegister = 0;
    }

    void storeChr(u16 addr, u8 val) {
        return chrRom.store(addr, val);
    }

private:
    uint _loadRegisterSize; /* Number of valid data bits in the load register */
    u8 _loadRegister;
    u8 _controlRegister;
    u8 _chrBank0Register;
    u8 _chrBank1Register;
    u8 _prgBankRegister;

    inline void writeChrBank0Register(u8 val)
    {
        _chrBank0Register = val;
        if (_controlRegister & 0x10) {
            /* Swap CHR-ROM bank 0 */
            chrRom.swapBank(0, val & 0x1f);
        } else {
            /* Swap CHR-ROM bank 0 and 1 */
            chrRom.swapBank(0, val & 0x1e);
            chrRom.swapBank(1, (val & 0x1e) + 1);
        }
    }

    inline void writeChrBank1Register(u8 val)
    {
        _chrBank1Register = val;
        if (_controlRegister & 0x10)
            /* Swap CHR-ROM bank 1 */
            chrRom.swapBank(1, val & 0x1f);
    }

    inline void writePrgBankRegister(u8 val)
    {
        Memory::prgRamWriteProtected = (val & 0x10) != 0;
        _prgBankRegister = val;
        u8 *prgRom = rom->prgRom;

        switch (_controlRegister & 0xc) {
            case 0x0:
            case 0x4:
                Memory::prgBank[0] = &prgRom[(val & 0xe) * 0x4000];
                Memory::prgBank[1] = Memory::prgBank[0] + 0x4000;
                break;
            case 0x8:
                Memory::prgBank[0] = prgRom;
                Memory::prgBank[1] = &prgRom[(val & 0xf) * 0x4000];
                break;
            default:
                Memory::prgBank[0] = &prgRom[(val & 0xf) * 0x4000];
                Memory::prgBank[1] = &prgRom[(rom->header.prom - 1) * 0x4000];
                break;
        }
    }

    inline void writeControlRegister(u8 val)
    {
        /* Write the control register. */
        _controlRegister = val;
        /* Change name table mirroring. */
        switch (_controlRegister & 0x3) {
            case 0: N2C02::set1ScreenMirroring(0); break;
            case 1: N2C02::set1ScreenMirroring(1); break;
            case 2: N2C02::setVerticalMirroring(); break;
            default: N2C02::setHorizontalMirroring(); break;
        }
        /* Change PRG bank switching mode. */
        writePrgBankRegister(_prgBankRegister);
        /* Change CHR bank switching mode. */
        writeChrBank0Register(_chrBank0Register);
        writeChrBank1Register(_chrBank1Register);
    }
};

static Mapper *createMMC1(Rom *rom) {
    return new MMC1(rom);
}

/**
 * Install the NROM mapper.
 */
static void insert(void) __attribute__((constructor));
static void insert(void)
{
    mappers[1] = createMMC1;
}

