#ifndef _JOYPAD_H_INCLUDED_
#define _JOYPAD_H_INCLUDED_

#include "type.h"

namespace Joypad {

enum JoypadButton {
    JOYPAD_BUTTON_A         = 0,
    JOYPAD_BUTTON_B         = 1,
    JOYPAD_BUTTON_SELECT    = 2,
    JOYPAD_BUTTON_START     = 3,
    JOYPAD_BUTTON_UP        = 4,
    JOYPAD_BUTTON_DOWN      = 5,
    JOYPAD_BUTTON_LEFT      = 6,
    JOYPAD_BUTTON_RIGHT     = 7,
    JOYPAD_BUTTON_COUNT     = 8,
};

/* Current state (such as pressed buttons). */
class Joypad
{
    public:
        Joypad();
        ~Joypad();

        u8 readRegister();
        void writeRegister(u8 val);
        void buttonUp(JoypadButton button);
        void buttonDown(JoypadButton button);

        /** Joypad status registers. */
        bool buttons[JOYPAD_BUTTON_COUNT];
        bool strobe;
        unsigned int reads;
};

extern Joypad *currentJoypad;

};

#endif /* _JOYPAD_H_INCLUDED_ */
