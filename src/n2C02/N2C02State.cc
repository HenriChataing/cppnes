
#include <iostream>
#include <pthread.h>
#include <SDL2/SDL.h>
#include <sys/time.h>

#include "N2C02State.h"
#include "M6502State.h"
#include "Memory.h"
#include "Rom.h"
#include "Timer.h"

using namespace N2C02;

#define N2C02_FRAME_RATE        (60.10f)
#define N2C02_FRAME_MS          ((unsigned long)(1000.f / N2C02_FRAME_RATE))

#define PPU_VBLOCKS             (30)
#define PPU_HBLOCKS             (32)
#define PPU_WIDTH               (PPU_HBLOCKS * 8)
#define PPU_HEIGHT              (PPU_VBLOCKS * 8)

#ifndef PPU_DEBUG
#define SCREEN_WIDTH            (PPU_WIDTH * 2)
#define SCREEN_HEIGHT           (PPU_HEIGHT * 2)
#else
#define SCREEN_WIDTH            (PPU_WIDTH * 4)
#define SCREEN_HEIGHT           (PPU_HEIGHT * 2)
#endif

#define VADDR_COARSE_X_MASK     (0x1f << 0)
#define VADDR_COARSE_X_MAX      (0x1f << 0)
#define VADDR_COARSE_Y_MASK     (0x1f << 5)
#define VADDR_COARSE_Y_MAX      (0x1d << 5)
#define VADDR_COARSE_Y_INCR     (0x1 << 5)
#define VADDR_FINE_Y_MASK       (0x7 << 12)
#define VADDR_FINE_Y_INCR       (0x1 << 12)
#define VADDR_NT_H_MASK         (0x1 << 10)
#define VADDR_NT_V_MASK         (0x1 << 11)
#define VADDR_NT_SHIFT          10
#define VADDR_NT_MASK           (VADDR_NT_H_MASK | VADDR_NT_V_MASK)
#define VADDR_X_MASK (VADDR_NT_H_MASK | VADDR_COARSE_X_MASK)
#define VADDR_Y_MASK (VADDR_NT_V_MASK | VADDR_COARSE_Y_MASK | VADDR_FINE_Y_MASK)

#define SPRITE_ATTR_PL  (0x3)
#define SPRITE_ATTR_ZO  (1 << 4)
#define SPRITE_ATTR_PR  (1 << 5)
#define SPRITE_ATTR_HF  (1 << 6)
#define SPRITE_ATTR_VF  (1 << 7)

#define PPUCTRL     0
#   define PPUCTRL_NN   (0x3)
#   define PPUCTRL_I    (1 << 2)
#   define PPUCTRL_S    (1 << 3)
#   define PPUCTRL_B    (1 << 4)
#   define PPUCTRL_H    (1 << 5)
#   define PPUCTRL_V    (1 << 7)
#define PPUMASK     1
#   define PPUMASK_G    (1 << 0)
#   define PPUMASK_m    (1 << 1)
#   define PPUMASK_M    (1 << 2)
#   define PPUMASK_b    (1 << 3)
#   define PPUMASK_s    (1 << 4)
#define PPUSTATUS   2
#   define PPUSTATUS_O  (1 << 5)
#   define PPUSTATUS_S  (1 << 6)
#   define PPUSTATUS_V  (1 << 7)
#define OAMADDR     3
#define OAMDATA     4
#define PPUSCROLL   5
#define PPUADDR     6
#define PPUDATA     7

#define PRERENDER   (currentState->scanline == 261)
#define POSTRENDER  (currentState->scanline == 240)
#define RENDEROFF   (!currentState->mask.br && !currentState->mask.sr)
#define RENDERON    (currentState->mask.br || currentState->mask.sr)
#define VBLANK      (currentState->scanline > 240)

/**
 * @brief Interleave the bits of two byte values.
 * @param b0    first byte
 * @param b1    second byte
 * @return      the short value with the bits `b1[7]b0[7] ... b0[0]`
 */
static u16 interleave(u8 b0, u8 b1);

/** 4-byte sprite definition. */
struct sprite {
    union {
        struct {
            u8 y;
            u8 index;
            u8 attr;
            u8 x;
        } val;
        u8 raw[4];
    };
};

/* Callbacks. */
static void (*scanlineCallback)(int, int);

/** SDL objects. */
static SDL_Window *window;
static SDL_Surface *screen;
static uint32_t *pixels;
static Timer fps;

/** SPR-RAM to store sprite attributes. */
static union {
    u8 raw[256];
    struct sprite sprites[64];
} oam;

/** Secondary sprite storage, used for sprite evaluation. */
static struct {
    struct sprite sprites[8];
    u8 bitmap[16];
    u8 cnt;
} oamsec;

/** Sprite registers, used for rendering. */
static struct {
    struct sprite sprites[8];
    u8 bitmap[16];
    u8 cnt;
} oamreg;

/** Allocate nametables. */
static u8 ntable0[0x400];
static u8 ntable1[0x400];
static u8 ntable2[0x400];
static u8 ntable3[0x400];

/** Setup nametable mirroring, which defaults to horizontal. */
static u8 *ntables[4] = {
    ntable0, ntable0, ntable1, ntable1
};

/** Contains the palette memory. */
extern u8 palette[32];

/* RGB color palette. */
extern const u32 colors[64];


namespace N2C02 {

State *currentState;

static u8 load(u16 addr);
static void store(u16 addr, u8 val);

};

/**
 * @brief Setup the PPU memory map and initialise the SDL objects.
 */
State::State()
{
    ctrl.i = 0x1;
}

/**
 * Read from one of the PPU io registers.
 */
