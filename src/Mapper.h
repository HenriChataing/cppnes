
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
    virtual void storeChr(u16 addr, u8 val) = 0;

    Rom *rom;
    const std::string name;

protected:
    Mapper(Rom *rom, const std::string name) : rom(rom), name(name) { }
};

typedef Mapper* (*MapperConstructor)(Rom *rom);

extern MapperConstructor mappers[];
extern Mapper *currentMapper;

#endif /* _MAPPER_H_INCLUDED_ */
