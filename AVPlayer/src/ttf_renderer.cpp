#include "ttf_renderer.h"

#ifdef __cplusplus
extern "C" {
#endif
#include <SDL_ttf.h>
#include <libavutil/log.h>
#ifdef __cplusplus
}
#endif

#include <vector>

TTFRenderer::TTFRenderer() = default;

TTFRenderer::~TTFRenderer() {
    Destroy();
}

int TTFRenderer::Init() {
    if (TTF_Init() < 0) {
        av_log(nullptr, AV_LOG_WARNING, "TTF_Init failed: %s\n", TTF_GetError());
        return -1;
    }
    LoadFont();
    return 0;
}

void TTFRenderer::Destroy() {
    if (overlayTexture_) {
        SDL_DestroyTexture(overlayTexture_);
        overlayTexture_ = nullptr;
    }
    if (font_) {
        TTF_CloseFont(static_cast<TTF_Font*>(font_));
        font_ = nullptr;
    }
    TTF_Quit();
}

void TTFRenderer::LoadFont() {
    const char* fontPaths[] = {
        "assets/fonts/simhei.ttf",
        "d:\\software\\VisualStudio\\code\\CustomData\\assets\\fonts\\simhei.ttf",
        "C:\\Windows\\Fonts\\simhei.ttf",
        "C:\\Windows\\Fonts\\msyh.ttc",
        "C:\\Windows\\Fonts\\arial.ttf"
    };
    for (const char* path : fontPaths) {
        TTF_Font* f = TTF_OpenFont(path, 24);
        if (f) {
            font_ = f;
            av_log(nullptr, AV_LOG_INFO, "TTF font loaded: %s\n", path);
            return;
        }
    }
    av_log(nullptr, AV_LOG_WARNING, "TTF_OpenFont failed: %s\n", TTF_GetError());
}

void TTFRenderer::SetOverlayData(const std::string& name,
                                   const std::string& company,
                                   const std::string& type) {
    if (name == usrName_ && company == usrCompany_ && type == usrType_) {
        return;
    }
    usrName_ = name;
    usrCompany_ = company;
    usrType_ = type;
    dataDirty_ = true;
}

void TTFRenderer::BuildOverlayTexture(SDL_Renderer* renderer) {
    if (!font_ || !renderer) return;

    std::vector<std::string> lines;
    if (!usrName_.empty())    lines.push_back("Name: "    + usrName_);
    if (!usrCompany_.empty()) lines.push_back("Company: " + usrCompany_);
    if (!usrType_.empty())    lines.push_back("Type: "    + usrType_);

    if (lines.empty()) return;

    SDL_Color white = { 255, 255, 255, 255 };
    TTF_Font* f = static_cast<TTF_Font*>(font_);

    int lineH = TTF_FontHeight(f);
    int maxW = 0;
    std::vector<SDL_Surface*> lineSurfaces;
    for (const auto& line : lines) {
        SDL_Surface* surf = TTF_RenderUTF8_Blended(f, line.c_str(), white);
        if (surf) {
            if (surf->w > maxW) maxW = surf->w;
        }
        lineSurfaces.push_back(surf);
    }
    if (maxW == 0 || lineH <= 0) {
        for (auto* s : lineSurfaces) if (s) SDL_FreeSurface(s);
        return;
    }

    int padding = 6;
    int totalW = maxW + padding * 2;
    int totalH = lineH * (int)lineSurfaces.size() + padding * 2;
    SDL_Surface* canvas = SDL_CreateRGBSurfaceWithFormat(0, totalW, totalH, 32, SDL_PIXELFORMAT_RGBA8888);
    if (!canvas) {
        for (auto* s : lineSurfaces) if (s) SDL_FreeSurface(s);
        return;
    }
    SDL_FillRect(canvas, nullptr, SDL_MapRGBA(canvas->format, 0, 0, 0, 160));

    for (size_t i = 0; i < lineSurfaces.size(); i++) {
        SDL_Surface* s = lineSurfaces[i];
        if (!s) continue;
        SDL_Rect dst = { padding, padding + (int)i * lineH, s->w, s->h };
        SDL_SetSurfaceBlendMode(s, SDL_BLENDMODE_BLEND);
        SDL_BlitSurface(s, nullptr, canvas, &dst);
        SDL_FreeSurface(s);
    }

    if (overlayTexture_) {
        SDL_DestroyTexture(overlayTexture_);
        overlayTexture_ = nullptr;
    }
    overlayTexture_ = SDL_CreateTextureFromSurface(renderer, canvas);
    if (overlayTexture_) {
        SDL_SetTextureBlendMode(overlayTexture_, SDL_BLENDMODE_BLEND);
        overlayW_ = canvas->w;
        overlayH_ = canvas->h;
    }
    SDL_FreeSurface(canvas);
    dataDirty_ = false;
}

void TTFRenderer::RenderOverlay(SDL_Renderer* renderer) {
    if (!font_ || !renderer || !dataDirty_ && overlayTexture_) {
        if (!font_ || !renderer) return;
    }
    if (dataDirty_) {
        BuildOverlayTexture(renderer);
    }
    if (overlayTexture_) {
        SDL_Rect dst = { 10, 10, overlayW_, overlayH_ };
        SDL_RenderCopy(renderer, overlayTexture_, nullptr, &dst);
    }
}
