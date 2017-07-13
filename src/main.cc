
#include <iostream>

#include "Rom.h"
#include "Core.h"
#include "Mapper.h"
#include "M6502State.h"

int main(int argc, char *argv[])
{
    currentRom = new Rom(argv[1]);
    M6502::currentState = new M6502::State();
    Core::emulate();
    return 0;
}
