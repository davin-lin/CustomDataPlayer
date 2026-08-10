#pragma once

#include <SDL.h>
#include <string>

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

private:
    void LoadFont();
    void BuildOverlayTexture(SDL_Renderer* renderer);

private:
    void* font_ = nullptr;
    SDL_Texture* overlayTexture_ = nullptr;
    int overlayW_ = 0;
    int overlayH_ = 0;

    std::string usrName_;
    std::string usrCompany_;
    std::string usrType_;
    bool dataDirty_ = true;
};
