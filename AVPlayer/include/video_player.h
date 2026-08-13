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
public:
    VideoPlayer(std::shared_ptr<Context> ctx);
    int Open();
    int Start();
    int Close();

    void UpdateWidthHeight(int width, int height);
    void ToggleFullScreen();

    int Run(int interval);
    double Refresh();

private:
    void Display();
    void Render();
    void RenderLastTexture(Frame* vp);
    void CreateTexture(Uint32 format, int width, int height, SDL_BlendMode blendmode);

    void CalcDisplayRect(int src_w, int src_h, int& fit_w, int& fit_h, int& offset_x, int& offset_y);
    void UploadTexture(Uint32 sdl_pix_fmt, uint8_t* const* data, const int* linesize, int height);
    void RenderGpuPath(Frame* vp, Uint32 sdl_pix_fmt, SDL_BlendMode sdl_blendmode,
                       int src_w, int src_h, int fit_w, int fit_h, int offset_x, int offset_y);
    void RenderCpuPath(Frame* vp, Uint32 sdl_pix_fmt, SDL_BlendMode sdl_blendmode,
                       int src_w, int src_h, int fit_w, int fit_h, int offset_x, int offset_y);

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
    bool lastPathWasGpu_ = false;   // 上一帧走的渲染路径：true=GPU直接上传，false=CPU swscale。切换路径时强制重新上传纹理，避免 width_/height_ 语义不一致

    TTFRenderer ttfRenderer_;
};
