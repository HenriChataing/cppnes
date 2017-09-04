
#include <cstdlib>
#include <cstring>
#include <iostream>

#include "Rom.h"
#include "Mapper.h"
#include "Memory.h"
#include "N2C02State.h"
#include "M6502State.h"

using namespace std::placeholders;

class MMC3: public Mapper
{
public:
    /* CHR-ROM memory of 0x2000 bytes, with 0x200 byte banks. */
    BankMemory<13, 10> chrRom;

    MMC3(Rom *rom) : Mapper(rom, "MMC3") {
        /* Define ROM geometry. */
        Memory::prgBankSize = 0x2000;
        Memory::prgBankMask = 0x1fff;
        Memory::prgBankShift = 13;
        Memory::prgBankMax = 3;

        /* Test presence of PRG-RAM */
        if (rom->prgRam == NULL)
            throw "MMC3: Missing PRG-RAM";

        Memory::prgRam = rom->prgRam;
        Memory::prgRamEnabled = true;
        Memory::prgRamWriteProtected = true;

        /* Allocate CHR-RAM */
        if (rom->chrRom == NULL) {
            rom->chrRom = new u8[0x2000];
            rom->header.crom = 1;
            if (rom->chrRom == NULL)
                throw "MMC3: Cannot allocate CHR-RAM";
            chrRom.readOnly = false;
        } else
            chrRom.readOnly = true;

        chrRom.readOnly = true;
        chrRom.squash = Memory::chrRom;
        chrRom.source = rom->chrRom;

        /* PRG-ROM bank size is 8Kb in this mapper, and CHR-ROM 1Kb. */
        rom->header.prom *= 2;
        rom->header.crom *= 8;

        /* Initial banks. */
        _bankSelectRegister = 0x00;
        setupBankRegisters();

        /* Install a callback for the scanline counter. */
        N2C02::setScanlineCallback(std::bind(&MMC3::ppuCallback, this, _1, _2));
    }

    void storePrg(u16 addr, u8 val)
    {
        if (addr < 0xa000) {
            if (addr & 0x1) {
                /* Bank data. */
                writeBankDataRegister(val);
            } else {
                /* Bank select. */
                writeBankSelectRegister(val);
            }
        }
        else
        if (addr < 0xc000) {
            if (addr & 0x1) {
                /* PRG-RAM protect. */
                _prgRamProtectRegister = val;
                Memory::prgRamEnabled = (val & 0x80) != 0;
                Memory::prgRamWriteProtected = (val & 0x40) != 0;
            } else {
                /* Mirroring control. */
                _mirroringRegister = val;
                if (val & 0x1)  N2C02::setHorizontalMirroring();
                else            N2C02::setVerticalMirroring();
            }
        }
        else
        if (addr < 0xe000) {
            if (addr & 0x1) {
                /* IRQ latch */
                _irqLatch = val;
                _irqReload = 1;
            } else
                /* IRQ reload */
                _irqCounter = _irqLatch;
        }
        else {
            if (addr & 0x1)
                /* IRQ enable */
                _irqEnabled = 1;
            else
                /* IRQ disable */
                _irqEnabled = 0;
        }
    }

    void storeChr(u16 addr, u8 val) {
        chrRom.store(addr, val);
    }

private:
    u8 _bankSelectRegister = 0;
    u8 _bankRegister[8] = { 0 };
    u8 _prgRamProtectRegister = 0;
    u8 _mirroringRegister = 0;
    u8 _irqLatch = 0;
    u8 _irqCounter = 0;

    bool _irqEnabled = 0;
    bool _irqReload = 0;

    u8 *_chrBank[8];

    void setupBankRegisters(void)
    {
        /* Setup CHR banks */
        if ((_bankSelectRegister & 0x80) == 0) {
            chrRom.swapBank(0, _bankRegister[0] & 0xfe);
            chrRom.swapBank(1, _bankRegister[0] | 0x01);
            chrRom.swapBank(2, _bankRegister[1] & 0xfe);
            chrRom.swapBank(3, _bankRegister[1] | 0x01);
            chrRom.swapBank(4, _bankRegister[2]);
            chrRom.swapBank(5, _bankRegister[3]);
            chrRom.swapBank(6, _bankRegister[4]);
            chrRom.swapBank(7, _bankRegister[5]);
        } else {
            chrRom.swapBank(0, _bankRegister[2]);
            chrRom.swapBank(1, _bankRegister[3]);
            chrRom.swapBank(2, _bankRegister[4]);
            chrRom.swapBank(3, _bankRegister[5]);
            chrRom.swapBank(4, _bankRegister[0] & 0xfe);
            chrRom.swapBank(5, _bankRegister[0] | 0x01);
            chrRom.swapBank(6, _bankRegister[1] & 0xfe);
            chrRom.swapBank(7, _bankRegister[1] | 0x01);
        }

        /* Setup PRG banks */
        Memory::prgBank[1] = &rom->prgRom[_bankRegister[7] * 0x2000];
        Memory::prgBank[3] = &rom->prgRom[(rom->header.prom - 1) * 0x2000];
        if ((_bankSelectRegister & 0x40) == 0) {
            Memory::prgBank[0] = &rom->prgRom[_bankRegister[6] * 0x2000];
            Memory::prgBank[2] = &rom->prgRom[(rom->header.prom - 2) * 0x2000];
        } else {
            Memory::prgBank[0] = &rom->prgRom[(rom->header.prom - 2) * 0x2000];
            Memory::prgBank[2] = &rom->prgRom[_bankRegister[6] * 0x2000];
        }
    }

    void writeBankSelectRegister(u8 val)
    {
        u8 old = _bankSelectRegister;
        _bankSelectRegister = val;

        /* Check whether the write changes the mapping options. */
        if ((val & 0xc0) != (old & 0xc0)) {
            setupBankRegisters();
        }
    }

    void writeBankDataRegister(u8 val)
    {
        _bankRegister[_bankSelectRegister & 0x7] = val;
        setupBankRegisters();
    }

    /**
     * Callback attached to the PPU to count scanlines.
     */
    void ppuCallback(int scanline, int tick)
    {
        (void)scanline; (void)tick;
        if (_irqReload) {
            _irqCounter = _irqLatch;
            _irqReload = 0;
        }

        if (_irqCounter)
            _irqCounter--;

        if (_irqCounter == 0) {
            M6502::currentState->irq = _irqEnabled;
            _irqCounter = _irqLatch;
        }
    }
};

static Mapper *createMMC3(Rom *rom) {
    return new MMC3(rom);
}

/**
 * Install the NROM mapper.
 */
static void insert(void) __attribute__((constructor));
static void insert(void)
{
    mappers[4] = createMMC3;
}