u8 State::readRegister(u16 addr)
{
    u8 val = 0x0;

    switch (addr & 0x7)
    {
        case PPUCTRL:
        case PPUMASK:
            val = bus;
            break;

        case PPUSTATUS:
            val = status | (bus & 0x1f);
            status &= ~PPUSTATUS_V;
            regs.w = 0;
            bus = val;
            break;

        case OAMADDR:
            val = bus;
            break;

        case OAMDATA:
            /*
             * Reads during the secondary OAM clear always return $ff
             * (tick 1-64 of the current scanline).
             */
            if (RENDERON && cycle <= 64) {
                val = 0xff;
            }
            /*
             * Reads during vertical or forced blanking return the value from
             * OAM at the address OAMADDR but do not increment.
             */
            else {
                val = oam.raw[oamaddr];
                if (!VBLANK)
                    oamaddr++;
            }
            bus = val;
            break;

        case PPUSCROLL:
        case PPUADDR:
            val = bus;
            break;

        case PPUDATA:
            /*
             * When reading while the VRAM address is in the range 0-$3eff
             * (i.e., before the palettes), the read will return the contents
             * of an internal read buffer. This internal buffer is updated only
             * when reading PPUDATA, and so is preserved across frames. After
             * the CPU reads and gets the contents of the internal buffer, the
             * PPU will immediately update the internal buffer with the u8 at
             * the current VRAM address.
             */
            if (regs.v < 0x3f00) {
                val = readBuffer;
                readBuffer = load(regs.v);
            }
            /*
             * Reading palette data from $3f00-$3fff works differently. The
             * palette data is placed immediately on the data bus, and hence
             * no dummy read is required. Reading the palettes still updates
             * the internal buffer though, but the data placed in it is the
             * mirrored nametable data that would appear "underneath" the
             * palette.
             */
            else {
                val = load(regs.v);
                readBuffer = load(regs.v & 0x2fff);
            }
            regs.v = (regs.v + ctrl.i) & 0x3fff;
            bus = val;
            break;
    }
    return val;
}

/**
 * Write to one of the PPU io registers.
 */
void State::writeRegister(u16 addr, u8 val)
{
    /* Set the value on the internal data bus. */
    bus = val;
    /* Analyse the write effects. */
    switch (addr & 0x7)
    {
        case PPUCTRL:
            regs.t &= ~VADDR_NT_MASK;
            regs.t |= (u16)(val & PPUCTRL_NN) << VADDR_NT_SHIFT;
            ctrl.i = (val & PPUCTRL_I) ? 0x20 : 0x1;
            ctrl.s = (val & PPUCTRL_S) ? 0x1000 : 0x0000;
            ctrl.b = (val & PPUCTRL_B) ? 0x1000 : 0x0000;
            ctrl.h = (val & PPUCTRL_H) != 0;
            ctrl.v = (val & PPUCTRL_V) != 0;
            break;

        case PPUMASK:
            mask.greyscale = (val & PPUMASK_G) != 0;
            mask.bc = (val & PPUMASK_m) == 0;
            mask.sc = (val & PPUMASK_M) == 0;
            mask.br = (val & PPUMASK_b) != 0;
            mask.sr = (val & PPUMASK_s) != 0;
            break;

        case PPUSTATUS:
            break;

        case OAMADDR:
            oamaddr = val;
            break;

        case OAMDATA:
            /*
             * Writes to OAMDATA during rendering (on the pre-render line and
             * the visible lines 0-239, provided either sprite or background
             * rendering is enabled) do not modify values in OAM, but do
             * perform a glitchy increment of OAMADDR, bumping only the
             * high 6 bits.
             */
            if (RENDERON && (scanline <= 239 || PRERENDER))
                oamaddr += 0x4;
            else
                oam.raw[oamaddr++] = val;
            break;

        case PPUSCROLL:
            if (regs.w) {
                regs.t =
                    (regs.t & 0x8c1f) |
                    ((u16)(val & 0x7) << 12) |
                    ((u16)(val & 0xf8) << 2);
                regs.w = 0;
            } else {
                regs.t =
                    (regs.t & 0xffe0) |
                    ((u16)(val >> 3));
                regs.x = val & 0x7;
                regs.w = 1;
            }
            break;

        case PPUADDR:
            if (regs.w) {
                regs.t = (regs.t & 0xff00) | val;
                regs.v = regs.t;
                regs.w = 0;
            } else {
                regs.t =
                    (regs.t & 0x00ff) |
                    ((u16)(val & 0x3f) << 8);
                regs.w = 1;
            }
            break;

        case PPUDATA:
            store(regs.v, val);
            regs.v = (regs.v + ctrl.i) & 0x3fff;
            break;
    }
}

/**
 * @brief Write one data u8 as part of a dma transfer.
 * @param val next data u8
 */
void State::dmaTransfer(u8 val)
{
    oam.raw[currentState->oamaddr++] = val;
}

namespace N2C02 {

int init()
{
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "failed to initialise SDL: " << SDL_GetError();
        std::cerr << std::endl;
        SDL_Quit();
        return -1;
    }

