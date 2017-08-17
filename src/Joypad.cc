#include <cstdlib>
#include <iostream>
#include <functional>
#include <string.h>

#include "Joypad.h"
#include "Events.h"

using namespace Joypad;

namespace Joypad {

Joypad::Joypad() : strobe(false), reads(0)
{
    memset(buttons, 0, sizeof(buttons));

    Events::bindKeyboardEvent(SDL_KEYDOWN, SDLK_w,
        std::bind(&Joypad::buttonDown, this, JOYPAD_BUTTON_A));
    Events::bindKeyboardEvent(SDL_KEYUP, SDLK_w,
        std::bind(&Joypad::buttonUp, this, JOYPAD_BUTTON_A));
    Events::bindKeyboardEvent(SDL_KEYDOWN, SDLK_x,
        std::bind(&Joypad::buttonDown, this, JOYPAD_BUTTON_B));
    Events::bindKeyboardEvent(SDL_KEYUP, SDLK_x,
        std::bind(&Joypad::buttonUp, this, JOYPAD_BUTTON_B));
    Events::bindKeyboardEvent(SDL_KEYDOWN, SDLK_SPACE,
        std::bind(&Joypad::buttonDown, this, JOYPAD_BUTTON_SELECT));
    Events::bindKeyboardEvent(SDL_KEYUP, SDLK_SPACE,
        std::bind(&Joypad::buttonUp, this, JOYPAD_BUTTON_SELECT));
    Events::bindKeyboardEvent(SDL_KEYDOWN, SDLK_RETURN,
        std::bind(&Joypad::buttonDown, this, JOYPAD_BUTTON_START));
    Events::bindKeyboardEvent(SDL_KEYUP, SDLK_RETURN,
        std::bind(&Joypad::buttonUp, this, JOYPAD_BUTTON_START));
    Events::bindKeyboardEvent(SDL_KEYDOWN, SDLK_UP,
        std::bind(&Joypad::buttonDown, this, JOYPAD_BUTTON_UP));
    Events::bindKeyboardEvent(SDL_KEYUP, SDLK_UP,
        std::bind(&Joypad::buttonUp, this, JOYPAD_BUTTON_UP));
    Events::bindKeyboardEvent(SDL_KEYDOWN, SDLK_DOWN,
        std::bind(&Joypad::buttonDown, this, JOYPAD_BUTTON_DOWN));
    Events::bindKeyboardEvent(SDL_KEYUP, SDLK_DOWN,
        std::bind(&Joypad::buttonUp, this, JOYPAD_BUTTON_DOWN));
    Events::bindKeyboardEvent(SDL_KEYDOWN, SDLK_LEFT,
        std::bind(&Joypad::buttonDown, this, JOYPAD_BUTTON_LEFT));
    Events::bindKeyboardEvent(SDL_KEYUP, SDLK_LEFT,
        std::bind(&Joypad::buttonUp, this, JOYPAD_BUTTON_LEFT));
    Events::bindKeyboardEvent(SDL_KEYDOWN, SDLK_RIGHT,
        std::bind(&Joypad::buttonDown, this, JOYPAD_BUTTON_RIGHT));
    Events::bindKeyboardEvent(SDL_KEYUP, SDLK_RIGHT,
        std::bind(&Joypad::buttonUp, this, JOYPAD_BUTTON_RIGHT));
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
