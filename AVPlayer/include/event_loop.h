#pragma once

#include <SDL.h>
#include "player.h"

class EventLoop {
public:
    EventLoop();

    int Run(Player& player);
};