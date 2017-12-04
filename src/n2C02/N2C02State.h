/*
 * Copyright (c) 2017-2017 Prove & Run S.A.S
 * All Rights Reserved.
 *
 * This software is the confidential and proprietary information of
 * Prove & Run S.A.S ("Confidential Information"). You shall not
 * disclose such Confidential Information and shall use it only in
 * accordance with the terms of the license agreement you entered
 * into with Prove & Run S.A.S
 *
 * PROVE & RUN S.A.S MAKES NO REPRESENTATIONS OR WARRANTIES ABOUT THE
 * SUITABILITY OF THE SOFTWARE, EITHER EXPRESS OR IMPLIED, INCLUDING
 * BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE, OR NON-INFRINGEMENT. PROVE & RUN S.A.S SHALL
 * NOT BE LIABLE FOR ANY DAMAGES SUFFERED BY LICENSEE AS A RESULT OF USING,
 * MODIFYING OR DISTRIBUTING THIS SOFTWARE OR ITS DERIVATIVES.
 */
/**
 * @file
 * @brief
 * @author Henri Chataing
 * @date July 20th, 2017 (creation)
 * @copyright (c) 2017-2017, Prove & Run and/or its affiliates.
 *   All rights reserved.
 */


#ifndef _N2C02STATE_H_INCLUDED_
#define _N2C02STATE_H_INCLUDED_

#include <functional>

#include "type.h"

namespace N2C02 {

/**
 * Internal PPU registers.
 */
struct Registers {
    /*
     * VRAM address (15 bits), and temporary VRAM address.
     *
     *   0yyy NN YYYYY XXXXX
     *    ||| || ||||| +++++-- coarse X scroll
     *    ||| || +++++-------- coarse Y scroll
     *    ||| ++-------------- nametable select
     *    +++----------------- fine Y scroll
     */
    u16 v;
    u16 t;
    /* Fine x scroll (3 bits). */
    u8 x;
    /* Write toggle (1 bit). */
    bool w;
    /* 2-bit latch for palette attribute. */
    u16 pall;
    /* Palette attribute shift registers */
    u16 pals;
    /* Pattern data shift registers */
    u32 pats;
    /* Name table u8 feched every 8 cycles. */
    u8 nt;
    /* Attribute u8 feched every 8 cycles. */
    u8 at;
};

/**
 * Breakdown of the PPUCTRL register bits.
 */
struct Control {
    /* Address increment. */
    u16 i;
    /*
     * Sprite pattern table address for 8x8 sprites.
     * Valid values are 0x0000 and 0x1000.
     */
    u16 s;
    /*
     * Background pattern table address.
     * Valid values are 0x0000 and 0x1000.
     */
    u16 b;
    /* Render 8x16 sprites. */
    bool h;
    /* Generate an NMI at the start of the vertical blanking interval. */
    bool v;
};

/**
 * Breakdown of the PPUMASK register bits.
 */
struct Mask {
    /* Greyscale display. */
    bool greyscale;
    /*
     * Background (resp. sprite) clipping. If true, the background
     * (resp sprites) will not be displayed on the leftmost 8 pixels of
     * the screen.
     */
    bool bc;
    bool sc;
    /*
     * Background (resp. sprite) rendering. If false, the background
     * (resp. sprites) will not be rendered.
     */
    bool br;
    bool sr;
};

class State
{
public:
    State();
    ~State();

    void clear();

    /*
     * Current scanline (0-261 inclusive)
     */
    unsigned int scanline;
    /*
     * Current scanline cycle (0-340 inclusive)
     */
    unsigned int cycle;
    /*
     * CPU cycle count at the last time the PPU was updated.
     */
    unsigned long sync;

    /*
     * The PPU has an internal data bus that it uses for communication with the
     * CPU. This bus behaves as an 8-bit dynamic latch due to capacitance of
     * very long traces that run to various parts of the PPU.
     * Writing any value to any PPU port, even to the nominally read-only
     * PPUSTATUS, will fill this latch. Reading any readable port (PPUSTATUS,
     * OAMDATA, or PPUDATA) also fills the latch with the bits read. Reading
     * a nominally "write-only" register returns the latch's current value,
     * as do the unused bits of PPUSTATUS. This value begins to decay after
     * a frame or so, faster once the PPU has warmed up, and it is likely
     * that values with alternating bit patterns (such as $55 or $AA) will
     * decay faster.
     */
    /// FIXME add decay countdown value
    u8 bus;
    /*
     * Read buffer attached to PPUDATA register.
     */
    u8 readBuffer;

    Registers regs;
    Control ctrl;
    Mask mask;
    u8 status;
    u8 oamaddr;
    // u8 oam[256];

    u8 readRegister(u16 addr, long quantum = 0);
    void writeRegister(u16 addr, u8 val, long quantum = 0);
    void dmaTransfer(u8 val);
};

extern State state;

void setVerticalMirroring(void);
void setHorizontalMirroring(void);
void set1ScreenMirroring(int upper);
void set4ScreenMirroring(void);
void setScanlineCallback(std::function<void(int, int)> callback);

int init();
void quit();
void dot();
void sync(long quantum);

};

#endif /* _N2C02STATE_H_INCLUDED_ */
