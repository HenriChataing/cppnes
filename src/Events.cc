#include <cstdlib>
#include <iostream>
#include <thread>

#include <SDL2/SDL.h>

#include "Events.h"
#include "Joypad.h"

using namespace Joypad;

namespace Events {

static std::thread *currentThread = NULL;

/**
 * @brief Wait for (and handle) SDL keyboard events.
 */
static void handleEvents()
{
    std::cerr << "Event thread started" << std::endl;
    /* Check for joypad events. */
    SDL_Event event;
    while (true)
    {
        int ret = SDL_WaitEvent(&event);
        if (ret == 0) {
            std::cerr << "Error waiting for SDL events (";
            std::cerr << SDL_GetError() << ")" << std::endl;
            continue;
        }

        switch (event.type) {
            case SDL_KEYUP:
                switch (event.key.keysym.sym) {
                    /* Joypad controls. */
                    case SDLK_w:
                        currentJoypad->buttonUp(JOYPAD_BUTTON_A);
                        break;
                    case SDLK_x:
                        currentJoypad->buttonUp(JOYPAD_BUTTON_B);
                        break;
                    case SDLK_SPACE:
                        currentJoypad->buttonUp(JOYPAD_BUTTON_SELECT);
                        break;
                    case SDLK_RETURN:
                        currentJoypad->buttonUp(JOYPAD_BUTTON_START);
                        break;
                    case SDLK_UP:
                        currentJoypad->buttonUp(JOYPAD_BUTTON_UP);
                        break;
                    case SDLK_DOWN:
                        currentJoypad->buttonUp(JOYPAD_BUTTON_DOWN);
                        break;
                    case SDLK_LEFT:
                        currentJoypad->buttonUp(JOYPAD_BUTTON_LEFT);
                        break;
                    case SDLK_RIGHT:
                        currentJoypad->buttonUp(JOYPAD_BUTTON_RIGHT);
                        break;
                    default:
                        break;
                }
                break;
            case SDL_KEYDOWN:
                if (event.key.repeat != 0)
                    break;
                switch (event.key.keysym.sym) {
                    /* Joypad controls. */
                    case SDLK_w:
                        currentJoypad->buttonDown(JOYPAD_BUTTON_A);
                        break;
                    case SDLK_x:
                        currentJoypad->buttonDown(JOYPAD_BUTTON_B);
                        break;
                    case SDLK_SPACE:
                        currentJoypad->buttonDown(JOYPAD_BUTTON_SELECT);
                        break;
                    case SDLK_RETURN:
                        currentJoypad->buttonDown(JOYPAD_BUTTON_START);
                        break;
                    case SDLK_UP:
                        currentJoypad->buttonDown(JOYPAD_BUTTON_UP);
                        break;
                    case SDLK_DOWN:
                        currentJoypad->buttonDown(JOYPAD_BUTTON_DOWN);
                        break;
                    case SDLK_LEFT:
                        currentJoypad->buttonDown(JOYPAD_BUTTON_LEFT);
                        break;
                    case SDLK_RIGHT:
                        currentJoypad->buttonDown(JOYPAD_BUTTON_RIGHT);
                        break;
                    default:
                        break;
                }
                break;
            case SDL_QUIT:
                std::cerr << "SDL QUIT" << std::endl;
                return;
            default:
                break;
        }
    }
}

/**
 * @brief Start a thread execlusively to handle events.
 */
void init()
{
    if (currentThread == NULL)
        currentThread = new std::thread(handleEvents);
}

};
