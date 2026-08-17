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

    void SetOverlayData(const std::string& name,
                        const std::string& company,
                        const std::string& type);

    void RenderOverlay(SDL_Renderer* renderer);

    // Streaming DATA overlay (rendered at the top-right corner).
    // lines: each entry is one text line to display.
    void SetStreamLines(const std::vector<std::string>& lines);
    void RenderStreamOverlay(SDL_Renderer* renderer);

private:
    void LoadFont();
    void BuildOverlayTexture(SDL_Renderer* renderer);
    void BuildStreamTexture(SDL_Renderer* renderer);

private:
    void* font_ = nullptr;
    SDL_Texture* overlayTexture_ = nullptr;
    int overlayW_ = 0;
    int overlayH_ = 0;

    std::string usrName_;
    std::string usrCompany_;
    std::string usrType_;
    bool dataDirty_ = true;

    // Streaming DATA overlay state
    SDL_Texture* streamTexture_ = nullptr;
    int streamW_ = 0;
    int streamH_ = 0;
    std::vector<std::string> streamLines_;
    bool streamDirty_ = false;
};