    window = SDL_CreateWindow("cnes 0.1",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
    if (!window) {
        std::cerr << "failed to create SDL window: " << SDL_GetError();
        std::cerr << std::endl;
        quit();
        return -1;
    }

    screen = SDL_GetWindowSurface(window);
    if (!screen) {
        std::cerr << "failed to obtain SDL screen: " << SDL_GetError();
        std::cerr << std::endl;
        quit();
        return -1;
    }
    if (screen->format->BytesPerPixel != 4) {
        std::cerr << "unsupported pixel format\n" << std::endl;
        quit();
        return -1;
    }
    /* Paint it blaaack. */
    pixels = (uint32_t *)screen->pixels;
    memset(pixels, 0, 4 * SCREEN_WIDTH * SCREEN_HEIGHT);
    SDL_UpdateWindowSurface(window);

    /* Setup name table mirroring. */
    if (currentRom->header.ctrl[0] & NES_CTRL0_MIRROR_4SCR)
        set4ScreenMirroring();
    else if (currentRom->header.ctrl[0] & NES_CTRL0_MIRROR_V)
        setVerticalMirroring();
    else
        setHorizontalMirroring();
    return 0;
}

void quit()
{
    if (window) {
        SDL_DestroyWindow(window);
        window = NULL;
    }
    SDL_Quit();
}

/**
 * Setup vertical mirroring for the nametables.
 */
void setVerticalMirroring(void)
{
    ntables[0] = ntable0;
    ntables[1] = ntable1;
    ntables[2] = ntable0;
    ntables[3] = ntable1;
}

/**
 * Setup horizontal mirroring for the nametables.
 */
void setHorizontalMirroring(void)
{
    ntables[0] = ntable0;
    ntables[1] = ntable0;
    ntables[2] = ntable1;
    ntables[3] = ntable1;
}

/**
 * Setup 1-scren mirroring for the nametables.
 */
void set1ScreenMirroring(int upper)
{
    u8 *sel = upper ? ntable1 : ntable0;
    ntables[0] = sel;
    ntables[1] = sel;
    ntables[2] = sel;
    ntables[3] = sel;
}

/**
 * Setup 4 screen mirroring for the nametables.
 */
void set4ScreenMirroring(void)
{
    ntables[0] = ntable0;
    ntables[1] = ntable1;
    ntables[2] = ntable2;
    ntables[3] = ntable3;
}

/**
 * Attach a callback called at the end of each scanline.
 */
void setScanlineCallback(void (*callback)(int, int))
{
    scanlineCallback = callback;
}

/**
 * Load a byte from an address in the PPU memory address space.
 * @param addr          absolute memory address
 * @return              the value read from the address [addr]
 */
static u8 load(u16 addr)
{
    if (addr < 0x2000)
        return Memory::chrRom[addr];
    else
    if (addr < 0x3f00)
        return ntables[(addr >> 10) & 0x3][addr & 0x3ff];
    else
    if ((addr & 0x3) == 0)
        return palette[addr % 16];
    else
        return palette[addr % 32];
    return 0;
}

/**
 * Store a byte at an address in the CPU memory address space.
 * @param addr          absolute memory address
 * @param val           written value
 */
static void store(u16 addr, u8 val)
{
    if (addr < 0x2000)
        currentMapper->storeChr(addr, val);
    else
    if (addr < 0x3f00)
        ntables[(addr >> 10) & 0x3][addr & 0x3ff] = val;
    else
    if ((addr & 0x3) == 0)
        palette[addr % 16] = val;
    else
        palette[addr % 32] = val;
}

/* ========================================================================== *
 *  Rendering                                                                 *
 * ========================================================================== */

/**
 * Description of the shift registers, copied from
 *  http://wiki.nesdev.com/w/index.php/PPU_rendering
 *
 *                                       [BBBBBBBB] - Next tile's pattern data,
 *                                       [BBBBBBBB] - 2 bits per pixel
 *                                        ||||||||
 *                                        vvvvvvvv
 *    Serial-to-parallel - [AAAAAAAA] <- [BBBBBBBB] - Parallel-to-serial
 *       shift registers - [AAAAAAAA] <- [BBBBBBBB] - shift registers
 *                          vvvvvvvv
 *                          ||||||||       [Sprites 0..7]----+
 *                          ||||||||                         |
 * [fine_x selects a ]---->[  Mux   ]--------------->[Priority mux]----->[Pixel]
 *         bit              ||||||||
 *                          ^^^^^^^^
 *                         [PPPPPPPP] <- [1-bit latch]
 *                         [PPPPPPPP] <- [1-bit latch]
 *                                              ^
 *                                              |
 *               [2-bit Palette Attribute for next tile (from attribute table)]
 */

/**
 * @brief Draw a single pixel with the given color index.
 * @param x,y pixel coordinates
 * @param c color index
 */
static inline void drawPixel(int x, int y, uint32_t c)
{
    if (x < 0 || y < 0 ||
        x >= 8 * PPU_HBLOCKS ||
        y >= 8 * PPU_VBLOCKS)
        return;
    x *= 2;
    y *= 2 * SCREEN_WIDTH;
    int z = x + y;
    pixels[z] = c;
    pixels[z + 1] = c;
    pixels[z + SCREEN_WIDTH] = c;
    pixels[z + SCREEN_WIDTH + 1] = c;
}

/**
 * @brief Refresh the game window.
 */
static inline void flushScreen(void)
{
    SDL_UpdateWindowSurface(window);
}

/**
 * @brief Shift all the registers.
 */
static inline void shiftRegisters(void)
{
    currentState->regs.pats <<= 2;
    currentState->regs.pals = (currentState->regs.pals << 2) | currentState->regs.pall;
}

/**
 * @brief Shift all the registers eight times.
 */
static inline void shiftRegisters8(void)
{
    currentState->regs.pats <<= 16;
    currentState->regs.pals = currentState->regs.pall * 0x5555;
}

/**
 * @brief Draw the pixel currently represented by the shift registers.
 */
static inline void drawNextPixel(void)
{
    /* Pixel data. */
    unsigned int px = currentState->cycle - 1, py = currentState->scanline;

    /* Early return when both background and sprite rendering are disabled. */
    if (!currentState->mask.br && !currentState->mask.sr) {
        drawPixel(px, py, 0x0);
        shiftRegisters();
        return;
    }

    /*
     * Early return when in first column and both background and sprite
     * clipping are activated.
     */
    if (px < 8 && currentState->mask.bc && currentState->mask.sc) {
        drawPixel(px, py, 0x0);
        shiftRegisters();
        return;
    }

    /* Background and sprite pixels. */
    u16 bpx = 0x0;
    u16 spx = 0x0;
    u8 zero = 0, front = 0, pal = 0x10;

    /* Background rendering. */
    if (currentState->mask.br && (!currentState->mask.bc || px >= 8)) {
        bpx  = (currentState->regs.pats >> (30 - 2 * currentState->regs.x)) & 0x3;
    }
    /* Sprite rendering. */
    if (currentState->mask.sr && (!currentState->mask.sc || px >= 8)) {
        for (unsigned int n = 0; n < oamreg.cnt; n++) {
            struct sprite sprite = oamreg.sprites[n];
            unsigned int sx = sprite.val.x;
            if (px >= sx && px < sx + 8) {
                unsigned int rx;
                if (sprite.val.attr & SPRITE_ATTR_HF)
                    rx = px - sx;
                else
                    rx = sx + 7 - px;
                spx  = (oamreg.bitmap[2 * n] >> rx) & 0x1;
                spx |= ((oamreg.bitmap[2 * n + 1] >> rx) << 1) & 0x2;
                if (spx != 0) {
                    front = (sprite.val.attr & SPRITE_ATTR_PR) == 0;
                    pal |= (sprite.val.attr & SPRITE_ATTR_PL) << 2;
                    zero = sprite.val.attr & SPRITE_ATTR_ZO;
                    break;
                }
            }
        }
    }

    /* Sprite 0 hit. */
    if (spx != 0 && bpx != 0 && zero && px < 255) {
        currentState->status |= PPUSTATUS_S;
    }

    /* Craft the final palette address. */
    u16 pa = 0x0;
    if (spx != 0 && (front || bpx == 0)) {
        pa = pal | spx;
    } else
    if (bpx != 0) {
        pa = bpx;
        pa |= (currentState->regs.pals >> (14 - 2 * currentState->regs.x) << 2) & 0xc;
    }

    u8 c = palette[pa];
    drawPixel(px, py, colors[c]);
    shiftRegisters();
}

/**
 * @brief Fetch the name table u8 for the current x, y coordinates.
 */
static inline void fetchPattern(void)
{
    currentState->regs.nt = ntables[(currentState->regs.v >> 10) & 0x3][currentState->regs.v & 0x3ff];
}

/**
 * @brief Fetch the attribute table u8 for the current x, y coordinates. The
 *  palette latch is updated at the same time.
 */
static inline void fetchAttribute(void)
{
    /*
     * The address of the attribute to fetch is constructed as follow:
     *
     * 10 NN 1111 YYY XXX
     * || || |||| ||| +++-- high 3 bits of coarse X (x/4)
     * || || |||| +++------ high 3 bits of coarse Y (y/4)
     * ++-||-++++---------- attribute offset 0x23c0
     *    ++--------------- nametable select
     */
    u16 pa =
        ((currentState->regs.v >> 2) & 0x0007) |
        ((currentState->regs.v >> 4) & 0x0038) |
        (currentState->regs.v & VADDR_NT_MASK) |
        0x23c0;
    u8 fullat = ntables[(pa >> 10) & 0x3][pa & 0x3ff];
    /*
     * The bits of the attribute u8 that corrspond to the current tile are
     * extracted at the shift composed of the bit 1 of the coarse
     * x,y coordinates.
     */
    u8 shift = (currentState->regs.v & 0x2) | ((currentState->regs.v >> 4) & 0x4);
    currentState->regs.at = fullat >> shift;
}

/**
 * @brief Fetch the bitmap line for the fetched name table pattern. The bitmap
 *  shift registers are updated at the same time.
 */
static inline void fetchBitmap(void)
{
    u16 pa = currentState->ctrl.b + ((currentState->regs.nt << 4) | (currentState->regs.v >> 12));
    u16 lo = Memory::chrRom[pa];
    u16 hi = Memory::chrRom[pa | 0x8];
    currentState->regs.pats = (currentState->regs.pats & 0xffff0000) | interleave(lo, hi);
    currentState->regs.pall = currentState->regs.at & 0x3;;
}

/**
 * @brief Increment the coarse X coordinate. The name table is changed on
 *  wrap around and the coarse X offset reset.
 */
static inline void incrCoarseX(void)
{
    if ((currentState->regs.v & VADDR_COARSE_X_MASK) == VADDR_COARSE_X_MAX) {
        currentState->regs.v &= ~VADDR_COARSE_X_MASK;
        currentState->regs.v ^= VADDR_NT_H_MASK;
    } else
        currentState->regs.v++;
}

/**
 * @brief Increment the fine Y coordinate. The name table is changed on wrap
 *  around, and the fine and corse Y offsets are both reset.
 */
static inline void incrFineY(void)
{
    /* No block change. */
    if ((currentState->regs.v & VADDR_FINE_Y_MASK) != VADDR_FINE_Y_MASK)
        currentState->regs.v += VADDR_FINE_Y_INCR;
    /* Increment coarse Y with no wrap around. */
    else if ((currentState->regs.v & VADDR_COARSE_Y_MASK) != VADDR_COARSE_Y_MAX)
        currentState->regs.v = (currentState->regs.v & ~VADDR_FINE_Y_MASK) + VADDR_COARSE_Y_INCR;
    /* Increment coarse Y with wrap around. */
    else {
        currentState->regs.v &= ~(VADDR_COARSE_Y_MASK | VADDR_FINE_Y_MASK);
        currentState->regs.v ^= VADDR_NT_V_MASK;
    }
}

/**
 * @brief Clear the secondary OAM memory in preparation for the scaline
 *  rendering.
 */
static inline void clearSecondaryOAM(void)
{
    memset(oamsec.sprites, 0xff, sizeof(oamsec.sprites));
    oamsec.cnt = 0;
}

/**
 * @brief Complete sprite evaluation.
 */
static void evaluateSprites(void)
{
    int n, m;
    u8 y;
    clearSecondaryOAM();
    /// FIXME should start at OAMADDR/4
    for (n = 0; n < 64; n++)
    {
        /* Check if sprite in vertical range. */
        y = oam.sprites[n].val.y;
        if (currentState->ctrl.h) {
            if (currentState->scanline < (uint)y ||
                currentState->scanline >= (uint)y + 16) {
                continue;
            }
        } else if (currentState->scanline < (uint)y ||
                   currentState->scanline >= (uint)y + 8) {
            continue;
        }
        /* Copy sprite to secondary OAM. */
        oamsec.sprites[oamsec.cnt].val = oam.sprites[n].val;
        if (n == 0)
            oamsec.sprites[oamsec.cnt].val.attr |= SPRITE_ATTR_ZO;
        oamsec.cnt++;
        if (oamsec.cnt >= 8) {
            n++;
            goto overflow;
        }
    }
    /* Completed without sprite overflow. */
    return;

    /*
     * Check for sprite overflow. The evaluation contains an hardware bug
     * whereby ALL sprite fields (and not just y) are tested for vertical
     * range.
     */
overflow:
    m = 0;
    while (n < 64) {
        y = oam.sprites[n].raw[m];
        if (currentState->scanline < (uint)y ||
            currentState->scanline >= (uint)y + 8) {
            n++;
            m = (m + 1) % 4; /* The bug is here. */
        } else {
            /* n += (m == 3) + 1; */
            currentState->status |= PPUSTATUS_O;
            break;
        }
    }
}

/**
 * Fetch the bitmaps for the secondary OAM sprites.
 */
static inline void fetchSpriteBitmap(void)
{
    int n = currentState->cycle / 8 - 33;
    u16 offset = currentState->scanline - oamsec.sprites[n].val.y;
    u16 pa;

    /*
     * For 8 x 16 sprites, the pattern table is selected by the bit 0 of the
     * sprite index.
     */
    if (currentState->ctrl.h) {
        pa = (oamsec.sprites[n].val.index & 0x1) * 0x1000;
        pa += (oamsec.sprites[n].val.index & ~0x1) * 16;

        /* Vertical flip */
        if (oamsec.sprites[n].val.attr & SPRITE_ATTR_VF) {
            /* Swap the high and low sprites and the sprite lines */
            if (offset >= 8)
                pa = pa + 15 - offset;
            else
                pa = pa + 23 - offset;
        } else {
            if (offset >= 8)
                pa = pa + 8 + offset;
            else
                pa = pa + offset;
        }
    }
    /*
     * The sprite pattern table is otherwise selected by the bit 3 of the
     * PPU control register.
     */
    else {
        pa = currentState->ctrl.s;
        pa += oamsec.sprites[n].val.index * 16;
        /* Vertical flip. */
        if (oamsec.sprites[n].val.attr & SPRITE_ATTR_VF)
            pa = pa + 7 - offset;
        else
            pa = pa + offset;
    }

    oamreg.bitmap[2 * n] = Memory::chrRom[pa];
    oamreg.bitmap[2 * n + 1] = Memory::chrRom[pa + 8];
    oamreg.sprites[n].val = oamsec.sprites[n].val;
    oamreg.cnt = oamsec.cnt;
}

static void prerenderBackground(void)
{
    u16 v = currentState->regs.t;
    int fx, fy, cx, cy, nt, ntp;
    fx = currentState->regs.x;
    fy = (currentState->regs.v >> 12) & 0x7;
    cx = v & 0x1f;
    cy = (v >> 5) & 0x1f;
    nt = (v >> 10) & 0x3;

    int t = v & 0x3ff;
    for (int ty = 0; ty <= PPU_VBLOCKS; ty++) {
        if (ty + cy == PPU_VBLOCKS) {
            nt ^= 2;
            t &= 0x1f;
        }
        ntp = nt;
        for (int tx = 0; tx <= PPU_HBLOCKS; tx++) {
            u8 pat = ntables[ntp][t];
            u8 att =
                ntables[ntp][0x3c0 | ((t >> 4) & 0x38) | ((t >> 2) & 0x7)];
            int shift = (t & 0x2) | ((t >> 4) & 0x4);
            att >>= shift;
            att &= 0x3;

            u16 pa = currentState->ctrl.b + (pat << 4);
            for (int j = 0; j < 8; j++) {
                u16 lo = Memory::chrRom[pa];
                u16 hi = Memory::chrRom[pa | 0x8];
                for (int i = 0; i < 8; i++) {
                    int p =
                        ((lo >> (7 - i)) & 0x1) |
                        (((hi >> (7 - i)) << 1) & 0x2);
                    u8 c = p ? palette[(att << 2) | p] : palette[0];
                    drawPixel(tx * 8 + i - fx, ty * 8 + j - fy, colors[c]);
                }
                pa++;
            }

            if (tx + cx == (PPU_HBLOCKS - 1)) {
                ntp ^= 1;
                t &= 0x3e0;
            } else {
                t++;
            }
        }
        t = (t + 0x20) - 1;
    }
}

#if 0
/**
 * @brief Pre-render the visible sprites.
 */
static void prerenderSprites(void)
{
    int vsize = currentState->ctrl.h ? 16 : 8;
    u16 pt, ptl;
    int px, py;
    u8 index, attr;
    u8 zero = SPRITE_ATTR_ZO;

    /* Wipe pre-render. */
    memset(spritePrerender, 0, sizeof(spritePrerender));

    /* Draw some sprites (backward iteration to respect priority). */
    for (int n = 0; n < 64; n++) {
        px = oam.sprites[n].val.x;
        py = oam.sprites[n].val.y + 1;
        index = oam.sprites[n].val.index;
        attr = oam.sprites[n].val.attr;

        if (py >= 8 * PPU_VBLOCKS)
            continue;
        if (currentState->ctrl.h) {
            pt = (index & 0x1) * 0x1000;
            pt += (index & ~0x1) * 16;
        } else {
            pt = currentState->ctrl.s + index * 16;
        }

        for (int line = 0; line < vsize; line++) {
            if (py + line >= 8 * PPU_VBLOCKS)
                break;
            /* Patch pt for vertical flipping */
            if (currentState->ctrl.h) {
                if (attr & SPRITE_ATTR_VF) {
                    /* Swap the high and low sprites and the sprite lines */
                    if (line >= 8)
                        ptl = pt + 15 - line;
                    else
                        ptl = pt + 23 - line;
                } else {
                    if (line >= 8)
                        ptl = pt + 8 + line;
                    else
                        ptl = pt + line;
                }
            } else {
                if (attr & SPRITE_ATTR_VF) {
                    ptl = pt + 7 - line;
                } else {
                    ptl = pt + line;
                }
            }

            u8 lo, hi;
            lo = load(ptl);
            hi = load(ptl + 8);
            for (int col = 7; col >= 0; col--) {
                int p;
                if (attr & SPRITE_ATTR_HF)
                    p = ((lo >> (7-col)) & 0x1) |
                        (((hi >> (7-col)) << 1) & 0x2);
                else
                    p = ((lo >> col) & 0x1) |
                        (((hi >> col) << 1) & 0x2);
                if (p) {
                    u16 ca = 0x3f10;
                    ca |= (attr & SPRITE_ATTR_PL) << 2;
                    ca |= p;
                    u8 c = load(ca);
                    drawPixel(px + 7 - col, py + line, colors[c]);
                }
            }
        }
    }
}
#endif

static void drawPatternTables(int sx, int sy);
static void drawNameTable(int sx, int sy, int sel);
static void drawAttrTable(int sx, int sy, int sel);
static void drawPalettes(int sx, int sy);
static void drawSprites(void);

/**
 * @brief Draw the next dot on screen (step equivalent).
 */
void dot(void)
{
    /* Odd frame tracking, the last cycle is skipped. */
    static bool oddframe = 0;

    if (scanlineCallback && RENDERON &&
        currentState->scanline < 240 && currentState->cycle == 260)
        scanlineCallback(currentState->scanline, currentState->cycle);

    /*
     * Pre render line is mostly garbage, but the last fetches must be
     * performed, as they represent the data for the first 2 tiles on the next
     * line.
     */
    if (PRERENDER) {
        /* Clear status flags. */
        if (currentState->cycle == 1)
            currentState->status = 0;
        /* Rendering disabled. */
        if (RENDEROFF)
            goto next;
        /* Reload coarse X coordinates. */
        else if (currentState->cycle == 257) {
            if (currentState->cycle == 65)
                evaluateSprites();
            currentState->oamaddr = 0;
            currentState->regs.v =
                (currentState->regs.v & ~VADDR_X_MASK) |
                (currentState->regs.t & VADDR_X_MASK);
        }
        /* Reload Y fine and coarse offsets. */
        else if (currentState->cycle >= 280 && currentState->cycle <= 304) {
            currentState->oamaddr = 0;
            currentState->regs.v =
                (currentState->regs.v & ~VADDR_Y_MASK) |
                (currentState->regs.t & VADDR_Y_MASK);
        }
        /* Sprite loading interval. */
        else if (currentState->cycle >= 258 && currentState->cycle <= 320) {
            currentState->oamaddr = 0;
            if (currentState->cycle % 8 == 0)
                fetchSpriteBitmap();
        }
        /* Pre load two tiles for next line. */
        else if (currentState->cycle >= 322 && currentState->cycle <= 340)
        {
            if (currentState->cycle % 8 == 2)
                fetchPattern();
            else if (currentState->cycle % 8 == 4)
                fetchAttribute();
            else if (currentState->cycle % 8 == 0) {
                shiftRegisters8();
                fetchBitmap();
                incrCoarseX();
            }
        }
    }
    /* Post render line is idle. */
    else if (POSTRENDER)
        goto next;
    /* PPU is idle while VBlank is occuring. */
    else if (VBLANK) {
        /* Set the vblank flag on tick 1 of the scanline 241. */
        if (currentState->scanline == 241 && currentState->cycle == 1) {
            currentState->status |= PPUSTATUS_V;
            M6502::currentState->nmi = currentState->ctrl.v;
#ifdef PPU_DEBUG
            drawPalettes(0, 0);
            drawNameTable(PPU_WIDTH, 0, 0);
            drawNameTable(PPU_WIDTH, PPU_HEIGHT, 1);
            drawAttrTable(PPU_WIDTH / 2, PPU_HEIGHT, 0);
            drawAttrTable(PPU_WIDTH / 2, PPU_HEIGHT + PPU_HEIGHT / 2, 1);
            SDL_UpdateWindowSurface(window);
#endif
            // prerenderBackground();
            if (currentState->mask.br)
                flushScreen();
#ifdef PPU_DEBUG
            /* Display the frame rate. */
#endif
            /* Adjust the frame rate. */
            fps.wait(N2C02_FRAME_MS);
            fps.reset();
        }
    }
    /* Rendering is disabled. */
    else if (RENDEROFF)
        goto next;
    /* Normal scanline. */
    else {
        /* First cycle is idle. */
        if (currentState->cycle == 0)
            goto next;
        /* Next cycles form the visible scanline. */
        else if (currentState->cycle <= 256) {
            /* Draw a pixel with previously loaded data. */
            drawNextPixel();
            /*
             * Fetch new name table and attribute u8s at regular intervals ;
             * then the pattern data, and increment coarse X offset.
             */
            if (currentState->cycle % 8 == 2)
                fetchPattern();
            else if (currentState->cycle % 8 == 4)
                fetchAttribute();
            else if (currentState->cycle % 8 == 0) {
                fetchBitmap();
                incrCoarseX();
            }
            /* Fine Y increment. */
            if (currentState->cycle == 256) {
                evaluateSprites();
                incrFineY();
            }
        }
        /* Copy coarse X from t to v. */
        else if (currentState->cycle == 257) {
            currentState->oamaddr = 0;
            currentState->regs.v =
                (currentState->regs.v & ~VADDR_X_MASK) |
                (currentState->regs.t & VADDR_X_MASK);
        }
        /* Load sprite pattern data. */
        else if (currentState->cycle < 321) {
            currentState->oamaddr = 0;
            if (currentState->cycle % 8 == 0)
                fetchSpriteBitmap();
        }
        /* Pre load two tiles for next line. */
        else if (currentState->cycle <= 340)
        {
            /* Exactly the same as visible scanline. */
            if (currentState->cycle % 8 == 2)
                fetchPattern();
            else if (currentState->cycle % 8 == 4)
                fetchAttribute();
            else if (currentState->cycle % 8 == 0) {
                shiftRegisters8();
                fetchBitmap();
                incrCoarseX();
            }
        }
    }

next:
    /* Increment dot tick and scanline ; and handle odd frames */
    currentState->cycle++;
    if (currentState->cycle == 341 && currentState->scanline == 261) {
        /* No skipped tick when BG rendering is off. */
        currentState->cycle = RENDERON ? oddframe : 0;
        oddframe = !oddframe;
        currentState->scanline = 0;
    } else if (currentState->cycle == 341) {
        currentState->scanline++;
        currentState->cycle = 0;
    }
}

/**
 * @brief Draw the patterns tables.
 */
static void drawPatternTables(int sx, int sy)
{
    uint32_t greyscale[4] = { 0xffffff, 0x404040, 0x808080, 0x0 };
    // uint32_t greyscale[4] = { 0xd8d8d8, 0xcc9e70, 0xcb733c, 0x712010 };
    u16 addr, line;
    int hpos = 0, vpos = 0, col;

    /* sx, sy refer to coordinates in the debug screen. */
    sx += 2 * PPU_WIDTH;

    for (addr = 0x0000; addr < 0x2000; addr += 16) {
        for (line = 0; line < 8; line++) {
            u8 lo = load(addr + line);
            u8 hi = load(addr + line + 8);
            for (col = 7; col >= 0; col--) {
                int p =
                    ((lo >> col) & 0x1) |
                    (((hi >> col) << 1) & 0x2);
                int x = 8 * hpos + 7 - col;
                int y = 8 * vpos + line;
                x = sx + 2 * x;
                y = (sy + 2 * y) * SCREEN_WIDTH;
                int z = x + sy + y;
                pixels[z] = greyscale[p];
                pixels[z + 1] = greyscale[p];
                pixels[z + SCREEN_WIDTH] = greyscale[p];
                pixels[z + SCREEN_WIDTH + 1] = greyscale[p];
            }
        }
        if (hpos >= PPU_HBLOCKS - 1) {
            hpos = 0;
            vpos++;
        } else
            hpos++;
    }
}

/**
 * @brief Draw the nth name table.
 * @param sel indicate which name table to draw
 */
static void drawNameTable(int sx, int sy, int sel)
{
    const uint32_t greyscale[4] = { 0xffffff, 0x404040, 0x808080, 0x0 };
    const u8 *ntable = ntables[sel % 4];
    const u16 ptable = currentState->ctrl.b;
    int hpos = 0, vpos = 0;

    /* sx, sy refer to coordinates in the debug screen. */
    sx += 2 * PPU_WIDTH;

    for (u16 addr = 0x0; addr < 0x3c0; addr++) {
        u8 nt = ntable[addr];
        for (u16 line = 0; line < 8; line++) {
            u8 lo = Memory::chrRom[ptable + (nt << 4) + line];
            u8 hi = Memory::chrRom[ptable + (nt << 4) + line + 8];
            for (int col = 7; col >= 0; col--) {
                int p =
                    ((lo >> col) & 0x1) |
                    (((hi >> col) << 1) & 0x2);
                int x = 8 * hpos + 7 - col;
                int y = 8 * vpos + line;
                pixels[SCREEN_WIDTH * (y + sy) + x + sx] = greyscale[p];
            }
        }
        if (hpos >= PPU_HBLOCKS - 1) {
            hpos = 0;
            vpos++;
        } else
            hpos++;
    }
}

/**
 * @brief Draw a tile displaying the configured palette colors.
 * @param sx    x offset
 * @param sy    y offset
 * @param c     selected color palette
 */
static void _drawPaletteTile(int sx, int sy, uint32_t c[4])
{
    int x, y;
    for (x = 0; x < 8; x ++)
        for (y = 0; y < 8; y ++) {
            int sel = (y / 4) * 2 + x / 4;
            pixels[SCREEN_WIDTH * (sy + y) + sx + x] = c[sel];
        }
}

/**
 * @brief Draw the attribute tables.
 * @param sel indicate which attribute table to draw
 */
static void drawAttrTable(int sx, int sy, int sel)
{
    const u16 attrtables[4] = { 0x23c0, 0x27c0, 0x2bc0, 0x2fc0 };
    uint32_t c[4];

    u16 addr, startaddr, endaddr;
    int hpos = 0, vpos = 0, bhpos, bvpos;

    startaddr = attrtables[sel % 4];
    endaddr = startaddr + 0x0040;

    /* sx, sy refer to coordinates in the debug screen. */
    sx += 2 * PPU_WIDTH;

    /*
     * The palette contains 4 different backgroud palettes, selected by two bits
     * in an attribute u8, which then contains the palette definition for a
     * block of 4x4 tiles.
     *
     *   ,---+---+---+---.
     *   |   |   |   |   |
     *   + D1-D0 + D3-D2 +
     *   |   |   |   |   |
     *   +---+---+---+---+
     *   |   |   |   |   |
     *   + D5-D4 + D7-D6 +
     *   |   |   |   |   |
     *   `---+---+---+---'
     */

    for (addr = startaddr; addr < endaddr; addr++) {
        u8 at = load(addr);

        for (bvpos = 0; bvpos < 3; bvpos += 2) {
            if (vpos + bvpos >= 30)
                break;
            for (bhpos = 0; bhpos < 3; bhpos += 2) {
                u16 pa = 0x3f00 + ((at & 0x3) << 2);
                c[0] = colors[load(pa)];
                c[1] = colors[load(pa + 1)];
                c[2] = colors[load(pa + 2)];
                c[3] = colors[load(pa + 3)];
                at >>= 2;
                /* Draw the palette for one tile. */
                _drawPaletteTile(
                    sx + (hpos + bhpos) * 4,
                    sy + (vpos + bvpos) * 4, c);
            }
        }
        if (hpos >= PPU_HBLOCKS - 4) {
            hpos = 0;
            vpos += 4;
        } else
            hpos += 4;
    }

    SDL_UpdateWindowSurface(window);
}

/**
 * @brief Draw the palettes.
 */
static void drawPalettes(int sx, int sy)
{
    u16 addr, line;
    int hpos = 0, vpos = 0, col;

    /* sx, sy refer to coordinates in the debug screen. */
    sx += 2 * PPU_WIDTH;

    for (addr = 0x3f00; addr < 0x3f20; addr++) {
        u8 c = load(addr);
        for (line = 0; line < 8; line++) {
            for (col = 0; col < 8; col++) {
                int x = 8 * hpos + col;
                int y = 8 * vpos + line;
                pixels[sx + x + (sy + y) * SCREEN_WIDTH] = colors[c];
            }
        }
        if (hpos >= PPU_HBLOCKS - 1) {
            hpos = 0;
            vpos++;
        } else
            hpos++;
    }

    SDL_UpdateWindowSurface(window);
}

/**
 * @brief Draw the visible sprites.
 */
static void drawSprites(void)
{
    int vsize = currentState->ctrl.h ? 16 : 8;
    u16 pt, ptl;
    int px, py;
    u8 index, attr;

    /* Clear the screen using the universal background color. */
    u8 c = load(0x3f00);
    for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++)
        pixels[i] = colors[c];

