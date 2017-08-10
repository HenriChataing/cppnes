#include <cstdlib>
#include <iostream>
#include <string.h>

#include "Joypad.h"

using namespace Joypad;

namespace Joypad {

Joypad::Joypad() : strobe(false), reads(0)
{
    memset(buttons, 0, sizeof(buttons));
}

Joypad::~Joypad()
{}

/**
 * @brief Read controller data
 * @return a button status
 */
u8 Joypad::readRegister()
{
    if (strobe)
        return buttons[JOYPAD_BUTTON_A];
    else if (reads < 8)
        return buttons[reads++];
    else
        return 0x0;
}

/**
 * @brief Perform a write on the joypad io register
 * @param val written byte
 */
void Joypad::writeRegister(u8 val)
{
    if (val & 0x1) {
        strobe = true;
        reads = 0;
    } else if (strobe)
        strobe = false;
}

void Joypad::buttonUp(JoypadButton button)
{
    if (button < JOYPAD_BUTTON_COUNT)
        buttons[button] = 0;
}

void Joypad::buttonDown(JoypadButton button)
{
    if (button < JOYPAD_BUTTON_COUNT)
        buttons[button] = 1;
}

Joypad *currentJoypad = NULL;

};
