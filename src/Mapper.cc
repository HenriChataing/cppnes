
#include <cstring>

#include "Memory.h"
#include "Mapper.h"

Mapper::Mapper(Rom *rom, const std::string name)
    : rom(rom), name(name), _chrRam(false)
{
    _chrRom[0] = NULL;
    _chrRom[1] = NULL;
}

void Mapper::storeChr(u16 addr, u8 val)
{
    if (_chrRam) {
        if (_chrRom[0] == _chrRom[1]) {
            _chrRom[0][addr & 0xfff] = val;
            Memory::chrRom[addr & 0xfff] = val;
            Memory::chrRom[0x1000 + (addr & 0xfff)] = val;
        }
        else
        if (addr < 0x1000) {
            _chrRom[0][addr] = val;
            Memory::chrRom[addr] = val;
        }
        else {
            _chrRom[1][addr & 0xfff] = val;
            Memory::chrRom[addr] = val;
        }
    }
}

/**
 * @brief Swap a CHR-ROM bank.
 * @param nr        Bank number
 * @param bank      Bank data buffer
 */
void Mapper::swapChrRomBank(int nr, u8 *bank)
{
    if (_chrRom[nr] != bank) {
        memcpy(&Memory::chrRom[nr * 0x1000], bank, 0x1000);
        _chrRom[nr] = bank;
    }
}
