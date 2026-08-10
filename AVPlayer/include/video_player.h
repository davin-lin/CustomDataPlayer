#pragma once

#include "context.h"
#include "ttf_renderer.h"
#ifdef __cplusplus
extern "C" {
#endif
#include <SDL.h>
#ifdef __cplusplus
}
#endif

#define SDL_WINDOW_DEFAULT_WIDTH  (1080)
#define SDL_WINDOW_DEFAULT_HEIGHT (720)

class VideoPlayer {
    friend Uint32 callback(Uint32 interval, void* param);
public:
    VideoPlayer(std::shared_ptr<Context> ctx);
    int Open();
    int Start();
    int Close();

    void UpdateWidthHeight(int width, int height);
    void ToggleFullScreen();

    int Run(int interval);
private:

    double Refresh();
    void Display();
    void Render();
    void RenderLastTexture(Frame* vp);
    void CreateTexture(Uint32 format, int width, int height, SDL_BlendMode blendmode);

    double ComputeTargetDelay(double delay);
    void UpdateVideoPts(double pts, int serial);
private:
    std::shared_ptr<Context> ctx_ = nullptr;
    SDL_TimerID timerId_ = 0;

    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    SDL_Texture* texture_ = nullptr;

    SwsContext* swsCtx_ = nullptr;
    uint8_t* data_[4] = { nullptr };
    int lineSize_[4] = { 0 };
    int dataSize_ = 0;

    int dstWidth_ = SDL_WINDOW_DEFAULT_WIDTH;
    int dstHeight_ = SDL_WINDOW_DEFAULT_HEIGHT;
    int width_ = SDL_WINDOW_DEFAULT_WIDTH;
    int height_ = SDL_WINDOW_DEFAULT_HEIGHT;
    int offsetX_ = 0;
    int offsetY_ = 0;
    bool isFullScreen_ = false;

    TTFRenderer ttfRenderer_;
};
