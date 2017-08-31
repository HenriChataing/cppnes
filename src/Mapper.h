
#ifndef _MAPPER_H_INCLUDED_
#define _MAPPER_H_INCLUDED_

#include <string>

#include "Rom.h"
#include "type.h"

class Rom;

class Mapper
{
public:
    ~Mapper() {}

    virtual void storePrg(u16 addr, u8 val) = 0;
    void storeChr(u16 addr, u8 val);

    Rom *rom;
    const std::string name;

protected:
    Mapper(Rom *rom, const std::string name);

    /**
     * @brief Swap a CHR-ROM bank.
     * @param nr        Bank number
     * @param bank      Bank data buffer
     */
    void swapChrRomBank(int nr, u8 *bank);

    u8 *_chrRom[2];
    bool _chrRam;
};

typedef Mapper* (*MapperConstructor)(Rom *rom);

extern MapperConstructor mappers[];
extern Mapper *currentMapper;

#endif /* _MAPPER_H_INCLUDED_ */
