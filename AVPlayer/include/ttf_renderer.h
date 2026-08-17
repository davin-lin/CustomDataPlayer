#pragma once

#include <SDL.h>
#include <string>
#include <vector>

class TTFRenderer {
public:
    TTFRenderer();
    ~TTFRenderer();

    int Init();
    void Destroy();

    // Streaming DATA overlay (rendered at the top-right corner).
    // lines: each entry is one text line to display.
    void SetStreamLines(const std::vector<std::string>& lines);
    void RenderStreamOverlay(SDL_Renderer* renderer);

private:
    void LoadFont();
    void BuildStreamTexture(SDL_Renderer* renderer);

private:
    void* font_ = nullptr;

    // Streaming DATA overlay state
    SDL_Texture* streamTexture_ = nullptr;
    int streamW_ = 0;
    int streamH_ = 0;
    std::vector<std::string> streamLines_;
    bool streamDirty_ = false;
};
