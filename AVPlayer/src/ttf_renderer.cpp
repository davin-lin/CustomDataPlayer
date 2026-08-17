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
    if (streamTexture_) {
        SDL_DestroyTexture(streamTexture_);
        streamTexture_ = nullptr;
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

void TTFRenderer::SetStreamLines(const std::vector<std::string>& lines) {
    // 防御:若输入 vector 过大(异常数据),拒绝更新以避免 string::assign 时
    // 触发 _Xlength_error("string too long") 导致进程崩溃
    if (lines.size() > 64) {
        streamDirty_ = false;
        return;
    }
    for (const auto& l : lines) {
        if (l.size() > 1024) {
            streamDirty_ = false;
            return;
        }
    }
    // 内容未变则不重建纹理,避免每帧重建(DATA 块内字段每 0.5s 才变化)
    if (lines == streamLines_) {
        return;
    }
    streamLines_ = lines;
    streamDirty_ = true;
}

void TTFRenderer::BuildStreamTexture(SDL_Renderer* renderer) {
    if (!font_ || !renderer) return;
    if (streamLines_.empty()) {
        if (streamTexture_) {
            SDL_DestroyTexture(streamTexture_);
            streamTexture_ = nullptr;
            streamW_ = 0;
            streamH_ = 0;
        }
        streamDirty_ = false;
        return;
    }

    SDL_Color white = { 255, 255, 255, 255 };
    TTF_Font* f = static_cast<TTF_Font*>(font_);

    int lineH = TTF_FontHeight(f);
    int maxW = 0;
    std::vector<SDL_Surface*> lineSurfaces;
    for (const auto& line : streamLines_) {
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

    if (streamTexture_) {
        SDL_DestroyTexture(streamTexture_);
        streamTexture_ = nullptr;
    }
    streamTexture_ = SDL_CreateTextureFromSurface(renderer, canvas);
    if (streamTexture_) {
        SDL_SetTextureBlendMode(streamTexture_, SDL_BLENDMODE_BLEND);
        streamW_ = canvas->w;
        streamH_ = canvas->h;
    }
    SDL_FreeSurface(canvas);
    streamDirty_ = false;
}

void TTFRenderer::RenderStreamOverlay(SDL_Renderer* renderer) {
    if (!font_ || !renderer) return;
    if (streamDirty_) {
        BuildStreamTexture(renderer);
    }
    if (!streamTexture_) return;

    // 取渲染器逻辑尺寸以定位右上角;未设置逻辑尺寸时回退到输出尺寸
    int logicalW = 0, logicalH = 0;
    SDL_RenderGetLogicalSize(renderer, &logicalW, &logicalH);
    if (logicalW <= 0) {
        SDL_GetRendererOutputSize(renderer, &logicalW, &logicalH);
    }
    if (logicalW <= 0) logicalW = streamW_ + 20;

    int margin = 10;
    SDL_Rect dst = { logicalW - streamW_ - margin, margin, streamW_, streamH_ };
    SDL_RenderCopy(renderer, streamTexture_, nullptr, &dst);
}

