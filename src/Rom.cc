
#include <cstdlib>
#include <iostream>
#include <iomanip>

#include <sys/mman.h>

#include "Rom.h"
#include "exception.h"

/**
 * The iNes file format is organized as follow:
 *  - header: 16 bytes
 *  - trainer: 0 or 512 bytes
 *  - PRGROM: (16384 . prom) bytes
 *  - CHRROM: (8192 . crom) bytes
 */

/**
 * Loaded ROM file.
 */
Rom *currentRom;

/**
 * Loaded mapper.
 */
Mapper *currentMapper;

/**
 * Array of supported mappers.
 */
MapperConstructor mappers[256];

/**
 * Load a game cartridge from the provided path.
 * @param file          path to the nes cartridge file
 */
Rom::Rom(const char *file)
{
    FILE *fd = fopen(file, "r");
    if (fd == NULL)
        throw std::invalid_argument("Cannot read from file");

    size_t n;
    int r = 0;
    u8 *prom = NULL, *crom = NULL, *pram = NULL;
    u8 mapperType;

    n = fread((void *)&header, sizeof(struct Header), 1, fd);
    if (n == 0)
        goto fail;

    /* Check file format. */
    if (header.nes[0] != 'N' || header.nes[1] != 'E' ||
        header.nes[2] != 'S' || header.nes[3] != 0x1a)
        goto fail;

    /* Dump header. */
    std::cerr << "Detected iNES ROM" << std::hex << std::endl;
    std::cerr << "  prom ..... " << std::setfill('0') << std::setw(2);
    std::cerr << (int)header.prom << std::endl;
    std::cerr << "  crom ..... " << std::setfill('0') << std::setw(2);
    std::cerr << (int)header.crom << std::endl;
    std::cerr << "  ctrl0 .... " << std::setfill('0') << std::setw(2);
    std::cerr << (int)header.ctrl[0] << std::endl;
    std::cerr << "  ctrl1 .... "  << std::setfill('0') << std::setw(2);
    std::cerr << (int)header.ctrl[1] << std::endl;
    std::cerr << "  pram ..... " << std::setfill('0') << std::setw(2);
    std::cerr << (int)header.pram << std::endl;

    if (header.pram == 0)
        header.pram = 1;

    /* Skip the trainer if present. */
    if (header.ctrl[0] & NES_CTRL0_TRAILER) {
        std::cerr << "Skipping trailer" << std::endl;
        fseek(fd, 512, SEEK_CUR);
    }

    /* Detect nametable mirroring type. */
    if (header.ctrl[0] & NES_CTRL0_MIRROR_V) {
        std::cerr << "Vertical mirroring" << std::endl;
    } else if (header.ctrl[0] & NES_CTRL0_MIRROR_4SCR) {
        std::cerr << "4-Screen VRAM" << std::endl;
    } else {
        std::cerr << "Horizontal mirroring" << std::endl;
    }

    /* Extract mapper type. */
    mapperType = (header.ctrl[1] & 0xf0) | (header.ctrl[0] >> 4);
    if (mappers[mapperType] == NULL) {
        std::cerr << "Unsupported mapper type " << (int)mapperType << std::endl;
        goto fail;
    } else {
        std::cerr << "Selected mapper " << (int)mapperType << std::endl;
    }

    /* Allocate space for the cartridge memory. */
    prom = new u8[header.prom * 0x4000];
    pram = new u8[header.pram * 0x2000];

    if (prom == NULL || pram == NULL)
        goto fail;

    if (header.crom > 0) {
        crom = new u8[header.crom * 0x2000];
        if (crom == NULL)
            goto fail;
    } else
        crom = NULL;

    /* Load the PRG-ROM bank(s). */
    std::cerr << "Loading " << (int)header.prom << " PRG-ROM 16Kb bank";
    if (header.prom > 1)
        std::cerr << "s";
    std::cerr << std::endl;

    r = fread(prom, header.prom * 0x4000 * sizeof(u8), 1, fd);
    if (r == 0)
        goto fail;

    /* Load the CHR-ROM bank(s). */
    if (header.crom > 0) {
        std::cerr << "Loading " << (int)header.crom << " CHR-ROM 8Kb bank";
        if (header.crom > 1)
            std::cerr << "s";
        std::cerr << std::endl;

        r = fread(crom, header.crom * 0x2000 * sizeof(u8), 1, fd);
        if (r == 0)
            goto fail;
    }

    /* Setup up the ROM object and install the selected mapper. */
    prgRom = prom;
    prgRam = pram;
    chrRom = crom;
    currentMapper = mappers[mapperType](this);
    return;

fail:
    delete prom;
    delete crom;
    delete pram;
    fclose(fd);
    throw InvalidRom();
}
