
#ifndef _ROM_H_INCLUDED_
#define _ROM_H_INCLUDED_

#include "Mapper.h"
#include "type.h"

struct header {
    u8 nes[4];    /* Should contain the string 'NES\n'. */
    u8 prom;      /* Number of PRG-ROM banks. */
    u8 crom;      /* Number of CHR-ROM banks. */
    u8 ctrl[2];   /* Two ROM control bytes. */
    u8 pram;      /* Number of RAM banks (0 is still treated as 1). */
    u8 unused[7]; /* Unused (all zeros). */
};

class Rom
{
    public:
        /**
         * Load a game cartridge from the provided path.
         * @param file          path to the nes cartridge file
         */
        Rom(const char *file);
        ~Rom();

        struct header header;
        u8 *prgRam;
        u8 *chrRom;
        u8 *prgRom;
};

extern Rom *currentRom;

#endif /* _ROM_H_INCLUDED_ */