    /* Draw some sprites (backward iteration to respect priority). */
    for (int n = 63; n >= 0; n--) {
        px = oam.sprites[n].val.x;
        py = oam.sprites[n].val.y + 1;
        index = oam.sprites[n].val.index;
        attr = oam.sprites[n].val.attr;

        if (py >= 8 * PPU_VBLOCKS)
            continue;
        if (currentState->ctrl.h) {
            pt = (index & 0x1) * 0x1000;
            pt += (index & ~0x1) * 16;
        } else {
            pt = currentState->ctrl.s + index * 16;
        }

        for (int line = 0; line < vsize; line++) {
            if (py + line >= 8 * PPU_VBLOCKS)
                break;
            /* Patch pt for vertical flipping */
            if (currentState->ctrl.h) {
                if (attr & SPRITE_ATTR_VF) {
                    /* Swap the high and low sprites and the sprite lines */
                    if (line >= 8)
                        ptl = pt + 15 - line;
                    else
                        ptl = pt + 23 - line;
                } else {
                    if (line >= 8)
                        ptl = pt + 8 + line;
                    else
                        ptl = pt + line;
                }
            } else {
                if (attr & SPRITE_ATTR_VF) {
                    ptl = pt + 7 - line;
                } else {
                    ptl = pt + line;
                }
            }

            u8 lo, hi;
            lo = load(ptl);
            hi = load(ptl + 8);
            for (int col = 7; col >= 0; col--) {
                int p;
                if (attr & SPRITE_ATTR_HF)
                    p = ((lo >> (7-col)) & 0x1) |
                        (((hi >> (7-col)) << 1) & 0x2);
                else
                    p = ((lo >> col) & 0x1) |
                        (((hi >> col) << 1) & 0x2);
                if (p) {
                    u16 ca = 0x3f10;
                    ca |= (attr & SPRITE_ATTR_PL) << 2;
                    ca |= p;
                    u8 c = load(ca);
                    drawPixel(px + 7 - col, py + line, colors[c]);
                }
            }
        }
    }
}

}; /* namespace N2C02 */

