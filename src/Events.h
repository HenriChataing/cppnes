
#ifndef _EVENTS_H_INCLUDED_
#define _EVENTS_H_INCLUDED_

#include <functional>
#include <SDL2/SDL.h>

#include "type.h"

namespace Events {

class EventHandler
{
    public:
        EventHandler() {};
        virtual ~EventHandler() {};
        virtual void operator()(SDL_Event &event) {};
};

void bindKeyboardEvent(int type, int sym, std::function<void()> callback);
void bindKeyboardEvent(int sym, bool *status);

void init();

};

#endif /* _EVENTS_H_INCLUDED_ */
