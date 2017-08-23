#include <cstdlib>
#include <iostream>
#include <thread>
#include <map>

#include <SDL2/SDL.h>

#include "Events.h"
#include "Joypad.h"

using namespace Joypad;
using namespace Events;

namespace Events {

static std::thread *currentThread = NULL;
static std::map<std::pair<int, int>, EventHandler *> keyboardEventHandlers;
static bool quitEvent = false;

class CallbackEventHandler : public EventHandler
{
    public:
        CallbackEventHandler(std::function<void()> &callback)
            : callback(callback) {}
        ~CallbackEventHandler() {}

        void operator()(SDL_Event &event) {
            (void)event;
            callback();
        }

        std::function<void()> callback;
};

class SetterEventHandler : public EventHandler
{
    public:
        SetterEventHandler(bool *ptr) : ptr(ptr) {}
        ~SetterEventHandler() {}

        void operator()(SDL_Event &event) {
            *ptr = (event.type == SDL_KEYDOWN);
        }

        bool *ptr;
};

static void bindKeyboardEvent(int type, int sym, EventHandler *handler)
{
    std::pair<int, int> key(type, sym);
    EventHandler *oldHandler = keyboardEventHandlers[key];
    if (oldHandler != NULL && oldHandler != handler)
        delete oldHandler;
    keyboardEventHandlers[key] = handler;
}

/**
 * @brief Register a callback for the specified keyboard event.
 * @param callback      event callback
 */
void bindKeyboardEvent(int type, int sym, std::function<void()> callback)
{
    bindKeyboardEvent(type, sym, new CallbackEventHandler(callback));
}

/**
 * @brief Register a callback for the specified keyboard event.
 *
 *  The callback will set the status variable
 *  accordingly (1 key down, 0 key up).
 *
 * @param callback      event callback
 */
void bindKeyboardEvent(int sym, bool *status)
{
    bindKeyboardEvent(SDL_KEYUP, sym, new SetterEventHandler(status));
    bindKeyboardEvent(SDL_KEYDOWN, sym, new SetterEventHandler(status));
}

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
            case SDL_KEYUP: {
                std::pair<int, int> key(SDL_KEYUP, event.key.keysym.sym);
                EventHandler *handler = keyboardEventHandlers[key];
                if (handler != NULL)
                    (*handler)(event);
                break;
            }
            case SDL_KEYDOWN: {
                if (event.key.repeat != 0)
                    break;
                std::pair<int, int> key(SDL_KEYDOWN, event.key.keysym.sym);
                EventHandler *handler = keyboardEventHandlers[key];
                if (handler != NULL)
                    (*handler)(event);
                break;
            }
            case SDL_QUIT:
                std::cerr << "Quitting..." << std::endl;
                quitEvent = true;
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
    quitEvent = false;
}

/**
 * @brief Check whether the quit event was raised.
 */
bool isQuit()
{
    return quitEvent;
}

};