const u32 colors[64] =
{
    0xff757575, 0xff271b8f, 0xff0000ab, 0xff47009f,
    0xff8f0077, 0xffab0013, 0xffa70000, 0xff7f0b00,
    0xff432f00, 0xff004700, 0xff005100, 0xff003f17,
    0xff1b3f5f, 0xff000000, 0xff000000, 0xff000000,
    0xffbcbcbc, 0xff0073ef, 0xff233bef, 0xff8300f3,
    0xffbf00bf, 0xffe7005b, 0xffdb2b00, 0xffcb4f0f,
    0xff8b7300, 0xff009700, 0xff00ab00, 0xff00933b,
    0xff00838b, 0xff000000, 0xff000000, 0xff000000,
    0xffffffff, 0xff3fbfff, 0xff5f97ff, 0xffa78bfd,
    0xfff77bff, 0xffff77b7, 0xffff7763, 0xffff9b3b,
    0xfff3bf3f, 0xff83d313, 0xff4fdf4b, 0xff58f898,
    0xff00ebdb, 0xff000000, 0xff000000, 0xff000000,
    0xffffffff, 0xffabe7ff, 0xffc7d7ff, 0xffd7cbff,
    0xffffc7ff, 0xffffc7db, 0xffffbfb3, 0xffffdbab,
    0xffffe7a3, 0xffe3ffa3, 0xffabf3bf, 0xffb3ffcf,
    0xff9ffff3, 0xff000000, 0xff000000, 0xff000000,
};

