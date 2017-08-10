
#ifndef _ROM_H_INCLUDED_
#define _ROM_H_INCLUDED_

#include "Mapper.h"
#include "type.h"

#define NES_CTRL0_MIRROR_V      (1 << 0)
#define NES_CTRL0_PRAM          (1 << 1)
#define NES_CTRL0_TRAILER       (1 << 2)
#define NES_CTRL0_MIRROR_4SCR   (1 << 3)

struct Header {
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

        Header header;
        u8 *prgRam;
        u8 *chrRom;
        u8 *prgRom;
};

extern Rom *currentRom;

#endif /* _ROM_H_INCLUDED_ */
