
#include <iostream>

#include "Rom.h"
#include "Core.h"
#include "Mapper.h"
#include "Joypad.h"
#include "Events.h"
#include "M6502State.h"
#include "N2C02State.h"

int main(int argc, char *argv[])
{
    try {
        currentRom = new Rom(argv[1]);
        M6502::currentState = new M6502::State();
        N2C02::currentState = new N2C02::State();
        Joypad::currentJoypad = new Joypad::Joypad();
        N2C02::init();
        Events::init();
        Core::emulate();
        N2C02::quit();
    } catch (const std::exception &exc) {
        std::cerr << "Fatal error (main): " << exc.what() << std::endl;
    }
    return 0;
}
