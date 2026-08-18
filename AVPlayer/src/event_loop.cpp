#include "event_loop.h"

EventLoop::EventLoop() {
    SDL_Init(SDL_INIT_EVERYTHING);

}

int EventLoop::Run(Player& player) {
    SDL_Event event;
    double remainingTime = 0.0;
    for (;;) {
        int timeout = SDL_WaitEventTimeout(&event, 20);
        player.Refresh();
        switch (event.type) {
        case SDL_QUIT: {
            player.Close();
            av_log(nullptr, AV_LOG_INFO, "player close finished\n");
            return 0;
        }
            break;
        case SDL_KEYDOWN: {
            switch (event.key.keysym.sym)
            {
            case SDLK_SPACE:
                player.TogglePause();
                break;
            case SDLK_m:
                player.ToggleMute();
                break;
            case SDLK_f:
                player.ToggleFullScreen();
                break;
            case SDLK_d:
                player.StepToNextFrame();
				break;
            case SDLK_UP:
                player.VolumeUp(5);
                break;
            case SDLK_DOWN:
                player.VolumeDown(5);
                break;
            case SDLK_LEFT:
                player.SeekBackward(-10.0);
                break;
            case SDLK_RIGHT:
                player.SeekForward(10.0);
                break; 
            default:
				av_log(nullptr, AV_LOG_WARNING, "unsupported keydown key, event.key.keysym.sym=%d\n", event.key.keysym.sym);
                break;
            }
        }
                break;
        case SDL_WINDOWEVENT: {
            switch (event.window.event) {
            case SDL_WINDOWEVENT_SIZE_CHANGED: {
                int screen_width = event.window.data1;
                int screen_height = event.window.data2;
                player.UpdateWidthHeight(screen_width, screen_height);
                player.ForceRefresh();
            }
                break;
            case SDL_WINDOWEVENT_EXPOSED: {
                player.ForceRefresh();
            }
                break;
            default:
                break;
            }
        }
            break;
        default:
            break;
        }
    }
}