u8 palette[32] = {
    0x09, 0x01, 0x00, 0x01, 0x00, 0x02, 0x02, 0x0d,
    0x08, 0x10, 0x08, 0x24, 0x00, 0x00, 0x04, 0x2c,
    0x09, 0x01, 0x34, 0x03, 0x00, 0x04, 0x00, 0x14,
    0x08, 0x3a, 0x00, 0x02, 0x00, 0x20, 0x2c, 0x08,
};

static const u16 MortonTable256[256] =
{
    0x0000, 0x0001, 0x0004, 0x0005, 0x0010, 0x0011,
    0x0014, 0x0015, 0x0040, 0x0041, 0x0044, 0x0045,
    0x0050, 0x0051, 0x0054, 0x0055, 0x0100, 0x0101,
    0x0104, 0x0105, 0x0110, 0x0111, 0x0114, 0x0115,
    0x0140, 0x0141, 0x0144, 0x0145, 0x0150, 0x0151,
    0x0154, 0x0155, 0x0400, 0x0401, 0x0404, 0x0405,
    0x0410, 0x0411, 0x0414, 0x0415, 0x0440, 0x0441,
    0x0444, 0x0445, 0x0450, 0x0451, 0x0454, 0x0455,
    0x0500, 0x0501, 0x0504, 0x0505, 0x0510, 0x0511,
    0x0514, 0x0515, 0x0540, 0x0541, 0x0544, 0x0545,
    0x0550, 0x0551, 0x0554, 0x0555, 0x1000, 0x1001,
    0x1004, 0x1005, 0x1010, 0x1011, 0x1014, 0x1015,
    0x1040, 0x1041, 0x1044, 0x1045, 0x1050, 0x1051,
    0x1054, 0x1055, 0x1100, 0x1101, 0x1104, 0x1105,
    0x1110, 0x1111, 0x1114, 0x1115, 0x1140, 0x1141,
    0x1144, 0x1145, 0x1150, 0x1151, 0x1154, 0x1155,
    0x1400, 0x1401, 0x1404, 0x1405, 0x1410, 0x1411,
    0x1414, 0x1415, 0x1440, 0x1441, 0x1444, 0x1445,
    0x1450, 0x1451, 0x1454, 0x1455, 0x1500, 0x1501,
    0x1504, 0x1505, 0x1510, 0x1511, 0x1514, 0x1515,
    0x1540, 0x1541, 0x1544, 0x1545, 0x1550, 0x1551,
    0x1554, 0x1555, 0x4000, 0x4001, 0x4004, 0x4005,
    0x4010, 0x4011, 0x4014, 0x4015, 0x4040, 0x4041,
    0x4044, 0x4045, 0x4050, 0x4051, 0x4054, 0x4055,
    0x4100, 0x4101, 0x4104, 0x4105, 0x4110, 0x4111,
    0x4114, 0x4115, 0x4140, 0x4141, 0x4144, 0x4145,
    0x4150, 0x4151, 0x4154, 0x4155, 0x4400, 0x4401,
    0x4404, 0x4405, 0x4410, 0x4411, 0x4414, 0x4415,
    0x4440, 0x4441, 0x4444, 0x4445, 0x4450, 0x4451,
    0x4454, 0x4455, 0x4500, 0x4501, 0x4504, 0x4505,
    0x4510, 0x4511, 0x4514, 0x4515, 0x4540, 0x4541,
    0x4544, 0x4545, 0x4550, 0x4551, 0x4554, 0x4555,
    0x5000, 0x5001, 0x5004, 0x5005, 0x5010, 0x5011,
    0x5014, 0x5015, 0x5040, 0x5041, 0x5044, 0x5045,
    0x5050, 0x5051, 0x5054, 0x5055, 0x5100, 0x5101,
    0x5104, 0x5105, 0x5110, 0x5111, 0x5114, 0x5115,
    0x5140, 0x5141, 0x5144, 0x5145, 0x5150, 0x5151,
    0x5154, 0x5155, 0x5400, 0x5401, 0x5404, 0x5405,
    0x5410, 0x5411, 0x5414, 0x5415, 0x5440, 0x5441,
    0x5444, 0x5445, 0x5450, 0x5451, 0x5454, 0x5455,
    0x5500, 0x5501, 0x5504, 0x5505, 0x5510, 0x5511,
    0x5514, 0x5515, 0x5540, 0x5541, 0x5544, 0x5545,
    0x5550, 0x5551, 0x5554, 0x5555,
};

inline __attribute__((always_inline))
static u16 interleave(u8 b0, u8 b1)
{
    u16 w0 = MortonTable256[b0];
    u16 w1 = MortonTable256[b1];
    return (w1 << 1) | w0;
}
